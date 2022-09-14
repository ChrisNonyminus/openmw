#include "aisequence.hpp"

#include <limits>
#include <algorithm>

#include <components/debug/debuglog.hpp>

#include "aiactions.hpp"
#include "../mwworld/class.hpp"

namespace F3Mechanics
{
void AiSequence::copy (const AiSequence& sequence)
{
    for (const auto& package : sequence.mPackages)
        mPackages.push_back(package->clone());

    // We need to keep an AiWander storage, if present - it has a state machine.
    // Not sure about another temporary storages
    sequence.mAiState.copy<AiWanderStorage>(mAiState);

    mNumCombatPackages = sequence.mNumCombatPackages;
}

AiSequence::AiSequence() : mDone (false), mLastAiPackage(AiPackageTypeId::None) {}


AiSequence::AiSequence (const AiSequence& sequence)
{
    copy (sequence);
    mDone = sequence.mDone;
    mLastAiPackage = sequence.mLastAiPackage;
}

AiSequence& AiSequence::operator= (const AiSequence& sequence)
{
    if (this!=&sequence)
    {
        clear();
        copy (sequence);
        mDone = sequence.mDone;
        mLastAiPackage = sequence.mLastAiPackage;
    }

    return *this;
}

AiSequence::~AiSequence()
{
    clear();
}

void AiSequence::onPackageAdded(const AiPackage& package)
{
    if (package.getTypeId() == AiPackageTypeId::Ambush ||
        package.getTypeId() == AiPackageTypeId::UseWeapon)
        mNumCombatPackages++;

    assert(mNumCombatPackages >= 0);
}


void AiSequence::onPackageRemoved(const AiPackage& package)
{
    if (package.getTypeId() == AiPackageTypeId::Ambush ||
        package.getTypeId() == AiPackageTypeId::UseWeapon)
        mNumCombatPackages--;

    assert(mNumCombatPackages >= 0);
}

AiPackageTypeId AiSequence::getTypeId() const
{
    if (mPackages.empty())
        return AiPackageTypeId::None;

    return mPackages.front()->getTypeId();
}

bool AiSequence::getCombatTarget(MWWorld::Ptr &targetActor) const
{
    if (!(getTypeId() == AiPackageTypeId::Ambush ||
        getTypeId() == AiPackageTypeId::UseWeapon))
        return false;

    targetActor = mPackages.front()->getTarget();

    return !targetActor.isEmpty();
}

bool AiSequence::getCombatTargets(std::vector<MWWorld::Ptr> &targetActors) const
{
    for (auto it = mPackages.begin(); it != mPackages.end(); ++it)
    {
        if (((*it)->getTypeId()  == AiPackageTypeId::Ambush ||
        (*it)->getTypeId()  == AiPackageTypeId::UseWeapon))
            targetActors.push_back((*it)->getTarget());
    }

    return !targetActors.empty();
}

AiPackages::iterator AiSequence::erase(AiPackages::iterator package)
{
    // Not sure if manually terminated packages should trigger mDone, probably not?
    auto& ptr = *package;
    onPackageRemoved(*ptr);

    return mPackages.erase(package);
}

bool AiSequence::isInCombat() const
{
    return mNumCombatPackages > 0;
}


bool AiSequence::isEngagedWithActor() const
{
    if (!isInCombat())
        return false;

    for (auto it = mPackages.begin(); it != mPackages.end(); ++it)
    {
        if (((*it)->getTypeId()  == AiPackageTypeId::Ambush ||
        (*it)->getTypeId()  == AiPackageTypeId::UseWeapon))
        {
            MWWorld::Ptr target2 = (*it)->getTarget();
            if (!target2.isEmpty() && target2.getClass().isNpc())
                return true;
        }
    }
    return false;
}

bool AiSequence::hasPackage(AiPackageTypeId typeId) const
{
    auto it = std::find_if(mPackages.begin(), mPackages.end(), [typeId](const auto& package)
    {
        return package->getTypeId() == typeId;
    });
    return it != mPackages.end();
}

bool AiSequence::isInCombat(const MWWorld::Ptr &actor) const
{
    if (!isInCombat())
        return false;

    for (auto it = mPackages.begin(); it != mPackages.end(); ++it)
    {
        if (((*it)->getTypeId()  == AiPackageTypeId::Ambush ||
        (*it)->getTypeId()  == AiPackageTypeId::UseWeapon))
        {
            if ((*it)->getTarget() == actor)
                return true;
        }
    }
    return false;
}

void AiSequence::removePackagesById(AiPackageTypeId id)
{
    for (auto it = mPackages.begin(); it != mPackages.end(); )
    {
        if ((*it)->getTypeId() == id)
        {
            it = erase(it);
        }
        else
            ++it;
    }
}

void AiSequence::stopCombat()
{
    removePackagesById(AiPackageTypeId::Ambush);
    removePackagesById(AiPackageTypeId::UseWeapon);
}

void AiSequence::stopCombat(const std::vector<MWWorld::Ptr>& targets)
{
    for(auto it = mPackages.begin(); it != mPackages.end(); )
    {
        if (((*it)->getTypeId()  == AiPackageTypeId::Ambush ||
        (*it)->getTypeId()  == AiPackageTypeId::UseWeapon) && std::find(targets.begin(), targets.end(), (*it)->getTarget()) != targets.end())
        {
            it = erase(it);
        }
        else
            ++it;
    }
}

bool AiSequence::isPackageDone() const
{
    return mDone;
}

namespace
{
    bool isActualAiPackage(AiPackageTypeId packageTypeId)
    {
        return (packageTypeId >= AiPackageTypeId::Find &&
                packageTypeId <= AiPackageTypeId::UseWeapon);
    }
}

void AiSequence::execute (const MWWorld::Ptr& actor, CharacterController& characterController, float duration, bool outOfRange)
{
    if (actor == MWMechanics::getPlayer())
    {
        // Players don't use this.
        return;
    }

    if (mPackages.empty())
    {
        mLastAiPackage = AiPackageTypeId::None;
        return;
    }

    auto* package = mPackages.front().get();
    if (!package->alwaysActive() && outOfRange)
        return;

    auto packageTypeId = package->getTypeId();
    // workaround ai packages not being handled as in the vanilla engine
    if (isActualAiPackage(packageTypeId))
        mLastAiPackage = packageTypeId;
    // if active package is combat one, choose nearest target
    if (packageTypeId == AiPackageTypeId::UseWeapon)
    {
        auto itActualCombat = mPackages.end();

        float nearestDist = std::numeric_limits<float>::max();
        osg::Vec3f vActorPos = actor.getRefData().getPosition().asVec3();

        float bestRating = 0.f;

        for (auto it = mPackages.begin(); it != mPackages.end();)
        {
            if ((*it)->getTypeId() != AiPackageTypeId::UseWeapon) break;

            MWWorld::Ptr target = (*it)->getTarget();

            // target disappeared (e.g. summoned creatures)
            if (target.isEmpty())
            {
                it = erase(it);
            }
            else
            {
                // todo
                // float rating = getBestActionRating(actor, target);

                // const ESM::Position &targetPos = target.getRefData().getPosition();

                // float distTo = (targetPos.asVec3() - vActorPos).length2();

                // // Small threshold for changing target
                // if (it == mPackages.begin())
                //     distTo = std::max(0.f, distTo - 2500.f);

                // // if a target has higher priority than current target or has same priority but closer
                // if (rating > bestRating || ((distTo < nearestDist) && rating == bestRating))
                // {
                //     nearestDist = distTo;
                //     itActualCombat = it;
                //     bestRating = rating;
                // }
                // ++it;
            }
        }

        if (mPackages.empty())
            return;

        if (nearestDist < std::numeric_limits<float>::max() && mPackages.begin() != itActualCombat)
        {
            assert(itActualCombat != mPackages.end());
            // move combat package with nearest target to the front
            std::rotate(mPackages.begin(), itActualCombat, std::next(itActualCombat));
        }

        package = mPackages.front().get();
        packageTypeId = package->getTypeId();
    }

    try
    {
        if (package->execute(actor, characterController, mAiState, duration))
        {
            // Put repeating non-combat AI packages on the end of the stack so they can be used again
            if (isActualAiPackage(packageTypeId) && package->getRepeat())
            {
                package->reset();
                mPackages.push_back(package->clone());
            }

            // The active package is typically the first entry, this is however not always the case
            // e.g. AiPursue executing a dialogue script that uses startCombat adds a combat package to the front
            // due to the priority.
            auto activePackageIt = std::find_if(mPackages.begin(), mPackages.end(), [&](auto& entry)
                {
                    return entry.get() == package;
                });

            erase(activePackageIt);

            if (isActualAiPackage(packageTypeId))
                mDone = true;
        }
        else
        {
            mDone = false;
        }
    }
    catch (std::exception& e)
    {
        Log(Debug::Error) << "Error during AiSequence::execute: " << e.what();
    }
}

void AiSequence::clear()
{
    mPackages.clear();
    mNumCombatPackages = 0;
}

void AiSequence::stack (const AiPackage& package, const MWWorld::Ptr& actor, bool cancelOther)
{
    if (actor == MWMechanics::getPlayer())
        throw std::runtime_error("Can't add AI packages to player");

    // Stop combat when a non-combat AI package is added
    if (isActualAiPackage(package.getTypeId()))
        stopCombat();

    // We should return a wandering actor back after combat, casting or pursuit.
    // The same thing for actors without AI packages.
    // Also there is no point to stack return packages.
    const auto currentTypeId = getTypeId();
    const auto newTypeId = package.getTypeId();
    if (currentTypeId <= AiPackageTypeId::Wander
        && !hasPackage(AiPackageTypeId::Travel)
        && (newTypeId == AiPackageTypeId::UseWeapon
        || newTypeId == AiPackageTypeId::Ambush))
    {
        osg::Vec3f dest;
        if (currentTypeId == AiPackageTypeId::Wander)
        {
            dest = getActivePackage().getDestination(actor);
        }
        else
        {
            dest = actor.getRefData().getPosition().asVec3();
        }

        // AiInternalTravel travelPackage(dest.x(), dest.y(), dest.z());
        // stack(travelPackage, actor, false);
    }

    // remove previous packages if required
    if (cancelOther && package.shouldCancelPreviousAi())
    {
        for (auto it = mPackages.begin(); it != mPackages.end();)
        {
            if((*it)->canCancel())
            {
                it = erase(it);
            }
            else
                ++it;
        }
    }

    // insert new package in correct place depending on priority
    for (auto it = mPackages.begin(); it != mPackages.end(); ++it)
    {

        if((*it)->getPriority() <= package.getPriority())
        {
            onPackageAdded(package);
            mPackages.insert(it, package.clone());
            return;
        }
    }

    onPackageAdded(package);
    mPackages.push_back(package.clone());

    // Make sure that temporary storage is empty
    if (cancelOther)
    {
        // mAiState.moveIn(std::make_unique<AiCombatStorage>());
        // mAiState.moveIn(std::make_unique<AiFollowStorage>());
        mAiState.moveIn(std::make_unique<AiWanderStorage>());
    }
}

bool AiSequence::isEmpty() const
{
    return mPackages.empty();
}

const AiPackage& AiSequence::getActivePackage() const
{
    if(mPackages.empty())
        throw std::runtime_error(std::string("No AI Package!"));
    return *mPackages.front();
}

void AiSequence::fill(const std::vector<ESM4::AIPackage>& list)
{
    for (const auto& esmPackage : list)
    {
        std::unique_ptr<AiPackage> package;
        if (esmPackage.mDataFO.type == uint32_t(AiPackageTypeId::Wander) || esmPackage.mDataFO.type == uint32_t(AiPackageTypeId::Sandbox))
        {
            package = std::make_unique<AiSandbox>(&esmPackage);
        }
        // todo: other package types
        
        if (package.get())
        {
            onPackageAdded(*package);
            mPackages.push_back(std::move(package));
        }
    }
}

void AiSequence::fastForward(const MWWorld::Ptr& actor)
{
    if (!mPackages.empty())
    {
        mPackages.front()->fastForward(actor, mAiState);
    }
}


}
