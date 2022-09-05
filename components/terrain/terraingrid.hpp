#ifndef COMPONENTS_TERRAIN_TERRAINGRID_H
#define COMPONENTS_TERRAIN_TERRAINGRID_H

#include <map>

#include <osg/Vec2f>

#include <components/esm4/common.hpp>
#include <components/esm4/loadcell.hpp>

#include "world.hpp"

namespace osg
{
    class Group;
    class Stats;
}

namespace Resource
{
    class ResourceSystem;
}

namespace Terrain
{
    class Storage;

    /// @brief Simple terrain implementation that loads cells in a grid, with no LOD. Only requested cells are loaded.
    class TerrainGrid : public Terrain::World
    {
    public:
        TerrainGrid(osg::Group* parent, osg::Group* compileRoot, Resource::ResourceSystem* resourceSystem, Storage* storage, unsigned int nodeMask, unsigned int preCompileMask=~0u, unsigned int borderMask=0);
        TerrainGrid(osg::Group* parent, Storage* storage, unsigned int nodeMask=~0u);
        ~TerrainGrid();

        void cacheCell(View* view, int x, int y) override;

        /// @note Not thread safe.
        void loadCell(int x, int y) override;

        /// @note Not thread safe.
        void unloadCell(int x, int y) override;

        /// @note Not thread safe.
        void loadCell(const ESM4::Cell* cell, const ESM4::Cell** chunkNeighbors) override;

        /// @note Not thread safe.
        void unloadCell(uint32_t wrldId, int x, int y) override;

        View* createView() override;

    protected:
        bool isGridEmpty() const { return mGrid.empty(); }
        bool isTES4GridEmpty() const { return mTES4Grid.empty(); }

    private:
        osg::ref_ptr<osg::Node> buildTerrain (osg::Group* parent, float chunkSize, const osg::Vec2f& chunkCenter);
        osg::ref_ptr<osg::Node> buildTerrain(osg::Group* parent, float chunkSize, const ESM4::Cell* chunkCenter, const ESM4::Cell** chunkNeighbors);
        void updateWaterCulling();

        // split each ESM::Cell into mNumSplits*mNumSplits terrain chunks
        unsigned int mNumSplits;

        CellBorder::CellGrid mGrid;

        CellBorder::TES4Grid mTES4Grid;
    };
}

#endif
