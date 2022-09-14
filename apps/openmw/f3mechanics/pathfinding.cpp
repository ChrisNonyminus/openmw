#include "pathfinding.hpp"

#include <iterator>
#include <limits>

#include <osg/io_utils>

#include <components/detournavigator/navigatorutils.hpp>
#include <components/detournavigator/debug.hpp>
#include <components/debug/debuglog.hpp>
#include <components/misc/coordinateconverter.hpp>
#include <components/misc/math.hpp>

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"

#include "../mwphysics/collisiontype.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/class.hpp"

#include "usings.hpp"

namespace
{
    // See https://theory.stanford.edu/~amitp/GameProgramming/Heuristics.html
    //
    // One of the smallest cost in Seyda Neen is between points 77 & 78:
    // pt      x     y
    // 77 = 8026, 4480
    // 78 = 7986, 4218
    //
    // Euclidean distance is about 262 (ignoring z) and Manhattan distance is 300
    // (again ignoring z).  Using a value of about 300 for D seems like a reasonable
    // starting point for experiments. If in doubt, just use value 1.
    //
    // The distance between 3 & 4 are pretty small, too.
    // 3 = 5435, 223
    // 4 = 5948, 193
    //
    // Approx. 514 Euclidean distance and 533 Manhattan distance.
    //
    float manhattan(const ESM4::Vertex& a, const ESM4::Vertex& b)
    {
        return 300.0f * (abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z));
    }

    // Choose a heuristics - Note that these may not be the best for directed
    // graphs with non-uniform edge costs.
    //
    //   distance:
    //   - sqrt((curr.x - goal.x)^2 + (curr.y - goal.y)^2 + (curr.z - goal.z)^2)
    //   - slower but more accurate
    //
    //   Manhattan:
    //   - |curr.x - goal.x| + |curr.y - goal.y| + |curr.z - goal.z|
    //   - faster but not the shortest path
    float costAStar(const ESM4::Vertex& a, const ESM4::Vertex& b)
    {
        //return distance(a, b);
        return manhattan(a, b);
    }

    float sqrDistance(const osg::Vec2f& lhs, const osg::Vec2f& rhs)
    {
        return (lhs - rhs).length2();
    }

    float sqrDistanceIgnoreZ(const osg::Vec3f& lhs, const osg::Vec3f& rhs)
    {
        return sqrDistance(osg::Vec2f(lhs.x(), lhs.y()), osg::Vec2f(rhs.x(), rhs.y()));
    }

    float getPathStepSize(const MWWorld::ConstPtr& actor)
    {
        const auto world = MWBase::Environment::get().getWorld();
        const auto realHalfExtents = world->getHalfExtents(actor);
        return 2 * std::max(realHalfExtents.x(), realHalfExtents.y());
    }

    float getHeight(const MWWorld::ConstPtr& actor)
    {
        const auto world = MWBase::Environment::get().getWorld();
        const auto halfExtents = world->getHalfExtents(actor);
        return 2.0 * halfExtents.z();
    }

    // Returns true if turn in `p2` is less than 10 degrees and all the 3 points are almost on one line.
    bool isAlmostStraight(const osg::Vec3f& p1, const osg::Vec3f& p2, const osg::Vec3f& p3, float pointTolerance) {
        osg::Vec3f v1 = p1 - p2;
        osg::Vec3f v3 = p3 - p2;
        v1.z() = v3.z() = 0;
        float dotProduct = v1.x() * v3.x() + v1.y() * v3.y();
        float crossProduct = v1.x() * v3.y() - v1.y() * v3.x();

        // Check that the angle between v1 and v3 is less or equal than 5 degrees.
        static const float cos175 = std::cos(osg::PI * (175.0 / 180));
        bool checkAngle = dotProduct <= cos175 * v1.length() * v3.length();

        // Check that distance from p2 to the line (p1, p3) is less or equal than `pointTolerance`.
        bool checkDist = std::abs(crossProduct) <= pointTolerance * (p3 - p1).length();

        return checkAngle && checkDist;
    }

    struct IsValidShortcut
    {
        const DetourNavigator::Navigator* mNavigator;
        const DetourNavigator::AgentBounds mAgentBounds;
        const DetourNavigator::Flags mFlags;

        bool operator()(const osg::Vec3f& start, const osg::Vec3f& end) const
        {
            const auto position = DetourNavigator::raycast(*mNavigator, mAgentBounds, start, end, mFlags);
            return position.has_value() && std::abs((position.value() - start).length2() - (end - start).length2()) <= 1;
        }
    };
}

namespace F3Mechanics
{
    float getPathDistance(const MWWorld::Ptr& actor, const osg::Vec3f& lhs, const osg::Vec3f& rhs)
    {
        if (std::abs(lhs.z() - rhs.z()) > getHeight(actor) || MWMechanics::canActorMoveByZAxis(actor))
            return distance(lhs, rhs);
        return distanceIgnoreZ(lhs, rhs);
    }

    bool checkWayIsClear(const osg::Vec3f& from, const osg::Vec3f& to, float offsetXY)
    {
        osg::Vec3f dir = to - from;
        dir.z() = 0;
        dir.normalize();
        float verticalOffset = 200; // instead of '200' here we want the height of the actor
        osg::Vec3f _from = from + dir*offsetXY + osg::Z_AXIS * verticalOffset;

        // cast up-down ray and find height of hit in world space
        float h = _from.z() - MWBase::Environment::get().getWorld()->getDistToNearestRayHit(_from, -osg::Z_AXIS, verticalOffset + PATHFIND_Z_REACH + 1);

        return (std::abs(from.z() - h) <= PATHFIND_Z_REACH);
    }

    float PathFinder::getZAngleToNext(float x, float y) const
    {
        // This should never happen (programmers should have an if statement checking
        // isPathConstructed that prevents this call if otherwise).
        if(mPath.empty())
            return 0.;

        const auto& nextPoint = mPath.front();
        const float directionX = nextPoint.x() - x;
        const float directionY = nextPoint.y() - y;

        return std::atan2(directionX, directionY);
    }

    float PathFinder::getXAngleToNext(float x, float y, float z) const
    {
        // This should never happen (programmers should have an if statement checking
        // isPathConstructed that prevents this call if otherwise).
        if(mPath.empty())
            return 0.;

        const osg::Vec3f dir = mPath.front() - osg::Vec3f(x, y, z);

        return getXAngleToDir(dir);
    }

    void PathFinder::update(const osg::Vec3f& position, float pointTolerance, float destinationTolerance,
        bool shortenIfAlmostStraight, bool canMoveByZ, const DetourNavigator::AgentBounds& agentBounds,
        const DetourNavigator::Flags flags)
    {
        if (mPath.empty())
            return;

        while (mPath.size() > 1 && sqrDistanceIgnoreZ(mPath.front(), position) < pointTolerance * pointTolerance)
            mPath.pop_front();

        const IsValidShortcut isValidShortcut {
            MWBase::Environment::get().getWorld()->getNavigator(),
            agentBounds, flags
        };

        if (shortenIfAlmostStraight)
        {
            while (mPath.size() > 2 && isAlmostStraight(mPath[0], mPath[1], mPath[2], pointTolerance)
                   && isValidShortcut(mPath[0], mPath[2]))
                mPath.erase(mPath.begin() + 1);
            if (mPath.size() > 1 && isAlmostStraight(position, mPath[0], mPath[1], pointTolerance)
                    && isValidShortcut(position, mPath[1]))
                mPath.pop_front();
        }

        if (mPath.size() > 1)
        {
            std::size_t begin = 0;
            for (std::size_t i = 1; i < mPath.size(); ++i)
            {
                const float sqrDistance = Misc::getVectorToLine(position, mPath[i - 1], mPath[i]).length2();
                if (sqrDistance < pointTolerance * pointTolerance && isValidShortcut(position, mPath[i]))
                    begin = i;
            }
            for (std::size_t i = 0; i < begin; ++i)
                mPath.pop_front();
        }

        if (mPath.size() == 1)
        {
            float distSqr;
            if (canMoveByZ)
                distSqr = (mPath.front() - position).length2();
            else
                distSqr = sqrDistanceIgnoreZ(mPath.front(), position);
            if (distSqr < destinationTolerance * destinationTolerance)
                mPath.pop_front();
        }
    }

    void PathFinder::buildStraightPath(const osg::Vec3f& endPoint)
    {
        mPath.clear();
        mPath.push_back(endPoint);
        mConstructed = true;
    }

    void PathFinder::buildPathByNavMesh(const MWWorld::ConstPtr& actor, const osg::Vec3f& startPoint,
        const osg::Vec3f& endPoint, const DetourNavigator::AgentBounds& agentBounds, const DetourNavigator::Flags flags,
        const DetourNavigator::AreaCosts& areaCosts, float endTolerance, PathType pathType)
    {
        mPath.clear();

        // If it's not possible to build path over navmesh due to disabled navmesh generation fallback to straight path
        DetourNavigator::Status status = buildPathByNavigatorImpl(actor, startPoint, endPoint, agentBounds, flags,
            areaCosts, endTolerance, pathType, std::back_inserter(mPath));

        if (status != DetourNavigator::Status::Success)
            mPath.clear();

        if (status == DetourNavigator::Status::NavMeshNotFound)
            mPath.push_back(endPoint);

        mConstructed = !mPath.empty();
    }

    DetourNavigator::Status PathFinder::buildPathByNavigatorImpl(const MWWorld::ConstPtr& actor, const osg::Vec3f& startPoint,
        const osg::Vec3f& endPoint, const DetourNavigator::AgentBounds& agentBounds, const DetourNavigator::Flags flags,
        const DetourNavigator::AreaCosts& areaCosts, float endTolerance, PathType pathType,
        std::back_insert_iterator<std::deque<osg::Vec3f>> out)
    {
        const auto world = MWBase::Environment::get().getWorld();
        const auto stepSize = getPathStepSize(actor);
        const auto navigator = world->getNavigator();
        const auto status = DetourNavigator::findPath(*navigator, agentBounds, stepSize,
            startPoint, endPoint, flags, areaCosts, endTolerance, out);

        if (pathType == PathType::Partial && status == DetourNavigator::Status::PartialPath)
            return DetourNavigator::Status::Success;

        if (status != DetourNavigator::Status::Success)
        {
            Log(Debug::Debug) << "Build path by navigator error: \"" << DetourNavigator::getMessage(status)
                << "\" for \"" << actor.getClass().getName(actor) << "\" (" << actor.getBase()
                << ") from " << startPoint << " to " << endPoint << " with flags ("
                << DetourNavigator::WriteFlags {flags} << ")";
        }

        return status;
    }

    void PathFinder::buildPathByNavMeshToNextPoint(const MWWorld::ConstPtr& actor,
        const DetourNavigator::AgentBounds& agentBounds, const DetourNavigator::Flags flags,
        const DetourNavigator::AreaCosts& areaCosts)
    {
        if (mPath.empty())
            return;

        const auto stepSize = getPathStepSize(actor);
        const auto startPoint = actor.getRefData().getPosition().asVec3();

        if (sqrDistanceIgnoreZ(mPath.front(), startPoint) <= 4 * stepSize * stepSize)
            return;

        const auto navigator = MWBase::Environment::get().getWorld()->getNavigator();
        std::deque<osg::Vec3f> prePath;
        auto prePathInserter = std::back_inserter(prePath);
        const float endTolerance = 0;
        const auto status = DetourNavigator::findPath(*navigator, agentBounds, stepSize,
            startPoint, mPath.front(), flags, areaCosts, endTolerance, prePathInserter);

        if (status == DetourNavigator::Status::NavMeshNotFound)
            return;

        if (status != DetourNavigator::Status::Success)
        {
            Log(Debug::Debug) << "Build path by navigator error: \"" << DetourNavigator::getMessage(status)
                << "\" for \"" << actor.getClass().getName(actor) << "\" (" << actor.getBase()
                << ") from " << startPoint << " to " << mPath.front() << " with flags ("
                << DetourNavigator::WriteFlags {flags} << ")";
            return;
        }

        while (!prePath.empty() && sqrDistanceIgnoreZ(prePath.front(), startPoint) < stepSize * stepSize)
            prePath.pop_front();

        while (!prePath.empty() && sqrDistanceIgnoreZ(prePath.back(), mPath.front()) < stepSize * stepSize)
            prePath.pop_back();

        std::copy(prePath.rbegin(), prePath.rend(), std::front_inserter(mPath));
    }

    bool PathFinder::isPointConnected(const int start, const int end) const
    {
        return (mNavMesh->mTriangles[start].edge0 == mNavMesh->mTriangles[end].edge0
        && mNavMesh->mTriangles[start].edge1 == mNavMesh->mTriangles[end].edge1
        && mNavMesh->mTriangles[start].edge2 == mNavMesh->mTriangles[end].edge2);
    }
    std::deque<ESM4::Vertex> PathFinder::aStarSearch(const int start, const int goal) const
    {
        std::deque<ESM4::Vertex> path;
        if(!isPointConnected(start, goal))
        {
            return path; // there is no path, return an empty path
        }

        int graphSize = static_cast<int> (mNavMesh->mTriangles.size());
        std::vector<float> gScore (graphSize, -1);
        std::vector<float> fScore (graphSize, -1);
        std::vector<int> graphParent (graphSize, -1);

        // gScore & fScore keep costs for each pathgrid point in mPoints
        gScore[start] = 0;
        fScore[start] = costAStar(mNavMesh->mVertices[start], mNavMesh->mVertices[goal]);

        std::list<int> openset;
        std::list<int> closedset;
        openset.push_back(start);

        int current = -1;

        while(!openset.empty())
        {
            current = openset.front(); // front has the lowest cost
            openset.pop_front();

            if(current == goal)
                break;

            closedset.push_back(current); // remember we've been here

            uint16_t edges[] = {mNavMesh->mTriangles[current].edge0,mNavMesh->mTriangles[current].edge1,mNavMesh->mTriangles[current].edge2};
            uint16_t indices[] = {mNavMesh->mTriangles[current].vertexIndex0,mNavMesh->mTriangles[current].vertexIndex1,mNavMesh->mTriangles[current].vertexIndex2};
            // check all edges for the current point index
            for(int j = 0; j < 3; j++)
            {
                if(std::find(closedset.begin(), closedset.end(), indices[j]) ==
                   closedset.end())
                {
                    // not in closedset - i.e. have not traversed this edge destination
                    int dest = indices[j];
                    float tentative_g = gScore[current] + edges[j];
                    bool isInOpenSet = std::find(openset.begin(), openset.end(), dest) != openset.end();
                    if(!isInOpenSet
                        || tentative_g < gScore[dest])
                    {
                        graphParent[dest] = current;
                        gScore[dest] = tentative_g;
                        fScore[dest] = tentative_g + costAStar(mNavMesh->mVertices[dest],
                                                               mNavMesh->mVertices[goal]);
                        if(!isInOpenSet)
                        {
                            // add this edge to openset, lowest cost goes to the front
                            // TODO: if this causes performance problems a hash table may help
                            std::list<int>::iterator it = openset.begin();
                            for(it = openset.begin(); it!= openset.end(); ++it)
                            {
                                if(fScore[*it] > fScore[dest])
                                    break;
                            }
                            openset.insert(it, dest);
                        }
                    }
                } // if in closedset, i.e. traversed this edge already, try the next edge
            }
        }

        if(current != goal)
            return path; // for some reason couldn't build a path

        // reconstruct path to return, using local coordinates
        while(graphParent[current] != -1)
        {
            path.push_front(mNavMesh->mVertices[current]);
            current = graphParent[current];
        }

        // add first node to path explicitly
        path.push_front(mNavMesh->mVertices[start]);
        return path;
    }

    void PathFinder::buildPath(const MWWorld::ConstPtr &actor, const osg::Vec3f &startPoint, const osg::Vec3f &endPoint, const MWWorld::CellStore *cell, const DetourNavigator::AgentBounds &agentBounds, DetourNavigator::Flags flags, const DetourNavigator::AreaCosts &areaCosts, float endTolerance, PathType pathType)
    {
        mPath.clear();
        mCell = cell;

        DetourNavigator::Status status = DetourNavigator::Status::NavMeshNotFound;

        if (!actor.getClass().isPureWaterCreature(actor) && !actor.getClass().isPureFlyingCreature(actor))
        {
            status = buildPathByNavigatorImpl(actor, startPoint, endPoint, agentBounds, flags, areaCosts,
                                              endTolerance, pathType, std::back_inserter(mPath));
            if (status != DetourNavigator::Status::Success)
                mPath.clear();
        }

        if (status != DetourNavigator::Status::NavMeshNotFound && mPath.empty() && (flags & DetourNavigator::Flag_usePathgrid) == 0)
        {
            status = buildPathByNavigatorImpl(actor, startPoint, endPoint, agentBounds,
                flags | DetourNavigator::Flag_usePathgrid, areaCosts, endTolerance, pathType, std::back_inserter(mPath));
            if (status != DetourNavigator::Status::Success)
                mPath.clear();
        }

        if (status == DetourNavigator::Status::NavMeshNotFound && mPath.empty())
            mPath.push_back(endPoint);

        mConstructed = !mPath.empty();
    }
}
