#ifndef MWRENDER_TERRAINSTORAGE_H
#define MWRENDER_TERRAINSTORAGE_H

#include <memory>

#include <components/esm3terrain/storage.hpp>

#include <components/resource/resourcesystem.hpp>

namespace MWRender
{

    class LandManager;
    class TES4LandManager;

    /// @brief Connects the ESM Store used in OpenMW with the ESMTerrain storage.
    class TerrainStorage : public ESMTerrain::Storage
    {
    public:

        TerrainStorage(Resource::ResourceSystem* resourceSystem, const std::string& normalMapPattern = "", const std::string& normalHeightMapPattern = "", bool autoUseNormalMaps = false, const std::string& specularMapPattern = "", bool autoUseSpecularMaps = false);
        ~TerrainStorage();

        osg::ref_ptr<const ESMTerrain::LandObject> getLand (int cellX, int cellY) override;
        osg::ref_ptr<const ESMTerrain::TES4LandObject> getTes4Land(ESM4::FormId id) override;
        osg::ref_ptr<const ESMTerrain::TES4LandObject> getTes4Land(int cellX, int cellY, ESM4::FormId id) override;
        const ESM::LandTexture* getLandTexture(int index, short plugin) override;
        const ESM4::LandTexture* getTes4LandTexture(int index, short plugin) override { return nullptr; /*TODO*/ }

        bool hasData(int cellX, int cellY) override;

        bool hasData(ESM4::FormId id) override;

        /// Get bounds of the whole terrain in cell units
        void getBounds(float& minX, float& maxX, float& minY, float& maxY) override;

        LandManager* getLandManager() const;
        TES4LandManager* getTES4LandManager() const;

    private:
       std::unique_ptr<LandManager> mLandManager;
       std::unique_ptr<TES4LandManager> mTES4LandManager;

       Resource::ResourceSystem* mResourceSystem;
    };

}


#endif
