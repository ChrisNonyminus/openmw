#include "statemanagerimp.hpp"

#include <filesystem>

#include <components/debug/debuglog.hpp>

#include <components/esm3/cellid.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/loadcell.hpp>
#include <components/esm3/loadclas.hpp>

#include <components/loadinglistener/loadinglistener.hpp>

#include <components/files/conversion.hpp>
#include <components/settings/settings.hpp>

#include <osg/Image>

#include <osgDB/Registry>

#include "../mwbase/dialoguemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/journal.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/globals.hpp"
#include "../mwworld/scene.hpp"
#include "../mwworld/worldmodel.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwscript/globalscripts.hpp"

#include "quicksavemanager.hpp"

void MWState::StateManager::cleanup(bool force)
{
    if (mState != State_NoGame || force)
    {
        MWBase::Environment::get().getSoundManager()->clear();
        MWBase::Environment::get().getDialogueManager()->clear();
        MWBase::Environment::get().getJournal()->clear();
        MWBase::Environment::get().getScriptManager()->clear();
        MWBase::Environment::get().getWindowManager()->clear();
        MWBase::Environment::get().getWorld()->clear();
        MWBase::Environment::get().getInputManager()->clear();
        MWBase::Environment::get().getMechanicsManager()->clear();

        mState = State_NoGame;
        mCharacterManager.setCurrentCharacter(nullptr);
        mTimePlayed = 0;

        MWMechanics::CreatureStats::cleanup();
    }
    MWBase::Environment::get().getLuaManager()->clear();
}

std::map<int, int> MWState::StateManager::buildContentFileIndexMap(const ESM::ESMReader& reader) const
{
    const std::vector<std::string>& current = MWBase::Environment::get().getWorld()->getContentFiles();

    const std::vector<ESM::Header::MasterData>& prev = reader.getGameFiles();

    std::map<int, int> map;

    for (int iPrev = 0; iPrev < static_cast<int>(prev.size()); ++iPrev)
    {
        std::string id = Misc::StringUtils::lowerCase(prev[iPrev].name);

        for (int iCurrent = 0; iCurrent < static_cast<int>(current.size()); ++iCurrent)
            if (id == Misc::StringUtils::lowerCase(current[iCurrent]))
            {
                map.insert(std::make_pair(iPrev, iCurrent));
                break;
            }
    }

    return map;
}

MWState::StateManager::StateManager(const std::filesystem::path& saves, const std::vector<std::string>& contentFiles)
    : mQuitRequest(false)
    , mAskLoadRecent(false)
    , mState(State_NoGame)
    , mCharacterManager(saves, contentFiles)
    , mTimePlayed(0)
{
}

void MWState::StateManager::requestQuit()
{
    mQuitRequest = true;
}

bool MWState::StateManager::hasQuitRequest() const
{
    return mQuitRequest;
}

void MWState::StateManager::askLoadRecent()
{
    if (MWBase::Environment::get().getWindowManager()->getMode() == MWGui::GM_MainMenu)
        return;

    if (!mAskLoadRecent)
    {
        const MWState::Character* character = getCurrentCharacter();
        if (!character || character->begin() == character->end()) // no saves
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
        }
        else
        {
            MWState::Slot lastSave = *character->begin();
            std::vector<std::string> buttons;
            buttons.emplace_back("#{sYes}");
            buttons.emplace_back("#{sNo}");
            std::string_view tag = "%s";
            std::string message{ MWBase::Environment::get().getWindowManager()->getGameSettingString(
                "sLoadLastSaveMsg", tag) };
            size_t pos = message.find(tag);
            message.replace(pos, tag.length(), lastSave.mProfile.mDescription);
            MWBase::Environment::get().getWindowManager()->interactiveMessageBox(message, buttons);
            mAskLoadRecent = true;
        }
    }
}

MWState::StateManager::State MWState::StateManager::getState() const
{
    return mState;
}

void MWState::StateManager::newGame(bool bypass)
{
    cleanup();

    if (!bypass)
        MWBase::Environment::get().getWindowManager()->setNewGame(true);

    try
    {
        Log(Debug::Info) << "Starting a new game";
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().addStartup();
        MWBase::Environment::get().getLuaManager()->newGameStarted();
        MWBase::Environment::get().getWorld()->startNewGame(bypass);

        mState = State_Running;

        MWBase::Environment::get().getWindowManager()->fadeScreenOut(0);
        MWBase::Environment::get().getWindowManager()->fadeScreenIn(1);
    }
    catch (std::exception& e)
    {
        std::stringstream error;
        error << "Failed to start new game: " << e.what();

        Log(Debug::Error) << error.str();
        cleanup(true);

        MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);

        std::vector<std::string> buttons;
        buttons.emplace_back("#{sOk}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(error.str(), buttons);
    }
}

void MWState::StateManager::endGame()
{
    mState = State_Ended;
}

void MWState::StateManager::resumeGame()
{
    mState = State_Running;
}

void MWState::StateManager::saveGame(const std::string& description, const Slot* slot)
{
    MWState::Character* character = getCurrentCharacter();

    try
    {
        const auto start = std::chrono::steady_clock::now();

        MWBase::Environment::get().getWindowManager()->asyncPrepareSaveMap();

        if (!character)
        {
            MWWorld::ConstPtr player = MWMechanics::getPlayer();
            const std::string& name = player.get<ESM::NPC>()->mBase->mName;

            character = mCharacterManager.createCharacter(name);
            mCharacterManager.setCurrentCharacter(character);
        }

        ESM::SavedGame profile;

        MWBase::World& world = *MWBase::Environment::get().getWorld();

        MWWorld::Ptr player = world.getPlayerPtr();

        profile.mContentFiles = world.getContentFiles();

        profile.mPlayerName = player.get<ESM::NPC>()->mBase->mName;
        profile.mPlayerLevel = player.getClass().getNpcStats(player).getLevel();

        const ESM::RefId& classId = player.get<ESM::NPC>()->mBase->mClass;
        if (world.getStore().get<ESM::Class>().isDynamic(classId))
            profile.mPlayerClassName = world.getStore().get<ESM::Class>().find(classId)->mName;
        else
            profile.mPlayerClassId = classId;

        profile.mPlayerCell = ESM::RefId::stringRefId(world.getCellName());
        profile.mInGameTime = world.getEpochTimeStamp();
        profile.mTimePlayed = mTimePlayed;
        profile.mDescription = description;

        Log(Debug::Info) << "Making a screenshot for saved game '" << description << "'";
        writeScreenshot(profile.mScreenshot);

        if (!slot)
            slot = character->createSlot(profile);
        else
            slot = character->updateSlot(slot, profile);

        // Make sure the animation state held by references is up to date before saving the game.
        MWBase::Environment::get().getMechanicsManager()->persistAnimationStates();

        Log(Debug::Info) << "Writing saved game '" << description << "' for character '" << profile.mPlayerName << "'";

        // Write to a memory stream first. If there is an exception during the save process, we don't want to trash the
        // existing save file we are overwriting.
        std::stringstream stream;

        ESM::ESMWriter writer;

        for (const std::string& contentFile : MWBase::Environment::get().getWorld()->getContentFiles())
            writer.addMaster(contentFile, 0); // not using the size information anyway -> use value of 0

        writer.setFormatVersion(ESM::CurrentSaveGameFormatVersion);

        // all unused
        writer.setVersion(0);
        writer.setType(0);
        writer.setAuthor("");
        writer.setDescription("");

        int recordCount = 1 // saved game header
            + MWBase::Environment::get().getJournal()->countSavedGameRecords()
            + MWBase::Environment::get().getLuaManager()->countSavedGameRecords()
            + MWBase::Environment::get().getWorld()->countSavedGameRecords()
            + MWBase::Environment::get().getScriptManager()->getGlobalScripts().countSavedGameRecords()
            + MWBase::Environment::get().getDialogueManager()->countSavedGameRecords()
            + MWBase::Environment::get().getMechanicsManager()->countSavedGameRecords()
            + MWBase::Environment::get().getInputManager()->countSavedGameRecords()
            + MWBase::Environment::get().getWindowManager()->countSavedGameRecords();
        writer.setRecordCount(recordCount);

        writer.save(stream);

        Loading::Listener& listener = *MWBase::Environment::get().getWindowManager()->getLoadingScreen();
        // Using only Cells for progress information, since they typically have the largest records by far
        listener.setProgressRange(MWBase::Environment::get().getWorld()->countSavedGameCells());
        listener.setLabel("#{sNotifyMessage4}", true);

        Loading::ScopedLoad load(&listener);

        writer.startRecord(ESM::REC_SAVE);
        slot->mProfile.save(writer);
        writer.endRecord(ESM::REC_SAVE);

        MWBase::Environment::get().getJournal()->write(writer, listener);
        MWBase::Environment::get().getDialogueManager()->write(writer, listener);
        // LuaManager::write should be called before World::write because world also saves
        // local scripts that depend on LuaManager.
        MWBase::Environment::get().getLuaManager()->write(writer, listener);
        MWBase::Environment::get().getWorld()->write(writer, listener);
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().write(writer, listener);
        MWBase::Environment::get().getMechanicsManager()->write(writer, listener);
        MWBase::Environment::get().getInputManager()->write(writer, listener);
        MWBase::Environment::get().getWindowManager()->write(writer, listener);

        // Ensure we have written the number of records that was estimated
        if (writer.getRecordCount() != recordCount + 1) // 1 extra for TES3 record
            Log(Debug::Warning) << "Warning: number of written savegame records does not match. Estimated: "
                                << recordCount + 1 << ", written: " << writer.getRecordCount();

        writer.close();

        if (stream.fail())
            throw std::runtime_error("Write operation failed (memory stream)");

        // All good, write to file
        std::ofstream filestream(slot->mPath, std::ios::binary);
        filestream << stream.rdbuf();

        if (filestream.fail())
            throw std::runtime_error("Write operation failed (file stream)");

        Settings::Manager::setString(
            "character", "Saves", Files::pathToUnicodeString(slot->mPath.parent_path().filename()));

        const auto finish = std::chrono::steady_clock::now();

        Log(Debug::Info) << '\'' << description << "' is saved in "
                         << std::chrono::duration_cast<std::chrono::duration<float, std::milli>>(finish - start).count()
                         << "ms";
    }
    catch (const std::exception& e)
    {
        std::stringstream error;
        error << "Failed to save game: " << e.what();

        Log(Debug::Error) << error.str();

        std::vector<std::string> buttons;
        buttons.emplace_back("#{sOk}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(error.str(), buttons);

        // If no file was written, clean up the slot
        if (character && slot && !std::filesystem::exists(slot->mPath))
        {
            character->deleteSlot(slot);
            character->cleanup();
        }
    }
}

void MWState::StateManager::quickSave(std::string name)
{
    if (!(mState == State_Running
            && MWBase::Environment::get().getWorld()->getGlobalInt(MWWorld::Globals::sCharGenState) == -1 // char gen
            && MWBase::Environment::get().getWindowManager()->isSavingAllowed()))
    {
        // You can not save your game right now
        MWBase::Environment::get().getWindowManager()->messageBox("#{sSaveGameDenied}");
        return;
    }

    int maxSaves = Settings::Manager::getInt("max quicksaves", "Saves");
    if (maxSaves < 1)
        maxSaves = 1;

    Character* currentCharacter = getCurrentCharacter(); // Get current character
    QuickSaveManager saveFinder = QuickSaveManager(name, maxSaves);

    if (currentCharacter)
    {
        for (auto& save : *currentCharacter)
        {
            // Visiting slots allows the quicksave finder to find the oldest quicksave
            saveFinder.visitSave(&save);
        }
    }

    // Once all the saves have been visited, the save finder can tell us which
    // one to replace (or create)
    saveGame(name, saveFinder.getNextQuickSaveSlot());
}

void MWState::StateManager::loadGame(const std::filesystem::path& filepath)
{
    for (const auto& character : mCharacterManager)
    {
        for (const auto& slot : character)
        {
            if (slot.mPath == filepath)
            {
                loadGame(&character, slot.mPath);
                return;
            }
        }
    }

    MWState::Character* character = getCurrentCharacter();
    loadGame(character, filepath);
}

void MWState::StateManager::loadGame(const Character* character, const std::filesystem::path& filepath)
{
    try
    {
        cleanup();

        Log(Debug::Info) << "Reading save file " << filepath.filename();

        ESM::ESMReader reader;
        reader.open(filepath);

        if (reader.getFormatVersion() > ESM::CurrentSaveGameFormatVersion)
            throw std::runtime_error(
                "This save file was created using a newer version of OpenMW and is thus not supported. Please upgrade "
                "to the newest OpenMW version to load this file.");

        std::map<int, int> contentFileMap = buildContentFileIndexMap(reader);
        MWBase::Environment::get().getLuaManager()->setContentFileMapping(contentFileMap);

        Loading::Listener& listener = *MWBase::Environment::get().getWindowManager()->getLoadingScreen();

        listener.setProgressRange(100);
        listener.setLabel("#{sLoadingMessage14}");

        Loading::ScopedLoad load(&listener);

        bool firstPersonCam = false;

        size_t total = reader.getFileSize();
        int currentPercent = 0;
        while (reader.hasMoreRecs())
        {
            ESM::NAME n = reader.getRecName();
            reader.getRecHeader();

            switch (n.toInt())
            {
                case ESM::REC_SAVE:
                {
                    ESM::SavedGame profile;
                    profile.load(reader);
                    if (!verifyProfile(profile))
                    {
                        cleanup(true);
                        MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
                        return;
                    }
                    mTimePlayed = profile.mTimePlayed;
                    Log(Debug::Info) << "Loading saved game '" << profile.mDescription << "' for character '"
                                     << profile.mPlayerName << "'";
                }
                break;

                case ESM::REC_JOUR:
                case ESM::REC_JOUR_LEGACY:
                case ESM::REC_QUES:

                    MWBase::Environment::get().getJournal()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_DIAS:

                    MWBase::Environment::get().getDialogueManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_ALCH:
                case ESM::REC_ARMO:
                case ESM::REC_BOOK:
                case ESM::REC_CLAS:
                case ESM::REC_CLOT:
                case ESM::REC_ENCH:
                case ESM::REC_NPC_:
                case ESM::REC_SPEL:
                case ESM::REC_WEAP:
                case ESM::REC_GLOB:
                case ESM::REC_PLAY:
                case ESM::REC_CSTA:
                case ESM::REC_WTHR:
                case ESM::REC_DYNA:
                case ESM::REC_ACTC:
                case ESM::REC_PROJ:
                case ESM::REC_MPRJ:
                case ESM::REC_ENAB:
                case ESM::REC_LEVC:
                case ESM::REC_LEVI:
                case ESM::REC_CREA:
                case ESM::REC_CONT:
                case ESM::REC_RAND:
                    MWBase::Environment::get().getWorld()->readRecord(reader, n.toInt(), contentFileMap);
                    break;

                case ESM::REC_CAM_:
                    reader.getHNT(firstPersonCam, "FIRS");
                    break;

                case ESM::REC_GSCR:

                    MWBase::Environment::get().getScriptManager()->getGlobalScripts().readRecord(
                        reader, n.toInt(), contentFileMap);
                    break;

                case ESM::REC_GMAP:
                case ESM::REC_KEYS:
                case ESM::REC_ASPL:
                case ESM::REC_MARK:

                    MWBase::Environment::get().getWindowManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_DCOU:
                case ESM::REC_STLN:

                    MWBase::Environment::get().getMechanicsManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_INPU:
                    MWBase::Environment::get().getInputManager()->readRecord(reader, n.toInt());
                    break;

                case ESM::REC_LUAM:
                    MWBase::Environment::get().getLuaManager()->readRecord(reader, n.toInt());
                    break;

                default:

                    // ignore invalid records
                    Log(Debug::Warning) << "Warning: Ignoring unknown record: " << n.toStringView();
                    reader.skipRecord();
            }
            int progressPercent = static_cast<int>(float(reader.getFileOffset()) / total * 100);
            if (progressPercent > currentPercent)
            {
                listener.increaseProgress(progressPercent - currentPercent);
                currentPercent = progressPercent;
            }
        }

        mCharacterManager.setCurrentCharacter(character);

        mState = State_Running;

        if (character)
            Settings::Manager::setString(
                "character", "Saves", Files::pathToUnicodeString(character->getPath().filename()));

        MWBase::Environment::get().getWindowManager()->setNewGame(false);
        MWBase::Environment::get().getWorld()->saveLoaded();
        MWBase::Environment::get().getWorld()->setupPlayer();
        MWBase::Environment::get().getWorld()->renderPlayer();
        MWBase::Environment::get().getWindowManager()->updatePlayer();
        MWBase::Environment::get().getMechanicsManager()->playerLoaded();

        if (firstPersonCam != MWBase::Environment::get().getWorld()->isFirstPerson())
            MWBase::Environment::get().getWorld()->togglePOV();

        MWWorld::ConstPtr ptr = MWMechanics::getPlayer();

        if (ptr.isInCell())
        {
            const ESM::CellId& cellId = ptr.getCell()->getCell()->getCellId();

            // Use detectWorldSpaceChange=false, otherwise some of the data we just loaded would be cleared again
            MWBase::Environment::get().getWorld()->changeToCell(cellId, ptr.getRefData().getPosition(), false, false);
        }
        else
        {
            // Cell no longer exists (i.e. changed game files), choose a default cell
            Log(Debug::Warning) << "Warning: Player character's cell no longer exists, changing to the default cell";
            MWWorld::CellStore* cell = MWBase::Environment::get().getWorldModel()->getExterior(0, 0);
            float x, y;
            MWBase::Environment::get().getWorld()->indexToPosition(0, 0, x, y, false);
            ESM::Position pos;
            pos.pos[0] = x;
            pos.pos[1] = y;
            pos.pos[2] = 0; // should be adjusted automatically (adjustPlayerPos=true)
            pos.rot[0] = 0;
            pos.rot[1] = 0;
            pos.rot[2] = 0;
            MWBase::Environment::get().getWorld()->changeToCell(cell->getCell()->getCellId(), pos, true, false);
        }

        MWBase::Environment::get().getWorld()->updateProjectilesCasters();

        // Vanilla MW will restart startup scripts when a save game is loaded. This is unintuitive,
        // but some mods may be using it as a reload detector.
        MWBase::Environment::get().getScriptManager()->getGlobalScripts().addStartup();

        // Since we passed "changeEvent=false" to changeCell, we shouldn't have triggered the cell change flag.
        // But make sure the flag is cleared anyway in case it was set from an earlier game.
        MWBase::Environment::get().getWorldScene()->markCellAsUnchanged();

        MWBase::Environment::get().getLuaManager()->gameLoaded();
    }
    catch (const std::exception& e)
    {
        std::stringstream error;
        error << "Failed to load saved game: " << e.what();

        Log(Debug::Error) << error.str();
        cleanup(true);

        MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);

        std::vector<std::string> buttons;
        buttons.emplace_back("#{sOk}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox(error.str(), buttons);
    }
}

void MWState::StateManager::quickLoad()
{
    if (Character* currentCharacter = getCurrentCharacter())
    {
        if (currentCharacter->begin() == currentCharacter->end())
            return;
        loadGame(currentCharacter, currentCharacter->begin()->mPath); // Get newest save
    }
}

void MWState::StateManager::deleteGame(const MWState::Character* character, const MWState::Slot* slot)
{
    mCharacterManager.deleteSlot(character, slot);
}

MWState::Character* MWState::StateManager::getCurrentCharacter()
{
    return mCharacterManager.getCurrentCharacter();
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterBegin()
{
    return mCharacterManager.begin();
}

MWState::StateManager::CharacterIterator MWState::StateManager::characterEnd()
{
    return mCharacterManager.end();
}

void MWState::StateManager::update(float duration)
{
    mTimePlayed += duration;

    // Note: It would be nicer to trigger this from InputManager, i.e. the very beginning of the frame update.
    if (mAskLoadRecent)
    {
        int iButton = MWBase::Environment::get().getWindowManager()->readPressedButton();
        MWState::Character* curCharacter = getCurrentCharacter();
        if (iButton == 0 && curCharacter)
        {
            mAskLoadRecent = false;
            // Load last saved game for current character

            MWState::Slot lastSave = *curCharacter->begin();
            loadGame(curCharacter, lastSave.mPath);
        }
        else if (iButton == 1)
        {
            mAskLoadRecent = false;
            MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_MainMenu);
        }
    }
}

bool MWState::StateManager::verifyProfile(const ESM::SavedGame& profile) const
{
    const std::vector<std::string>& selectedContentFiles = MWBase::Environment::get().getWorld()->getContentFiles();
    bool notFound = false;
    for (const std::string& contentFile : profile.mContentFiles)
    {
        if (std::find(selectedContentFiles.begin(), selectedContentFiles.end(), contentFile)
            == selectedContentFiles.end())
        {
            Log(Debug::Warning) << "Warning: Saved game dependency " << contentFile << " is missing.";
            notFound = true;
        }
    }
    if (notFound)
    {
        std::vector<std::string> buttons;
        buttons.emplace_back("#{sYes}");
        buttons.emplace_back("#{sNo}");
        MWBase::Environment::get().getWindowManager()->interactiveMessageBox("#{sMissingMastersMsg}", buttons, true);
        int selectedButton = MWBase::Environment::get().getWindowManager()->readPressedButton();
        if (selectedButton == 1 || selectedButton == -1)
            return false;
    }
    return true;
}

void MWState::StateManager::writeScreenshot(std::vector<char>& imageData) const
{
    int screenshotW = 259 * 2, screenshotH = 133 * 2; // *2 to get some nice antialiasing

    osg::ref_ptr<osg::Image> screenshot(new osg::Image);

    MWBase::Environment::get().getWorld()->screenshot(screenshot.get(), screenshotW, screenshotH);

    osgDB::ReaderWriter* readerwriter = osgDB::Registry::instance()->getReaderWriterForExtension("jpg");
    if (!readerwriter)
    {
        Log(Debug::Error) << "Error: Unable to write screenshot, can't find a jpg ReaderWriter";
        return;
    }

    std::ostringstream ostream;
    osgDB::ReaderWriter::WriteResult result = readerwriter->writeImage(*screenshot, ostream);
    if (!result.success())
    {
        Log(Debug::Error) << "Error: Unable to write screenshot: " << result.message() << " code " << result.status();
        return;
    }

    std::string data = ostream.str();
    imageData = std::vector<char>(data.begin(), data.end());
}
