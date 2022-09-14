#include "stats.hpp"


namespace F3Mechanics
{

    SkillValue::SkillValue(bool tagged) :
        MWMechanics::SkillValue(), mTagged(tagged)
    {
    }

    Stats::Stats ()
        : mDead (false), mDeathAnimationFinished(false), mDied (false), mMurdered(false), mFriendlyHits (0),
          mTalkedTo (false), mAlarmed (false), mAttacked (false),
          mKnockdown(false), mKnockdownOneFrame(false), mKnockdownOverOneFrame(false),
          mHitRecovery(false), mBlock(false), mMovementFlags(0),
          mFallHeight(0), mCaps(0), mActorId(-1),
          mDeathAnimation(-1), mTimeOfDeath(), mLevel (0)
    {

    }

    float Stats::land(bool isPlayer)
    {
        // todo
        return 0.0f;
    }
}
