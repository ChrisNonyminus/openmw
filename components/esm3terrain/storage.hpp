#ifndef COMPONENTS_ESM_TERRAIN_STORAGE_H
#define COMPONENTS_ESM_TERRAIN_STORAGE_H

#include <cassert>
#include <mutex>

#include <components/terrain/storage.hpp>

#include <components/esm3/loadland.hpp>
#include <components/esm3/loadltex.hpp>

#include <components/esm4/loadland.hpp>
#include <components/esm4/loadcell.hpp>
#include <components/esm4/loadltex.hpp>

namespace VFS
{
    class Manager;
}

namespace ESMTerrain
{

    class LandCache;
    class TES4LandCache;

    /// @brief Wrapper around Land Data with reference counting. The wrapper needs to be held as long as the data is still in use
    class LandObject : public osg::Object
    {
    public:
        LandObject();
        LandObject(const ESM::Land* land, int loadFlags);
        LandObject(const LandObject& copy, const osg::CopyOp& copyop);
        virtual ~LandObject();

        META_Object(ESMTerrain, LandObject)

        inline const ESM::Land::LandData* getData(int flags) const
        {
            if ((mData.mDataLoaded & flags) != flags)
                return nullptr;
            return &mData;
        }
        inline int getPlugin() const { return mLand->getPlugin(); }

    private:
        const ESM::Land* mLand;
        int mLoadFlags;

        ESM::Land::LandData mData;
    };

    /// @brief Wrapper around Land Data with reference counting. The wrapper needs to be held as long as the data is still in use
    class TES4LandObject : public osg::Object
    {
    public:
        TES4LandObject();
        TES4LandObject(const ESM4::Land* land, int loadFlags);
        TES4LandObject(const TES4LandObject& copy, const osg::CopyOp& copyop);
        virtual ~TES4LandObject();

        META_Object(ESMTerrain, TES4LandObject)

        inline const ESM4::Land* getData(int flags) const
        {
            return &mLand;
        }
        inline int getPlugin() const { return 0; /*TODO*/ }

    private:
        ESM4::Land mLand;
        int mLoadFlags;
    };

    /// @brief Feeds data from ESM terrain records (ESM::Land, ESM::LandTexture)
    ///        into the terrain component, converting it on the fly as needed.
    class Storage : public Terrain::Storage
    {
    public:
        Storage(const VFS::Manager* vfs, const std::string& normalMapPattern = "", const std::string& normalHeightMapPattern = "", bool autoUseNormalMaps = false, const std::string& specularMapPattern = "", bool autoUseSpecularMaps = false);

        // Not implemented in this class, because we need different Store implementations for game and editor
        virtual osg::ref_ptr<const LandObject> getLand (int cellX, int cellY)= 0;
        virtual const ESM::LandTexture* getLandTexture(int index, short plugin) = 0;
        virtual osg::ref_ptr<const TES4LandObject> getTes4Land(ESM4::FormId id) = 0;
        virtual osg::ref_ptr<const TES4LandObject> getTes4Land(int cellX, int cellY, uint32_t formId) = 0;
        virtual const ESM4::LandTexture* getTes4LandTexture(int index, short plugin) = 0;
        /// Get bounds of the whole terrain in cell units
        void getBounds(float& minX, float& maxX, float& minY, float& maxY) override = 0;

        /// Get the minimum and maximum heights of a terrain region.
        /// @note Will only be called for chunks with size = minBatchSize, i.e. leafs of the quad tree.
        ///        Larger chunks can simply merge AABB of children.
        /// @param size size of the chunk in cell units
        /// @param center center of the chunk in cell units
        /// @param min min height will be stored here
        /// @param max max height will be stored here
        /// @return true if there was data available for this terrain chunk
        bool getMinMaxHeights (float size, const osg::Vec2f& center, float& min, float& max) override;

        /// Fill vertex buffers for a terrain chunk.
        /// @note May be called from background threads. Make sure to only call thread-safe functions from here!
        /// @note Vertices should be written in row-major order (a row is defined as parallel to the x-axis).
        ///       The specified positions should be in local space, i.e. relative to the center of the terrain chunk.
        /// @param lodLevel LOD level, 0 = most detailed
        /// @param size size of the terrain chunk in cell units
        /// @param center center of the chunk in cell units
        /// @param positions buffer to write vertices
        /// @param normals buffer to write vertex normals
        /// @param colours buffer to write vertex colours
        void fillVertexBuffers (int lodLevel, float size, const osg::Vec2f& center,
                                osg::ref_ptr<osg::Vec3Array> positions,
                                osg::ref_ptr<osg::Vec3Array> normals,
                                osg::ref_ptr<osg::Vec4ubArray> colours) override;
        void fillTes4VertexBuffers(int lodLevel, float size, const ESM4::Cell* startCell,
                                    osg::ref_ptr<osg::Vec3Array> positions,
                                    osg::ref_ptr<osg::Vec3Array> normals,
                                    osg::ref_ptr<osg::Vec4ubArray> colours) override;

        /// Create textures holding layer blend values for a terrain chunk.
        /// @note The terrain chunk shouldn't be larger than one cell since otherwise we might
        ///       have to do a ridiculous amount of different layers. For larger chunks, composite maps should be used.
        /// @note May be called from background threads.
        /// @param chunkSize size of the terrain chunk in cell units
        /// @param chunkCenter center of the chunk in cell units
        /// @param blendmaps created blendmaps will be written here
        /// @param layerList names of the layer textures used will be written here
        void getBlendmaps (float chunkSize, const osg::Vec2f& chunkCenter, ImageVector& blendmaps,
                               std::vector<Terrain::LayerInfo>& layerList) override;

        float getHeightAt (const osg::Vec3f& worldPos, uint32_t wrldid) override;

        /// Get the transformation factor for mapping cell units to world units.
        float getCellWorldSize() override;

        /// Get the number of vertices on one side for each cell. Should be (power of two)+1
        int getCellVertices() override;

        int getBlendmapScale(float chunkSize) override;

        float getVertexHeight (const ESM::Land::LandData* data, int x, int y)
        {
            assert(x < ESM::Land::LAND_SIZE);
            assert(y < ESM::Land::LAND_SIZE);
            return data->mHeights[y * ESM::Land::LAND_SIZE + x];
        }

        float getVertexHeight (const ESM4::Land* data, int x, int y)
        {
            assert(x < ESM4::Land::VERTS_PER_SIDE);
            assert(y < ESM4::Land::VERTS_PER_SIDE);
            return data->mHeightMapF[y * ESM4::Land::VERTS_PER_SIDE + x];
        }

        osg::Vec2f getTes4MinMaxHeights(const ESM4::Land* data, float defMin, float defMax) override;

    private:
        const VFS::Manager* mVFS;

        inline void fixNormal (osg::Vec3f& normal, int cellX, int cellY, int col, int row, LandCache& cache);
        inline void fixColour (osg::Vec4ub& colour, int cellX, int cellY, int col, int row, LandCache& cache);
        inline void averageNormal (osg::Vec3f& normal, int cellX, int cellY, int col, int row, LandCache& cache);

        inline void fixNormal (osg::Vec3f& normal, uint32_t formId, int cellX, int cellY, int col, int row, TES4LandCache& cache);
        inline void fixColour (osg::Vec4ub& colour, uint32_t formId, int cellX, int cellY, int col, int row, TES4LandCache& cache);
        inline void averageNormal (osg::Vec3f& normal, uint32_t formId, int cellX, int cellY, int col, int row, TES4LandCache& cache);

        inline const LandObject* getLand(int cellX, int cellY, LandCache& cache);
        inline const TES4LandObject* getTes4Land(uint32_t formId, TES4LandCache& cache);
        inline const TES4LandObject* getTes4Land(int cellX, int cellY, uint32_t wrldId, TES4LandCache& cache);

        virtual bool useAlteration() const { return false; }
        virtual void adjustColor(int col, int row, const ESM::Land::LandData *heightData, osg::Vec4ub& color) const;
        virtual void adjustColor(int col, int row, const ESM4::Land *heightData, osg::Vec4ub& color) const;
        virtual float getAlteredHeight(int col, int row) const;

        // Since plugins can define new texture palettes, we need to know the plugin index too
        // in order to retrieve the correct texture name.
        // pair  <texture id, plugin id>
        typedef std::pair<short, short> UniqueTextureId;

        inline UniqueTextureId getVtexIndexAt(int cellX, int cellY, int x, int y, LandCache&);

        inline UniqueTextureId getVtexIndexAt(uint32_t formId, int cellX, int cellY, int x, int y, TES4LandCache&);
        std::string getTextureName (UniqueTextureId id);

        std::map<std::string, Terrain::LayerInfo> mLayerInfoMap;
        std::mutex mLayerInfoMutex;

        std::string mNormalMapPattern;
        std::string mNormalHeightMapPattern;
        bool mAutoUseNormalMaps;

        std::string mSpecularMapPattern;
        bool mAutoUseSpecularMaps;

        Terrain::LayerInfo getLayerInfo(const std::string& texture);
    };

}

#endif
