#include "charactercreation.hpp"

#include <MyGUI_ITexture.h>

#include <components/debug/debuglog.hpp>
#include <components/fallback/fallback.hpp>
#include <components/misc/rng.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/npcstats.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/player.hpp"

#include "birth.hpp"
#include "class.hpp"
#include "inventorywindow.hpp"
#include "race.hpp"
#include "review.hpp"
#include "textinput.hpp"

namespace
{
    struct Response
    {
        const std::string mText;
        const ESM::Class::Specialization mSpecialization;
    };

    struct Step
    {
        const std::string mText;
        const Response mResponses[3];
        const std::string mSound;
    };

    Step sGenerateClassSteps(int number)
    {
        number++;

        std::string question{ Fallback::Map::getString("Question_" + MyGUI::utility::toString(number) + "_Question") };
        std::string answer0{ Fallback::Map::getString("Question_" + MyGUI::utility::toString(number) + "_AnswerOne") };
        std::string answer1{ Fallback::Map::getString("Question_" + MyGUI::utility::toString(number) + "_AnswerTwo") };
        std::string answer2{ Fallback::Map::getString(
            "Question_" + MyGUI::utility::toString(number) + "_AnswerThree") };
        std::string sound = "vo\\misc\\chargen qa" + MyGUI::utility::toString(number) + ".wav";

        Response r0 = { answer0, ESM::Class::Combat };
        Response r1 = { answer1, ESM::Class::Magic };
        Response r2 = { answer2, ESM::Class::Stealth };

        // randomize order in which responses are displayed
        int order = Misc::Rng::rollDice(6);

        switch (order)
        {
            case 0:
                return { question, { r0, r1, r2 }, sound };
            case 1:
                return { question, { r0, r2, r1 }, sound };
            case 2:
                return { question, { r1, r0, r2 }, sound };
            case 3:
                return { question, { r1, r2, r0 }, sound };
            case 4:
                return { question, { r2, r0, r1 }, sound };
            default:
                return { question, { r2, r1, r0 }, sound };
        }
    }
}

namespace MWGui
{

    CharacterCreation::CharacterCreation(osg::Group* parent, Resource::ResourceSystem* resourceSystem)
        : mParent(parent)
        , mResourceSystem(resourceSystem)
        , mGenerateClassStep(0)
    {
        mCreationStage = CSE_NotStarted;
        mGenerateClassResponses[0] = ESM::Class::Combat;
        mGenerateClassResponses[1] = ESM::Class::Magic;
        mGenerateClassResponses[2] = ESM::Class::Stealth;
        mGenerateClassSpecializations[0] = 0;
        mGenerateClassSpecializations[1] = 0;
        mGenerateClassSpecializations[2] = 0;

        // Setup player stats
        for (int i = 0; i < ESM::Attribute::Length; ++i)
            mPlayerAttributes.emplace(ESM::Attribute::sAttributeIds[i], MWMechanics::AttributeValue());

        for (int i = 0; i < ESM::Skill::Length; ++i)
            mPlayerSkillValues.emplace(ESM::Skill::sSkillIds[i], MWMechanics::SkillValue());
    }

    void CharacterCreation::setValue(std::string_view id, const MWMechanics::AttributeValue& value)
    {
        static const char* ids[] = {
            "AttribVal1",
            "AttribVal2",
            "AttribVal3",
            "AttribVal4",
            "AttribVal5",
            "AttribVal6",
            "AttribVal7",
            "AttribVal8",
            nullptr,
        };

        for (int i = 0; ids[i]; ++i)
        {
            if (ids[i] == id)
            {
                mPlayerAttributes[static_cast<ESM::Attribute::AttributeID>(i)] = value;
                if (mReviewDialog)
                    mReviewDialog->setAttribute(static_cast<ESM::Attribute::AttributeID>(i), value);

                break;
            }
        }
    }

    void CharacterCreation::setValue(std::string_view id, const MWMechanics::DynamicStat<float>& value)
    {
        if (mReviewDialog)
        {
            if (id == "HBar")
            {
                mReviewDialog->setHealth(value);
            }
            else if (id == "MBar")
            {
                mReviewDialog->setMagicka(value);
            }
            else if (id == "FBar")
            {
                mReviewDialog->setFatigue(value);
            }
        }
    }

    void CharacterCreation::setValue(const ESM::Skill::SkillEnum parSkill, const MWMechanics::SkillValue& value)
    {
        mPlayerSkillValues[parSkill] = value;
        if (mReviewDialog)
            mReviewDialog->setSkillValue(parSkill, value);
    }

    void CharacterCreation::configureSkills(const SkillList& major, const SkillList& minor)
    {
        if (mReviewDialog)
            mReviewDialog->configureSkills(major, minor);

        mPlayerMajorSkills = major;
        mPlayerMinorSkills = minor;
    }

    void CharacterCreation::onFrame(float duration)
    {
        if (mReviewDialog)
            mReviewDialog->onFrame(duration);
    }

    void CharacterCreation::spawnDialog(const char id)
    {
        try
        {
            switch (id)
            {
                case GM_Name:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mNameDialog));
                    mNameDialog = std::make_unique<TextInputDialog>();
                    mNameDialog->setTextLabel(
                        MWBase::Environment::get().getWindowManager()->getGameSettingString("sName", "Name"));
                    mNameDialog->setTextInput(mPlayerName);
                    mNameDialog->setNextButtonShow(mCreationStage >= CSE_NameChosen);
                    mNameDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onNameDialogDone);
                    mNameDialog->setVisible(true);
                    break;

                case GM_Race:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mRaceDialog));
                    mRaceDialog = std::make_unique<RaceDialog>(mParent, mResourceSystem);
                    mRaceDialog->setNextButtonShow(mCreationStage >= CSE_RaceChosen);
                    mRaceDialog->setRaceId(mPlayerRaceId);
                    mRaceDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onRaceDialogDone);
                    mRaceDialog->eventBack += MyGUI::newDelegate(this, &CharacterCreation::onRaceDialogBack);
                    mRaceDialog->setVisible(true);
                    if (mCreationStage < CSE_NameChosen)
                        mCreationStage = CSE_NameChosen;
                    break;

                case GM_Class:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mClassChoiceDialog));
                    mClassChoiceDialog = std::make_unique<ClassChoiceDialog>();
                    mClassChoiceDialog->eventButtonSelected
                        += MyGUI::newDelegate(this, &CharacterCreation::onClassChoice);
                    mClassChoiceDialog->setVisible(true);
                    if (mCreationStage < CSE_RaceChosen)
                        mCreationStage = CSE_RaceChosen;
                    break;

                case GM_ClassPick:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mPickClassDialog));
                    mPickClassDialog = std::make_unique<PickClassDialog>();
                    mPickClassDialog->setNextButtonShow(mCreationStage >= CSE_ClassChosen);
                    mPickClassDialog->setClassId(mPlayerClass.mId);
                    mPickClassDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onPickClassDialogDone);
                    mPickClassDialog->eventBack += MyGUI::newDelegate(this, &CharacterCreation::onPickClassDialogBack);
                    mPickClassDialog->setVisible(true);
                    if (mCreationStage < CSE_RaceChosen)
                        mCreationStage = CSE_RaceChosen;
                    break;

                case GM_Birth:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mBirthSignDialog));
                    mBirthSignDialog = std::make_unique<BirthDialog>();
                    mBirthSignDialog->setNextButtonShow(mCreationStage >= CSE_BirthSignChosen);
                    mBirthSignDialog->setBirthId(mPlayerBirthSignId);
                    mBirthSignDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onBirthSignDialogDone);
                    mBirthSignDialog->eventBack += MyGUI::newDelegate(this, &CharacterCreation::onBirthSignDialogBack);
                    mBirthSignDialog->setVisible(true);
                    if (mCreationStage < CSE_ClassChosen)
                        mCreationStage = CSE_ClassChosen;
                    break;

                case GM_ClassCreate:
                    if (mCreateClassDialog == nullptr)
                    {
                        mCreateClassDialog = std::make_unique<CreateClassDialog>();
                        mCreateClassDialog->eventDone
                            += MyGUI::newDelegate(this, &CharacterCreation::onCreateClassDialogDone);
                        mCreateClassDialog->eventBack
                            += MyGUI::newDelegate(this, &CharacterCreation::onCreateClassDialogBack);
                    }
                    mCreateClassDialog->setNextButtonShow(mCreationStage >= CSE_ClassChosen);
                    mCreateClassDialog->setVisible(true);
                    if (mCreationStage < CSE_RaceChosen)
                        mCreationStage = CSE_RaceChosen;
                    break;
                case GM_ClassGenerate:
                    mGenerateClassStep = 0;
                    mGenerateClass = ESM::RefId();
                    mGenerateClassSpecializations[0] = 0;
                    mGenerateClassSpecializations[1] = 0;
                    mGenerateClassSpecializations[2] = 0;
                    showClassQuestionDialog();
                    if (mCreationStage < CSE_RaceChosen)
                        mCreationStage = CSE_RaceChosen;
                    break;
                case GM_Review:
                    MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mReviewDialog));
                    mReviewDialog = std::make_unique<ReviewDialog>();

                    MWBase::World* world = MWBase::Environment::get().getWorld();

                    const ESM::NPC* playerNpc = world->getPlayerPtr().get<ESM::NPC>()->mBase;

                    const MWWorld::Player player = world->getPlayer();

                    const ESM::Class* playerClass = world->getStore().get<ESM::Class>().find(playerNpc->mClass);

                    mReviewDialog->setPlayerName(playerNpc->mName);
                    mReviewDialog->setRace(playerNpc->mRace);
                    mReviewDialog->setClass(*playerClass);
                    mReviewDialog->setBirthSign(player.getBirthSign());

                    MWWorld::Ptr playerPtr = MWMechanics::getPlayer();
                    const MWMechanics::CreatureStats& stats = playerPtr.getClass().getCreatureStats(playerPtr);

                    mReviewDialog->setHealth(stats.getHealth());
                    mReviewDialog->setMagicka(stats.getMagicka());
                    mReviewDialog->setFatigue(stats.getFatigue());
                    for (auto& attributePair : mPlayerAttributes)
                    {
                        mReviewDialog->setAttribute(
                            static_cast<ESM::Attribute::AttributeID>(attributePair.first), attributePair.second);
                    }
                    for (auto& skillPair : mPlayerSkillValues)
                    {
                        mReviewDialog->setSkillValue(
                            static_cast<ESM::Skill::SkillEnum>(skillPair.first), skillPair.second);
                    }
                    mReviewDialog->configureSkills(mPlayerMajorSkills, mPlayerMinorSkills);

                    mReviewDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onReviewDialogDone);
                    mReviewDialog->eventBack += MyGUI::newDelegate(this, &CharacterCreation::onReviewDialogBack);
                    mReviewDialog->eventActivateDialog
                        += MyGUI::newDelegate(this, &CharacterCreation::onReviewActivateDialog);
                    mReviewDialog->setVisible(true);
                    if (mCreationStage < CSE_BirthSignChosen)
                        mCreationStage = CSE_BirthSignChosen;
                    break;
            }
        }
        catch (std::exception& e)
        {
            Log(Debug::Error) << "Error: Failed to create chargen window: " << e.what();
        }
    }

    void CharacterCreation::onReviewDialogDone(WindowBase* parWindow)
    {
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mReviewDialog));
        MWBase::Environment::get().getWindowManager()->popGuiMode();
    }

    void CharacterCreation::onReviewDialogBack()
    {
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mReviewDialog));
        mCreationStage = CSE_ReviewBack;

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Birth);
    }

    void CharacterCreation::onReviewActivateDialog(int parDialog)
    {
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mReviewDialog));
        mCreationStage = CSE_ReviewNext;

        MWBase::Environment::get().getWindowManager()->popGuiMode();

        switch (parDialog)
        {
            case ReviewDialog::NAME_DIALOG:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Name);
                break;
            case ReviewDialog::RACE_DIALOG:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Race);
                break;
            case ReviewDialog::CLASS_DIALOG:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
                break;
            case ReviewDialog::BIRTHSIGN_DIALOG:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Birth);
        };
    }

    void CharacterCreation::selectPickedClass()
    {
        if (mPickClassDialog)
        {
            const ESM::RefId& classId = mPickClassDialog->getClassId();
            if (!classId.empty())
                MWBase::Environment::get().getMechanicsManager()->setPlayerClass(classId);

            const ESM::Class* klass = MWBase::Environment::get().getWorld()->getStore().get<ESM::Class>().find(classId);
            if (klass)
            {
                mPlayerClass = *klass;
            }
            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mPickClassDialog));
        }
    }

    void CharacterCreation::onPickClassDialogDone(WindowBase* parWindow)
    {
        selectPickedClass();

        handleDialogDone(CSE_ClassChosen, GM_Birth);
    }

    void CharacterCreation::onPickClassDialogBack()
    {
        selectPickedClass();

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
    }

    void CharacterCreation::onClassChoice(int _index)
    {
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mClassChoiceDialog));

        MWBase::Environment::get().getWindowManager()->popGuiMode();

        switch (_index)
        {
            case ClassChoiceDialog::Class_Generate:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_ClassGenerate);
                break;
            case ClassChoiceDialog::Class_Pick:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_ClassPick);
                break;
            case ClassChoiceDialog::Class_Create:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_ClassCreate);
                break;
            case ClassChoiceDialog::Class_Back:
                MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Race);
                break;
        };
    }

    void CharacterCreation::onNameDialogDone(WindowBase* parWindow)
    {
        if (mNameDialog)
        {
            mPlayerName = mNameDialog->getTextInput();
            MWBase::Environment::get().getMechanicsManager()->setPlayerName(mPlayerName);
            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mNameDialog));
        }

        handleDialogDone(CSE_NameChosen, GM_Race);
    }

    void CharacterCreation::selectRace()
    {
        if (mRaceDialog)
        {
            const ESM::NPC& data = mRaceDialog->getResult();
            mPlayerRaceId = data.mRace;
            if (!mPlayerRaceId.empty())
            {
                MWBase::Environment::get().getMechanicsManager()->setPlayerRace(
                    data.mRace, data.isMale(), data.mHead, data.mHair);
            }
            MWBase::Environment::get().getWindowManager()->getInventoryWindow()->rebuildAvatar();

            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mRaceDialog));
        }
    }

    void CharacterCreation::onRaceDialogBack()
    {
        selectRace();

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Name);
    }

    void CharacterCreation::onRaceDialogDone(WindowBase* parWindow)
    {
        selectRace();

        handleDialogDone(CSE_RaceChosen, GM_Class);
    }

    void CharacterCreation::selectBirthSign()
    {
        if (mBirthSignDialog)
        {
            mPlayerBirthSignId = mBirthSignDialog->getBirthId();
            if (!mPlayerBirthSignId.empty())
                MWBase::Environment::get().getMechanicsManager()->setPlayerBirthsign(mPlayerBirthSignId);
            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mBirthSignDialog));
        }
    }

    void CharacterCreation::onBirthSignDialogDone(WindowBase* parWindow)
    {
        selectBirthSign();

        handleDialogDone(CSE_BirthSignChosen, GM_Review);
    }

    void CharacterCreation::onBirthSignDialogBack()
    {
        selectBirthSign();

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
    }

    void CharacterCreation::selectCreatedClass()
    {
        if (mCreateClassDialog)
        {
            ESM::Class klass;
            klass.mName = mCreateClassDialog->getName();
            klass.mDescription = mCreateClassDialog->getDescription();
            klass.mData.mSpecialization = mCreateClassDialog->getSpecializationId();
            klass.mData.mIsPlayable = 0x1;
            klass.mRecordFlags = 0;

            std::vector<int> attributes = mCreateClassDialog->getFavoriteAttributes();
            assert(attributes.size() == 2);
            klass.mData.mAttribute[0] = attributes[0];
            klass.mData.mAttribute[1] = attributes[1];

            std::vector<ESM::Skill::SkillEnum> majorSkills = mCreateClassDialog->getMajorSkills();
            std::vector<ESM::Skill::SkillEnum> minorSkills = mCreateClassDialog->getMinorSkills();
            assert(majorSkills.size() >= sizeof(klass.mData.mSkills) / sizeof(klass.mData.mSkills[0]));
            assert(minorSkills.size() >= sizeof(klass.mData.mSkills) / sizeof(klass.mData.mSkills[0]));
            for (size_t i = 0; i < sizeof(klass.mData.mSkills) / sizeof(klass.mData.mSkills[0]); ++i)
            {
                klass.mData.mSkills[i][1] = majorSkills[i];
                klass.mData.mSkills[i][0] = minorSkills[i];
            }

            MWBase::Environment::get().getMechanicsManager()->setPlayerClass(klass);
            mPlayerClass = klass;

            // Do not delete dialog, so that choices are remembered in case we want to go back and adjust them later
            mCreateClassDialog->setVisible(false);
        }
    }

    void CharacterCreation::onCreateClassDialogDone(WindowBase* parWindow)
    {
        selectCreatedClass();

        handleDialogDone(CSE_ClassChosen, GM_Birth);
    }

    void CharacterCreation::onCreateClassDialogBack()
    {
        // not done in MW, but we do it for consistency with the other dialogs
        selectCreatedClass();

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
    }

    void CharacterCreation::onClassQuestionChosen(int _index)
    {
        MWBase::Environment::get().getSoundManager()->stopSay();

        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mGenerateClassQuestionDialog));

        if (_index < 0 || _index >= 3)
        {
            MWBase::Environment::get().getWindowManager()->popGuiMode();
            MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
            return;
        }

        ESM::Class::Specialization specialization = mGenerateClassResponses[_index];
        if (specialization == ESM::Class::Combat)
            ++mGenerateClassSpecializations[0];
        else if (specialization == ESM::Class::Magic)
            ++mGenerateClassSpecializations[1];
        else if (specialization == ESM::Class::Stealth)
            ++mGenerateClassSpecializations[2];
        ++mGenerateClassStep;
        showClassQuestionDialog();
    }

    void CharacterCreation::showClassQuestionDialog()
    {
        if (mGenerateClassStep == 10)
        {
            unsigned combat = mGenerateClassSpecializations[0];
            unsigned magic = mGenerateClassSpecializations[1];
            unsigned stealth = mGenerateClassSpecializations[2];
            std::string_view className;
            if (combat > 7)
            {
                className = "Warrior";
            }
            else if (magic > 7)
            {
                className = "Mage";
            }
            else if (stealth > 7)
            {
                className = "Thief";
            }
            else
            {
                switch (combat)
                {
                    case 4:
                        className = "Rogue";
                        break;
                    case 5:
                        if (stealth == 3)
                            className = "Scout";
                        else
                            className = "Archer";
                        break;
                    case 6:
                        if (stealth == 1)
                            className = "Barbarian";
                        else if (stealth == 3)
                            className = "Crusader";
                        else
                            className = "Knight";
                        break;
                    case 7:
                        className = "Warrior";
                        break;
                    default:
                        switch (magic)
                        {
                            case 4:
                                className = "Spellsword";
                                break;
                            case 5:
                                className = "Witchhunter";
                                break;
                            case 6:
                                if (combat == 2)
                                    className = "Sorcerer";
                                else if (combat == 3)
                                    className = "Healer";
                                else
                                    className = "Battlemage";
                                break;
                            case 7:
                                className = "Mage";
                                break;
                            default:
                                switch (stealth)
                                {
                                    case 3:
                                        if (magic == 3)
                                            className = "Bard"; // unreachable
                                        else
                                            className = "Warrior";
                                        break;
                                    case 5:
                                        if (magic == 3)
                                            className = "Monk";
                                        else
                                            className = "Pilgrim";
                                        break;
                                    case 6:
                                        if (magic == 1)
                                            className = "Agent";
                                        else if (magic == 3)
                                            className = "Assassin";
                                        else
                                            className = "Acrobat";
                                        break;
                                    case 7:
                                        className = "Thief";
                                        break;
                                    default:
                                        className = "Warrior";
                                }
                        }
                }
            }
            mGenerateClass = ESM::RefId::stringRefId(className);
            MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mGenerateClassResultDialog));

            mGenerateClassResultDialog = std::make_unique<GenerateClassResultDialog>();
            mGenerateClassResultDialog->setClassId(mGenerateClass);
            mGenerateClassResultDialog->eventBack += MyGUI::newDelegate(this, &CharacterCreation::onGenerateClassBack);
            mGenerateClassResultDialog->eventDone += MyGUI::newDelegate(this, &CharacterCreation::onGenerateClassDone);
            mGenerateClassResultDialog->setVisible(true);
            return;
        }

        if (mGenerateClassStep > 10)
        {
            MWBase::Environment::get().getWindowManager()->popGuiMode();
            MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
            return;
        }

        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mGenerateClassQuestionDialog));

        mGenerateClassQuestionDialog = std::make_unique<InfoBoxDialog>();

        Step step = sGenerateClassSteps(mGenerateClassStep);
        mGenerateClassResponses[0] = step.mResponses[0].mSpecialization;
        mGenerateClassResponses[1] = step.mResponses[1].mSpecialization;
        mGenerateClassResponses[2] = step.mResponses[2].mSpecialization;

        InfoBoxDialog::ButtonList buttons;
        mGenerateClassQuestionDialog->setText(step.mText);
        buttons.push_back(step.mResponses[0].mText);
        buttons.push_back(step.mResponses[1].mText);
        buttons.push_back(step.mResponses[2].mText);
        mGenerateClassQuestionDialog->setButtons(buttons);
        mGenerateClassQuestionDialog->eventButtonSelected
            += MyGUI::newDelegate(this, &CharacterCreation::onClassQuestionChosen);
        mGenerateClassQuestionDialog->setVisible(true);

        MWBase::Environment::get().getSoundManager()->say(step.mSound);
    }

    void CharacterCreation::selectGeneratedClass()
    {
        MWBase::Environment::get().getWindowManager()->removeDialog(std::move(mGenerateClassResultDialog));

        MWBase::Environment::get().getMechanicsManager()->setPlayerClass(mGenerateClass);

        const ESM::Class* klass
            = MWBase::Environment::get().getWorld()->getStore().get<ESM::Class>().find(mGenerateClass);

        mPlayerClass = *klass;
    }

    void CharacterCreation::onGenerateClassBack()
    {
        selectGeneratedClass();

        MWBase::Environment::get().getWindowManager()->popGuiMode();
        MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Class);
    }

    void CharacterCreation::onGenerateClassDone(WindowBase* parWindow)
    {
        selectGeneratedClass();

        handleDialogDone(CSE_ClassChosen, GM_Birth);
    }

    CharacterCreation::~CharacterCreation() = default;

    void CharacterCreation::handleDialogDone(CSE currentStage, int nextMode)
    {
        MWBase::Environment::get().getWindowManager()->popGuiMode();
        if (mCreationStage == CSE_ReviewNext)
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode(GM_Review);
        }
        else if (mCreationStage >= currentStage)
        {
            MWBase::Environment::get().getWindowManager()->pushGuiMode((GuiMode)nextMode);
        }
        else
        {
            mCreationStage = currentStage;
        }
    }
}
