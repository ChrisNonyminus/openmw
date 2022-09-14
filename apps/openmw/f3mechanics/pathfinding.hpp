#ifndef GAME_F3MECHANICS_PATHFINDING_H
#define GAME_F3MECHANICS_PATHFINDING_H

#include <cassert>
#include <deque>
#include <iterator>

#include <components/detournavigator/areatype.hpp>
#include <components/detournavigator/flags.hpp>
#include <components/detournavigator/status.hpp>
#include <components/esm/defs.hpp>
#include <components/esm4/loadnavm.hpp>

namespace MWWorld
{
    class CellStore;
    class ConstPtr;
    class Ptr;
}

namespace DetourNavigator
{
    struct AgentBounds;
}

namespace F3Mechanics
{

    template <class T>
    inline float distance(const T& lhs, const T& rhs)
    {
        static_assert(std::is_same<T, osg::Vec2f>::value
                || std::is_same<T, osg::Vec3f>::value,
            "T is not a position");
        return (lhs - rhs).length();
    }

    inline float distanceIgnoreZ(const osg::Vec3f& lhs, const osg::Vec3f& rhs)
    {
        return distance(osg::Vec2f(lhs.x(), lhs.y()), osg::Vec2f(rhs.x(), rhs.y()));
    }

    float getPathDistance(const MWWorld::Ptr& actor, const osg::Vec3f& lhs, const osg::Vec3f& rhs);

    inline float getZAngleToDir(const osg::Vec3f& dir)
    {
        return std::atan2(dir.x(), dir.y());
    }

    inline float getXAngleToDir(const osg::Vec3f& dir)
    {
        float dirLen = dir.length();
        return (dirLen != 0) ? -std::asin(dir.z() / dirLen) : 0;
    }

    inline float getZAngleToPoint(const osg::Vec3f& origin, const osg::Vec3f& dest)
    {
        return getZAngleToDir(dest - origin);
    }

    inline float getXAngleToPoint(const osg::Vec3f& origin, const osg::Vec3f& dest)
    {
        return getXAngleToDir(dest - origin);
    }

    const float PATHFIND_Z_REACH = 50.0f;
    // distance after which actor (failed previously to shortcut) will try again
    const float PATHFIND_SHORTCUT_RETRY_DIST = 300.0f;

    const float MIN_TOLERANCE = 1.0f;
    const float DEFAULT_TOLERANCE = 32.0f;

    // cast up-down ray with some offset from actor position to check for pits/obstacles on the way to target;
    // magnitude of pits/obstacles is defined by PATHFIND_Z_REACH
    bool checkWayIsClear(const osg::Vec3f& from, const osg::Vec3f& to, float offsetXY);

    enum class PathType
    {
        Full,
        Partial,
    };

    class PathFinder
    {
    public:
        PathFinder() : mConstructed(false), mCell(nullptr)
        {
        }

        void clearPath()
        {
            mConstructed = false;
            mPath.clear();
            mCell = nullptr;
        }

        // returns true if end point is strongly connected (i.e. reachable
        // from start point) both start and end are pathgrid point indexes
        bool isPointConnected(const int start, const int end) const;
        std::deque<ESM4::Vertex> aStarSearch(const int start, const int end) const;

        void buildStraightPath(const osg::Vec3f& endPoint);

        void buildPath(const MWWorld::ConstPtr& actor, const osg::Vec3f& startPoint, const osg::Vec3f& endPoint,
        const MWWorld::CellStore* cell, const DetourNavigator::AgentBounds& agentBounds,
        const DetourNavigator::Flags flags, const DetourNavigator::AreaCosts& areaCosts, float endTolerance,
        PathType pathType);

        void buildPathByNavMesh(const MWWorld::ConstPtr& actor, const osg::Vec3f& startPoint,
            const osg::Vec3f& endPoint, const DetourNavigator::AgentBounds& agentBounds,
            const DetourNavigator::Flags flags, const DetourNavigator::AreaCosts& areaCosts, float endTolerance,
            PathType pathType);

        void buildPathByNavMeshToNextPoint(const MWWorld::ConstPtr& actor, const DetourNavigator::AgentBounds& agentBounds,
            const DetourNavigator::Flags flags, const DetourNavigator::AreaCosts& areaCosts);

        /// Remove front point if exist and within tolerance
        void update(const osg::Vec3f& position, float pointTolerance, float destinationTolerance,
            bool shortenIfAlmostStraight, bool canMoveByZ, const DetourNavigator::AgentBounds& agentBounds,
            const DetourNavigator::Flags flags);

        bool checkPathCompleted() const
        {
            return mConstructed && mPath.empty();
        }

        /// In radians
        float getZAngleToNext(float x, float y) const;

        float getXAngleToNext(float x, float y, float z) const;

        bool isPathConstructed() const
        {
            return mConstructed && !mPath.empty();
        }

        std::size_t getPathSize() const
        {
            return mPath.size();
        }

        const std::deque<osg::Vec3f>& getPath() const
        {
            return mPath;
        }

        const MWWorld::CellStore* getPathCell() const
        {
            return mCell;
        }

        void addPointToPath(const osg::Vec3f& point)
        {
            mConstructed = true;
            mPath.push_back(point);
        }

        static ESM4::Vertex makePathgridPoint(const osg::Vec3f& v)
        {
            return { v[0], v[1], v[2] };
        }

        static ESM4::Vertex makePathgridPoint(const ESM::Position& p)
        {
            return { p.pos[0], p.pos[1], p.pos[2] };
        }

        static osg::Vec3f makeOsgVec3(const ESM4::Vertex& v)
        {
            return (osg::Vec3f(v.x, v.y, v.z));
        }

        // Slightly cheaper version for comparisons.
        // Caller needs to be careful for very short distances (i.e. less than 1)
        // or when accumuating the results i.e. (a + b)^2 != a^2 + b^2
        //
        static float distanceSquared(const ESM4::Vertex& point, const osg::Vec3f& pos)
        {
            return (PathFinder::makeOsgVec3(point) - pos).length2();
        }

        // Return the closest pathgrid point index from the specified position
        // coordinates.  NOTE: Does not check if there is a sensible way to get there
        // (e.g. a cliff in front).
        //
        // NOTE: pos is expected to be in local coordinates, as is grid->mPoints
        //
        static int getClosestPoint(const ESM4::NavMesh* grid, const osg::Vec3f& pos)
        {
            assert(grid && !grid->mVertices.empty());

            float distanceBetween = distanceSquared(grid->mVertices[0], pos);
            int closestIndex = 0;

            // TODO: if this full scan causes performance problems mapping pathgrid
            //       points to a quadtree may help
            for (unsigned int counter = 1; counter < grid->mVertices.size(); counter++)
            {
                float potentialDistBetween = distanceSquared(grid->mVertices[counter], pos);
                if (potentialDistBetween < distanceBetween)
                {
                    distanceBetween = potentialDistBetween;
                    closestIndex = counter;
                }
            }

            return closestIndex;
        }

    private:
        bool mConstructed;
        std::deque<osg::Vec3f> mPath;
        const ESM4::NavMesh* mNavMesh;

        const MWWorld::CellStore* mCell;

        [[nodiscard]] DetourNavigator::Status buildPathByNavigatorImpl(const MWWorld::ConstPtr& actor,
            const osg::Vec3f& startPoint, const osg::Vec3f& endPoint, const DetourNavigator::AgentBounds& agentBounds,
            const DetourNavigator::Flags flags, const DetourNavigator::AreaCosts& areaCosts, float endTolerance, PathType pathType,
            std::back_insert_iterator<std::deque<osg::Vec3f>> out);
    };
}

#endif
