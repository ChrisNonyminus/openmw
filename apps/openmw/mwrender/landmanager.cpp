#include "landmanager.hpp"

#include <osg/Stats>

#include <components/resource/objectcache.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwworld/esmstore.hpp"

namespace MWRender
{

LandManager::LandManager(int loadFlags)
    : GenericResourceManager<std::pair<int, int> >(nullptr)
    , mLoadFlags(loadFlags)
{
    mCache = new CacheType;
}

osg::ref_ptr<ESMTerrain::LandObject> LandManager::getLand(int x, int y)
{
    osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(std::make_pair(x,y));
    if (obj)
        return static_cast<ESMTerrain::LandObject*>(obj.get());
    else
    {
        const auto world = MWBase::Environment::get().getWorld();
        if (!world)
            return nullptr;
        const ESM::Land* land = world->getStore().get<ESM::Land>().search(x,y);
        if (!land)
            return nullptr;
        osg::ref_ptr<ESMTerrain::LandObject> landObj (new ESMTerrain::LandObject(land, mLoadFlags));
        mCache->addEntryToObjectCache(std::make_pair(x,y), landObj.get());
        return landObj;
    }
}

void LandManager::reportStats(unsigned int frameNumber, osg::Stats* stats) const
{
    stats->setAttribute(frameNumber, "Land", mCache->getCacheSize());
}


TES4LandManager::TES4LandManager(int loadFlags)
: GenericResourceManager<uint32_t >(nullptr)
    , mLoadFlags(loadFlags)
{
    mCache = new CacheType;
}

osg::ref_ptr<ESMTerrain::TES4LandObject> TES4LandManager::getLand(uint32_t id)
{
    osg::ref_ptr<osg::Object> obj = mCache->getRefFromObjectCache(id);
    if (obj)
        return static_cast<ESMTerrain::TES4LandObject*>(obj.get());
    else
    {
        const auto world = MWBase::Environment::get().getWorld();
        if (!world)
            return nullptr;
        std::string idStr = ESM4::formIdToString(id);
        const ESM4::Land* land = world->getStore().get<ESM4::Land>().search(idStr);
        if (!land)
            return nullptr;
        osg::ref_ptr<ESMTerrain::TES4LandObject> landObj(new ESMTerrain::TES4LandObject(land, mLoadFlags));
        mCache->addEntryToObjectCache(id, landObj.get());
        return landObj;
    }
}

void TES4LandManager::reportStats(unsigned int frameNumber, osg::Stats* stats) const
{
    stats->setAttribute(frameNumber, "Land (TES4)", mCache->getCacheSize());
}

}
