#include "aicast.hpp"

#include <components/misc/constants.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"

#include "aicombataction.hpp"
#include "steering.hpp"

namespace MWMechanics
{
    namespace
    {
        float getInitialDistance(const ESM::RefId& spellId)
        {
            ActionSpell action = ActionSpell(spellId);
            bool isRanged;
            return action.getCombatRange(isRanged);
        }
    }
}

MWMechanics::AiCast::AiCast(const ESM::RefId& targetId, const ESM::RefId& spellId, bool manualSpell)
    : mTargetId(targetId)
    , mSpellId(spellId)
    , mCasting(false)
    , mManual(manualSpell)
    , mDistance(getInitialDistance(spellId))
{
}

bool MWMechanics::AiCast::execute(const MWWorld::Ptr& actor, MWMechanics::CharacterController& characterController,
    MWMechanics::AiState& state, float duration)
{
    MWWorld::Ptr target;
    if (actor.getCellRef().getRefId() == mTargetId)
    {
        // If the target has the same ID as caster, consider that actor casts spell with Self range.
        target = actor;
    }
    else
    {
        target = getTarget();
        if (target.isEmpty())
            return true;

        if (!mManual && !pathTo(actor, target.getRefData().getPosition().asVec3(), duration, mDistance))
        {
            return false;
        }
    }

    osg::Vec3f targetPos = target.getRefData().getPosition().asVec3();
    // If the target of an on-target spell is an actor that is not the caster
    // the target position must be adjusted so that it's not casted at the actor's feet.
    if (target != actor && target.getClass().isActor())
    {
        osg::Vec3f halfExtents = MWBase::Environment::get().getWorld()->getHalfExtents(target);
        targetPos.z() += halfExtents.z() * 2 * Constants::TorsoHeight;
    }

    osg::Vec3f actorPos = actor.getRefData().getPosition().asVec3();
    osg::Vec3f halfExtents = MWBase::Environment::get().getWorld()->getHalfExtents(actor);
    actorPos.z() += halfExtents.z() * 2 * Constants::TorsoHeight;

    osg::Vec3f dir = targetPos - actorPos;

    bool turned = smoothTurn(actor, getZAngleToDir(dir), 2, osg::DegreesToRadians(3.f));
    turned &= smoothTurn(actor, getXAngleToDir(dir), 0, osg::DegreesToRadians(3.f));

    if (!turned)
        return false;

    // Check if the actor is already casting another spell
    bool isCasting = MWBase::Environment::get().getMechanicsManager()->isCastingSpell(actor);
    if (isCasting && !mCasting)
        return false;

    if (!mCasting)
    {
        MWBase::Environment::get().getMechanicsManager()->castSpell(actor, mSpellId, mManual);
        mCasting = true;
        return false;
    }

    // Finish package, if actor finished spellcasting
    return !isCasting;
}

MWWorld::Ptr MWMechanics::AiCast::getTarget() const
{
    MWWorld::Ptr target = MWBase::Environment::get().getWorld()->searchPtr(mTargetId, false);

    return target;
}
