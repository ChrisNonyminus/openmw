#include "cellstore.hpp"
#include "magiceffects.hpp"

#include <algorithm>

#include <components/debug/debuglog.hpp>

#include <components/esm3/cellstate.hpp>
#include <components/esm3/cellid.hpp>
#include <components/esm3/cellref.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/objectstate.hpp>
#include <components/esm3/containerstate.hpp>
#include <components/esm3/npcstate.hpp>
#include <components/esm3/creaturestate.hpp>
#include <components/esm3/fogstate.hpp>
#include <components/esm3/creaturelevliststate.hpp>
#include <components/esm3/doorstate.hpp>
#include <components/esm3/readerscache.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/luamanager.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwmechanics/creaturestats.hpp"
#include "../mwmechanics/recharge.hpp"

#include "ptr.hpp"
#include "esmloader.hpp"
#include "esmstore.hpp"
#include "class.hpp"
#include "containerstore.hpp"

namespace
{
    template<typename T>
    MWWorld::Ptr searchInContainerList(MWWorld::CellRefList<T>& containerList, std::string_view id)
    {
        for (typename MWWorld::CellRefList<T>::List::iterator iter (containerList.mList.begin());
             iter!=containerList.mList.end(); ++iter)
        {
            MWWorld::Ptr container (&*iter, nullptr);

            if (container.getRefData().getCustomData() == nullptr)
                continue;

            MWWorld::Ptr ptr =
                container.getClass().getContainerStore (container).search (id);

            if (!ptr.isEmpty())
                return ptr;
        }

        return MWWorld::Ptr();
    }

    template<typename T>
    MWWorld::Ptr searchViaActorId (MWWorld::CellRefList<T>& actorList, int actorId,
        MWWorld::CellStore *cell, const std::map<MWWorld::LiveCellRefBase*, MWWorld::CellStore*>& toIgnore)
    {
        for (typename MWWorld::CellRefList<T>::List::iterator iter (actorList.mList.begin());
             iter!=actorList.mList.end(); ++iter)
        {
            MWWorld::Ptr actor (&*iter, cell);

            if (toIgnore.find(&*iter) != toIgnore.end())
                continue;

            if (actor.getClass().getCreatureStats (actor).matchesActorId (actorId) && actor.getRefData().getCount() > 0)
                return actor;
        }

        return MWWorld::Ptr();
    }

    template<typename RecordType, typename T>
    void writeReferenceCollection (ESM::ESMWriter& writer,
        const MWWorld::CellRefList<T>& collection)
    {
        if (!collection.mList.empty())
        {
            // references
            for (typename MWWorld::CellRefList<T>::List::const_iterator
                iter (collection.mList.begin());
                iter!=collection.mList.end(); ++iter)
            {
                if (!iter->mData.hasChanged() && !iter->mRef.hasChanged() && iter->mRef.hasContentFile())
                {
                    // Reference that came from a content file and has not been changed -> ignore
                    continue;
                }
                if (iter->mData.getCount()==0 && !iter->mRef.hasContentFile())
                {
                    // Deleted reference that did not come from a content file -> ignore
                    continue;
                }

                RecordType state;
                iter->save (state);

                // recordId currently unused
                writer.writeHNT ("OBJE", collection.mList.front().mBase->sRecordId);

                state.save (writer);
            }
        }
    }

    template<class RecordType, class T>
    void fixRestockingImpl(const T* base, RecordType& state)
    {
        // Workaround for old saves not containing negative quantities
        for(const auto& baseItem : base->mInventory.mList)
        {
            if(baseItem.mCount < 0)
            {
                for(auto& item : state.mInventory.mItems)
                {
                    if(item.mCount > 0 && Misc::StringUtils::ciEqual(baseItem.mItem, item.mRef.mRefID))
                        item.mCount = -item.mCount;
                }
            }
        }
    }

    template<class RecordType, class T>
    void fixRestocking(const T* base, RecordType& state)
    {}

    template<>
    void fixRestocking<>(const ESM::Creature* base, ESM::CreatureState& state)
    {
        fixRestockingImpl(base, state);
    }

    template<>
    void fixRestocking<>(const ESM::NPC* base, ESM::NpcState& state)
    {
        fixRestockingImpl(base, state);
    }

    template<>
    void fixRestocking<>(const ESM::Container* base, ESM::ContainerState& state)
    {
        fixRestockingImpl(base, state);
    }

    template<typename RecordType, typename T>
    void readReferenceCollection (ESM::ESMReader& reader,
        MWWorld::CellRefList<T>& collection, const ESM::CellRef& cref, const std::map<int, int>& contentFileMap, MWWorld::CellStore* cellstore)
    {
        const MWWorld::ESMStore& esmStore = MWBase::Environment::get().getWorld()->getStore();

        RecordType state;
        state.mRef = cref;
        state.load(reader);

        // If the reference came from a content file, make sure this content file is loaded
        if (state.mRef.mRefNum.hasContentFile())
        {
            std::map<int, int>::const_iterator iter =
                contentFileMap.find (state.mRef.mRefNum.mContentFile);

            if (iter==contentFileMap.end())
                return; // content file has been removed -> skip

            state.mRef.mRefNum.mContentFile = iter->second;
        }

        if (!MWWorld::LiveCellRef<T>::checkState (state))
            return; // not valid anymore with current content files -> skip

        const T *record = esmStore.get<T>().search (state.mRef.mRefID);

        if (!record)
            return;

        if (state.mVersion < 15)
            fixRestocking(record, state);
        if (state.mVersion < 17)
        {
            if constexpr (std::is_same_v<T, ESM::Creature>)
                MWWorld::convertMagicEffects(state.mCreatureStats, state.mInventory);
            else if constexpr (std::is_same_v<T, ESM::NPC>)
                MWWorld::convertMagicEffects(state.mCreatureStats, state.mInventory, &state.mNpcStats);
        }
        else if(state.mVersion < 20)
        {
            if constexpr (std::is_same_v<T, ESM::Creature> || std::is_same_v<T, ESM::NPC>)
                MWWorld::convertStats(state.mCreatureStats);
        }

        if (state.mRef.mRefNum.hasContentFile())
        {
            for (typename MWWorld::CellRefList<T>::List::iterator iter (collection.mList.begin());
                iter!=collection.mList.end(); ++iter)
                if (iter->mRef.getRefNum()==state.mRef.mRefNum && iter->mRef.getRefId() == state.mRef.mRefID)
                {
                    // overwrite existing reference
                    float oldscale = iter->mRef.getScale();
                    iter->load (state);
                    const ESM::Position & oldpos = iter->mRef.getPosition();
                    const ESM::Position & newpos = iter->mData.getPosition();
                    const MWWorld::Ptr ptr(&*iter, cellstore);
                    if ((oldscale != iter->mRef.getScale() || oldpos.asVec3() != newpos.asVec3() || oldpos.rot[0] != newpos.rot[0] || oldpos.rot[1] != newpos.rot[1] || oldpos.rot[2] != newpos.rot[2]) && !ptr.getClass().isActor())
                        MWBase::Environment::get().getWorld()->moveObject(ptr, newpos.asVec3());
                    if (!iter->mData.isEnabled())
                    {
                        iter->mData.enable();
                        MWBase::Environment::get().getWorld()->disable(MWWorld::Ptr(&*iter, cellstore));
                    }
                    else
                        MWBase::Environment::get().getLuaManager()->registerObject(MWWorld::Ptr(&*iter, cellstore));
                    return;
                }

            Log(Debug::Warning) << "Warning: Dropping reference to " << state.mRef.mRefID << " (invalid content file link)";
            return;
        }

        // new reference
        MWWorld::LiveCellRef<T> ref (record);
        ref.load (state);
        collection.mList.push_back (ref);

        MWWorld::LiveCellRefBase* base = &collection.mList.back();
        MWBase::Environment::get().getLuaManager()->registerObject(MWWorld::Ptr(base, cellstore));
    }
}

// TODO: consolidate the 3 below rigmaroles with their equivalent in ESMStore

template <size_t I = 0, typename... Ts, typename F>
constexpr void iterateTuple(std::tuple<Ts...> tup, F&& fun)
{
    if constexpr (I < sizeof...(Ts))
    {
        fun(std::get<I>(tup));
        iterateTuple<I + 1>(tup, fun);
    }
}

template <typename T>
struct TypeGetter
{
    typedef T type;

    void print() { std::cout << typeid(type).name() << "\n"; } // for debugging
};

namespace MWWorld
{

    template <typename... T>
    struct AllTypeGetters
    {
        static std::tuple<T...> sAll;
        static constexpr size_t size = sizeof...(T);
    };

    int CellStore::TypeMap::sLastTypeId = 0;

    using CellStoreTypes = AllTypeGetters<
        TypeGetter<ESM::Activator>,
        TypeGetter<ESM::Potion>,
        TypeGetter<ESM::Apparatus>,
        TypeGetter<ESM::Armor>,
        TypeGetter<ESM::Book>,
        TypeGetter<ESM::Clothing>,
        TypeGetter<ESM::Container>,
        TypeGetter<ESM::Creature>,
        TypeGetter<ESM::Door>,
        TypeGetter<ESM::Ingredient>,
        TypeGetter<ESM::CreatureLevList>,
        TypeGetter<ESM::ItemLevList>,
        TypeGetter<ESM::Light>,
        TypeGetter<ESM::Lockpick>,
        TypeGetter<ESM::Miscellaneous>,
        TypeGetter<ESM::NPC>,
        TypeGetter<ESM::Probe>,
        TypeGetter<ESM::Repair>,
        TypeGetter<ESM::Static>,
        TypeGetter<ESM::Weapon>,
        TypeGetter<ESM::BodyPart>,

        TypeGetter<ESM4::Activator>,
        TypeGetter<ESM4::Static>,
        TypeGetter<ESM4::Light>,
        TypeGetter<ESM4::Sound>,
        TypeGetter<ESM4::Creature>,
        TypeGetter<ESM4::LevelledCreature>,
        TypeGetter<ESM4::AIPackage>,
        TypeGetter<ESM4::Armor>,
        TypeGetter<ESM4::Weapon>,
        TypeGetter<ESM4::LevelledItem>,
        TypeGetter<ESM4::Ammunition>,
        TypeGetter<ESM4::Npc>
        >;

    template <>
    std::tuple<
        TypeGetter<ESM::Activator>,
        TypeGetter<ESM::Potion>,
        TypeGetter<ESM::Apparatus>,
        TypeGetter<ESM::Armor>,
        TypeGetter<ESM::Book>,
        TypeGetter<ESM::Clothing>,
        TypeGetter<ESM::Container>,
        TypeGetter<ESM::Creature>,
        TypeGetter<ESM::Door>,
        TypeGetter<ESM::Ingredient>,
        TypeGetter<ESM::CreatureLevList>,
        TypeGetter<ESM::ItemLevList>,
        TypeGetter<ESM::Light>,
        TypeGetter<ESM::Lockpick>,
        TypeGetter<ESM::Miscellaneous>,
        TypeGetter<ESM::NPC>,
        TypeGetter<ESM::Probe>,
        TypeGetter<ESM::Repair>,
        TypeGetter<ESM::Static>,
        TypeGetter<ESM::Weapon>,
        TypeGetter<ESM::BodyPart>,
        TypeGetter<ESM4::Activator>,
        TypeGetter<ESM4::Static>,
        TypeGetter<ESM4::Light>,
        TypeGetter<ESM4::Sound>,
        TypeGetter<ESM4::Creature>,
        TypeGetter<ESM4::LevelledCreature>,
        TypeGetter<ESM4::AIPackage>,
        TypeGetter<ESM4::Armor>,
        TypeGetter<ESM4::Weapon>,
        TypeGetter<ESM4::LevelledItem>,
        TypeGetter<ESM4::Ammunition>,
        TypeGetter<ESM4::Npc>>
        CellStoreTypes::sAll = std::make_tuple(
            
        TypeGetter<ESM::Activator>(),
        TypeGetter<ESM::Potion>(),
        TypeGetter<ESM::Apparatus>(),
        TypeGetter<ESM::Armor>(),
        TypeGetter<ESM::Book>(),
        TypeGetter<ESM::Clothing>(),
        TypeGetter<ESM::Container>(),
        TypeGetter<ESM::Creature>(),
        TypeGetter<ESM::Door>(),
        TypeGetter<ESM::Ingredient>(),
        TypeGetter<ESM::CreatureLevList>(),
        TypeGetter<ESM::ItemLevList>(),
        TypeGetter<ESM::Light>(),
        TypeGetter<ESM::Lockpick>(),
        TypeGetter<ESM::Miscellaneous>(),
        TypeGetter<ESM::NPC>(),
        TypeGetter<ESM::Probe>(),
        TypeGetter<ESM::Repair>(),
        TypeGetter<ESM::Static>(),
        TypeGetter<ESM::Weapon>(),
        TypeGetter<ESM::BodyPart>(),
        TypeGetter<ESM4::Activator>(),
        TypeGetter<ESM4::Static>(),
        TypeGetter<ESM4::Light>(),
        TypeGetter<ESM4::Sound>(),
        TypeGetter<ESM4::Creature>(),
        TypeGetter<ESM4::LevelledCreature>(),
        TypeGetter<ESM4::AIPackage>(),
        TypeGetter<ESM4::Armor>(),
        TypeGetter<ESM4::Weapon>(),
        TypeGetter<ESM4::LevelledItem>(),
        TypeGetter<ESM4::Ammunition>(),
        TypeGetter<ESM4::Npc>());
    template <typename X>
    void CellRefList<X>::load(ESM::CellRef &ref, bool deleted, const MWWorld::ESMStore &esmStore)
    {
        const MWWorld::Store<X> &store = esmStore.get<X>();

        if (const X *ptr = store.search (ref.mRefID))
        {
            typename std::list<LiveRef>::iterator iter =
                std::find(mList.begin(), mList.end(), ref.mRefNum);

            LiveRef liveCellRef (ref, ptr);

            if (deleted)
                liveCellRef.mData.setDeletedByContentFile(true);

            if (iter != mList.end())
                *iter = liveCellRef;
            else
                mList.push_back (liveCellRef);
        }
        else
        {
            Log(Debug::Warning)
                << "Warning: could not resolve cell reference '" << ref.mRefID << "'"
                << " (dropping reference)";
        }
    }

    template <typename X>
    void CellRefList<X>::load(const ESM4::Reference& ref, bool deleted, const MWWorld::ESMStore& esmStore)
    {
        const MWWorld::Store<X>& store = esmStore.get<X>();

        if (const X* ptr = store.search(ref.mBaseObj))
        {
            //typename std::list<LiveRef>::iterator iter = std::find(mList.begin(), mList.end(), ref.);

            LiveRef liveCellRef(ref, ptr);

            if (deleted)
                liveCellRef.mData.setDeletedByContentFile(true);

            /*if (iter != mList.end())
                *iter = liveCellRef;
            else
                */mList.push_back(liveCellRef);
        }
        else
        {
            Log(Debug::Warning)
                << "Warning: could not resolve cell reference '" << esmStore.getFormName(ref.mBaseObj) << "'"
                << " (dropping reference)";
        }
    }

    template<typename X> bool operator==(const LiveCellRef<X>& ref, int pRefnum)
    {
        return (ref.mRef.mRefnum == pRefnum);
    }

    Ptr CellStore::getCurrentPtr(LiveCellRefBase *ref)
    {
        MovedRefTracker::iterator found = mMovedToAnotherCell.find(ref);
        if (found != mMovedToAnotherCell.end())
            return Ptr(ref, found->second);
        return Ptr(ref, this);
    }

    void CellStore::moveFrom(const Ptr &object, CellStore *from)
    {
        if (mState != State_Loaded)
            load();

        mHasState = true;
        MovedRefTracker::iterator found = mMovedToAnotherCell.find(object.getBase());
        if (found != mMovedToAnotherCell.end())
        {
            // A cell we had previously moved an object to is returning it to us.
            assert (found->second == from);
            mMovedToAnotherCell.erase(found);
        }
        else
        {
            mMovedHere.insert(std::make_pair(object.getBase(), from));
        }
        updateMergedRefs();
    }

    MWWorld::Ptr CellStore::moveTo(const Ptr &object, CellStore *cellToMoveTo)
    {
        if (cellToMoveTo == this)
            throw std::runtime_error("moveTo: object is already in this cell");

        // We assume that *this is in State_Loaded since we could hardly have reference to a live object otherwise.
        if (mState != State_Loaded)
            throw std::runtime_error("moveTo: can't move object from a non-loaded cell (how did you get this object anyway?)");

        // Ensure that the object actually exists in the cell
        if (searchViaRefNum(object.getCellRef().getRefNum()).isEmpty())
            throw std::runtime_error("moveTo: object is not in this cell");

        MWBase::Environment::get().getLuaManager()->registerObject(MWWorld::Ptr(object.getBase(), cellToMoveTo));

        MovedRefTracker::iterator found = mMovedHere.find(object.getBase());
        if (found != mMovedHere.end())
        {
            // Special case - object didn't originate in this cell
            // Move it back to its original cell first
            CellStore* originalCell = found->second;
            assert (originalCell != this);
            originalCell->moveFrom(object, this);

            mMovedHere.erase(found);

            // Now that object is back to its rightful owner, we can move it
            if (cellToMoveTo != originalCell)
            {
                originalCell->moveTo(object, cellToMoveTo);
            }

            updateMergedRefs();
            return MWWorld::Ptr(object.getBase(), cellToMoveTo);
        }

        cellToMoveTo->moveFrom(object, this);
        mMovedToAnotherCell.insert(std::make_pair(object.getBase(), cellToMoveTo));

        updateMergedRefs();
        return MWWorld::Ptr(object.getBase(), cellToMoveTo);
    }

    struct MergeVisitor
    {
        MergeVisitor(std::vector<LiveCellRefBase*>& mergeTo, const std::map<LiveCellRefBase*, MWWorld::CellStore*>& movedHere,
                     const std::map<LiveCellRefBase*, MWWorld::CellStore*>& movedToAnotherCell)
            : mMergeTo(mergeTo)
            , mMovedHere(movedHere)
            , mMovedToAnotherCell(movedToAnotherCell)
        {
        }

        bool operator() (const MWWorld::Ptr& ptr)
        {
            if (mMovedToAnotherCell.find(ptr.getBase()) != mMovedToAnotherCell.end())
                return true;
            mMergeTo.push_back(ptr.getBase());
            return true;
        }

        void merge()
        {
            for (const auto & [base, _] : mMovedHere)
                mMergeTo.push_back(base);
        }

    private:
        std::vector<LiveCellRefBase*>& mMergeTo;

        const std::map<LiveCellRefBase*, MWWorld::CellStore*>& mMovedHere;
        const std::map<LiveCellRefBase*, MWWorld::CellStore*>& mMovedToAnotherCell;
    };

    void CellStore::updateMergedRefs()
    {
        mMergedRefs.clear();
        mRechargingItemsUpToDate = false;
        MergeVisitor visitor(mMergedRefs, mMovedHere, mMovedToAnotherCell);
        forEachInternal(visitor);
        visitor.merge();
    }

    bool CellStore::movedHere(const MWWorld::Ptr& ptr) const
    {
        if (ptr.isEmpty())
            return false;

        if (mMovedHere.find(ptr.getBase()) != mMovedHere.end())
            return true;

        return false;
    }

    CellStore::CellStore(const ESM::Cell* cell, const MWWorld::ESMStore& esmStore, ESM::ReadersCache& readers)
        : mStore(esmStore)
        , mReaders(readers)
        , mCell(cell), mCell4(nullptr)
        , mState(State_Unloaded)
        , mHasState(false)
        , mLastRespawn(0, 0), mIsTes4(false)
        , mRechargingItemsUpToDate(false), mLand(0)
    {
        mWaterLevel = cell->mWater;
        iterateTuple(CellStoreTypes::sAll, [&](auto typeGetter)
        {
            using type = typename decltype(typeGetter)::type;
            mTypeMap.put<type>(new CellRefList<type>());
        });
    }

    CellStore::CellStore(const ESM4::Cell* cell, const MWWorld::ESMStore& store, ESM::ReadersCache& readers)
        : mStore(store),
          mCell4(cell), mCell(nullptr),
        mState(State_Unloaded),
        mHasState(false),
          mLastRespawn(0, 0), mIsTes4(true),
        mRechargingItemsUpToDate(false),
          mReaders(readers), mLand(0)
    {
        mWaterLevel = cell->mParent ? store.get<ESM4::World>().search(store.getFormName(cell->mParent))->mWaterLevel : cell->mWaterHeight;
        iterateTuple(CellStoreTypes::sAll, [&](auto typeGetter)
            {
                using type = typename decltype(typeGetter)::type;
                mTypeMap.put<type>(new CellRefList<type>());
            });
    }

    const ESM::Cell *CellStore::getCell() const
    {
        return mCell;
    }

    const ESM4::Cell* CellStore::getCell4() const
    {
        return mCell4;
    }

    ESM4::FormId CellStore::getLandId() const
    {
        assert(isTes4());
        return mLand;
    }

    CellStore::State CellStore::getState() const
    {
        return mState;
    }

    const std::vector<std::string> &CellStore::getPreloadedIds() const
    {
        return mIds;
    }

    bool CellStore::hasState() const
    {
        return mHasState;
    }

    bool CellStore::hasId(std::string_view id) const
    {
        if (mState==State_Unloaded)
            return false;

        if (mState==State_Preloaded)
            return std::binary_search (mIds.begin(), mIds.end(), id);

        return searchConst (id).isEmpty();
    }

    template <typename PtrType>
    struct SearchVisitor
    {
        PtrType mFound;
        std::string_view mIdToFind;
        bool operator()(const PtrType& ptr)
        {
            if (ptr.getCellRef().getRefId() == mIdToFind)
            {
                mFound = ptr;
                return false;
            }
            return true;
        }
    };

    Ptr CellStore::search(std::string_view id)
    {
        SearchVisitor<MWWorld::Ptr> searchVisitor;
        searchVisitor.mIdToFind = id;
        forEach(searchVisitor);
        return searchVisitor.mFound;
    }

    ConstPtr CellStore::searchConst(std::string_view id) const
    {
        SearchVisitor<MWWorld::ConstPtr> searchVisitor;
        searchVisitor.mIdToFind = id;
        forEachConst(searchVisitor);
        return searchVisitor.mFound;
    }

    Ptr CellStore::searchViaActorId (int id)
    {
        if (Ptr ptr = ::searchViaActorId (get<ESM::NPC>(), id, this, mMovedToAnotherCell))
            return ptr;

        if (Ptr ptr = ::searchViaActorId (get<ESM::Creature>(), id, this, mMovedToAnotherCell))
            return ptr;

        for (const auto& [base, _] : mMovedHere)
        {
            MWWorld::Ptr actor (base, this);
            if (!actor.getClass().isActor())
                continue;
            if (actor.getClass().getCreatureStats (actor).matchesActorId (id) && actor.getRefData().getCount() > 0)
                return actor;
        }

        return Ptr();
    }

    Ptr CellStore::searchViaFormId(uint32_t id)
    {

        for (const auto& [base, _] : mMovedHere)
        {
            MWWorld::Ptr actor(base, this);
            if (!actor.getClass().isActor())
                continue;
            if (actor.getClass().hasFormId() && actor.getCellRef().getRefr().mFormId == id)
                return actor;
        }

        return Ptr();
    }

    Ptr CellStore::searchViaEditorId(const std::string& id)
    {

        for (const auto& [base, _] : mMovedHere)
        {
            MWWorld::Ptr actor(base, this);
            if (!actor.getClass().isActor())
                continue;
            if (actor.getClass().hasFormId() && actor.getClass().getId(actor) == id)
                return actor;
        }

        return Ptr();
    }

    class RefNumSearchVisitor
    {
        const ESM::RefNum& mRefNum;
    public:
        RefNumSearchVisitor(const ESM::RefNum& refNum) : mRefNum(refNum) {}

        Ptr mFound;

        bool operator()(const Ptr& ptr)
        {
            if (ptr.getCellRef().getRefNum() == mRefNum)
            {
                mFound = ptr;
                return false;
            }
            return true;
        }
    };

    Ptr CellStore::searchViaRefNum (const ESM::RefNum& refNum)
    {
        RefNumSearchVisitor searchVisitor(refNum);
        forEach(searchVisitor);
        return searchVisitor.mFound;
    }

    float CellStore::getWaterLevel() const
    {
        return mWaterLevel;
    }

    void CellStore::setWaterLevel (float level)
    {
        mWaterLevel = level;
        mHasState = true;
    }

    std::size_t CellStore::count() const
    {
        return mMergedRefs.size();
    }

    void CellStore::load ()
    {
        if (mState!=State_Loaded)
        {
            if (mState==State_Preloaded)
                mIds.clear();

            loadRefs ();

            mState = State_Loaded;
        }
    }

    void CellStore::preload ()
    {
        if (mState==State_Unloaded)
        {
            listRefs ();

            mState = State_Preloaded;
        }
    }

    void CellStore::listRefs()
    {
        if (isTes4())
            return;
        assert (mCell);

        if (mCell->mContextList.empty())
            return; // this is a dynamically generated cell -> skipping.

        // Load references from all plugins that do something with this cell.
        for (size_t i = 0; i < mCell->mContextList.size(); i++)
        {
            try
            {
                // Reopen the ESM reader and seek to the right position.
                const std::size_t index = static_cast<std::size_t>(mCell->mContextList[i].index);
                const ESM::ReadersCache::BusyItem reader = mReaders.get(index);
                mCell->restore(*reader, i);

                ESM::CellRef ref;

                // Get each reference in turn
                ESM::MovedCellRef cMRef;
                cMRef.mRefNum.mIndex = 0;
                bool deleted = false;
                bool moved = false;
                while (ESM::Cell::getNextRef(*reader, ref, deleted, cMRef, moved, ESM::Cell::GetNextRefMode::LoadOnlyNotMoved))
                {
                    if (deleted || moved)
                        continue;

                    // Don't list reference if it was moved to a different cell.
                    ESM::MovedCellRefTracker::const_iterator iter =
                        std::find(mCell->mMovedRefs.begin(), mCell->mMovedRefs.end(), ref.mRefNum);
                    if (iter != mCell->mMovedRefs.end()) {
                        continue;
                    }

                    Misc::StringUtils::lowerCaseInPlace(ref.mRefID);
                    mIds.push_back(std::move(ref.mRefID));
                }
            }
            catch (std::exception& e)
            {
                Log(Debug::Error) << "An error occurred listing references for cell " << getCell()->getDescription() << ": " << e.what();
            }
        }

        // List moved references, from separately tracked list.
        for (const auto& [ref, deleted]: mCell->mLeasedRefs)
        {
            if (!deleted)
                mIds.push_back(Misc::StringUtils::lowerCase(ref.mRefID));
        }

        std::sort (mIds.begin(), mIds.end());
    }

    void CellStore::loadRefs()
    {
        if (mIsTes4)
        {
            assert(mCell4);
            auto& refs = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Reference>();
            auto it = refs.begin();

            std::map<ESM::RefNum, std::string> refNumToID; // used to detect refID modifications
            while (it != refs.end())
            {
                if (it->mParent == mCell4->mFormId)
                {
                    if ((it->mFlags & ESM4::Rec_Deleted) == 0)
                    {
                        loadRef(*it, false, refNumToID);
                    }
                }
                ++it;
            }

            // hacky: get the land
            auto& land = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Land>();
            auto lit = land.begin();
            while (lit != land.end())
            {
                if (lit->mCell == mCell4->mFormId)
                {
                    mLand = lit->mFormId;
                    break;
                }
                ++lit;
            }

            // test

            for (auto& obj : get<ESM4::Activator>().mList)
            {
                mMergedRefs.push_back(&obj);
            }
            for (auto& obj : get<ESM4::Static>().mList)
            {
                mMergedRefs.push_back(&obj);
            }
            for (auto& obj : get<ESM4::Light>().mList)
            {
                mMergedRefs.push_back(&obj);
            }
            for (auto& obj : get<ESM4::Sound>().mList)
            {
                mMergedRefs.push_back(&obj);
            }
        }
        else
        {
            assert(mCell);

            if (mCell->mContextList.empty())
                return; // this is a dynamically generated cell -> skipping.

            std::map<ESM::RefNum, std::string> refNumToID; // used to detect refID modifications

            // Load references from all plugins that do something with this cell.
            for (size_t i = 0; i < mCell->mContextList.size(); i++)
            {
                try
                {
                    // Reopen the ESM reader and seek to the right position.
                    const std::size_t index = static_cast<std::size_t>(mCell->mContextList[i].index);
                    const ESM::ReadersCache::BusyItem reader = mReaders.get(index);
                    mCell->restore(*reader, i);

                    ESM::CellRef ref;
                    ref.mRefNum.unset();

                    // Get each reference in turn
                    ESM::MovedCellRef cMRef;
                    cMRef.mRefNum.mIndex = 0;
                    bool deleted = false;
                    bool moved = false;
                    while (ESM::Cell::getNextRef(*reader, ref, deleted, cMRef, moved, ESM::Cell::GetNextRefMode::LoadOnlyNotMoved))
                    {
                        if (moved)
                            continue;

                        // Don't load reference if it was moved to a different cell.
                        ESM::MovedCellRefTracker::const_iterator iter = std::find(mCell->mMovedRefs.begin(), mCell->mMovedRefs.end(), ref.mRefNum);
                        if (iter != mCell->mMovedRefs.end())
                        {
                            continue;
                        }

                        loadRef(ref, deleted, refNumToID);
                    }
                }
                catch (std::exception& e)
                {
                    Log(Debug::Error) << "An error occurred loading references for cell " << getCell()->getDescription() << ": " << e.what();
                }
            }

            // Load moved references, from separately tracked list.
            for (const auto& leasedRef : mCell->mLeasedRefs)
            {
                ESM::CellRef& ref = const_cast<ESM::CellRef&>(leasedRef.first);
                bool deleted = leasedRef.second;

                loadRef(ref, deleted, refNumToID);
            }

            updateMergedRefs();
        }
    }

    bool CellStore::isExterior() const
    {
        if (isTes4())
            return mCell4->isExterior();
        return mCell->isExterior();
    }

    bool CellStore::isQuasiExterior() const
    {
        if (isTes4())
            return false;
        return (mCell->mData.mFlags & ESM::Cell::QuasiEx) != 0;
    }

    Ptr CellStore::searchInContainer(std::string_view id)
    {
        bool oldState = mHasState;

        mHasState = true;

        if (Ptr ptr = searchInContainerList (get<ESM::Container>(), id))
            return ptr;

        if (Ptr ptr = searchInContainerList (get<ESM::Creature>(), id))
            return ptr;

        if (Ptr ptr = searchInContainerList (get<ESM::NPC>(), id))
            return ptr;

        mHasState = oldState;

        return Ptr();
    }

    void CellStore::loadRef (ESM::CellRef& ref, bool deleted, std::map<ESM::RefNum, std::string>& refNumToID)
    {
        Misc::StringUtils::lowerCaseInPlace (ref.mRefID);

        const MWWorld::ESMStore& store = mStore;

        std::map<ESM::RefNum, std::string>::iterator it = refNumToID.find(ref.mRefNum);
        if (it != refNumToID.end())
        {
            if (it->second != ref.mRefID)
            {
                // refID was modified, make sure we don't end up with duplicated refs
                switch (store.find(it->second))
                {
                    case ESM::REC_ACTI: get<ESM::Activator>().remove(ref.mRefNum); break;
                    case ESM::REC_ALCH: get<ESM::Potion>().remove(ref.mRefNum); break;
                    case ESM::REC_APPA: get<ESM::Apparatus>().remove(ref.mRefNum); break;
                    case ESM::REC_ARMO: get<ESM::Armor>().remove(ref.mRefNum); break;
                    case ESM::REC_BOOK: get<ESM::Book>().remove(ref.mRefNum); break;
                    case ESM::REC_CLOT: get<ESM::Clothing>().remove(ref.mRefNum); break;
                    case ESM::REC_CONT: get<ESM::Container>().remove(ref.mRefNum); break;
                    case ESM::REC_CREA: get<ESM::Creature>().remove(ref.mRefNum); break;
                    case ESM::REC_DOOR: get<ESM::Door>().remove(ref.mRefNum); break;
                    case ESM::REC_INGR: get<ESM::Ingredient>().remove(ref.mRefNum); break;
                    case ESM::REC_LEVC: get<ESM::CreatureLevList>().remove(ref.mRefNum); break;
                    case ESM::REC_LEVI: get<ESM::ItemLevList>().remove(ref.mRefNum); break;
                    case ESM::REC_LIGH: get<ESM::Light>().remove(ref.mRefNum); break;
                    case ESM::REC_LOCK: get<ESM::Lockpick>().remove(ref.mRefNum); break;
                    case ESM::REC_MISC: get<ESM::Miscellaneous>().remove(ref.mRefNum); break;
                    case ESM::REC_NPC_: get<ESM::NPC>().remove(ref.mRefNum); break;
                    case ESM::REC_PROB: get<ESM::Probe>().remove(ref.mRefNum); break;
                    case ESM::REC_REPA: get<ESM::Repair>().remove(ref.mRefNum); break;
                    case ESM::REC_STAT: get<ESM::Static>().remove(ref.mRefNum); break;
                    case ESM::REC_WEAP: get<ESM::Weapon>().remove(ref.mRefNum); break;
                    case ESM::REC_BODY: get<ESM::BodyPart>().remove(ref.mRefNum); break;
                    default:
                        break;
                }
            }
        }

        switch (store.find (ref.mRefID))
        {
            case ESM::REC_ACTI: get<ESM::Activator>().load(ref, deleted, store); break;
            case ESM::REC_ALCH: get<ESM::Potion>().load(ref, deleted,store); break;
            case ESM::REC_APPA: get<ESM::Apparatus>().load(ref, deleted, store); break;
            case ESM::REC_ARMO: get<ESM::Armor>().load(ref, deleted, store); break;
            case ESM::REC_BOOK: get<ESM::Book>().load(ref, deleted, store); break;
            case ESM::REC_CLOT: get<ESM::Clothing>().load(ref, deleted, store); break;
            case ESM::REC_CONT: get<ESM::Container>().load(ref, deleted, store); break;
            case ESM::REC_CREA: get<ESM::Creature>().load(ref, deleted, store); break;
            case ESM::REC_DOOR: get<ESM::Door>().load(ref, deleted, store); break;
            case ESM::REC_INGR: get<ESM::Ingredient>().load(ref, deleted, store); break;
            case ESM::REC_LEVC: get<ESM::CreatureLevList>().load(ref, deleted, store); break;
            case ESM::REC_LEVI: get<ESM::ItemLevList>().load(ref, deleted, store); break;
            case ESM::REC_LIGH: get<ESM::Light>().load(ref, deleted, store); break;
            case ESM::REC_LOCK: get<ESM::Lockpick>().load(ref, deleted, store); break;
            case ESM::REC_MISC: get<ESM::Miscellaneous>().load(ref, deleted, store); break;
            case ESM::REC_NPC_: get<ESM::NPC>().load(ref, deleted, store); break;
            case ESM::REC_PROB: get<ESM::Probe>().load(ref, deleted, store); break;
            case ESM::REC_REPA: get<ESM::Repair>().load(ref, deleted, store); break;
            case ESM::REC_STAT: get<ESM::Static>().load(ref, deleted, store); break;
            case ESM::REC_WEAP: get<ESM::Weapon>().load(ref, deleted, store); break;
            case ESM::REC_BODY: get<ESM::BodyPart>().load(ref, deleted, store); break;

            case 0: Log(Debug::Error) << "Cell reference '" + ref.mRefID + "' not found!"; return;

            default:
                Log(Debug::Error) << "Error: Ignoring reference '" << ref.mRefID << "' of unhandled type";
                return;
        }

        refNumToID[ref.mRefNum] = ref.mRefID;
    }

    void CellStore::loadRef(const ESM4::Reference& ref, bool deleted, std::map<ESM::RefNum, std::string>& refNumToID)
    {
        std::string id = MWBase::Environment::get().getWorld()->getStore().getFormName(ref.mBaseObj);
        Misc::StringUtils::lowerCaseInPlace(id);

        const MWWorld::ESMStore& store = mStore;

        //std::map<ESM::RefNum, std::string>::iterator it = refNumToID.find(ref);
        //if (it != refNumToID.end())
        //{
        //    if (it->second != ref.mRefID)
        //    {
        //        // refID was modified, make sure we don't end up with duplicated refs
        //        switch (store.find(it->second))
        //        {
        //            case ESM::REC_ACTI: get<ESM::Activator>().remove(ref.mRefNum); break;
        //            case ESM::REC_ALCH: get<ESM::Potion>().remove(ref.mRefNum); break;
        //            case ESM::REC_APPA: get<ESM::Apparatus>().remove(ref.mRefNum); break;
        //            case ESM::REC_ARMO: get<ESM::Armor>().remove(ref.mRefNum); break;
        //            case ESM::REC_BOOK: get<ESM::Book>().remove(ref.mRefNum); break;
        //            case ESM::REC_CLOT: get<ESM::Clothing>().remove(ref.mRefNum); break;
        //            case ESM::REC_CONT: get<ESM::Container>().remove(ref.mRefNum); break;
        //            case ESM::REC_CREA: get<ESM::Creature>().remove(ref.mRefNum); break;
        //            case ESM::REC_DOOR: get<ESM::Door>().remove(ref.mRefNum); break;
        //            case ESM::REC_INGR: get<ESM::Ingredient>().remove(ref.mRefNum); break;
        //            case ESM::REC_LEVC: get<ESM::CreatureLevList>().remove(ref.mRefNum); break;
        //            case ESM::REC_LEVI: get<ESM::ItemLevList>().remove(ref.mRefNum); break;
        //            case ESM::REC_LIGH: get<ESM::Light>().remove(ref.mRefNum); break;
        //            case ESM::REC_LOCK: get<ESM::Lockpick>().remove(ref.mRefNum); break;
        //            case ESM::REC_MISC: get<ESM::Miscellaneous>().remove(ref.mRefNum); break;
        //            case ESM::REC_NPC_: get<ESM::NPC>().remove(ref.mRefNum); break;
        //            case ESM::REC_PROB: get<ESM::Probe>().remove(ref.mRefNum); break;
        //            case ESM::REC_REPA: get<ESM::Repair>().remove(ref.mRefNum); break;
        //            case ESM::REC_STAT: get<ESM::Static>().remove(ref.mRefNum); break;
        //            case ESM::REC_WEAP: get<ESM::Weapon>().remove(ref.mRefNum); break;
        //            case ESM::REC_BODY: get<ESM::BodyPart>().remove(ref.mRefNum); break;
        //            default:
        //                break;
        //        }
        //    }
        //}

        switch (store.find(id))
        {
            case ESM::REC_ACTI4: get<ESM4::Activator>().load(ref, deleted, store); break;
            case ESM::REC_STAT4: get<ESM4::Static>().load(ref, deleted, store); break;
            case ESM::REC_LIGH4: get<ESM4::Light>().load(ref, deleted, store); break;
            case ESM::REC_SOUN4: get<ESM4::Sound>().load(ref, deleted, store); break;

            case 0: Log(Debug::Error) << "Cell reference '" + id + "' not found!"; return;

            default:
                Log(Debug::Error) << "Error: Ignoring reference '" << id << "' of unhandled type";
                return;
        }
    }

    bool CellStore::isTes4()
    {
        return mIsTes4;
    }

    bool CellStore::isTes4() const
    {
        return mIsTes4;
    }

    void CellStore::loadState (const ESM::CellState& state)
    {
        mHasState = true;

        if (mCell->mData.mFlags & ESM::Cell::Interior && mCell->mData.mFlags & ESM::Cell::HasWater)
            mWaterLevel = state.mWaterLevel;

        mLastRespawn = MWWorld::TimeStamp(state.mLastRespawn);
    }

    void CellStore::saveState (ESM::CellState& state) const
    {
        state.mId = mCell->getCellId();

        if (mCell->mData.mFlags & ESM::Cell::Interior && mCell->mData.mFlags & ESM::Cell::HasWater)
            state.mWaterLevel = mWaterLevel;

        state.mHasFogOfWar = (mFogState.get() ? 1 : 0);
        state.mLastRespawn = mLastRespawn.toEsm();
    }

    void CellStore::writeFog(ESM::ESMWriter &writer) const
    {
        if (mFogState.get())
        {
            mFogState->save(writer, mCell->mData.mFlags & ESM::Cell::Interior);
        }
    }

    void CellStore::readFog(ESM::ESMReader &reader)
    {
        mFogState = std::make_unique<ESM::FogState>();
        mFogState->load(reader);
    }

    void CellStore::writeReferences (ESM::ESMWriter& writer) const
    {
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Activator>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Potion>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Apparatus>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Armor>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Book>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Clothing>());
        writeReferenceCollection<ESM::ContainerState> (writer, get<ESM::Container>());
        writeReferenceCollection<ESM::CreatureState> (writer, get<ESM::Creature>());
        writeReferenceCollection<ESM::DoorState> (writer, get<ESM::Door>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Ingredient>());
        writeReferenceCollection<ESM::CreatureLevListState> (writer, get<ESM::CreatureLevList>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::ItemLevList>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Light>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Lockpick>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Miscellaneous>());
        writeReferenceCollection<ESM::NpcState> (writer, get<ESM::NPC>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Probe>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Repair>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Static>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::Weapon>());
        writeReferenceCollection<ESM::ObjectState> (writer, get<ESM::BodyPart>());

        for (const auto& [base, store] : mMovedToAnotherCell)
        {
            ESM::RefNum refNum = base->mRef.getRefNum();
            ESM::CellId movedTo = store->getCell()->getCellId();

            refNum.save(writer, true, "MVRF");
            movedTo.save(writer);
        }
    }

    void CellStore::readReferences (ESM::ESMReader& reader, const std::map<int, int>& contentFileMap, GetCellStoreCallback* callback)
    {
        mHasState = true;

        while (reader.isNextSub ("OBJE"))
        {
            unsigned int unused;
            reader.getHT (unused);

            // load the RefID first so we know what type of object it is
            ESM::CellRef cref;
            cref.loadId(reader, true);

            int type = MWBase::Environment::get().getWorld()->getStore().find(cref.mRefID);
            if (type == 0)
            {
                Log(Debug::Warning) << "Dropping reference to '" << cref.mRefID << "' (object no longer exists)";
                // Skip until the next OBJE or MVRF
                while(reader.hasMoreSubs() && !reader.peekNextSub("OBJE") && !reader.peekNextSub("MVRF"))
                {
                    reader.getSubName();
                    reader.skipHSub();
                }
                continue;
            }

            switch (type)
            {
                case ESM::REC_ACTI:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Activator>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_ALCH:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Potion>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_APPA:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Apparatus>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_ARMO:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Armor>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_BOOK:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Book>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_CLOT:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Clothing>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_CONT:

                    readReferenceCollection<ESM::ContainerState> (reader, get<ESM::Container>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_CREA:

                    readReferenceCollection<ESM::CreatureState> (reader, get<ESM::Creature>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_DOOR:

                    readReferenceCollection<ESM::DoorState> (reader, get<ESM::Door>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_INGR:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Ingredient>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_LEVC:

                    readReferenceCollection<ESM::CreatureLevListState> (reader, get<ESM::CreatureLevList>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_LEVI:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::ItemLevList>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_LIGH:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Light>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_LOCK:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Lockpick>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_MISC:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Miscellaneous>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_NPC_:

                    readReferenceCollection<ESM::NpcState> (reader, get<ESM::NPC>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_PROB:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Probe>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_REPA:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Repair>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_STAT:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Static>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_WEAP:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::Weapon>(), cref, contentFileMap, this);
                    break;

                case ESM::REC_BODY:

                    readReferenceCollection<ESM::ObjectState> (reader, get<ESM::BodyPart>(), cref, contentFileMap, this);
                    break;

                default:

                    throw std::runtime_error ("unknown type in cell reference section");
            }
        }

        // Do another update here to make sure objects referred to by MVRF tags can be found
        // This update is only needed for old saves that used the old copy&delete way of moving objects
        updateMergedRefs();

        while (reader.isNextSub("MVRF"))
        {
            reader.cacheSubName();
            ESM::RefNum refnum;
            ESM::CellId movedTo;
            refnum.load(reader, true, "MVRF");
            movedTo.load(reader);

            if (refnum.hasContentFile())
            {
                auto iter = contentFileMap.find(refnum.mContentFile);
                if (iter != contentFileMap.end())
                    refnum.mContentFile = iter->second;
            }

            // Search for the reference. It might no longer exist if its content file was removed.
            Ptr movedRef = searchViaRefNum(refnum);
            if (movedRef.isEmpty())
            {
                Log(Debug::Warning) << "Warning: Dropping moved ref tag for " << refnum.mIndex << " (moved object no longer exists)";
                continue;
            }

            CellStore* otherCell = callback->getCellStore(movedTo);

            if (otherCell == nullptr)
            {
                Log(Debug::Warning) << "Warning: Dropping moved ref tag for " << movedRef.getCellRef().getRefId()
                                    << " (target cell " << movedTo.mWorldspace << " no longer exists). Reference moved back to its original location.";
                // Note by dropping tag the object will automatically re-appear in its original cell, though potentially at inapproriate coordinates.
                // Restore original coordinates:
                movedRef.getRefData().setPosition(movedRef.getCellRef().getPosition());
                continue;
            }

            if (otherCell == this)
            {
                // Should never happen unless someone's tampering with files.
                Log(Debug::Warning) << "Found invalid moved ref, ignoring";
                continue;
            }

            moveTo(movedRef, otherCell);
        }
    }

    bool operator== (const CellStore& left, const CellStore& right)
    {
        if (left.isTes4() != right.isTes4())
            return false;
        if (left.isTes4())
        {
            return (left.getCell4()->mFormId == right.getCell4()->mFormId);
        }
        return  (left.getCell()->getCellId() == right.getCell()->getCellId());
    }

    bool operator!= (const CellStore& left, const CellStore& right)
    {
        return !(left==right);
    }

    void CellStore::setFog(std::unique_ptr<ESM::FogState>&& fog)
    {
        mFogState = std::move(fog);
    }

    ESM::FogState* CellStore::getFog() const
    {
        return mFogState.get();
    }

    void clearCorpse(const MWWorld::Ptr& ptr)
    {
        const MWMechanics::CreatureStats& creatureStats = ptr.getClass().getCreatureStats(ptr);
        static const float fCorpseClearDelay = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>().find("fCorpseClearDelay")->mValue.getFloat();
        if (creatureStats.isDead() &&
            creatureStats.isDeathAnimationFinished() &&
            !ptr.getClass().isPersistent(ptr) &&
            creatureStats.getTimeOfDeath() + fCorpseClearDelay <= MWBase::Environment::get().getWorld()->getTimeStamp())
        {
            MWBase::Environment::get().getWorld()->deleteObject(ptr);
        }
    }

    void CellStore::rest(double hours)
    {
        if (mState == State_Loaded)
        {
            for (CellRefList<ESM::Creature>::List::iterator it (get<ESM::Creature>().mList.begin()); it!=get<ESM::Creature>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                if (!ptr.isEmpty() && ptr.getRefData().getCount() > 0)
                {
                    MWBase::Environment::get().getMechanicsManager()->restoreDynamicStats(ptr, hours, true);
                }
            }
            for (CellRefList<ESM::NPC>::List::iterator it (get<ESM::NPC>().mList.begin()); it!=get<ESM::NPC>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                if (!ptr.isEmpty() && ptr.getRefData().getCount() > 0)
                {
                    MWBase::Environment::get().getMechanicsManager()->restoreDynamicStats(ptr, hours, true);
                }
            }
        }
    }

    void CellStore::recharge(float duration)
    {
        if (duration <= 0)
            return;

        if (mState == State_Loaded)
        {
            for (CellRefList<ESM::Creature>::List::iterator it (get<ESM::Creature>().mList.begin()); it!=get<ESM::Creature>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                if (!ptr.isEmpty() && ptr.getRefData().getCount() > 0)
                {
                    ptr.getClass().getContainerStore(ptr).rechargeItems(duration);
                }
            }
            for (CellRefList<ESM::NPC>::List::iterator it (get<ESM::NPC>().mList.begin()); it!=get<ESM::NPC>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                if (!ptr.isEmpty() && ptr.getRefData().getCount() > 0)
                {
                    ptr.getClass().getContainerStore(ptr).rechargeItems(duration);
                }
            }
            for (CellRefList<ESM::Container>::List::iterator it (get<ESM::Container>().mList.begin()); it!=get<ESM::Container>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                if (!ptr.isEmpty() && ptr.getRefData().getCustomData() != nullptr && ptr.getRefData().getCount() > 0
                && ptr.getClass().getContainerStore(ptr).isResolved())
                {
                    ptr.getClass().getContainerStore(ptr).rechargeItems(duration);
                }
            }

            rechargeItems(duration);
        }
    }

    void CellStore::respawn()
    {
        if (mState == State_Loaded)
        {
            static const int iMonthsToRespawn = MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>().find("iMonthsToRespawn")->mValue.getInteger();
            if (MWBase::Environment::get().getWorld()->getTimeStamp() - mLastRespawn > 24*30*iMonthsToRespawn)
            {
                mLastRespawn = MWBase::Environment::get().getWorld()->getTimeStamp();
                for (CellRefList<ESM::Container>::List::iterator it (get<ESM::Container>().mList.begin()); it!=get<ESM::Container>().mList.end(); ++it)
                {
                    Ptr ptr = getCurrentPtr(&*it);
                    ptr.getClass().respawn(ptr);
                }
            }

            for (CellRefList<ESM::Creature>::List::iterator it (get<ESM::Creature>().mList.begin()); it!=get<ESM::Creature>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                clearCorpse(ptr);
                ptr.getClass().respawn(ptr);
            }
            for (CellRefList<ESM::NPC>::List::iterator it (get<ESM::NPC>().mList.begin()); it!=get<ESM::NPC>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                clearCorpse(ptr);
                ptr.getClass().respawn(ptr);
            }
            for (CellRefList<ESM::CreatureLevList>::List::iterator it (get<ESM::CreatureLevList>().mList.begin()); it!=get<ESM::CreatureLevList>().mList.end(); ++it)
            {
                Ptr ptr = getCurrentPtr(&*it);
                // no need to clearCorpse, handled as part of get<ESM::Creature>()
                ptr.getClass().respawn(ptr);
            }
        }
    }

    void MWWorld::CellStore::rechargeItems(float duration)
    {
        if (!mRechargingItemsUpToDate)
        {
            updateRechargingItems();
            mRechargingItemsUpToDate = true;
        }
        for (const auto& [item, charge] : mRechargingItems)
        {
            MWMechanics::rechargeItem(item, charge, duration);
        }
    }

    void MWWorld::CellStore::updateRechargingItems()
    {
        mRechargingItems.clear();

        const auto update = [this](auto& list)
        {
            for (auto & item : list)
            {
                Ptr ptr = getCurrentPtr(&item);
                if (!ptr.isEmpty() && ptr.getRefData().getCount() > 0)
                {
                    checkItem(ptr);
                }
            }
        };

        update(get<ESM::Weapon>().mList);
        update(get<ESM::Armor>().mList);
        update(get<ESM::Clothing>().mList);
        update(get<ESM::Book>().mList);
    }

    void MWWorld::CellStore::checkItem(const Ptr& ptr)
    {
        std::string_view enchantmentId = ptr.getClass().getEnchantment(ptr);
        if (enchantmentId.empty())
            return;

        const ESM::Enchantment* enchantment = MWBase::Environment::get().getWorld()->getStore().get<ESM::Enchantment>().search(enchantmentId);
        if (!enchantment)
        {
            Log(Debug::Warning) << "Warning: Can't find enchantment '" << enchantmentId << "' on item " << ptr.getCellRef().getRefId();
            return;
        }

        if (enchantment->mData.mType == ESM::Enchantment::WhenUsed
                || enchantment->mData.mType == ESM::Enchantment::WhenStrikes)
            mRechargingItems.emplace_back(ptr.getBase(), static_cast<float>(enchantment->mData.mCharge));
    }

    Ptr MWWorld::CellStore::getMovedActor(int actorId) const
    {
        for(const auto& [cellRef, cell] : mMovedToAnotherCell)
        {
            if(cellRef->mClass->isActor() && cellRef->mData.getCustomData())
            {
                Ptr actor(cellRef, cell);
                if(actor.getClass().getCreatureStats(actor).getActorId() == actorId)
                    return actor;
            }
        }
        return {};
    }
}
