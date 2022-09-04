#ifndef GAME_MWWORLD_CELLSTORE_H
#define GAME_MWWORLD_CELLSTORE_H

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeinfo>
#include <map>
#include <memory>

#include "livecellref.hpp"
#include "cellreflist.hpp"

#include <components/esm3/loadacti.hpp>
#include <components/esm3/loadalch.hpp>
#include <components/esm3/loadappa.hpp>
#include <components/esm3/loadarmo.hpp>
#include <components/esm3/loadbook.hpp>
#include <components/esm3/loadclot.hpp>
#include <components/esm3/loadcont.hpp>
#include <components/esm3/loadcrea.hpp>
#include <components/esm3/loaddoor.hpp>
#include <components/esm3/loadingr.hpp>
#include <components/esm3/loadlevlist.hpp>
#include <components/esm3/loadligh.hpp>
#include <components/esm3/loadlock.hpp>
#include <components/esm3/loadprob.hpp>
#include <components/esm3/loadrepa.hpp>
#include <components/esm3/loadstat.hpp>
#include <components/esm3/loadweap.hpp>
#include <components/esm3/loadnpc.hpp>
#include <components/esm3/loadmisc.hpp>
#include <components/esm3/loadbody.hpp>
#include <components/esm3/fogstate.hpp>

#include <components/esm4/records.hpp>

#include "timestamp.hpp"
#include "ptr.hpp"

namespace ESM
{
    class ReadersCache;
    struct Cell;
    struct CellState;
    struct CellId;
    struct RefNum;
}

namespace MWWorld
{
    class ESMStore;

    /// \brief Mutable state of a cell
    class CellStore
    {
        public:

            enum State
            {
                State_Unloaded, State_Preloaded, State_Loaded
            };

            // https://gpfault.net/posts/mapping-types-to-values.txt.html
            class TypeMap
            {
            private:
                typedef std::map<int, CellRefListBase*> InternalMap;
                InternalMap mMap;

            public:
                typedef typename InternalMap::iterator iterator;
                typedef typename InternalMap::const_iterator const_iterator;
                typedef typename InternalMap::value_type value_type;

                const_iterator begin() const { return mMap.begin(); }
                const_iterator end() const { return mMap.end(); }
                iterator begin() { return mMap.begin(); }
                iterator end() { return mMap.end(); }

                template <class Key>
                iterator find() { return mMap.find(getTypeId<Key>()); }

                template <class Key>
                const_iterator find() const { return mMap.find(getTypeId<Key>()); }

                template <class Key>
                CellRefList<Key>& get() { return *static_cast<CellRefList<Key>*>(find<Key>()->second); }

                template <class Key>
                const CellRefList<Key>& get() const { return *static_cast<CellRefList<Key>*>(find<Key>()->second); }

                template <class Key>
                void put(CellRefList<Key>*&& value)
                {
                    mMap[getTypeId<Key>()] = std::forward<CellRefList<Key>*>(value);
                }

            private:
                template <class Key>
                inline static int getTypeId()
                {
                    static const int id = sLastTypeId++;
                    return id;
                }

                static int sLastTypeId;
            };

        private:

            const MWWorld::ESMStore& mStore;
            ESM::ReadersCache& mReaders;

            // Even though fog actually belongs to the player and not cells,
            // it makes sense to store it here since we need it once for each cell.
            // Note this is nullptr until the cell is explored to save some memory
            std::unique_ptr<ESM::FogState> mFogState;

            const ESM::Cell *mCell;
            const ESM4::Cell* mCell4;
            bool mIsTes4;
            State mState;
            bool mHasState;
            std::vector<std::string> mIds;
            float mWaterLevel;

            MWWorld::TimeStamp mLastRespawn;
            
            // map of lists stored by this cell
            TypeMap mTypeMap;
            typedef std::map<LiveCellRefBase*, MWWorld::CellStore*> MovedRefTracker;
            // References owned by a different cell that have been moved here.
            // <reference, cell the reference originally came from>
            MovedRefTracker mMovedHere;
            // References owned by this cell that have been moved to another cell.
            // <reference, cell the reference was moved to>
            MovedRefTracker mMovedToAnotherCell;

            // Merged list of ref's currently in this cell - i.e. with added refs from mMovedHere, removed refs from mMovedToAnotherCell
            std::vector<LiveCellRefBase*> mMergedRefs;

            // Get the Ptr for the given ref which originated from this cell (possibly moved to another cell at this point).
            Ptr getCurrentPtr(MWWorld::LiveCellRefBase* ref);

            /// Moves object from the given cell to this cell.
            void moveFrom(const MWWorld::Ptr& object, MWWorld::CellStore* from);

            /// Repopulate mMergedRefs.
            void updateMergedRefs();

            // (item, max charge)
            typedef std::vector<std::pair<LiveCellRefBase*, float> > TRechargingItems;
            TRechargingItems mRechargingItems;

            bool mRechargingItemsUpToDate;

            void updateRechargingItems();
            void rechargeItems(float duration);
            void checkItem(const Ptr& ptr);

            // helper function for forEachInternal
            template<class Visitor, class List>
            bool forEachImp (Visitor& visitor, List& list)
            {
                for (typename List::List::iterator iter (list.mList.begin()); iter!=list.mList.end();
                    ++iter)
                {
                    if (!isAccessible(iter->mData, iter->mRef))
                        continue;
                    if (!visitor (MWWorld::Ptr(&*iter, this)))
                        return false;
                }
                return true;
            }

            // listing only objects owned by this cell. Internal use only, you probably want to use forEach() so that moved objects are accounted for.
            template<class Visitor>
            bool forEachInternal (Visitor& visitor)
            {
                return
                    forEachImp (visitor, get<ESM::Activator>()) &&
                    forEachImp (visitor, get<ESM::Potion>()) &&
                    forEachImp (visitor, get<ESM::Apparatus>()) &&
                    forEachImp (visitor, get<ESM::Armor>()) &&
                    forEachImp (visitor, get<ESM::Book>()) &&
                    forEachImp (visitor, get<ESM::Clothing>()) &&
                    forEachImp (visitor, get<ESM::Container>()) &&
                    forEachImp (visitor, get<ESM::Door>()) &&
                    forEachImp (visitor, get<ESM::Ingredient>()) &&
                    forEachImp (visitor, get<ESM::ItemLevList>()) &&
                    forEachImp (visitor, get<ESM::Light>()) &&
                    forEachImp (visitor, get<ESM::Lockpick>()) &&
                    forEachImp (visitor, get<ESM::Miscellaneous>()) &&
                    forEachImp (visitor, get<ESM::Probe>()) &&
                    forEachImp (visitor, get<ESM::Repair>()) &&
                    forEachImp (visitor, get<ESM::Static>()) &&
                    forEachImp (visitor, get<ESM::Weapon>()) &&
                    forEachImp (visitor, get<ESM::BodyPart>()) &&
                    forEachImp (visitor, get<ESM::Creature>()) &&
                    forEachImp (visitor, get<ESM::NPC>()) &&
                    forEachImp (visitor, get<ESM::CreatureLevList>());
            }

            /// @note If you get a linker error here, this means the given type can not be stored in a cell. The supported types are
            /// defined at the bottom of this file.
            template <class T>
            CellRefList<T>& get()
            {
                return mTypeMap.get<T>();
            }
            template <class T>
            const CellRefList<T>& get() const
            {
                return mTypeMap.get<T>();
            }

        public:

            /// Should this reference be accessible to the outside world (i.e. to scripts / game logic)?
            /// Determined based on the deletion flags. By default, objects deleted by content files are never accessible;
            /// objects deleted by setCount(0) are still accessible *if* they came from a content file (needed for vanilla
            /// scripting compatibility, and the fact that objects may be "un-deleted" in the original game).
            static bool isAccessible(const MWWorld::RefData& refdata, const MWWorld::CellRef& cref)
            {
                return !refdata.isDeletedByContentFile() && (cref.hasContentFile() || refdata.getCount() > 0);
            }

            /// Moves object from this cell to the given cell.
            /// @note automatically updates given cell by calling cellToMoveTo->moveFrom(...)
            /// @note throws exception if cellToMoveTo == this
            /// @return updated MWWorld::Ptr with the new CellStore pointer set.
            MWWorld::Ptr moveTo(const MWWorld::Ptr& object, MWWorld::CellStore* cellToMoveTo);

            void rest(double hours);
            void recharge(float duration);

            /// Make a copy of the given object and insert it into this cell.
            /// @note If you get a linker error here, this means the given type can not be inserted into a cell.
            /// The supported types are defined at the bottom of this file.
            template <typename T>
            LiveCellRefBase* insert(const LiveCellRef<T>* ref)
            {
                mHasState = true;
                CellRefList<T>& list = get<T>();
                LiveCellRefBase* ret = &list.insert(*ref);
                updateMergedRefs();
                return ret;
            }

            /// @param readerList The readers to use for loading of the cell on-demand.
            CellStore(const ESM::Cell* cell, const MWWorld::ESMStore& store, ESM::ReadersCache& readers);
            CellStore(const ESM4::Cell* cell, const MWWorld::ESMStore& store, ESM::ReadersCache& readers);

            const ESM::Cell* getCell() const;
            const ESM4::Cell* getCell4() const;

            State getState() const;

            const std::vector<std::string>& getPreloadedIds() const;
            ///< Get Ids of objects in this cell, only valid in State_Preloaded

            bool hasState() const;
            ///< Does this cell have state that needs to be stored in a saved game file?

            bool hasId(std::string_view id) const;
            ///< May return true for deleted IDs when in preload state. Will return false, if cell is
            /// unloaded.
            /// @note Will not account for moved references which may exist in Loaded state. Use search() instead if the cell is loaded.

            Ptr search(std::string_view id);
            ///< Will return an empty Ptr if cell is not loaded. Does not check references in
            /// containers.
            /// @note Triggers CellStore hasState flag.

            ConstPtr searchConst(std::string_view id) const;
            ///< Will return an empty Ptr if cell is not loaded. Does not check references in
            /// containers.
            /// @note Does not trigger CellStore hasState flag.

            Ptr searchViaActorId (int id);
            ///< Will return an empty Ptr if cell is not loaded.

            Ptr searchViaRefNum (const ESM::RefNum& refNum);
            ///< Will return an empty Ptr if cell is not loaded. Does not check references in
            /// containers.
            /// @note Triggers CellStore hasState flag.

            float getWaterLevel() const;

            bool movedHere(const MWWorld::Ptr& ptr) const;

            void setWaterLevel (float level);

            void setFog(std::unique_ptr<ESM::FogState>&& fog);
            ///< \note Takes ownership of the pointer

            ESM::FogState* getFog () const;

            std::size_t count() const;
            ///< Return total number of references, including deleted ones.

            void load ();
            ///< Load references from content file.

            void preload ();
            ///< Build ID list from content file.

            /// Call visitor (MWWorld::Ptr) for each reference. visitor must return a bool. Returning
            /// false will abort the iteration.
            /// \note Prefer using forEachConst when possible.
            /// \note Do not modify this cell (i.e. remove/add objects) during the forEach, doing this may result in unintended behaviour.
            /// \attention This function also lists deleted (count 0) objects!
            /// \return Iteration completed?
            template<class Visitor>
            bool forEach (Visitor&& visitor)
            {
                if (mState != State_Loaded)
                    return false;

                if (mMergedRefs.empty())
                    return true;

                mHasState = true;

                for (unsigned int i=0; i<mMergedRefs.size(); ++i)
                {
                    if (!isAccessible(mMergedRefs[i]->mData, mMergedRefs[i]->mRef))
                        continue;

                    if (!visitor(MWWorld::Ptr(mMergedRefs[i], this)))
                        return false;
                }
                return true;
            }

            /// Call visitor (MWWorld::ConstPtr) for each reference. visitor must return a bool. Returning
            /// false will abort the iteration.
            /// \note Do not modify this cell (i.e. remove/add objects) during the forEach, doing this may result in unintended behaviour.
            /// \attention This function also lists deleted (count 0) objects!
            /// \return Iteration completed?
            template<class Visitor>
            bool forEachConst (Visitor&& visitor) const
            {
                if (mState != State_Loaded)
                    return false;

                for (unsigned int i=0; i<mMergedRefs.size(); ++i)
                {
                    if (!isAccessible(mMergedRefs[i]->mData, mMergedRefs[i]->mRef))
                        continue;

                    if (!visitor(MWWorld::ConstPtr(mMergedRefs[i], this)))
                        return false;
                }
                return true;
            }


            /// Call visitor (ref) for each reference of given type. visitor must return a bool. Returning
            /// false will abort the iteration.
            /// \note Do not modify this cell (i.e. remove/add objects) during the forEach, doing this may result in unintended behaviour.
            /// \attention This function also lists deleted (count 0) objects!
            /// \return Iteration completed?
            template <class T, class Visitor>
            bool forEachType(Visitor& visitor)
            {
                if (mState != State_Loaded)
                    return false;

                if (mMergedRefs.empty())
                    return true;

                mHasState = true;

                CellRefList<T>& list = get<T>();

                for (typename CellRefList<T>::List::iterator it (list.mList.begin()); it!=list.mList.end(); ++it)
                {
                    LiveCellRefBase* base = &*it;
                    if (mMovedToAnotherCell.find(base) != mMovedToAnotherCell.end())
                        continue;
                    if (!isAccessible(base->mData, base->mRef))
                        continue;
                    if (!visitor(MWWorld::Ptr(base, this)))
                        return false;
                }

                for (MovedRefTracker::const_iterator it = mMovedHere.begin(); it != mMovedHere.end(); ++it)
                {
                    LiveCellRefBase* base = it->first;
                    if (dynamic_cast<LiveCellRef<T>*>(base))
                        if (!visitor(MWWorld::Ptr(base, this)))
                            return false;
                }
                return true;
            }

            // NOTE: does not account for moved references
            // Should be phased out when we have const version of forEach
            inline const CellRefList<ESM::Door>& getReadOnlyDoors() const
            {
                return mTypeMap.get<ESM::Door>();
            }
            inline const CellRefList<ESM::Static>& getReadOnlyStatics() const
            {
                return mTypeMap.get<ESM::Static>();
            }
            inline const CellRefList<ESM4::Static>& getReadOnlyTes4Statics() const
            {
                return mTypeMap.get<ESM4::Static>();
            }

            bool isExterior() const;

            bool isQuasiExterior() const;

            Ptr searchInContainer(std::string_view id);

            void loadState (const ESM::CellState& state);

            void saveState (ESM::CellState& state) const;

            void writeFog (ESM::ESMWriter& writer) const;

            void readFog (ESM::ESMReader& reader);

            void writeReferences (ESM::ESMWriter& writer) const;

            struct GetCellStoreCallback
            {
            public:
                ///@note must return nullptr if the cell is not found
                virtual CellStore* getCellStore(const ESM::CellId& cellId) = 0;
                virtual ~GetCellStoreCallback() = default;
            };

            /// @param callback to use for retrieving of additional CellStore objects by ID (required for resolving moved references)
            void readReferences (ESM::ESMReader& reader, const std::map<int, int>& contentFileMap, GetCellStoreCallback* callback);

            void respawn ();
            ///< Check mLastRespawn and respawn references if necessary. This is a no-op if the cell is not loaded.

            Ptr getMovedActor(int actorId) const;

            bool isTes4();

            bool isTes4() const;

        private:

            /// Run through references and store IDs
            void listRefs();

            void loadRefs();

            void loadRef (ESM::CellRef& ref, bool deleted, std::map<ESM::RefNum, std::string>& refNumToID);

            void loadRef (const ESM4::Reference& ref, bool deleted, std::map<ESM::RefNum, std::string>& refNumToID);
            ///< Make case-adjustments to \a ref and insert it into the respective container.
            ///
            /// Invalid \a ref objects are silently dropped.
    };

    bool operator== (const CellStore& left, const CellStore& right);
    bool operator!= (const CellStore& left, const CellStore& right);
}

#endif
