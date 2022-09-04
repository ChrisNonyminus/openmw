#ifndef GAME_MWWORLD_CELLS_H
#define GAME_MWWORLD_CELLS_H

#include <map>
#include <list>
#include <string>

#include "ptr.hpp"
#include "store.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;
    class ReadersCache;
    struct CellId;
    struct Cell;
    struct RefNum;
}

namespace ESM4
{
    struct Cell;
    struct Reference;
    class Reader;
}

namespace Loading
{
    class Listener;
}

namespace MWWorld
{
    class ESMStore;

    /// \brief Cell container
    class Cells
    {
            typedef std::vector<std::pair<std::string, CellStore *> > IdCache;
            const MWWorld::ESMStore& mStore;
            ESM::ReadersCache& mReaders;
            mutable std::map<std::string, CellStore> mInteriors;
            mutable std::map<std::pair<int, int>, CellStore> mExteriors;
            mutable std::map<Store<ESM4::Cell>::ExtLocation, CellStore> mExteriorsByWorldSpace;
            IdCache mIdCache;
            std::size_t mIdCacheIndex;

            Cells (const Cells&);
            Cells& operator= (const Cells&);

            CellStore *getCellStore (const ESM::Cell *cell);

            CellStore *getCellStore (const ESM4::Cell *cell);

            Ptr getPtrAndCache(std::string_view name, CellStore& cellStore);

            Ptr getPtr(CellStore& cellStore, const std::string& id, const ESM::RefNum& refNum);

            void writeCell (ESM::ESMWriter& writer, CellStore& cell) const;

        public:

            void clear();

            explicit Cells(const MWWorld::ESMStore& store, ESM::ReadersCache& reader);

            CellStore *getExterior (int x, int y, ESM4::FormId wrld = 0);

            CellStore* getInterior(std::string_view name);

            CellStore *getCell (const ESM::CellId& id);

            CellStore *getCell (const std::string& id);

            Ptr getPtr(std::string_view name, CellStore& cellStore, bool searchInContainers = false);
            ///< \param searchInContainers Only affect loaded cells.
            /// @note name must be lower case

            /// @note name must be lower case
            Ptr getPtr (const std::string& name);

            Ptr getPtr(const std::string& id, const ESM::RefNum& refNum);

            void rest (double hours);
            void recharge (float duration);

            /// Get all Ptrs referencing \a name in exterior cells
            /// @note Due to the current implementation of getPtr this only supports one Ptr per cell.
            /// @note name must be lower case
            void getExteriorPtrs(std::string_view name, std::vector<MWWorld::Ptr>& out);

            /// Get all Ptrs referencing \a name in interior cells
            /// @note Due to the current implementation of getPtr this only supports one Ptr per cell.
            /// @note name must be lower case
            void getInteriorPtrs (const std::string& name, std::vector<MWWorld::Ptr>& out);

            std::vector<MWWorld::Ptr> getAll(const std::string& id);

            int countSavedGameRecords() const;

            void write (ESM::ESMWriter& writer, Loading::Listener& progress) const;

            bool readRecord (ESM::ESMReader& reader, uint32_t type,
                const std::map<int, int>& contentFileMap);

            bool readRecord(ESM4::Reader& reader, uint32_t type,
                const std::map<int, int>& contentFileMap);
    };
}

#endif
