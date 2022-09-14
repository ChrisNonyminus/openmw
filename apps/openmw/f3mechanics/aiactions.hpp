#ifndef GAME_F3MECHANICS_AIACTIONS_H
#define GAME_F3MECHANICS_AIACTIONS_H

// todo: split this into multiple files per action (or split by category of action (i.e wandering, patrol, and sandbox in an "aiidle" file))

#include "typedaipackage.hpp"

#include <vector>

#include "pathfinding.hpp"
#include "usings.hpp"

#include <components/esm4/loadidle.hpp>
#include <components/esm4/loadinfo.hpp>

#include "../mwworld/esmstore.hpp"
#include "../mwworld/store.hpp"

namespace ESM4
{
    struct Cell;
    struct AIPackage;
    struct AIData;
}

namespace F3Mechanics
{
    enum WanderFlags // flags used by wander and sandbox packages
    {
        Wander_NoEating = 1,
        Wander_NoSleeping = 2,
        Wander_NoConvo = 4,
        Wander_NoIdleMarkers = 8,
        Wander_NoFurniture = 0x10,
        Wander_NoWandering = 0x20
    };
    struct AiCommonStorage : AiTemporaryBase
    {
        AiReactionTimer mReaction;
        std::vector<ESM4::IdleAnimation> mIdleAnims;
        ESM4::AIPackage::PSDT mSchedule;
        ESM4::AIPackage::PLDT mLocation;
        ESM4::AIPackage::PTDT mTarget;
        ESM4::AIPackage::FO_PKDT mData;
        std::vector<ESM4::TargetCondition> mConditions;

        std::vector<ESM4::Vertex> mAllowedNodes;
        ESM4::Vertex mCurrentNode;

        bool mPopulateAvailableNodes;
        bool mTrimCurrentNode;
        bool mCanWanderAlongPathGrid;

        int mStuckCount;

        AiCommonStorage();
    };

    struct AiWanderStorage : AiCommonStorage
    {
        // AiWander states
        enum WanderState
        {
            Wander_ChooseAction,
            Wander_IdleNow,
            Wander_MoveNow,
            Wander_Walking
        };
        WanderState mState;

        bool mIsWanderingManually;

        float mCheckIdlePositionTimer;

        void setState(const WanderState wanderState, const bool isManualWander = false)
        {
            mState = wanderState;
            mIsWanderingManually = isManualWander;
        }
    };

    // wander and sandbox
    class AiSandbox final : public TypedAiPackage<AiSandbox>
    {
    public:
        /// Constructor
        explicit AiSandbox(const ESM4::AIPackage* wander, bool repeat = true);

        bool execute(const MWWorld::Ptr& actor, CharacterController& characterController, AiState& state, float duration) override;

        static constexpr AiPackageTypeId getTypeId() { return AiPackageTypeId::Wander; }
        static constexpr Options makeDefaultOptions()
        {
            AiPackage::Options options;
            options.mUseVariableSpeed = true;
            return options;
        }

        void writeState(std::vector<ESM4::AIPackage>& sequence) const override;

        void fastForward(const MWWorld::Ptr& actor, AiState& state) override;

        osg::Vec3f getDestination(const MWWorld::Ptr& actor) const override;

        osg::Vec3f getDestination() const override
        {
            switch (mLocationType)
            {
                // todo: handle some more types
                case 0:
                {
                    return MWBase::Environment::get().getWorld()->searchPtrViaFormId(mLocation.asRefId).getRefData().getPosition().asVec3();
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
                default: return mDestination.valid() ? mDestination : osg::Vec3f(INFINITY, INFINITY, INFINITY);
            }
        }

        bool isStationary() const { return mRadius == 0; }

    private:
        void stopWalking(const MWWorld::Ptr& actor);

        void setPathToAnAllowedNode(const MWWorld::Ptr& actor, AiWanderStorage& storage, const ESM::Position& actorPos);
        void evadeObstacles(const MWWorld::Ptr& actor, AiWanderStorage& storage);
        void doPerFrameActionsForState(const MWWorld::Ptr& actor, float duration, AiWanderStorage& storage);
        void onIdleStatePerFrameActions(const MWWorld::Ptr& actor, float duration, AiWanderStorage& storage);
        void onWalkingStatePerFrameActions(const MWWorld::Ptr& actor, float duration, AiWanderStorage& storage);
        void onChooseActionStatePerFrameActions(const MWWorld::Ptr& actor, AiWanderStorage& storage);
        bool reactionTimeActions(const MWWorld::Ptr& actor, AiWanderStorage& storage, ESM::Position& pos);
        inline bool isPackageCompleted() const
        {
            // End package if duration is complete
            return mDuration && mRemainingDuration <= 0;
        }
        void wanderNearStart(const MWWorld::Ptr& actor, AiWanderStorage& storage, int wanderDistance);
        bool destinationIsAtWater(const MWWorld::Ptr& actor, const osg::Vec3f& destination);
        void completeManualWalking(const MWWorld::Ptr& actor, AiWanderStorage& storage);
        bool isNearAllowedNode(const MWWorld::Ptr& actor, const AiWanderStorage& storage, float distance) const;

        const int mRadius;
        const int mDuration;
        float mRemainingDuration;
        const int mTimeOfDay;
        const std::vector<ESM4::FormId> mIdle;

        bool mStoredInitialActorPosition;
        osg::Vec3f mInitialActorPosition; // Note: an original engine does not reset coordinates even when actor changes a cell

        union Location
        {
            ESM4::FormId asRefId; // if location type is 0
            ESM4::FormId asCellId; // if location type is 1
            // osg::Vec3f asCurrentLocation; // if location type is 2
            // osg::Vec3f asEditorLocation; // if location type is 3
            ESM4::FormId asObjectId; // if location type is 4
            uint32_t asObjectType; // if location type is 5
            ESM4::FormId asLinkedRef; // if location type is 6
            Location() {}
            ~Location(){};
        };
        Location mLocation;
        uint32_t mLocationType;
        AiCommonStorage mBaseStorage;

        osg::Vec3f mDestination;

        void getNeighbouringNodes(ESM4::Vertex dest, const MWWorld::CellStore* currentCell, std::vector<ESM4::Vertex>& points);

        void getAllowedNodes(const MWWorld::Ptr& actor, const ESM4::Cell* cell, AiCommonStorage& storage);

        void trimAllowedNodes(std::vector<ESM4::Vertex>& nodes, const PathFinder& pathfinder);

        // constants for converting idleSelect values into groupNames
        enum GroupIndex
        {
            GroupIndex_MinIdle = 2,
            GroupIndex_MaxIdle = 9
        };

        /// convert point from local (i.e. cell) to world coordinates
        void ToWorldCoordinates(ESM4::Vertex& point, const ESM4::Cell* cell);

        void SetCurrentNodeToClosestAllowedNode(const osg::Vec3f& npcPos, AiCommonStorage& storage);

        /// lookup table for converting idleSelect value to groupName
        static const std::string sIdleSelectToGroupName[GroupIndex_MaxIdle - GroupIndex_MinIdle + 1];

        static int OffsetToPreventOvercrowding();
    };
};

#endif
