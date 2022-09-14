#ifndef OPENMW_MWWORLD_CELLUTILS_H
#define OPENMW_MWWORLD_CELLUTILS_H

#include <components/misc/constants.hpp>

#include <osg/Vec2i>

#include <cmath>

namespace MWWorld
{
    osg::Vec2i positionToCellIndex(float x, float y);
}

#endif
