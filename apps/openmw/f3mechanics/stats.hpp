#ifndef GAME_F3MECHANICS_STATS_H
#define GAME_F3MECHANICS_STATS_H

#include <algorithm>
#include <limits>

#include <map>
#include <set>
#include <string>
#include <stdexcept>

#include "../mwmechanics/stat.hpp"
#include "../mwworld/timestamp.hpp"

#include <components/esm4/actor.hpp>

#include "aisequence.hpp"

namespace F3Mechanics
{

    class SkillValue : public MWMechanics::SkillValue
    {
        bool mTagged;
    public:
        SkillValue(bool tagged = false);

        void setTag(bool tag);
        bool isTagged() { return mTagged; }
    };

    inline bool operator== (const SkillValue& left, const SkillValue& right)
    {
        return left.getBase() == right.getBase()
                && left.getModifier() == right.getModifier()
                && left.getDamage() == right.getDamage()
                && left.getProgress() == right.getProgress();
    }
    inline bool operator!= (const SkillValue& left, const SkillValue& right)
    {
        return !(left == right);
    }

    enum Skill
    {
        Skill_Barter,
        Skill_BigGuns, // unused in fonv
        Skill_EnergyWeapons,
        Skill_Explosives,
        Skill_Lockpick,
        Skill_Medicine,
        Skill_MeleeWeapons,
        Skill_Repair,
        Skill_Science,
        Skill_Guns, // small guns in fo3
        Skill_Sneak,
        Skill_Speech,
        Skill_Survival, // throwing in fo3
        Skill_Unarmed,
        Skill_MAX
    };

    enum SPECIAL
    {
        Stat_Strength,
        Stat_Perception,
        Stat_Endurance,
        Stat_Charisma,
        Stat_Intelligence,
        Stat_Agility,
        Stat_Luck,
        Stat_MAX
    };

    struct Reputation
    {
        int32_t fame;
        int32_t infamy;
    };

    struct Stats
    {
        // SPECIAL stats
        MWMechanics::AttributeValue mStats[SPECIAL::Stat_MAX]; 
        MWMechanics::DynamicStat<float> mHealth;
        MWMechanics::DynamicStat<float> mActionPoints;
        
        std::map<std::string, MWMechanics::Stat<float>, Misc::StringUtils::CiComp> mDynamic; //todo: is this good?
        
        std::map<std::string, MWMechanics::Stat<int>, Misc::StringUtils::CiComp> mActorValues; //todo is this good?
        
        SkillValue mSkills[Skill::Skill_MAX];
        uint8_t mSkillOffsets[Skill::Skill_MAX];

        //AiSequence mAiSequence;

        bool mDead;
        bool mDeathAnimationFinished;
        bool mDied; // flag for OnDeath script function
        bool mMurdered;
        int mFriendlyHits;
        bool mTalkedTo;
        bool mAlarmed;
        bool mAttacked;
        bool mKnockdown;
        bool mKnockdownOneFrame;
        bool mKnockdownOverOneFrame;
        bool mHitRecovery;
        bool mBlock;
        enum MovementFlags
        {
            Movement_None = 1 << 0,
            Movement_Walk = 1 << 1,
            Movement_Run = 1 << 2,
        };
        enum Stance
        {
            Stance_Normal = 1 << 0,
            Stance_Sneaking = 1 << 1,
        };
        uint32_t mMovementFlags;
        uint32_t mStance;

        int32_t mDisposition; // disposition to player

        float mFallHeight;

        ESM4::FormId mLastHitObject; // The last object to hit this actor
        ESM4::FormId mLastHitAttemptObject; // The last object to attempt to hit this actor

        int mCaps;

        int mActorId;

        ESM4::FormId mFormId;
        
        int mDeathAnimation;

        MWWorld::TimeStamp mTimeOfDeath;

        int mLevel;
        bool mAttacking;
        
        Stats();

        /// Reset the fall height
        /// @return total fall height
        float land(bool isPlayer=false);

        ESM4::AIData mAiData;
        ESM4::ActorBaseConfig mBaseConfig;
        ESM4::ActorBaseConfig mCurrentState;

        AiSequence mAiSequence;

        std::map<ESM4::FormId, Reputation> mFactionReputations; // the reputation an actor has in each faction
                                                            // indexed by form id of faction
        std::map<ESM4::FormId, int> mFactionRanks;       // the rank an actor has in each faction
                                                            // indexed by form id of faction
        
        // TODO
    };

}

#endif
