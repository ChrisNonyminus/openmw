#include "aiactions.hpp"

#include <algorithm>

#include <osg/Matrixf>

#include <components/debug/debuglog.hpp>
#include <components/detournavigator/navigatorutils.hpp>
#include <components/misc/coordinateconverter.hpp>
#include <components/misc/rng.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwphysics/collisiontype.hpp"

#include "aistate.hpp"
#include "pathfinding.hpp"
#include "stats.hpp"
#include "usings.hpp"

namespace F3Mechanics
{
    static const int COUNT_BEFORE_RESET = 10;
    static const float IDLE_POSITION_CHECK_INTERVAL = 1.5f;

    // to prevent overcrowding
    static const int DESTINATION_TOLERANCE = 64;

    // distance must be long enough that NPC will need to move to get there.
    static const int MINIMUM_WANDER_DISTANCE = DESTINATION_TOLERANCE * 2;

    static const std::size_t MAX_IDLE_SIZE = 8;

    namespace
    {
        inline int getCountBeforeReset(const MWWorld::ConstPtr& actor)
        {
            if (actor.getClass().isPureWaterCreature(actor) || actor.getClass().isPureFlyingCreature(actor))
                return 1;
            return COUNT_BEFORE_RESET;
        }

        osg::Vec3f getRandomPointAround(const osg::Vec3f& position, const float distance)
        {
            auto& prng = MWBase::Environment::get().getWorld()->getPrng();
            const float randomDirection = Misc::Rng::rollClosedProbability(prng) * 2.0f * osg::PI;
            osg::Matrixf rotation;
            rotation.makeRotate(randomDirection, osg::Vec3f(0.0, 0.0, 1.0));
            return position + osg::Vec3f(distance, 0.0, 0.0) * rotation;
        }

        bool isDestinationHidden(const MWWorld::ConstPtr& actor, const osg::Vec3f& destination)
        {
            const auto position = actor.getRefData().getPosition().asVec3();
            const bool isWaterCreature = actor.getClass().isPureWaterCreature(actor);
            const bool isFlyingCreature = actor.getClass().isPureFlyingCreature(actor);
            const osg::Vec3f halfExtents = MWBase::Environment::get().getWorld()->getPathfindingAgentBounds(actor).mHalfExtents;
            osg::Vec3f direction = destination - position;
            direction.normalize();
            const auto visibleDestination = (isWaterCreature || isFlyingCreature
                                                    ? destination
                                                    : destination + osg::Vec3f(0, 0, halfExtents.z()))
                + direction * std::max(halfExtents.x(), std::max(halfExtents.y(), halfExtents.z()));
            const int mask = MWPhysics::CollisionType_World
                | MWPhysics::CollisionType_HeightMap
                | MWPhysics::CollisionType_Door
                | MWPhysics::CollisionType_Actor;
            return MWBase::Environment::get().getWorld()->castRay(position, visibleDestination, mask, actor);
        }

        void stopMovement(const MWWorld::Ptr& actor)
        {
            auto& movementSettings = actor.getClass().getMovementSettings(actor);
            movementSettings.mPosition[0] = 0;
            movementSettings.mPosition[1] = 0;
        }

    }

}

F3Mechanics::AiCommonStorage::AiCommonStorage() : mReaction(MWBase::Environment::get().getWorld()->getPrng())
{
}

F3Mechanics::AiSandbox::AiSandbox(const ESM4::AIPackage* wander, bool repeat)
    : TypedAiPackage<AiSandbox>(repeat, wander->mEditorId),
      mDuration(wander->mSchedule.duration),
      mRadius(wander->mLocation.radius),
      mTimeOfDay(wander->mSchedule.time)
{
    // todo: fill idle animations
    mRemainingDuration = mDuration;
    mLocationType = wander->mLocation.type;
    switch (mLocationType)
    {
        case 0:
        case 1:
        case 4: mLocation.asObjectId = wander->mLocation.location; break;
        default: mLocation.asObjectId = 0; break; // FIXME
    }
    for (auto& cnd : wander->mConditions)
    {
        mBaseStorage.mConditions.push_back(cnd);
    }
    mBaseStorage.mData = wander->mDataFO;
}

bool F3Mechanics::AiSandbox::execute(const MWWorld::Ptr& actor,
    F3Mechanics::CharacterController& /*characterController*/,
    F3Mechanics::AiState& state, float duration)
{
    Stats& cStats = actor.getClass().getFOStats(actor);
    if (cStats.mDead || cStats.mHealth.getCurrent() <= 0)
        return true;
    AiWanderStorage& storage = state.get<AiWanderStorage>();
    mRemainingDuration -= ((duration * MWBase::Environment::get().getWorld()->getTimeScaleFactor()) / 3600);

    cStats.mMovementFlags &= ~Stats::Movement_Run;

    ESM::Position pos = actor.getRefData().getPosition();

    if (!mPathFinder.isPathConstructed() && getDestination(actor).valid())
    {
        const auto agentBounds = MWBase::Environment::get().getWorld()->getPathfindingAgentBounds(actor);
        constexpr float endTolerance = 0;
        mPathFinder.buildPath(actor, pos.asVec3(), getDestination(actor), actor.getCell(),
            agentBounds, getNavigatorFlags(actor), getAreaCosts(actor),
            endTolerance, PathType::Full);

        if (mPathFinder.isPathConstructed())
            storage.setState(AiWanderStorage::Wander_Walking);
    }

    MWMechanics::GreetingState greetingState = MWBase::Environment::get().getMechanicsManager("FO3")->getGreetingState(actor);
    if (greetingState == MWMechanics::Greet_InProgress)
    {
        if (storage.mState == AiWanderStorage::Wander_Walking)
        {
            stopMovement(actor);
            mObstacleCheck.clear();
            storage.setState(AiWanderStorage::Wander_IdleNow);
        }
    }

    doPerFrameActionsForState(actor, duration, storage);
    return true;
}

void F3Mechanics::AiSandbox::writeState(std::vector<ESM4::AIPackage>& sequence) const
{
    float remainingDuration;
    if (mRemainingDuration > 0 && mRemainingDuration < 24)
        remainingDuration = mRemainingDuration;
    else
        remainingDuration = mDuration;

    // TODO
}

void F3Mechanics::AiSandbox::fastForward(const MWWorld::Ptr& actor, F3Mechanics::AiState& state)
{
    mRemainingDuration--;
    if (mRadius == 0)
        return;

    AiWanderStorage& storage = state.get<AiWanderStorage>();
    if (storage.mPopulateAvailableNodes)
        getAllowedNodes(actor, actor.getCell()->getCell4(), storage);

    if (storage.mAllowedNodes.empty())
        return;

    auto& prng = MWBase::Environment::get().getWorld()->getPrng();
    int index = Misc::Rng::rollDice(storage.mAllowedNodes.size(), prng);
    ESM4::Vertex dest = storage.mAllowedNodes[index];
    ESM4::Vertex worldDest = dest;
    ToWorldCoordinates(worldDest, actor.getCell()->getCell4());

    bool isPathGridOccupied = MWBase::Environment::get().getMechanicsManager("FO3")->isAnyActorInRange(PathFinder::makeOsgVec3(worldDest), 60);

    // add offset only if the selected pathgrid is occupied by another actor
    if (isPathGridOccupied)
    {
        std::vector<ESM4::Vertex> points;
        getNeighbouringNodes(dest, actor.getCell(), points);

        // there are no neighbouring nodes, nowhere to move
        if (points.empty())
            return;

        int initialSize = points.size();
        bool isOccupied = false;
        // AI will try to move the NPC towards every neighboring node until suitable place will be found
        for (int i = 0; i < initialSize; i++)
        {
            int randomIndex = Misc::Rng::rollDice(points.size(), prng);
            ESM4::Vertex connDest = points[randomIndex];

            // add an offset towards random neighboring node
            osg::Vec3f dir = PathFinder::makeOsgVec3(connDest) - PathFinder::makeOsgVec3(dest);
            float length = dir.length();
            dir.normalize();

            for (int j = 1; j <= 3; j++)
            {
                // move for 5-15% towards random neighboring node
                dest = PathFinder::makePathgridPoint(PathFinder::makeOsgVec3(dest) + dir * (j * 5 * length / 100.f));
                worldDest = dest;
                ToWorldCoordinates(worldDest, actor.getCell()->getCell4());

                isOccupied = MWBase::Environment::get().getMechanicsManager("FO3")->isAnyActorInRange(PathFinder::makeOsgVec3(worldDest), 60);

                if (!isOccupied)
                    break;
            }

            if (!isOccupied)
                break;

            // Will try an another neighboring node
            points.erase(points.begin() + randomIndex);
        }

        // there is no free space, nowhere to move
        if (isOccupied)
            return;
    }

    // place above to prevent moving inside objects, e.g. stairs, because a vector between pathgrids can be underground.
    // Adding 20 in adjustPosition() is not enough.
    dest.z += 60;

    ToWorldCoordinates(dest, actor.getCell()->getCell4());

    state.moveIn(std::make_unique<AiWanderStorage>());

    osg::Vec3f pos((dest.x), (dest.y), (dest.z));
    MWBase::Environment::get().getWorld()->moveObject(actor, pos);
    actor.getClass().adjustPosition(actor, false);
}

osg::Vec3f F3Mechanics::AiSandbox::getDestination(const MWWorld::Ptr& actor) const
{
    switch (mLocationType)
    {
        // todo: handle some more types
        case 0:
        {
            if (MWBase::Environment::get().getWorld()->searchPtrViaFormId(mLocation.asRefId).isEmpty())
                break;
            return MWBase::Environment::get().getWorld()->searchPtrViaFormId(mLocation.asRefId).getRefData().getPosition().asVec3();
        }
        case 2:
        {
            return actor.getRefData().getPosition().asVec3();
        }
        case 3:
        {
            return actor.getCellRef().getPosition().asVec3();
        }
        case 4:
        {
            const auto& store = MWBase::Environment::get().getWorld()->getStore();
            // TODO: more than just ACTI
            if (const auto* acti = store.get<ESM4::Activator>().search(mLocation.asObjectId))
            {
                // TODO: find a way to automatically set the location as any nearby object of this type
            }
        }
        case 6:
        {
            return MWBase::Environment::get().getWorld()->searchPtrViaFormId(mLocation.asLinkedRef).getRefData().getPosition().asVec3();
        }
        default: break; // if no location can be returned, either return mDestination, or return an invalid vec3f that will be avoided by execute()
    }
    return (mDestination != osg::Vec3f(0, 0, 0)) ? mDestination : actor.getRefData().getPosition().asVec3();
}

void F3Mechanics::AiSandbox::stopWalking(const MWWorld::Ptr& actor)
{
    mPathFinder.clearPath();
    stopMovement(actor);
}

void F3Mechanics::AiSandbox::setPathToAnAllowedNode(const MWWorld::Ptr& actor, AiWanderStorage& storage, const ESM::Position& actorPos)
{
    auto& prng = MWBase::Environment::get().getWorld()->getPrng();
    unsigned int randNode = Misc::Rng::rollDice(storage.mAllowedNodes.size(), prng);
    ESM4::Vertex dest(storage.mAllowedNodes[randNode]);

    ToWorldCoordinates(dest, actor.getCell()->getCell4());

    // actor position is already in world coordinates
    const osg::Vec3f start = actorPos.asVec3();

    // don't take shortcuts for wandering
    const osg::Vec3f destVec3f = PathFinder::makeOsgVec3(dest);
    mPathFinder.buildStraightPath(destVec3f);

    if (mPathFinder.isPathConstructed())
    {
        mDestination = destVec3f;
        // Remove this node as an option and add back the previously used node (stops NPC from picking the same node):
        ESM4::Vertex temp = storage.mAllowedNodes[randNode];
        storage.mAllowedNodes.erase(storage.mAllowedNodes.begin() + randNode);
        // check if mCurrentNode was taken out of mAllowedNodes
        if (storage.mTrimCurrentNode && storage.mAllowedNodes.size() > 1)
            storage.mTrimCurrentNode = false;
        else
            storage.mAllowedNodes.push_back(storage.mCurrentNode);
        storage.mCurrentNode = temp;

        storage.setState(AiWanderStorage::Wander_Walking);
    }
    // Choose a different node and delete this one from possible nodes because it is uncreachable:
    else
        storage.mAllowedNodes.erase(storage.mAllowedNodes.begin() + randNode);
}

void F3Mechanics::AiSandbox::evadeObstacles(const MWWorld::Ptr& actor, F3Mechanics::AiWanderStorage& storage)
{
    const auto agentBounds = MWBase::Environment::get().getWorld()->getPathfindingAgentBounds(actor);
    mPathFinder.buildPathByNavMeshToNextPoint(actor, agentBounds, getNavigatorFlags(actor),
        getAreaCosts(actor));

    if (mObstacleCheck.isEvading())
    {
        // first check if we're walking into a door
        static float distance = MWBase::Environment::get().getWorld()->getMaxActivationDistance();
        if (MWMechanics::proximityToDoor(actor, distance))
        {
            // remove allowed points then select another random destination
            storage.mTrimCurrentNode = true;
            trimAllowedNodes(storage.mAllowedNodes, mPathFinder);
            mObstacleCheck.clear();
            stopWalking(actor);
            storage.setState(AiWanderStorage::Wander_MoveNow);
        }

        storage.mStuckCount++; // TODO: maybe no longer needed
    }

    // if stuck for sufficiently long, act like current location was the destination
    if (storage.mStuckCount >= getCountBeforeReset(actor)) // something has gone wrong, reset
    {
        mObstacleCheck.clear();
        stopWalking(actor);
        storage.setState(AiWanderStorage::Wander_ChooseAction);
        storage.mStuckCount = 0;
    }
}

void F3Mechanics::AiSandbox::doPerFrameActionsForState(const MWWorld::Ptr& actor, float duration, F3Mechanics::AiWanderStorage& storage)
{
    switch (storage.mState)
    {
        case AiWanderStorage::Wander_IdleNow:
            onIdleStatePerFrameActions(actor, duration, storage);
            break;

        case AiWanderStorage::Wander_Walking:
            onWalkingStatePerFrameActions(actor, duration, storage);
            break;

        case AiWanderStorage::Wander_ChooseAction:
            onChooseActionStatePerFrameActions(actor, storage);
            break;

        case AiWanderStorage::Wander_MoveNow:
            break; // nothing to do

        default:
            // should never get here
            assert(false);
            break;
    }
}

void F3Mechanics::AiSandbox::onIdleStatePerFrameActions(const MWWorld::Ptr& actor, float duration, F3Mechanics::AiWanderStorage& storage)
{
    // Check if an idle actor is too far from all allowed nodes or too close to a door - if so start walking.
    storage.mCheckIdlePositionTimer += duration;

    if (storage.mCheckIdlePositionTimer >= IDLE_POSITION_CHECK_INTERVAL && !isStationary())
    {
        storage.mCheckIdlePositionTimer = 0; // restart timer
        static float distance = MWBase::Environment::get().getWorld()->getMaxActivationDistance() * 1.6f;
        if (MWMechanics::proximityToDoor(actor, distance) || !isNearAllowedNode(actor, storage, distance))
        {
            storage.setState(AiWanderStorage::Wander_MoveNow);
            storage.mTrimCurrentNode = false; // just in case
            return;
        }
    }

    // Check if idle animation finished
    MWMechanics::GreetingState greetingState = MWBase::Environment::get().getMechanicsManager("FO3")->getGreetingState(actor);
    if ((greetingState == MWMechanics::Greet_Done || greetingState == MWMechanics::Greet_None))
    {
        if (mPathFinder.isPathConstructed())
            storage.setState(AiWanderStorage::Wander_Walking);
        else
            storage.setState(AiWanderStorage::Wander_ChooseAction);
    }
}

void F3Mechanics::AiSandbox::onWalkingStatePerFrameActions(const MWWorld::Ptr& actor, float duration, F3Mechanics::AiWanderStorage& storage)
{
    // Is there no destination or are we there yet?
    if ((!mPathFinder.isPathConstructed()) || pathTo(actor, osg::Vec3f(mPathFinder.getPath().back()), duration, DESTINATION_TOLERANCE))
    {
        stopWalking(actor);
        storage.setState(AiWanderStorage::Wander_ChooseAction);
    }
    else
    {
        // have not yet reached the destination
        evadeObstacles(actor, storage);
    }
}

void F3Mechanics::AiSandbox::onChooseActionStatePerFrameActions(const MWWorld::Ptr& actor, F3Mechanics::AiWanderStorage& storage)
{
    // Wait while fully stop before starting idle animation (important if "smooth movement" is enabled).
    if (actor.getClass().getCurrentSpeed(actor) > 0)
        return;

    if (mRadius)
    {
        storage.setState(AiWanderStorage::Wander_MoveNow);
        return;
    }
    // todo: idles

    storage.setState(AiWanderStorage::Wander_IdleNow);
}

bool F3Mechanics::AiSandbox::reactionTimeActions(const MWWorld::Ptr& actor, F3Mechanics::AiWanderStorage& storage, ESM::Position& pos)
{
    if (mRadius <= 0)
        storage.mCanWanderAlongPathGrid = false;

    if (isPackageCompleted())
    {
        stopWalking(actor);
        // Reset package so it can be used again
        mRemainingDuration = mDuration;
        return true;
    }

    if (!mStoredInitialActorPosition)
    {
        mInitialActorPosition = actor.getRefData().getPosition().asVec3();
        mStoredInitialActorPosition = true;
    }

    // Initialization to discover & store allowed node points for this actor.
    if (storage.mPopulateAvailableNodes)
    {
        getAllowedNodes(actor, actor.getCell()->getCell4(), storage);
    }

    auto& prng = MWBase::Environment::get().getWorld()->getPrng();
    if (MWMechanics::canActorMoveByZAxis(actor) && mRadius > 0)
    {
        // Typically want to idle for a short time before the next wander
        if (Misc::Rng::rollDice(100, prng) >= 92 && storage.mState != AiWanderStorage::Wander_Walking)
        {
            wanderNearStart(actor, storage, mRadius);
        }

        storage.mCanWanderAlongPathGrid = false;
    }
    // If the package has a wander distance but no pathgrid is available,
    // randomly idle or wander near spawn point
    else if (storage.mAllowedNodes.empty() && mRadius > 0 && !storage.mIsWanderingManually)
    {
        // Typically want to idle for a short time before the next wander
        if (Misc::Rng::rollDice(100, prng) >= 96)
        {
            wanderNearStart(actor, storage, mRadius);
        }
        else
        {
            storage.setState(AiWanderStorage::Wander_IdleNow);
        }
    }
    else if (storage.mAllowedNodes.empty() && !storage.mIsWanderingManually)
    {
        storage.mCanWanderAlongPathGrid = false;
    }

    // If Wandering manually and hit an obstacle, stop
    if (storage.mIsWanderingManually && mObstacleCheck.isEvading())
    {
        completeManualWalking(actor, storage);
    }

    if (storage.mState == AiWanderStorage::Wander_MoveNow && storage.mCanWanderAlongPathGrid)
    {
        // Construct a new path if there isn't one
        if (!mPathFinder.isPathConstructed())
        {
            if (!storage.mAllowedNodes.empty())
            {
                setPathToAnAllowedNode(actor, storage, pos);
            }
        }
    }
    else if (storage.mIsWanderingManually && mPathFinder.checkPathCompleted())
    {
        completeManualWalking(actor, storage);
    }

    if (storage.mIsWanderingManually
        && storage.mState == AiWanderStorage::Wander_Walking
        && (mPathFinder.getPathSize() == 0
            || isDestinationHidden(actor, mPathFinder.getPath().back())
            || MWMechanics::isAreaOccupiedByOtherActor(actor, mPathFinder.getPath().back())))
        completeManualWalking(actor, storage);

    return false; // AiWander package not yet completed
}

void F3Mechanics::AiSandbox::wanderNearStart(const MWWorld::Ptr& actor, F3Mechanics::AiWanderStorage& storage, int wanderDistance)
{
    const auto currentPosition = actor.getRefData().getPosition().asVec3();

    std::size_t attempts = 10; // If a unit can't wander out of water, don't want to hang here
    const bool isWaterCreature = actor.getClass().isPureWaterCreature(actor);
    const bool isFlyingCreature = actor.getClass().isPureFlyingCreature(actor);
    const auto world = MWBase::Environment::get().getWorld();
    const auto agentBounds = world->getPathfindingAgentBounds(actor);
    const auto navigator = world->getNavigator();
    const auto navigatorFlags = getNavigatorFlags(actor);
    const auto areaCosts = getAreaCosts(actor);
    auto& prng = MWBase::Environment::get().getWorld()->getPrng();

    do
    {

        // Determine a random location within radius of original position
        const float wanderRadius = (0.2f + Misc::Rng::rollClosedProbability(prng) * 0.8f) * wanderDistance;
        if (!isWaterCreature && !isFlyingCreature)
        {
            // findRandomPointAroundCircle uses wanderDistance as limit for random and not as exact distance
            const auto getRandom = []()
            {
                return Misc::Rng::rollProbability(MWBase::Environment::get().getWorld()->getPrng());
            };
            auto destination = DetourNavigator::findRandomPointAroundCircle(*navigator, agentBounds,
                mInitialActorPosition, wanderRadius, navigatorFlags, getRandom);
            if (destination.has_value())
            {
                osg::Vec3f direction = *destination - mInitialActorPosition;
                if (direction.length() > wanderDistance)
                {
                    direction.normalize();
                    const osg::Vec3f adjustedDestination = mInitialActorPosition + direction * wanderRadius;
                    destination = DetourNavigator::raycast(*navigator, agentBounds, currentPosition,
                        adjustedDestination, navigatorFlags);
                    if (destination.has_value() && (*destination - mInitialActorPosition).length() > wanderDistance)
                        continue;
                }
            }
            mDestination = destination.has_value() ? *destination
                                                   : getRandomPointAround(mInitialActorPosition, wanderRadius);
        }
        else
            mDestination = getRandomPointAround(mInitialActorPosition, wanderRadius);

        // Check if land creature will walk onto water or if water creature will swim onto land
        if (!isWaterCreature && destinationIsAtWater(actor, mDestination))
            continue;

        if (isDestinationHidden(actor, mDestination))
            continue;

        if (MWMechanics::isAreaOccupiedByOtherActor(actor, mDestination))
            continue;

        constexpr float endTolerance = 0;

        if (isWaterCreature || isFlyingCreature)
            mPathFinder.buildStraightPath(mDestination);
        else
            mPathFinder.buildPathByNavMesh(actor, currentPosition, mDestination, agentBounds, navigatorFlags,
                areaCosts, endTolerance, PathType::Full);

        if (mPathFinder.isPathConstructed())
        {
            storage.setState(AiWanderStorage::Wander_Walking, true);
        }

        break;
    } while (--attempts);
}

bool F3Mechanics::AiSandbox::destinationIsAtWater(const MWWorld::Ptr& actor, const osg::Vec3f& destination)
{
    float heightToGroundOrWater = MWBase::Environment::get().getWorld()->getDistToNearestRayHit(destination, osg::Vec3f(0, 0, -1), 1000.0, true);
    osg::Vec3f positionBelowSurface = destination;
    positionBelowSurface[2] = positionBelowSurface[2] - heightToGroundOrWater - 1.0f;
    return MWBase::Environment::get().getWorld()->isUnderwater(actor.getCell(), positionBelowSurface);
}

void F3Mechanics::AiSandbox::completeManualWalking(const MWWorld::Ptr& actor, F3Mechanics::AiWanderStorage& storage)
{
    stopWalking(actor);
    mObstacleCheck.clear();
    storage.setState(AiWanderStorage::Wander_IdleNow);
}

bool F3Mechanics::AiSandbox::isNearAllowedNode(const MWWorld::Ptr& actor, const F3Mechanics::AiWanderStorage& storage, float distance) const
{
    const osg::Vec3f actorPos = actor.getRefData().getPosition().asVec3();
    auto cell = actor.getCell()->getCell4();
    for (const auto& node : storage.mAllowedNodes)
    {
        osg::Vec3f point(node.x, node.y, node.z);
        Misc::CoordinateConverter(cell).toWorld(point);
        if ((actorPos - point).length2() < distance * distance)
            return true;
    }
    return false;
}

void F3Mechanics::AiSandbox::getNeighbouringNodes(ESM4::Vertex dest, const MWWorld::CellStore* currentCell, std::vector<ESM4::Vertex>& points)
{
    const ESM4::NavMesh* navm = nullptr;
    for (auto& nmesh : MWBase::Environment::get().getWorld()->getStore().get<ESM4::NavMesh>())
    {
        if (currentCell->isTes4() && nmesh.mDataFO3.cell == currentCell->getCell4()->mFormId)
            navm = &nmesh;
    }
    if (navm == nullptr || navm->mVertices.empty())
        return;
    int index = PathFinder::getClosestPoint(navm, PathFinder::makeOsgVec3(dest));

    for (auto& extConn : navm->mExtConns)
    {
        if (extConn.triangleIndex != index)
            continue;
        if (const auto* neighbor = MWBase::Environment::get().getWorld()->getStore().get<ESM4::NavMesh>().search(extConn.navMesh))
        {
            points.push_back(navm->mVertices[index]);
        }
    }
}

void F3Mechanics::AiSandbox::getAllowedNodes(const MWWorld::Ptr& actor, const ESM4::Cell* cell, AiCommonStorage& storage)
{
    const ESM4::NavMesh* navm = nullptr;
    for (auto& nmesh : MWBase::Environment::get().getWorld()->getStore().get<ESM4::NavMesh>())
    {
        if  (nmesh.mDataFO3.cell == cell->mFormId)
            navm = &nmesh;
    }
    const MWWorld::CellStore* cellStore = actor.getCell();

    storage.mAllowedNodes.clear();

    if (!navm || (navm->mVertices.size() < 2))
        storage.mCanWanderAlongPathGrid = false;

    if (mRadius && storage.mCanWanderAlongPathGrid && !actor.getClass().isPureWaterCreature(actor))
    {
        osg::Vec3f npcPos(mInitialActorPosition);
        Misc::CoordinateConverter(cell).toLocal(npcPos);

        int closestPointIndex = PathFinder::getClosestPoint(navm, npcPos);

        int pointIndex = 0;
        for (unsigned int counter = 0; counter < navm->mVertices.size(); counter++)
        {
            osg::Vec3f nodePos(PathFinder::makeOsgVec3(navm->mVertices[counter]));
            if ((npcPos - nodePos).length2() <= mRadius * mRadius /*&& getNavMesh(cellStore).isPointConnected(closestPointIndex, counter)*/ /*todo*/)
            {
                storage.mAllowedNodes.push_back(navm->mVertices[counter]);
                pointIndex = counter;
            }
        }
        if (storage.mAllowedNodes.size() == 1)
        {
            // todo
        }
        if (!storage.mAllowedNodes.empty())
        {
            SetCurrentNodeToClosestAllowedNode(npcPos, storage);
        }

        storage.mPopulateAvailableNodes = false;
    }
}

void F3Mechanics::AiSandbox::trimAllowedNodes(std::vector<ESM4::Vertex>& nodes, const PathFinder& pathfinder)
{
    // TODO: how to add these back in once the door opens?
    // Idea: keep a list of detected closed doors (see aicombat.cpp)
    // Every now and then check whether one of the doors is opened. (maybe
    // at the end of playing idle?) If the door is opened then re-calculate
    // allowed nodes starting from the spawn point.
    auto paths = pathfinder.getPath();
    while (paths.size() >= 2)
    {
        const auto pt = paths.back();
        for (unsigned int j = 0; j < nodes.size(); j++)
        {
            // FIXME: doesn't handle a door with the same X/Y
            //        coordinates but with a different Z
            if (std::abs(nodes[j].x - pt.x()) <= 0.5 && std::abs(nodes[j].y - pt.y()) <= 0.5)
            {
                nodes.erase(nodes.begin() + j);
                break;
            }
        }
        paths.pop_back();
    }
}

void F3Mechanics::AiSandbox::ToWorldCoordinates(ESM4::Vertex& point, const ESM4::Cell* cell)
{
    Misc::CoordinateConverter(cell).toWorld(point);
}

void F3Mechanics::AiSandbox::SetCurrentNodeToClosestAllowedNode(const osg::Vec3f& npcPos, AiCommonStorage& storage)
{
    float distanceToClosestNode = std::numeric_limits<float>::max();
    unsigned int index = 0;
    for (unsigned int counterThree = 0; counterThree < storage.mAllowedNodes.size(); counterThree++)
    {
        osg::Vec3f nodePos(PathFinder::makeOsgVec3(storage.mAllowedNodes[counterThree]));
        float tempDist = (npcPos - nodePos).length2();
        if (tempDist < distanceToClosestNode)
        {
            index = counterThree;
            distanceToClosestNode = tempDist;
        }
    }
    storage.mCurrentNode = storage.mAllowedNodes[index];
    storage.mAllowedNodes.erase(storage.mAllowedNodes.begin() + index);
}

int F3Mechanics::AiSandbox::OffsetToPreventOvercrowding()
{
    return 0;
}
