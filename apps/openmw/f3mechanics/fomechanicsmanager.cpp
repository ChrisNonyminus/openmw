#include "fomechanicsmanager.hpp"

#include <components/misc/rng.hpp>

#include <components/esm4/loadgmst.hpp>
#include <components/esm4/records.hpp>

#include <components/sceneutil/positionattitudetransform.hpp>


#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/ptr.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/dialoguemanager.hpp"

#include "aiactions.hpp"
#include "actors.hpp"
#include "usings.hpp"
#include "stats.hpp"

namespace
{
    bool isOwned(const MWWorld::Ptr& ptr, const MWWorld::Ptr& target, MWWorld::Ptr& victim)
    {
        const MWWorld::CellRef& cellref = target.getCellRef();

        /*const std::string& owner = cellref.getOwner();
        bool isOwned = !owner.empty() && owner != "player";*/
        bool isOwned = false;

        ESM4::FormId faction = cellref.getESM4Faction();
        bool isFactionOwned = false;
        if (faction != 0 && ptr.getClass().isNpc())
        {
            const std::map<ESM4::FormId, int>& factions = ptr.getClass().getFOStats(ptr).mFactionRanks;
            auto found = factions.find(faction);
            if (found == factions.end() || found->second < cellref.getFactionRank())
                isFactionOwned = true;
        }

        /*const std::string& globalVariable = cellref.getGlobalVariable();
        if (!globalVariable.empty() && MWBase::Environment::get().getWorld()->getGlobalInt(globalVariable))
        {
            isOwned = false;
            isFactionOwned = false;
        }*/

        /*if (isOwned)
            victim = MWBase::Environment::get().getWorld()->searchPtrViaFormId(cellref.getESM4Faction());*/

        return isOwned || isFactionOwned;
    }
}

namespace F3Mechanics
{
    void MechanicsManager::buildPlayer()
    {
        MWWorld::Ptr ptr = MWMechanics::getPlayer();

        Stats& stats = ptr.getClass().getFOStats(ptr);

        const ESM4::Npc* player = ptr.get<ESM4::Npc>()->mBase;

        // reset
        stats.mLevel = player->mBaseConfig.fo3.levelOrMult;

        const auto& store = MWBase::Environment::get().getWorld()->getStore();
        const ESM4::Class* clas = store.get<ESM4::Class>().find(player->mClass);
        stats.mStats[Stat_Strength].setBase(clas->mSpecial.strength);
        stats.mStats[Stat_Perception].setBase(clas->mSpecial.perception);
        stats.mStats[Stat_Endurance].setBase(clas->mSpecial.endurance);
        stats.mStats[Stat_Charisma].setBase(clas->mSpecial.charisma);
        stats.mStats[Stat_Intelligence].setBase(clas->mSpecial.intelligence);
        stats.mStats[Stat_Agility].setBase(clas->mSpecial.agility);
        stats.mStats[Stat_Luck].setBase(clas->mSpecial.luck);
        static const float fAVDTagSkillBonus = store.get<ESM4::GameSetting>().find("fAVDTagSkillBonus")->mFloatValue; // note: in an original engine this can be changed on the fly, but is only updated outside of the pipboy.
                                                                                                                      // however, just getting the original value and putting it into a static const variable would be faster probably.

        for (int i = 0; i < Skill_MAX; i++)
        {
            calcBaseSkill(ptr, static_cast<Skill>(i));
            ESM4::Class::FO_ActorValues skillAv = static_cast<ESM4::Class::FO_ActorValues>(i + 32);
            bool isTagged = (clas->mFOData.mTagSkill1 == skillAv) || (clas->mFOData.mTagSkill2 == skillAv) || (clas->mFOData.mTagSkill3 == skillAv) || (clas->mFOData.mTagSkill4 == skillAv);
            stats.mSkills[i].setTag(isTagged);
            if (isTagged)
            {
                stats.mSkills[i].setModifier(fAVDTagSkillBonus);
            }
        }

        // FIXME: use whatever formulae and GMSTs exist for calculating health and actionpoints
        // for now I'll make them 100

        stats.mHealth.setBase(100);
        stats.mActionPoints.setBase(100);

        mActors.updateActor(ptr, 0);
    }

    MechanicsManager::MechanicsManager()
        : mUpdatePlayer(true), mAI(true)
    {}

    void MechanicsManager::add(const MWWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.addActor(ptr);
        else
            mObjects.addObject(ptr);
    }

    void MechanicsManager::castSpell(const MWWorld::Ptr& ptr, const std::string& spellId, bool manualSpell)
    {
        if (ptr.getClass().isActor())
            mActors.castSpell(ptr, spellId, manualSpell);
    }

    void MechanicsManager::remove(const MWWorld::Ptr& ptr, bool keepActive)
    {
        if (ptr == MWBase::Environment::get().getWindowManager()->getWatchedActor())
            MWBase::Environment::get().getWindowManager()->watchActor(MWWorld::Ptr());
        mActors.removeActor(ptr, keepActive);
        mObjects.removeObject(ptr);
    }

    void MechanicsManager::updateCell(const MWWorld::Ptr& old, const MWWorld::Ptr& ptr)
    {
        if (old == MWBase::Environment::get().getWindowManager()->getWatchedActor())
            MWBase::Environment::get().getWindowManager()->watchActor(ptr);

        if (ptr.getClass().isActor())
            mActors.updateActor(old, ptr);
        else
            mObjects.updateObject(old, ptr);
    }

    void MechanicsManager::drop(const MWWorld::CellStore* cellStore)
    {
        mActors.dropActors(cellStore, MWMechanics::getPlayer());
        mObjects.dropObjects(cellStore);
    }

    

    void MechanicsManager::update(float duration, bool paused)
    {
        // Note: we should do it here since game mechanics and world updates use these values
        MWWorld::Ptr ptr = MWMechanics::getPlayer();
        MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();

        MWWorld::InventoryStore& inv = ptr.getClass().getInventoryStore(ptr);
        MWWorld::ContainerStoreIterator weapon = inv.getSlot(MWWorld::InventoryStore::Slot_CarriedRight);

        // Update the equipped weapon icon
        if (weapon == inv.end())
            winMgr->unsetSelectedWeapon();
        else
            winMgr->setSelectedWeapon(*weapon);

        if (mUpdatePlayer)
        {
            mUpdatePlayer = false;

            // HACK? The player has been changed, so a new Animation object may
            // have been made for them. Make sure they're properly updated.
            mActors.removeActor(ptr, true);
            mActors.addActor(ptr, true);
        }

        mActors.update(duration, paused);
        mObjects.update(duration, paused);
    }

    void MechanicsManager::processChangedSettings(const Settings::CategorySettingVector& changed)
    {
        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "Game" && it->second == "actors processing range")
            {
                int state = MWBase::Environment::get().getStateManager()->getState();
                if (state != MWBase::StateManager::State_Running)
                    continue;

                mActors.updateProcessingRange();

                // Update mechanics for new processing range immediately
                update(0.f, false);
            }
        }
    }

    void MechanicsManager::notifyDied(const MWWorld::Ptr& actor)
    {
        mActors.notifyDied(actor);
    }

    float MechanicsManager::getActorsProcessingRange() const
    {
        return mActors.getProcessingRange();
    }

    bool MechanicsManager::isActorDetected(const MWWorld::Ptr& actor, const MWWorld::Ptr& observer)
    {
        return mActors.isActorDetected(actor, observer);
    }

    bool MechanicsManager::isAttackPreparing(const MWWorld::Ptr& ptr)
    {
        return mActors.isAttackPreparing(ptr);
    }

    bool MechanicsManager::isRunning(const MWWorld::Ptr& ptr)
    {
        return mActors.isRunning(ptr);
    }

    bool MechanicsManager::isSneaking(const MWWorld::Ptr& ptr)
    {
        Stats& stats = ptr.getClass().getFOStats(ptr);
        MWBase::World* world = MWBase::Environment::get().getWorld();
        bool animActive = mActors.isSneaking(ptr);
        bool stanceOn = (stats.mStance & Stats::Stance_Sneaking) != 0;
        bool inair = !world->isOnGround(ptr) && !world->isSwimming(ptr) && !world->isFlying(ptr);
        return stanceOn && (animActive || inair);
    }

    void MechanicsManager::rest(double hours, bool sleep)
    {
        if (sleep)
            MWBase::Environment::get().getWorld()->rest(hours);

        mActors.rest(hours, sleep);
    }

    void MechanicsManager::restoreDynamicStats(const MWWorld::Ptr& actor, double hours, bool sleep)
    {
        mActors.restoreDynamicStats(actor, hours, sleep);
    }

    int MechanicsManager::getHoursToRest() const
    {
        return mActors.getHoursToRest(MWMechanics::getPlayer());
    }

    void MechanicsManager::setPlayerName(const std::string& name)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        ESM4::Npc player = *world->getPlayerPtr().get<ESM4::Npc>()->mBase;
        player.mFullName = name;

        world->createRecord(player);

        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerRace(const std::string& race, bool male, const std::string& head, const std::string& hair)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        ESM4::FormId raceId = ESM4::stringToFormId(race);

        ESM4::Npc player = *world->getPlayerPtr().get<ESM4::Npc>()->mBase;

        player.mRace = raceId;
        //player.mHead = head;
        //player.mHair = hair;
        if (male)
            player.mBaseConfig.fo3.flags &= ~ESM4::Npc::FO3_Female;
        else
            player.mBaseConfig.fo3.flags |= ESM4::Npc::FO3_Female;

        world->createRecord(player);

        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass(const std::string& id)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        ESM4::Npc player = *world->getPlayerPtr().get<ESM4::Npc>()->mBase;
        player.mClass = ESM4::stringToFormId(id);

        buildPlayer();
        mUpdatePlayer = true;
    }

    void MechanicsManager::setPlayerClass(const ESM4::Class& cls)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        const ESM4::Class* ptr = world->createRecord(cls);

        ESM4::Npc player = *world->getPlayerPtr().get<ESM4::Npc>()->mBase;
        player.mClass = ptr->mFormId;

        world->createRecord(player);

        buildPlayer();
        mUpdatePlayer = true;
    }

    int MechanicsManager::getDerivedDisposition(const MWWorld::Ptr& ptr, bool clamp)
    {
        // todo
        return 0;
        //const Stats& npcSkill = ptr.getClass().getFOStats(ptr);
        //float x = static_cast<float>(npcSkill.);

        //MWWorld::LiveCellRef<ESM::NPC>* npc = ptr.get<ESM::NPC>();
        //MWWorld::Ptr playerPtr = getPlayer();
        //MWWorld::LiveCellRef<ESM::NPC>* player = playerPtr.get<ESM::NPC>();
        //const MWMechanics::NpcStats& playerStats = playerPtr.getClass().getNpcStats(playerPtr);

        //const MWWorld::Store<ESM::GameSetting>& gmst = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();
        //static const float fDispRaceMod = gmst.find("fDispRaceMod")->mValue.getFloat();
        //if (Misc::StringUtils::ciEqual(npc->mBase->mRace, player->mBase->mRace))
        //    x += fDispRaceMod;

        //static const float fDispPersonalityMult = gmst.find("fDispPersonalityMult")->mValue.getFloat();
        //static const float fDispPersonalityBase = gmst.find("fDispPersonalityBase")->mValue.getFloat();
        //x += fDispPersonalityMult * (playerStats.getAttribute(ESM::Attribute::Personality).getModified() - fDispPersonalityBase);

        //float reaction = 0;
        //int rank = 0;
        //std::string npcFaction = Misc::StringUtils::lowerCase(ptr.getClass().getPrimaryFaction(ptr));

        //if (playerStats.getFactionRanks().find(npcFaction) != playerStats.getFactionRanks().end())
        //{
        //    if (!playerStats.getExpelled(npcFaction))
        //    {
        //        // faction reaction towards itself. yes, that exists
        //        reaction = static_cast<float>(MWBase::Environment::get().getDialogueManager("FO3")->getFactionReaction(npcFaction, npcFaction));

        //        rank = playerStats.getFactionRanks().find(npcFaction)->second;
        //    }
        //}
        //else if (!npcFaction.empty())
        //{
        //    std::map<std::string, int>::const_iterator playerFactionIt = playerStats.getFactionRanks().begin();
        //    for (; playerFactionIt != playerStats.getFactionRanks().end(); ++playerFactionIt)
        //    {
        //        const std::string& itFaction = playerFactionIt->first;

        //        // Ignore the faction, if a player was expelled from it.
        //        if (playerStats.getExpelled(itFaction))
        //            continue;

        //        int itReaction = MWBase::Environment::get().getDialogueManager("FO3")->getFactionReaction(npcFaction, itFaction);
        //        if (playerFactionIt == playerStats.getFactionRanks().begin() || itReaction < reaction)
        //        {
        //            reaction = static_cast<float>(itReaction);
        //            rank = playerFactionIt->second;
        //        }
        //    }
        //}
        //else
        //{
        //    reaction = 0;
        //    rank = 0;
        //}

        //static const float fDispFactionRankMult = gmst.find("fDispFactionRankMult")->mValue.getFloat();
        //static const float fDispFactionRankBase = gmst.find("fDispFactionRankBase")->mValue.getFloat();
        //static const float fDispFactionMod = gmst.find("fDispFactionMod")->mValue.getFloat();
        //x += (fDispFactionRankMult * rank
        //         + fDispFactionRankBase)
        //    * fDispFactionMod * reaction;

        //static const float fDispCrimeMod = gmst.find("fDispCrimeMod")->mValue.getFloat();
        //static const float fDispDiseaseMod = gmst.find("fDispDiseaseMod")->mValue.getFloat();
        //x -= fDispCrimeMod * playerStats.getBounty();
        //if (playerStats.hasCommonDisease() || playerStats.hasBlightDisease())
        //    x += fDispDiseaseMod;

        //static const float fDispWeaponDrawn = gmst.find("fDispWeaponDrawn")->mValue.getFloat();
        //if (playerStats.getDrawState() == MWMechanics::DrawState::Weapon)
        //    x += fDispWeaponDrawn;

        //x += ptr.getClass().getCreatureStats(ptr).getMagicEffects().get(ESM::MagicEffect::Charm).getMagnitude();

        //if (clamp)
        //    return std::clamp<int>(x, 0, 100); //, normally clamped to [0..100] when used
        //return static_cast<int>(x);
    }

    int MechanicsManager::getBarterOffer(const MWWorld::Ptr& ptr, int basePrice, bool buying)
    {
        // Make sure zero base price items/services can't be bought/sold for 1 gold
        // and return the intended base price for creature merchants
        if (basePrice == 0 || ptr.getType() == ESM4::Creature::sRecordId)
            return basePrice;

        const Stats& sellerStats = ptr.getClass().getFOStats(ptr);

        MWWorld::Ptr playerPtr = MWMechanics::getPlayer();
        const Stats& playerStats = playerPtr.getClass().getFOStats(playerPtr);

        // I suppose the temporary disposition change (second param to getDerivedDisposition()) _has_ to be considered here,
        // otherwise one would get different prices when exiting and re-entering the dialogue window...
        int clampedDisposition = getDerivedDisposition(ptr);
        float a = std::min(playerPtr.getClass().getSkill(playerPtr, ESM::Skill::Mercantile), 100.f);
        float b = std::min(0.1f * playerStats.mStats[Stat_Luck].getModified(), 10.f);
        float c = std::min(0.2f * playerStats.mStats[Stat_Charisma].getModified(), 10.f);
        float d = std::min(ptr.getClass().getSkill(ptr, Skill_Speech), 100.f);
        float e = std::min(0.1f * sellerStats.mStats[Stat_Luck].getModified(), 10.f);
        float f = std::min(0.2f * sellerStats.mStats[Stat_Charisma].getModified(), 10.f);
        float pcTerm = (clampedDisposition - 50 + a + b + c);
        float npcTerm = (d + e + f);
        float buyTerm = 0.01f * (100 - 0.5f * (pcTerm - npcTerm));
        float sellTerm = 0.01f * (50 - 0.5f * (npcTerm - pcTerm));
        int offerPrice = int(basePrice * (buying ? buyTerm : sellTerm));
        return std::max(1, offerPrice);
    }

    int MechanicsManager::countDeaths(const std::string& id) const
    {
        return mActors.countDeaths(id);
    }

    void MechanicsManager::getPersuasionDispositionChange(const MWWorld::Ptr& npc, PersuasionType type, bool& success, int& tempChange, int& permChange)
    {
        // todo
        //const MWWorld::Store<ESM4::GameSetting>& gmst = MWBase::Environment::get().getWorld()->getStore().get<ESM4::GameSetting>();

        //MWMechanics::NpcStats& npcStats = npc.getClass().getNpcStats(npc);

        //MWWorld::Ptr playerPtr = MWMechanics::getPlayer();
        //const MWMechanics::NpcStats& playerStats = playerPtr.getClass().getNpcStats(playerPtr);

        //float npcRating1, npcRating2, npcRating3 = 0;
        ////getPersuasionRatings(npcStats, npcRating1, npcRating2, npcRating3, false);

        //float playerRating1, playerRating2, playerRating3 = 0;
        ////getPersuasionRatings(playerStats, playerRating1, playerRating2, playerRating3, true);

        //int currentDisposition = getDerivedDisposition(npc);

        //float d = 1 - 0.02f * abs(currentDisposition - 50);
        //float target1 = d * (playerRating1 - npcRating1 + 50);
        //float target2 = d * (playerRating2 - npcRating2 + 50);

        //float bribeMod;
        //if (type == PT_Bribe10)
        //    bribeMod = gmst.find("fBribe10Mod")->mValue.getFloat();
        //else if (type == PT_Bribe100)
        //    bribeMod = gmst.find("fBribe100Mod")->mValue.getFloat();
        //else
        //    bribeMod = gmst.find("fBribe1000Mod")->mValue.getFloat();

        //float target3 = d * (playerRating3 - npcRating3 + 50) + bribeMod;

        //float iPerMinChance = floor(gmst.find("iPerMinChance")->mValue.getFloat());
        //float iPerMinChange = floor(gmst.find("iPerMinChange")->mValue.getFloat());
        //float fPerDieRollMult = gmst.find("fPerDieRollMult")->mValue.getFloat();
        //float fPerTempMult = gmst.find("fPerTempMult")->mValue.getFloat();

        //float x = 0;
        //float y = 0;

        //auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        //int roll = Misc::Rng::roll0to99(prng);

        //if (type == PT_Admire)
        //{
        //    target1 = std::max(iPerMinChance, target1);
        //    success = (roll <= target1);
        //    float c = floor(fPerDieRollMult * (target1 - roll));
        //    x = success ? std::max(iPerMinChange, c) : c;
        //}
        //else if (type == PT_Intimidate)
        //{
        //    target2 = std::max(iPerMinChance, target2);

        //    success = (roll <= target2);

        //    float r;
        //    if (roll != target2)
        //        r = floor(target2 - roll);
        //    else
        //        r = 1;

        //    if (roll <= target2)
        //    {
        //        float s = floor(r * fPerDieRollMult * fPerTempMult);

        //        const int flee = npcStats.getAiSetting(MWMechanics::AiSetting::Flee).getBase();
        //        const int fight = npcStats.getAiSetting(MWMechanics::AiSetting::Fight).getBase();
        //        npcStats.setAiSetting(MWMechanics::AiSetting::Flee,
        //            std::clamp(flee + int(std::max(iPerMinChange, s)), 0, 100));
        //        npcStats.setAiSetting(MWMechanics::AiSetting::Fight,
        //            std::clamp(fight + int(std::min(-iPerMinChange, -s)), 0, 100));
        //    }

        //    float c = -std::abs(floor(r * fPerDieRollMult));
        //    if (success)
        //    {
        //        if (std::abs(c) < iPerMinChange)
        //        {
        //            // Deviating from Morrowind here: it doesn't increase disposition on marginal wins,
        //            // which seems to be a bug (MCP fixes it too).
        //            // Original logic: x = 0, y = -iPerMinChange
        //            x = iPerMinChange;
        //            y = x; // This goes unused.
        //        }
        //        else
        //        {
        //            x = -floor(c * fPerTempMult);
        //            y = c;
        //        }
        //    }
        //    else
        //    {
        //        x = floor(c * fPerTempMult);
        //        y = c;
        //    }
        //}
        //else if (type == PT_Taunt)
        //{
        //    target1 = std::max(iPerMinChance, target1);
        //    success = (roll <= target1);

        //    float c = std::abs(floor(target1 - roll));

        //    if (success)
        //    {
        //        float s = c * fPerDieRollMult * fPerTempMult;
        //        const int flee = npcStats.getAiSetting(AiSetting::Flee).getBase();
        //        const int fight = npcStats.getAiSetting(AiSetting::Fight).getBase();
        //        npcStats.setAiSetting(AiSetting::Flee,
        //            std::clamp(flee + std::min(-int(iPerMinChange), int(-s)), 0, 100));
        //        npcStats.setAiSetting(AiSetting::Fight,
        //            std::clamp(fight + std::max(int(iPerMinChange), int(s)), 0, 100));
        //    }
        //    x = floor(-c * fPerDieRollMult);

        //    if (success && std::abs(x) < iPerMinChange)
        //        x = -iPerMinChange;
        //}
        //else // Bribe
        //{
        //    target3 = std::max(iPerMinChance, target3);
        //    success = (roll <= target3);
        //    float c = floor((target3 - roll) * fPerDieRollMult);

        //    x = success ? std::max(iPerMinChange, c) : c;
        //}

        //tempChange = type == PT_Intimidate ? int(x) : int(x * fPerTempMult);

        //int cappedDispositionChange = tempChange;
        //if (currentDisposition + tempChange > 100)
        //    cappedDispositionChange = 100 - currentDisposition;
        //if (currentDisposition + tempChange < 0)
        //{
        //    cappedDispositionChange = -currentDisposition;
        //    tempChange = cappedDispositionChange;
        //}

        //permChange = floor(cappedDispositionChange / fPerTempMult);
        //if (type == PT_Intimidate)
        //{
        //    permChange = success ? -int(cappedDispositionChange / fPerTempMult) : int(y);
        //}
    }

    void MechanicsManager::forceStateUpdate(const MWWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.forceStateUpdate(ptr);
    }

    bool MechanicsManager::playAnimationGroup(const MWWorld::Ptr& ptr, std::string_view groupName, int mode, int number, bool persist)
    {
        if (ptr.getClass().isActor())
            return mActors.playAnimationGroup(ptr, groupName, mode, number, persist);
        else
            return mObjects.playAnimationGroup(ptr, groupName, mode, number, persist);
    }

    void MechanicsManager::skipAnimation(const MWWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            mActors.skipAnimation(ptr);
        else
            mObjects.skipAnimation(ptr);
    }

    bool MechanicsManager::checkAnimationPlaying(const MWWorld::Ptr& ptr, const std::string& groupName)
    {
        if (ptr.getClass().isActor())
            return mActors.checkAnimationPlaying(ptr, groupName);
        else
            return false;
    }

    bool MechanicsManager::onOpen(const MWWorld::Ptr& ptr)
    {
        if (ptr.getClass().isActor())
            return true;
        else
            return mObjects.onOpen(ptr);
    }

    void MechanicsManager::onClose(const MWWorld::Ptr& ptr)
    {
        if (!ptr.getClass().isActor())
            mObjects.onClose(ptr);
    }

    void MechanicsManager::persistAnimationStates()
    {
        mActors.persistAnimationStates();
        mObjects.persistAnimationStates();
    }

    void MechanicsManager::updateMagicEffects(const MWWorld::Ptr& ptr)
    {
        mActors.updateMagicEffects(ptr);
    }

    bool MechanicsManager::toggleAI()
    {
        mAI = !mAI;
        return mAI;
    }

    bool MechanicsManager::isAIActive()
    {
        return mAI;
    }

    void MechanicsManager::playerLoaded()
    {
        mUpdatePlayer = true;
        mAI = true;
    }

    bool MechanicsManager::isBoundItem(const MWWorld::Ptr& item)
    {
        // todo
        //static std::set<std::string> boundItemIDCache;

        //// If this is empty then we haven't executed the GMST cache logic yet; or there isn't any sMagicBound* GMST's for some reason
        //if (boundItemIDCache.empty())
        //{
        //    // Build a list of known bound item ID's
        //    const MWWorld::Store<ESM4::GameSetting>& gameSettings = MWBase::Environment::get().getWorld()->getStore().get<ESM4::GameSetting>();

        //    for (const ESM4::GameSetting& currentSetting : gameSettings)
        //    {
        //        // Don't bother checking this GMST if it's not a sMagicBound* one.
        //        if (!Misc::StringUtils::ciStartsWith(currentSetting.mEditorId, "smagicbound"))
        //            continue;

        //        // All sMagicBound* GMST's should be of type string
        //        std::string currentGMSTValue = currentSetting.mStringValue;
        //        Misc::StringUtils::lowerCaseInPlace(currentGMSTValue);

        //        boundItemIDCache.insert(currentGMSTValue);
        //    }
        //}

        //// Perform bound item check and assign the Flag_Bound bit if it passes
        //std::string tempItemID = item.getCellRef().getRefId();
        //Misc::StringUtils::lowerCaseInPlace(tempItemID);

        //if (boundItemIDCache.count(tempItemID) != 0)
        //    return true;

        return false;
    }

    bool MechanicsManager::isAllowedToUse(const MWWorld::Ptr& ptr, const MWWorld::Ptr& target, MWWorld::Ptr& victim)
    {
        if (target.isEmpty())
            return true;

        const MWWorld::CellRef& cellref = target.getCellRef();
        // there is no harm to use unlocked doors
        int lockLevel = cellref.getLockLevel();
        if (target.getClass().isDoor() && (lockLevel <= 0 || lockLevel == ESM::UnbreakableLock) && cellref.getTrap().empty())
        {
            return true;
        }

        if (!target.getClass().hasToolTip(target))
            return true;

        // TODO: implement a better check to check if target is owned bed
        if (target.getClass().isActivator() && target.getClass().getScript(target).compare(0, 3, "Bed") != 0)
            return true;

        if (target.getClass().isNpc())
        {
            if (target.getClass().getFOStats(target).mDead)
                return true;

            if (target.getClass().getFOStats(target).mAiSequence.isInCombat())
                return true;

            // check if a player tries to pickpocket a target NPC
            if (target.getClass().getFOStats(target).mKnockdown || isSneaking(ptr))
                return false;

            return true;
        }

        if (isOwned(ptr, target, victim))
            return false;

        // A special case for evidence chest - we should not allow to take items even if it is technically permitted
        return !Misc::StringUtils::ciEqual(cellref.getRefId(), "stolen_goods");
    }

    bool MechanicsManager::sleepInBed(const MWWorld::Ptr& ptr, const MWWorld::Ptr& bed)
    {

        if (MWBase::Environment::get().getWorld()->getPlayer().enemiesNearby())
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage2}");
            return true;
        }

        MWWorld::Ptr victim;
        if (isAllowedToUse(ptr, bed, victim))
            return false;

        if (commitCrime(ptr, victim, OT_SleepingInOwnedBed, bed.getCellRef().getFaction()))
        {
            MWBase::Environment::get().getWindowManager()->messageBox("#{sNotifyMessage64}");
            return true;
        }
        else
            return false;
    }

    void MechanicsManager::unlockAttempted(const MWWorld::Ptr& ptr, const MWWorld::Ptr& item)
    {
        MWWorld::Ptr victim;
        if (isOwned(ptr, item, victim))
        {
            // Note that attempting to unlock something that has ever been locked (even ESM::UnbreakableLock) is a crime even if it's already unlocked.
            // Likewise, it's illegal to unlock something that has a trap but isn't otherwise locked.
            const auto& cellref = item.getCellRef();
            if (cellref.getLockLevel() || !cellref.getTrap().empty())
                commitCrime(ptr, victim, OT_Trespassing, item.getCellRef().getFaction());
        }
    }

    std::vector<std::pair<std::string, int>> MechanicsManager::getStolenItemOwners(const std::string& itemid)
    {
        std::vector<std::pair<std::string, int>> result;
        StolenItemsMap::const_iterator it = mStolenItems.find(Misc::StringUtils::lowerCase(itemid));
        if (it == mStolenItems.end())
            return result;
        else
        {
            const OwnerMap& owners = it->second;
            for (OwnerMap::const_iterator ownerIt = owners.begin(); ownerIt != owners.end(); ++ownerIt)
                result.emplace_back(ownerIt->first.first, ownerIt->second);
            return result;
        }
    }

    bool MechanicsManager::isItemStolenFrom(const std::string& itemid, const MWWorld::Ptr& ptr)
    {
        StolenItemsMap::const_iterator it = mStolenItems.find(Misc::StringUtils::lowerCase(itemid));
        if (it == mStolenItems.end())
            return false;

        const OwnerMap& owners = it->second;
        const std::string& ownerid = ptr.getCellRef().getRefId();
        OwnerMap::const_iterator ownerFound = owners.find(std::make_pair(Misc::StringUtils::lowerCase(ownerid), false));
        if (ownerFound != owners.end())
            return true;

        const std::string_view factionid = ptr.getClass().getPrimaryFaction(ptr);
        if (!factionid.empty())
        {
            OwnerMap::const_iterator factionOwnerFound = owners.find(std::make_pair(Misc::StringUtils::lowerCase(factionid), true));
            return factionOwnerFound != owners.end();
        }

        return false;
    }

    void MechanicsManager::confiscateStolenItemToOwner(const MWWorld::Ptr& player, const MWWorld::Ptr& item, const MWWorld::Ptr& victim, int count)
    {
        if (player != MWMechanics::getPlayer())
            return;

        const std::string itemId = Misc::StringUtils::lowerCase(item.getCellRef().getRefId());

        StolenItemsMap::iterator stolenIt = mStolenItems.find(itemId);
        if (stolenIt == mStolenItems.end())
            return;

        Owner owner;
        owner.first = victim.getCellRef().getRefId();
        owner.second = false;

        const std::string_view victimFaction = victim.getClass().getPrimaryFaction(victim);
        if (!victimFaction.empty() && Misc::StringUtils::ciEqual(item.getCellRef().getFaction(), victimFaction)) // Is the item faction-owned?
        {
            owner.first = victimFaction;
            owner.second = true;
        }

        Misc::StringUtils::lowerCaseInPlace(owner.first);

        // decrease count of stolen items
        int toRemove = std::min(count, mStolenItems[itemId][owner]);
        mStolenItems[itemId][owner] -= toRemove;
        if (mStolenItems[itemId][owner] == 0)
        {
            // erase owner from stolen items owners
            OwnerMap& owners = stolenIt->second;
            OwnerMap::iterator ownersIt = owners.find(owner);
            if (ownersIt != owners.end())
                owners.erase(ownersIt);
        }

        MWWorld::ContainerStore& store = player.getClass().getContainerStore(player);

        // move items from player to owner and report about theft
        victim.getClass().getContainerStore(victim).add(item, toRemove, victim);
        store.remove(item, toRemove, player);
        commitCrime(player, victim, OT_Theft, item.getCellRef().getFaction(), item.getClass().getValue(item) * toRemove);
    }

    void MechanicsManager::confiscateStolenItems(const MWWorld::Ptr& player, const MWWorld::Ptr& targetContainer)
    {
        MWWorld::ContainerStore& store = player.getClass().getContainerStore(player);
        MWWorld::ContainerStore& containerStore = targetContainer.getClass().getContainerStore(targetContainer);
        for (MWWorld::ContainerStoreIterator it = store.begin(); it != store.end(); ++it)
        {
            StolenItemsMap::iterator stolenIt = mStolenItems.find(Misc::StringUtils::lowerCase(it->getCellRef().getRefId()));
            if (stolenIt == mStolenItems.end())
                continue;
            OwnerMap& owners = stolenIt->second;
            int itemCount = it->getRefData().getCount();
            for (OwnerMap::iterator ownerIt = owners.begin(); ownerIt != owners.end();)
            {
                int toRemove = std::min(itemCount, ownerIt->second);
                itemCount -= toRemove;
                ownerIt->second -= toRemove;
                if (ownerIt->second == 0)
                    owners.erase(ownerIt++);
                else
                    ++ownerIt;
            }

            int toMove = it->getRefData().getCount() - itemCount;

            containerStore.add(*it, toMove, targetContainer);
            store.remove(*it, toMove, player);
        }
        // TODO: unhardcode the locklevel
        targetContainer.getCellRef().lock(50);
    }

    void MechanicsManager::itemTaken(const MWWorld::Ptr& ptr, const MWWorld::Ptr& item, const MWWorld::Ptr& container,
        int count, bool alarm)
    {
        if (ptr != MWMechanics::getPlayer())
            return;

        MWWorld::Ptr victim;

        bool isAllowed = true;
        const MWWorld::CellRef* ownerCellRef = &item.getCellRef();
        if (!container.isEmpty())
        {
            // Inherit the owner of the container
            ownerCellRef = &container.getCellRef();
            isAllowed = isAllowedToUse(ptr, container, victim);
        }
        else
        {
            isAllowed = isAllowedToUse(ptr, item, victim);
            if (!item.getCellRef().hasContentFile())
            {
                // this is a manually placed item, which means it was already stolen
                return;
            }
        }

        if (isAllowed)
            return;

        Owner owner;
        owner.second = false;
        if (!container.isEmpty() && container.getClass().isActor())
        {
            // "container" is an actor inventory, so just take actor's ID
            owner.first = ownerCellRef->getRefId();
        }
        else
        {
            owner.first = ownerCellRef->getOwner();
            if (owner.first.empty())
            {
                owner.first = ownerCellRef->getFaction();
                owner.second = true;
            }
        }

        Misc::StringUtils::lowerCaseInPlace(owner.first);

        if (!Misc::StringUtils::ciEqual(item.getCellRef().getRefId(), "Caps001"))
        {
            if (victim.isEmpty() || (victim.getClass().isActor() && victim.getRefData().getCount() > 0 && !victim.getClass().getFOStats(victim).mDead))
                mStolenItems[Misc::StringUtils::lowerCase(item.getCellRef().getRefId())][owner] += count;
        }
        if (alarm)
            commitCrime(ptr, victim, OT_Theft, ownerCellRef->getFaction(), item.getClass().getValue(item) * count);
    }

    bool MechanicsManager::commitCrime(const MWWorld::Ptr& player, const MWWorld::Ptr& victim, OffenseType type, const std::string& factionId, int arg, bool victimAware)
    {
        // NOTE: victim may be empty

        // Only player can commit crime
        if (player != MWMechanics::getPlayer())
            return false;

        // Find all the actors within the alarm radius
        std::vector<MWWorld::Ptr> neighbors;

        osg::Vec3f from(player.getRefData().getPosition().asVec3());
        const MWWorld::ESMStore& esmStore = MWBase::Environment::get().getWorld()->getStore();
        float radius = esmStore.get<ESM4::GameSetting>().find("iCrimeAlarmRecDistance")->mIntValue;

        mActors.getObjectsInRange(from, radius, neighbors);

        // victim should be considered even beyond alarm radius
        if (!victim.isEmpty() && (from - victim.getRefData().getPosition().asVec3()).length2() > radius * radius)
            neighbors.push_back(victim);

        // get the player's followers / allies (works recursively) that will not report crimes
        std::set<MWWorld::Ptr> playerFollowers;
        getActorsSidingWith(player, playerFollowers);

        // Did anyone see it?
        bool crimeSeen = false;
        for (const MWWorld::Ptr& neighbor : neighbors)
        {
            if (!canReportCrime(neighbor, victim, playerFollowers))
                continue;

            if ((neighbor == victim && victimAware)
                // Murder crime can be reported even if no one saw it (hearing is enough, I guess).
                // TODO: Add mod support for stealth executions!
                || (type == OT_Murder && neighbor != victim)
                || (MWBase::Environment::get().getWorld()->getLOS(player, neighbor) && awarenessCheck(player, neighbor)))
            {

                // NPC will complain about theft even if he will do nothing about it
                switch (type)
                {
                    case OT_Theft: MWBase::Environment::get().getDialogueManager("FO3")->say(neighbor, "steal"); break;
                    case OT_Pickpocket: MWBase::Environment::get().getDialogueManager("FO3")->say(neighbor, "pickpocket"); break;
                    case OT_Assault: MWBase::Environment::get().getDialogueManager("FO3")->say(neighbor, "assault"); break;
                    case OT_Murder: MWBase::Environment::get().getDialogueManager("FO3")->say(neighbor, "murder"); break;
                    case OT_SleepingInOwnedBed: 
                    case OT_Trespassing: MWBase::Environment::get().getDialogueManager("FO3")->say(neighbor, "guardtrespass"); break;
                    default: break;
                }

                crimeSeen = true;
            }
        }

        if (crimeSeen)
            reportCrime(player, victim, type, factionId, arg);
        else if (type == OT_Assault && !victim.isEmpty())
        {
            bool reported = false;
            if (victim.getClass().isClass(victim, "guard")
                && !victim.getClass().getFOStats(victim).mAiSequence.hasPackage(AiPackageTypeId::Follow))
                reported = reportCrime(player, victim, type, std::string(), arg);

            if (!reported)
                startCombat(victim, player); // TODO: combat should be started with an "unaware" flag, which makes the victim flee?
        }
        return crimeSeen;
    }

    bool MechanicsManager::canReportCrime(const MWWorld::Ptr& actor, const MWWorld::Ptr& victim, std::set<MWWorld::Ptr>& playerFollowers)
    {
        if (actor == MWMechanics::getPlayer()
            || !actor.getClass().isNpc() || actor.getClass().getFOStats(actor).mDead)
            return false;

        if (actor.getClass().getFOStats(actor).mAiSequence.isInCombat(victim))
            return false;

        // Unconsious actor can not report about crime and should not become hostile
        if (actor.getClass().getFOStats(actor).mKnockdown)
            return false;

        const auto faction = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(actor.getCellRef().getFaction());
        bool isFriendToPlayer = false;
        for (const auto& r : faction->mRelations)
        {
            if (Misc::StringUtils::lowerCase(MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(r.target)->mEditorId) == "playerfaction")
                if (r.reaction == ESM4::Faction::GCR_Friend || r.reaction == ESM4::Faction::GCR_Ally)
                    isFriendToPlayer = true;
        }
        // Player's allies should not attack player, or try to arrest him
        if (actor.getClass().getFOStats(actor).mAiSequence.hasPackage(AiPackageTypeId::Follow) && isFriendToPlayer)
        {
            return false;
        }

        return true;
    }

    bool MechanicsManager::reportCrime(const MWWorld::Ptr& player, const MWWorld::Ptr& victim, OffenseType type, const std::string& factionId, int arg)
    {
        const MWWorld::Store<ESM4::GameSetting>& store = MWBase::Environment::get().getWorld()->getStore().get<ESM4::GameSetting>();

        if (type == OT_Murder && !victim.isEmpty())
            victim.getClass().getFOStats(victim).mMurdered = true;

        // Bounty and disposition penalty for each type of crime
        float disp = 0.f, dispVictim = 0.f;

        // Make surrounding actors within alarm distance respond to the crime
        std::vector<MWWorld::Ptr> neighbors;

        const MWWorld::ESMStore& esmStore = MWBase::Environment::get().getWorld()->getStore();

        osg::Vec3f from(player.getRefData().getPosition().asVec3());
        float radius = store.find("iCrimeAlarmRecDistance")->mIntValue;

        mActors.getObjectsInRange(from, radius, neighbors);

        // victim should be considered even beyond alarm radius
        if (!victim.isEmpty() && (from - victim.getRefData().getPosition().asVec3()).length2() > radius * radius)
            neighbors.push_back(victim);

        int id = MWBase::Environment::get().getWorld()->getPlayer().getNewCrimeId();

        // What amount of provocation did this crime generate?
        // Controls whether witnesses will engage combat with the criminal.
        int fight = 0, fightVictim = 0;
        // todo
        //if (type == OT_Trespassing || type == OT_SleepingInOwnedBed)
        //    fight = fightVictim = esmStore.get<ESM::GameSetting>().find("iFightTrespass")->mValue.getInteger();
        //else if (type == OT_Pickpocket)
        //{
        //    fight = esmStore.get<ESM::GameSetting>().find("iFightPickpocket")->mValue.getInteger();
        //    fightVictim = esmStore.get<ESM::GameSetting>().find("iFightPickpocket")->mValue.getInteger() * 4; // *4 according to research wiki
        //}
        //else if (type == OT_Assault)
        //{
        //    fight = esmStore.get<ESM::GameSetting>().find("iFightAttacking")->mValue.getInteger();
        //    fightVictim = esmStore.get<ESM::GameSetting>().find("iFightAttack")->mValue.getInteger();
        //}
        //else if (type == OT_Murder)
        //    fight = fightVictim = esmStore.get<ESM::GameSetting>().find("iFightKilling")->mValue.getInteger();
        //else if (type == OT_Theft)
        //    fight = fightVictim = esmStore.get<ESM::GameSetting>().find("fFightStealing")->mValue.getInteger();

        bool reported = false;

        std::set<MWWorld::Ptr> playerFollowers;
        getActorsSidingWith(player, playerFollowers);

        // Tell everyone (including the original reporter) in alarm range
        for (const MWWorld::Ptr& actor : neighbors)
        {
            if (!canReportCrime(actor, victim, playerFollowers))
                continue;

            // Will the witness report the crime?
            if (actor.getClass().getFOStats(actor).mActorValues.at("Alarm").getBase() >= 100)
            {
                reported = true;
            }
        }

        for (const MWWorld::Ptr& actor : neighbors)
        {
            if (!canReportCrime(actor, victim, playerFollowers))
                continue;

            if (reported && actor.getClass().isClass(actor, "guard"))
            {
                // Mark as Alarmed for dialogue
                actor.getClass().getFOStats(actor).mAlarmed = true;

                // Set the crime ID, which we will use to calm down participants
                // once the bounty has been paid.
                //actor.getClass().getNpcStats(actor).setCrimeId(id);

                /*if (!actor.getClass().getFOStats(actor).mAiSequence.isInPursuit())
                {
                    actor.getClass().getCreatureStats(actor).getAiSequence().stack(AiPursue(player), actor);
                }*/
            }
            else
            {
                float dispTerm = (actor == victim) ? dispVictim : disp;

                float alarmTerm = 0.01f * actor.getClass().getFOStats(actor).mActorValues.at("Alarm").getBase();
                if (type == OT_Pickpocket && alarmTerm <= 0)
                    alarmTerm = 1.0;

                if (actor != victim)
                    dispTerm *= alarmTerm;

                float fightTerm = static_cast<float>((actor == victim) ? fightVictim : fight);
                /*fightTerm += getFightDispositionBias(dispTerm);
                fightTerm += getFightDistanceBias(actor, player);*/
                fightTerm *= alarmTerm;

                const int observerFightRating = actor.getClass().getFOStats(actor).mActorValues.at("Fight").getBase();
                if (observerFightRating + fightTerm > 100)
                    fightTerm = static_cast<float>(100 - observerFightRating);
                fightTerm = std::max(0.f, fightTerm);

                if (observerFightRating + fightTerm >= 100)
                {
                    startCombat(actor, player);

                    Stats& observerStats = actor.getClass().getFOStats(actor);
                    // Apply aggression value to the base Fight rating, so that the actor can continue fighting
                    // after a Calm spell wears off
                    observerStats.mActorValues["Fight"].setModifier(observerFightRating + static_cast<int>(fightTerm));

                    //observerStats.setBaseDisposition(observerStats.getBaseDisposition() + static_cast<int>(dispTerm));

                    // Set the crime ID, which we will use to calm down participants
                    // once the bounty has been paid.
                    //observerStats.setCrimeId(id);

                    // Mark as Alarmed for dialogue
                    //observerStats.setAlarmed(true);
                }
                else
                {
                    MWBase::Environment::get().getDialogueManager("FO3")->say(actor, "observecombat");
                }
            }
        }

        if (reported)
        {
            /*player.getClass().getFOStats(player).setBounty(player.getClass().getNpcStats(player).getBounty()
                + arg);*/

            // If committing a crime against a faction member, expell from the faction
            if (!victim.isEmpty() && victim.getClass().isNpc())
            {
                std::string_view factionID = victim.getClass().getPrimaryFaction(victim);
                const auto faction = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().find(factionID)->mFormId;

                // todo: new vegas reputations
                auto& playerRanks = player.getClass().getFOStats(player).mFactionRanks;
                if (playerRanks.find(faction) != playerRanks.end())
                {
                    playerRanks.erase(playerRanks.find(faction));
                }
                auto& reps = player.getClass().getFOStats(player).mFactionReputations;
                if (reps.count(faction))
                    reps[faction].infamy += 1;
                else
                {
                    reps[faction] = { 0, 1 };
                }
            }
            else if (!factionId.empty())
            {
                auto& playerRanks = player.getClass().getFOStats(player).mFactionRanks;
                const auto faction = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().find(factionId)->mFormId;
                if (playerRanks.find(faction) != playerRanks.end())
                {
                    playerRanks.erase(playerRanks.find(faction));
                }
                auto& reps = player.getClass().getFOStats(player).mFactionReputations;
                if (reps.count(faction))
                    reps[faction].infamy += 1;
                else
                {
                    reps[faction] = { 0, 1 };
                }
            }

            if (type == OT_Assault && !victim.isEmpty()
                && !victim.getClass().getFOStats(victim).mAiSequence.isInCombat(player)
                && victim.getClass().isNpc())
            {
                // Attacker is in combat with us, but we are not in combat with the attacker yet. Time to fight back.
                // Note: accidental or collateral damage attacks are ignored.
                startCombat(victim, player);

                // Set the crime ID, which we will use to calm down participants
                // once the bounty has been paid.
                //victim.getClass().getFOStats(victim).setCrimeId(id);
            }
        }

        return reported;
    }

    bool MechanicsManager::actorAttacked(const MWWorld::Ptr& target, const MWWorld::Ptr& attacker)
    {
        const MWWorld::Ptr& player = MWMechanics::getPlayer();
        if (target == player || !attacker.getClass().isActor())
            return false;

        Stats& statsTarget = target.getClass().getFOStats(target);
        if (attacker == player)
        {
            std::set<MWWorld::Ptr> followersAttacker;
            getActorsSidingWith(attacker, followersAttacker);
            if (followersAttacker.find(target) != followersAttacker.end())
            {

                if (statsTarget.mFriendlyHits < 4)
                {
                    MWBase::Environment::get().getDialogueManager("FO3")->say(target, "hit");
                    return false;
                }
            }
        }

        if (canCommitCrimeAgainst(target, attacker))
            commitCrime(attacker, target, MWBase::MechanicsManager::OT_Assault);

        AiSequence& seq = statsTarget.mAiSequence;

        if (!attacker.isEmpty()
            && (attacker.getClass().getFOStats(attacker).mAiSequence.isInCombat(target) || attacker == player)
            && !seq.isInCombat(attacker))
        {
            // Attacker is in combat with us, but we are not in combat with the attacker yet. Time to fight back.
            // Note: accidental or collateral damage attacks are ignored.
            if (/*!target.getClass().getFOStats(target).getAiSequence().isInPursuit()*/true)
            {
                // If an actor has OnPCHitMe declared in his script, his Fight = 0 and the attacker is player,
                // he will attack the player only if we will force him (e.g. via StartCombat console command)
                bool peaceful = false;
                std::string_view script = target.getClass().getScript(target);
                if (!script.empty() && target.getRefData().getLocals().hasVar(script, "onpchitme") && attacker == player)
                {
                    const int fight = target.getClass().getFOStats(target).mActorValues.at("Fight").getModified();
                    peaceful = (fight == 0);
                }

                if (!peaceful)
                {
                    startCombat(target, attacker);
                    // Force friendly actors into combat to prevent infighting between followers
                    std::set<MWWorld::Ptr> followersTarget;
                    getActorsSidingWith(target, followersTarget);
                    for (const auto& follower : followersTarget)
                    {
                        if (follower != attacker && follower != player)
                            startCombat(follower, attacker);
                    }
                }
            }
        }

        return true;
    }

    bool MechanicsManager::canCommitCrimeAgainst(const MWWorld::Ptr& target, const MWWorld::Ptr& attacker)
    {
        const auto& seq = target.getClass().getFOStats(target).mAiSequence;
        return target.getClass().isNpc() && !attacker.isEmpty() && !seq.isInCombat(attacker)
            && !isAggressive(target, attacker) && !seq.isEngagedWithActor()
            /*&& !target.getClass().getCreatureStats(target).getAiSequence().isInPursuit()*/;
    }

    void MechanicsManager::actorKilled(const MWWorld::Ptr& victim, const MWWorld::Ptr& attacker)
    {
        if (attacker.isEmpty() || victim.isEmpty())
            return;

        if (victim == attacker)
            return; // known to happen

        if (!victim.getClass().isNpc())
            return; // TODO: implement animal rights

        const MWMechanics::NpcStats& victimStats = victim.getClass().getNpcStats(victim);
        const MWWorld::Ptr& player = MWMechanics::getPlayer();
        bool canCommit = attacker == player && canCommitCrimeAgainst(victim, attacker);

        // For now we report only about crimes of player and player's followers
        if (attacker != player)
        {
            std::set<MWWorld::Ptr> playerFollowers;
            getActorsSidingWith(player, playerFollowers);
            if (playerFollowers.find(attacker) == playerFollowers.end())
                return;
        }

        if (!canCommit)
            return;

        // Simple check for who attacked first: if the player attacked first, a crimeId should be set
        // Doesn't handle possible edge case where no one reported the assault, but in such a case,
        // for bystanders it is not possible to tell who attacked first, anyway.
        commitCrime(player, victim, MWBase::MechanicsManager::OT_Murder);
    }

    bool MechanicsManager::awarenessCheck(const MWWorld::Ptr& ptr, const MWWorld::Ptr& observer)
    {
        if (observer.getClass().getFOStats(observer).mDead || !observer.getRefData().isEnabled())
            return false;

        const MWWorld::Store<ESM4::GameSetting>& store = MWBase::Environment::get().getWorld()->getStore().get<ESM4::GameSetting>();

        Stats& stats = ptr.getClass().getFOStats(ptr);

        float sneakTerm = 0;
        if (isSneaking(ptr))
        {
            static float fSneakSkillMult = store.find("fSneakSkillMult")->mFloatValue;
            static float fSneakBootMult = store.find("fSneakBootWeightMult")->mFloatValue;
            float sneak = static_cast<float>(ptr.getClass().getSkill(ptr, Skill_Sneak));
            float agility = stats.mStats[Stat_Agility].getModified();
            float luck = stats.mStats[Stat_Luck].getModified();
            float bootWeight = 0;
            if (ptr.getClass().isNpc() && MWBase::Environment::get().getWorld()->isOnGround(ptr))
            {
                const MWWorld::InventoryStore& inv = ptr.getClass().getInventoryStore(ptr);
                MWWorld::ConstContainerStoreIterator it = inv.getSlot(MWWorld::InventoryStore::Slot_Boots);
                if (it != inv.end())
                    bootWeight = it->getClass().getWeight(*it);
            }
            sneakTerm = fSneakSkillMult * sneak + 0.2f * agility + 0.1f * luck + bootWeight * fSneakBootMult;
        }

        static float fSneakDistBase = store.find("fSneakMaxDistance")->mFloatValue;
        static float fSneakDistMult = /*store.find("fSneakDistanceMultiplier")->mValue.getFloat()*/1;

        osg::Vec3f pos1(ptr.getRefData().getPosition().asVec3());
        osg::Vec3f pos2(observer.getRefData().getPosition().asVec3());
        float distTerm = fSneakDistBase + fSneakDistMult * (pos1 - pos2).length();

        /*float chameleon = stats.getMagicEffects().get(ESM::MagicEffect::Chameleon).getMagnitude();
        float invisibility = stats.getMagicEffects().get(ESM::MagicEffect::Invisibility).getMagnitude();*/
        float x = sneakTerm * distTerm /** stats.getFatigueTerm() + chameleon*/;
        /*if (invisibility > 0.f)
            x += 100.f;*/

        Stats& observerStats = observer.getClass().getFOStats(observer);
        float obsAgility = observerStats.mStats[Stat_Agility].getModified();
        float obsLuck = observerStats.mStats[Stat_Luck].getModified();
        float obsSneak = observer.getClass().getSkill(observer, Skill_Sneak);

        float obsTerm = obsSneak + 0.2f * obsAgility + 0.1f * obsLuck;

        // is ptr behind the observer?
        static float fDetectionViewCone = store.find("fDetectionViewCone")->mFloatValue;
        float y = 0;
        osg::Vec3f vec = pos1 - pos2;
        if (observer.getRefData().getBaseNode())
        {
            osg::Vec3f observerDir = (observer.getRefData().getBaseNode()->getAttitude() * osg::Vec3f(0, 1, 0));

            float angleRadians = std::acos(observerDir * vec / (observerDir.length() * vec.length()));
            angleRadians = std::max(angleRadians, fDetectionViewCone);
            angleRadians = std::min(angleRadians, 360.f);
            y = obsTerm;
        }

        float target = x - y;
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        return (Misc::Rng::roll0to99(prng) >= target);
    }

    void MechanicsManager::startCombat(const MWWorld::Ptr& ptr, const MWWorld::Ptr& target)
    {
        Stats& stats = ptr.getClass().getFOStats(ptr);

        // Don't add duplicate packages nor add packages to dead actors.
        if (stats.mDead || stats.mAiSequence.isInCombat(target))
            return;

        // The target is somehow the same as the actor. Early-out.
        if (ptr == target)
        {
            // We don't care about dialogue filters since the target is invalid.
            // We still want to play the combat taunt.
            MWBase::Environment::get().getDialogueManager("FO3")->say(ptr, "attack");
            return;
        }

        //stats.mAiSequence.stack(AiCombat(target), ptr); // todo: combat
        if (target == MWMechanics::getPlayer())
        {
            // if guard starts combat with player, guards pursuing player should do the same
            if (ptr.getClass().isClass(ptr, "Guard"))
            {
                stats.mLastHitAttemptObject = (target.getClass().getFOStats(target).mFormId); // Stops guard from ending combat if player is unreachable
            }
        }

        // Must be done after the target is set up, so that CreatureTargetted dialogue filter works properly
        MWBase::Environment::get().getDialogueManager("FO3")->say(ptr, "normaltocombat");
    }

    void MechanicsManager::stopCombat(const MWWorld::Ptr& actor)
    {
        mActors.stopCombat(actor);
    }

    void MechanicsManager::getObjectsInRange(const osg::Vec3f& position, float radius, std::vector<MWWorld::Ptr>& objects)
    {
        mActors.getObjectsInRange(position, radius, objects);
        mObjects.getObjectsInRange(position, radius, objects);
    }

    void MechanicsManager::getActorsInRange(const osg::Vec3f& position, float radius, std::vector<MWWorld::Ptr>& objects)
    {
        mActors.getObjectsInRange(position, radius, objects);
    }

    bool MechanicsManager::isAnyActorInRange(const osg::Vec3f& position, float radius)
    {
        return mActors.isAnyObjectInRange(position, radius);
    }

    std::vector<MWWorld::Ptr> MechanicsManager::getActorsSidingWith(const MWWorld::Ptr& actor)
    {
        return mActors.getActorsSidingWith(actor);
    }

    std::vector<MWWorld::Ptr> MechanicsManager::getActorsFollowing(const MWWorld::Ptr& actor)
    {
        return mActors.getActorsFollowing(actor);
    }

    std::vector<int> MechanicsManager::getActorsFollowingIndices(const MWWorld::Ptr& actor)
    {
        return mActors.getActorsFollowingIndices(actor);
    }

    std::map<int, MWWorld::Ptr> MechanicsManager::getActorsFollowingByIndex(const MWWorld::Ptr& actor)
    {
        return mActors.getActorsFollowingByIndex(actor);
    }

    std::vector<MWWorld::Ptr> MechanicsManager::getActorsFighting(const MWWorld::Ptr& actor)
    {
        return mActors.getActorsFighting(actor);
    }

    std::vector<MWWorld::Ptr> MechanicsManager::getEnemiesNearby(const MWWorld::Ptr& actor)
    {
        return mActors.getEnemiesNearby(actor);
    }

    void MechanicsManager::getActorsFollowing(const MWWorld::Ptr& actor, std::set<MWWorld::Ptr>& out)
    {
        mActors.getActorsFollowing(actor, out);
    }

    void MechanicsManager::getActorsSidingWith(const MWWorld::Ptr& actor, std::set<MWWorld::Ptr>& out)
    {
        mActors.getActorsSidingWith(actor, out);
    }

    int MechanicsManager::countSavedGameRecords() const
    {
        return 1 // Death counter
            + 1; // Stolen items
    }

    void MechanicsManager::clear()
    {
        mActors.clear();
        mStolenItems.clear();
    }

    bool MechanicsManager::isAggressive(const MWWorld::Ptr& ptr, const MWWorld::Ptr& target)
    {
        // Don't become aggressive if a calm effect is active, since it would cause combat to cycle on/off as
        // combat is activated here and then canceled by the calm effect
        /*if ((ptr.getClass().isNpc() && ptr.getClass().getFOStats(ptr).getMagicEffects().get(ESM::MagicEffect::CalmHumanoid).getMagnitude() > 0)
            || (!ptr.getClass().isNpc() && ptr.getClass().getCreatureStats(ptr).getMagicEffects().get(ESM::MagicEffect::CalmCreature).getMagnitude() > 0))
            return false;*/

        int disposition = 50;
        if (ptr.getClass().isNpc())
            disposition = getDerivedDisposition(ptr);

        int fight = ptr.getClass().getFOStats(ptr).mActorValues.at("Fight").getModified()
            /*+ static_cast<int>(getFightDistanceBias(ptr, target) + getFightDispositionBias(static_cast<float>(disposition)))*/;

        return (fight >= 100);
    }

    void MechanicsManager::resurrect(const MWWorld::Ptr& ptr)
    {
        Stats& stats = ptr.getClass().getFOStats(ptr);
        if (stats.mDead)
        {
            //stats.resurrect();
            mActors.resurrect(ptr);
        }
    }

    bool MechanicsManager::isCastingSpell(const MWWorld::Ptr& ptr) const
    {
        return mActors.isCastingSpell(ptr);
    }

    bool MechanicsManager::isReadyToBlock(const MWWorld::Ptr& ptr) const
    {
        return mActors.isReadyToBlock(ptr);
    }

    bool MechanicsManager::isAttackingOrSpell(const MWWorld::Ptr& ptr) const
    {
        return mActors.isAttackingOrSpell(ptr);
    }

    void MechanicsManager::setWerewolf(const MWWorld::Ptr& actor, bool werewolf) {}

    void MechanicsManager::applyWerewolfAcrobatics(const MWWorld::Ptr& actor) {}

    void MechanicsManager::cleanupSummonedCreature(const MWWorld::Ptr& caster, int creatureActorId)
    {
        //mActors.cleanupSummonedCreature(caster.getClass().getCreatureStats(caster), creatureActorId);
    }

    void MechanicsManager::reportStats(unsigned int frameNumber, osg::Stats& stats) const
    {
        stats.setAttribute(frameNumber, "Mechanics Actors", mActors.size());
        stats.setAttribute(frameNumber, "Mechanics Objects", mObjects.size());
    }

    int MechanicsManager::getGreetingTimer(const MWWorld::Ptr& ptr) const
    {
        return mActors.getGreetingTimer(ptr);
    }

    float MechanicsManager::getAngleToPlayer(const MWWorld::Ptr& ptr) const
    {
        return mActors.getAngleToPlayer(ptr);
    }

    Mechanics::GreetingState MechanicsManager::getGreetingState(const MWWorld::Ptr& ptr) const
    {
        return mActors.getGreetingState(ptr);
    }

    bool MechanicsManager::isTurningToPlayer(const MWWorld::Ptr& ptr) const
    {
        return mActors.isTurningToPlayer(ptr);
    }
}
