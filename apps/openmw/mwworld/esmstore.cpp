#include "esmstore.hpp"

#include <algorithm>
#include <fstream>

#include <components/debug/debuglog.hpp>
#include <components/esm3/esmreader.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/loadinglistener/loadinglistener.hpp>
#include <components/lua/configuration.hpp>
#include <components/misc/algorithm.hpp>
#include <components/esm3/readerscache.hpp>
#include <components/esmloader/load.hpp>

#include "../mwmechanics/spelllist.hpp"

namespace
{
    struct TES4Ref
    {
        ESM4::Reference mRecord;
    };
    struct Ref
    {
        ESM::RefNum mRefNum;
        std::size_t mRefID;

        Ref(ESM::RefNum refNum, std::size_t refID) : mRefNum(refNum), mRefID(refID) {}
    };

    constexpr std::size_t deletedRefID = std::numeric_limits<std::size_t>::max();

    void readRefs(const ESM::Cell& cell, std::vector<Ref>& refs, std::vector<std::string>& refIDs, ESM::ReadersCache& readers)
    {
        // TODO: we have many similar copies of this code.
        for (size_t i = 0; i < cell.mContextList.size(); i++)
        {
            const std::size_t index = static_cast<std::size_t>(cell.mContextList[i].index);
            const ESM::ReadersCache::BusyItem reader = readers.get(index);
            cell.restore(*reader, i);
            ESM::CellRef ref;
            ref.mRefNum.unset();
            bool deleted = false;
            while (cell.getNextRef(*reader, ref, deleted))
            {
                if(deleted)
                    refs.emplace_back(ref.mRefNum, deletedRefID);
                else if (std::find(cell.mMovedRefs.begin(), cell.mMovedRefs.end(), ref.mRefNum) == cell.mMovedRefs.end())
                {
                    refs.emplace_back(ref.mRefNum, refIDs.size());
                    refIDs.push_back(std::move(ref.mRefID));
                }
            }
        }
        for(const auto& [value, deleted] : cell.mLeasedRefs)
        {
            if(deleted)
                refs.emplace_back(value.mRefNum, deletedRefID);
            else
            {
                refs.emplace_back(value.mRefNum, refIDs.size());
                refIDs.push_back(value.mRefID);
            }
        }
    }

    void readRefs(const ESM4::Cell& cell, std::vector<TES4Ref>& refs, std::vector<std::string>& refIDs,
        MWWorld::ESMStore* store)
    {
        auto it = store->get<ESM4::Reference>().begin();
        while (it != store->get<ESM4::Reference>().end())
        {
            if (it->mParent == cell.mFormId)
            {
                if ((it->mFlags & ESM4::Rec_Deleted) == 0)
                {
                    refs.push_back({ *it });
                    refIDs.push_back(store->getFormName(it->mBaseObj));
                }
            }
            ++it;
        }
        // TODO: we have many similar copies of this code.
        /*for (size_t i = 0; i < cell.mContextList.size(); i++)
        {
            const std::size_t index = static_cast<std::size_t>(cell.mContextList[i].index);
            const ESM::ReadersCache::BusyItem reader = readers.get(index);
            cell.restore(*reader, i);
            ESM::CellRef ref;
            ref.mRefNum.unset();
            bool deleted = false;
            while (cell.getNextRef(*reader, ref, deleted))
            {
                if(deleted)
                    refs.emplace_back(ref.mRefNum, deletedRefID);
                else if (std::find(cell.mMovedRefs.begin(), cell.mMovedRefs.end(), ref.mRefNum) == cell.mMovedRefs.end())
                {
                    refs.emplace_back(ref.mRefNum, refIDs.size());
                    refIDs.push_back(std::move(ref.mRefID));
                }
            }
        }
        for(const auto& [value, deleted] : cell.mLeasedRefs)
        {
            if(deleted)
                refs.emplace_back(value.mRefNum, deletedRefID);
            else
            {
                refs.emplace_back(value.mRefNum, refIDs.size());
                refIDs.push_back(value.mRefID);
            }
        }*/
    }

    const std::string& getDefaultClass(const MWWorld::Store<ESM::Class>& classes)
    {
        auto it = classes.begin();
        if (it != classes.end())
            return it->mId;
        throw std::runtime_error("List of NPC classes is empty!");
    }

    std::vector<ESM::NPC> getNPCsToReplace(const MWWorld::Store<ESM::Faction>& factions, const MWWorld::Store<ESM::Class>& classes, const std::unordered_map<std::string, ESM::NPC, Misc::StringUtils::CiHash, Misc::StringUtils::CiEqual>& npcs)
    {
        // Cache first class from store - we will use it if current class is not found
        const std::string& defaultCls = getDefaultClass(classes);

        // Validate NPCs for non-existing class and faction.
        // We will replace invalid entries by fixed ones
        std::vector<ESM::NPC> npcsToReplace;

        for (const auto& npcIter : npcs)
        {
            ESM::NPC npc = npcIter.second;
            bool changed = false;

            const std::string& npcFaction = npc.mFaction;
            if (!npcFaction.empty())
            {
                const ESM::Faction *fact = factions.search(npcFaction);
                if (!fact)
                {
                    Log(Debug::Verbose) << "NPC '" << npc.mId << "' (" << npc.mName << ") has nonexistent faction '" << npc.mFaction << "', ignoring it.";
                    npc.mFaction.clear();
                    npc.mNpdt.mRank = 0;
                    changed = true;
                }
            }

            const std::string& npcClass = npc.mClass;
            const ESM::Class *cls = classes.search(npcClass);
            if (!cls)
            {
                Log(Debug::Verbose) << "NPC '" << npc.mId << "' (" << npc.mName << ") has nonexistent class '" << npc.mClass << "', using '" << defaultCls << "' class as replacement.";
                npc.mClass = defaultCls;
                changed = true;
            }

            if (changed)
                npcsToReplace.push_back(npc);
        }

        return npcsToReplace;
    }

    // Custom enchanted items can reference scripts that no longer exist, this doesn't necessarily mean the base item no longer exists however.
    // So instead of removing the item altogether, we're only removing the script.
    template<class MapT>
    void removeMissingScripts(const MWWorld::Store<ESM::Script>& scripts, MapT& items)
    {
        for(auto& [id, item] : items)
        {
            if(!item.mScript.empty() && !scripts.search(item.mScript))
            {
                item.mScript.clear();
                Log(Debug::Verbose) << "Item '" << id << "' (" << item.mName << ") has nonexistent script '" << item.mScript << "', ignoring it.";
            }
        }
    }
}


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

int ESMStore::ESMTypeMap::sLastTypeId = 0;

// i hate myself and want to die
using Types = AllTypeGetters<
    TypeGetter<ESM::Activator>,
    TypeGetter<ESM::Potion>,
    TypeGetter<ESM::Apparatus>,
    TypeGetter<ESM::Armor>,
    TypeGetter<ESM::BodyPart>,
    TypeGetter<ESM::Book>,
    TypeGetter<ESM::BirthSign>,
    TypeGetter<ESM::Class>,
    TypeGetter<ESM::Clothing>,
    TypeGetter<ESM::Container>,
    TypeGetter<ESM::Creature>,
    TypeGetter<ESM::Dialogue>,
    TypeGetter<ESM::Door>,
    TypeGetter<ESM::Enchantment>,
    TypeGetter<ESM::Faction>,
    TypeGetter<ESM::Global>,
    TypeGetter<ESM::Ingredient>,
    TypeGetter<ESM::CreatureLevList>,
    TypeGetter<ESM::ItemLevList>,
    TypeGetter<ESM::Light>,
    TypeGetter<ESM::Lockpick>,
    TypeGetter<ESM::Miscellaneous>,
    TypeGetter<ESM::NPC>,
    TypeGetter<ESM::Probe>,
    TypeGetter<ESM::Race>,
    TypeGetter<ESM::Region>,
    TypeGetter<ESM::Repair>,
    TypeGetter<ESM::SoundGenerator>,
    TypeGetter<ESM::Sound>,
    TypeGetter<ESM::Spell>,
    TypeGetter<ESM::StartScript>,
    TypeGetter<ESM::Static>,
    TypeGetter<ESM::Weapon>,

    TypeGetter<ESM::GameSetting>,
    TypeGetter<ESM::Script>,
    
    TypeGetter<ESM::Cell>,
    TypeGetter<ESM::Land>,
    TypeGetter<ESM::LandTexture>,
    TypeGetter<ESM::Pathgrid>,

    // TES4
    TypeGetter<ESM4::Activator>,
    TypeGetter<ESM4::Cell>,
    TypeGetter<ESM4::Reference>,
    TypeGetter<ESM4::Static>
>;

template <>
std::tuple<
    TypeGetter<ESM::Activator>,
    TypeGetter<ESM::Potion>,
    TypeGetter<ESM::Apparatus>,
    TypeGetter<ESM::Armor>,
    TypeGetter<ESM::BodyPart>,
    TypeGetter<ESM::Book>,
    TypeGetter<ESM::BirthSign>,
    TypeGetter<ESM::Class>,
    TypeGetter<ESM::Clothing>,
    TypeGetter<ESM::Container>,
    TypeGetter<ESM::Creature>,
    TypeGetter<ESM::Dialogue>,
    TypeGetter<ESM::Door>,
    TypeGetter<ESM::Enchantment>,
    TypeGetter<ESM::Faction>,
    TypeGetter<ESM::Global>,
    TypeGetter<ESM::Ingredient>,
    TypeGetter<ESM::CreatureLevList>,
    TypeGetter<ESM::ItemLevList>,
    TypeGetter<ESM::Light>,
    TypeGetter<ESM::Lockpick>,
    TypeGetter<ESM::Miscellaneous>,
    TypeGetter<ESM::NPC>,
    TypeGetter<ESM::Probe>,
    TypeGetter<ESM::Race>,
    TypeGetter<ESM::Region>,
    TypeGetter<ESM::Repair>,
    TypeGetter<ESM::SoundGenerator>,
    TypeGetter<ESM::Sound>,
    TypeGetter<ESM::Spell>,
    TypeGetter<ESM::StartScript>,
    TypeGetter<ESM::Static>,
    TypeGetter<ESM::Weapon>,

    TypeGetter<ESM::GameSetting>,
    TypeGetter<ESM::Script>,
    
    TypeGetter<ESM::Cell>,
    TypeGetter<ESM::Land>,
    TypeGetter<ESM::LandTexture>,
    TypeGetter<ESM::Pathgrid>,

    // TES4
    TypeGetter<ESM4::Activator>,
    TypeGetter<ESM4::Cell>,
    TypeGetter<ESM4::Reference>,
    TypeGetter<ESM4::Static>
    >
    Types::sAll = std::make_tuple(
        TypeGetter<ESM::Activator>(),
        TypeGetter<ESM::Potion>(),
        TypeGetter<ESM::Apparatus>(),
        TypeGetter<ESM::Armor>(),
        TypeGetter<ESM::BodyPart>(),
        TypeGetter<ESM::Book>(),
        TypeGetter<ESM::BirthSign>(),
        TypeGetter<ESM::Class>(),
        TypeGetter<ESM::Clothing>(),
        TypeGetter<ESM::Container>(),
        TypeGetter<ESM::Creature>(),
        TypeGetter<ESM::Dialogue>(),
        TypeGetter<ESM::Door>(),
        TypeGetter<ESM::Enchantment>(),
        TypeGetter<ESM::Faction>(),
        TypeGetter<ESM::Global>(),
        TypeGetter<ESM::Ingredient>(),
        TypeGetter<ESM::CreatureLevList>(),
        TypeGetter<ESM::ItemLevList>(),
        TypeGetter<ESM::Light>(),
        TypeGetter<ESM::Lockpick>(),
        TypeGetter<ESM::Miscellaneous>(),
        TypeGetter<ESM::NPC>(),
        TypeGetter<ESM::Probe>(),
        TypeGetter<ESM::Race>(),
        TypeGetter<ESM::Region>(),
        TypeGetter<ESM::Repair>(),
        TypeGetter<ESM::SoundGenerator>(),
        TypeGetter<ESM::Sound>(),
        TypeGetter<ESM::Spell>(),
        TypeGetter<ESM::StartScript>(),
        TypeGetter<ESM::Static>(),
        TypeGetter<ESM::Weapon>(),

        TypeGetter<ESM::GameSetting>(),
        TypeGetter<ESM::Script>(),

        TypeGetter<ESM::Cell>(),
        TypeGetter<ESM::Land>(),
        TypeGetter<ESM::LandTexture>(),
        TypeGetter<ESM::Pathgrid>(),

        // TES4
        TypeGetter<ESM4::Activator>(),
        TypeGetter<ESM4::Cell>(),
        TypeGetter<ESM4::Reference>(),
        TypeGetter<ESM4::Static>());

static bool isCacheableRecord(int id)
{
    if (id == ESM::REC_ACTI || id == ESM::REC_ALCH || id == ESM::REC_APPA || id == ESM::REC_ARMO ||
        id == ESM::REC_BOOK || id == ESM::REC_CLOT || id == ESM::REC_CONT || id == ESM::REC_CREA ||
        id == ESM::REC_DOOR || id == ESM::REC_INGR || id == ESM::REC_LEVC || id == ESM::REC_LEVI ||
        id == ESM::REC_LIGH || id == ESM::REC_LOCK || id == ESM::REC_MISC || id == ESM::REC_NPC_ ||
        id == ESM::REC_PROB || id == ESM::REC_REPA || id == ESM::REC_STAT || id == ESM::REC_WEAP ||
        id == ESM::REC_BODY || id == ESM::REC_ACTI4 || id == ESM::REC_REFR4 || id == ESM::REC_STAT4)
    {
        return true;
    }
    return false;
}

struct ESMStoreImpl
{
    // Stores that don't inherit StoreBase and thus can't be in the typemap
    Store<ESM::MagicEffect> mMagicEffects;
    Store<ESM::Skill> mSkills;

    // Special entry which is hardcoded and not loaded from an ESM
    Store<ESM::Attribute> mAttributes;

    std::map<ESM::RecNameInts, StoreBase*> mRecordToStore;
    std::map<ESM4::FormId, std::string> mTES4RecordIds;

    // Lookup of all IDs. Makes looking up references faster. Just
    // maps the id name to the record type.
    using IDMap = std::map<std::string, int>;
    IDMap mIds;
    IDMap mStaticIds;


    /// Look up the given ID in 'all'. Returns 0 if not found.
    int find(const std::string& id) const
    {
        IDMap::const_iterator it = mIds.find(id);
        if (it == mIds.end())
        {
            return 0;
        }
        return it->second;
    }

    int findStatic(const std::string& id) const
    {
        IDMap::const_iterator it = mStaticIds.find(id);
        if (it == mStaticIds.end())
        {
            return 0;
        }
        return it->second;
    }


    template <typename T>
    static const T* insert(ESMStore& stores, const T& toInsert)
    {
        const std::string id = "$dynamic" + std::to_string(stores.mDynamicCount++);

        Store<T>& store = stores.get<T>();
        if (store.search(id) != nullptr)
        {
            const std::string msg = "Try to override existing record '" + id + "'";
            throw std::runtime_error(msg);
        }
        T record = toInsert;

        record.mId = id;

        T* ptr = store.insert(record);
        auto esm3RecordType_find = stores.mImpl->mRecordToStore.find(T::sRecordId);
        if (esm3RecordType_find != stores.mImpl->mRecordToStore.end())
        {
            stores.mImpl->mIds[ptr->mId] = esm3RecordType_find->first;
        }
        return ptr;
    }

    template <class T>
    static const T* overrideRecord(ESMStore& stores, const T& x)
    {
        Store<T>& store = stores.get<T>();

        T* ptr = store.insert(x);
        auto esm3RecordType_find = stores.mImpl->mRecordToStore.find(T::sRecordId);
        if (esm3RecordType_find != stores.mImpl->mRecordToStore.end())
        {
            stores.mImpl->mIds[ptr->mId] = esm3RecordType_find->first;
        }
        return ptr;
    }



    template <class T>
    static const T* insertStatic(ESMStore& stores, const T& x)
    {
        const std::string id = "$dynamic" + std::to_string(stores.mDynamicCount++);

        Store<T>& store = stores.get<T>();
        if (store.search(id) != nullptr)
        {
            const std::string msg = "Try to override existing record '" + id + "'";
            throw std::runtime_error(msg);
        }
        T record = x;

        T* ptr = store.insertStatic(record);
        auto esm3RecordType_find = stores.mImpl->mRecordToStore.find(T::sRecordId);
        if (esm3RecordType_find != stores.mImpl->mRecordToStore.end())
        {
            if constexpr (requires(T & obj) { obj.mId; })
                stores.mImpl->mIds[ptr->mId] = esm3RecordType_find->first;
            else if constexpr (requires (T& obj) { obj.mEditorId; })
                stores.mImpl->mIds[ptr->mEditorId] = esm3RecordType_find->first;
        }
        return ptr;
    }
};

void ESMStore::load(ESM::ESMReader &esm, Loading::Listener* listener, ESM::Dialogue*& dialogue)
{
    if (listener != nullptr)
        listener->setProgressRange(::EsmLoader::fileProgress);

    // Land texture loading needs to use a separate internal store for each plugin.
    // We set the number of plugins here so we can properly verify if valid plugin
    // indices are being passed to the LandTexture Store retrieval methods.
    mStores.get<ESM::LandTexture>().resize(esm.getIndex()+1);

    // Loop through all records
    while(esm.hasMoreRecs())
    {
        ESM::NAME n = esm.getRecName();
        esm.getRecHeader();
        if (esm.getRecordFlags() & ESM::FLAG_Ignored)
        {
            esm.skipRecord();
            continue;
        }

        // Look up the record type.
        const auto& it = mImpl->mRecordToStore.find(static_cast<ESM::RecNameInts>(n.toInt()));

        if (it == mImpl->mRecordToStore.end()) {
            if (n.toInt() == ESM::REC_INFO) {
                if (dialogue)
                {
                    dialogue->readInfo(esm, esm.getIndex() != 0);
                }
                else
                {
                    Log(Debug::Error) << "Error: info record without dialog";
                    esm.skipRecord();
                }
            } else if (n.toInt() == ESM::REC_MGEF) {
                get<ESM::MagicEffect>().load (esm);
            } else if (n.toInt() == ESM::REC_SKIL) {
                get<ESM::Skill>().load (esm);
            }
            else if (n.toInt() == ESM::REC_FILT || n.toInt() == ESM::REC_DBGP)
            {
                // ignore project file only records
                esm.skipRecord();
            }
            else if (n.toInt() == ESM::REC_LUAL)
            {
                ESM::LuaScriptsCfg cfg;
                cfg.load(esm);
                cfg.adjustRefNums(esm);
                mLuaContent.push_back(std::move(cfg));
            }
            else {
                throw std::runtime_error("Unknown record: " + n.toString());
            }
        } else {
            RecordId id = it->second->load(esm);
            if (id.mIsDeleted)
            {
                it->second->eraseStatic(id.mId);
                continue;
            }

            if (n.toInt() == ESM::REC_DIAL) {
                dialogue = const_cast<ESM::Dialogue*>(get<ESM::Dialogue>().find(id.mId));
            } else {
                dialogue = nullptr;
            }
        }
        if (listener != nullptr)
            listener->setProgress(::EsmLoader::fileProgress * esm.getFileOffset() / esm.getFileSize());
    }
}

// ESM4 reading
struct ESM4Reading
{
    template <class T>
    static void readUnimplementedTypedRecord(ESMStore& store, ESM4::Reader& reader, ESM4::Dialogue* dialogue)
    {
        reader.skipRecordData();

        Log(Debug::Warning) << "Unimplemented Record: " << ESM::NAME(reader.hdr().record.typeId).toStringView();
    }
    template <class T>
    static void readImplementedTypedRecord(ESMStore& store, ESM4::Reader& reader, ESM::NAME n, ESM4::Dialogue* dialogue)
    {
        reader.getRecordData();
        // Look up the record type.
        const auto& it = store.mImpl->mRecordToStore.find(static_cast<ESM::RecNameInts>(n.toInt()));
        ESM::NAME unmasked = ESM::NAME(n.toInt() & ESM::esm4RecnameFlag);

        if (it == store.mImpl->mRecordToStore.end())
        {
            if (n.toInt() == ESM::REC_INFO4)
            {
                if (dialogue)
                {
                    // TODO: put readInfo in ESM4::Dialogue
                    //dialogue->readInfo(esm, esm.getIndex() != 0);
                }
                else
                {
                    Log(Debug::Error) << "Error: info record without dialog";
                }
            }
            else if (n.toInt() == ESM::REC_MGEF4)
            {
                // TODO
                //get<ESM4::MagicEffect>().load(esm);
            }
            else
            {
                Log(Debug::Error) << ("Unknown record: " + unmasked.toString());
            }
        }
        else
        {
            RecordId id = it->second->load(reader);
            if (id.mIsDeleted)
            {
                it->second->eraseStatic(id.mId);
                return;
            }
            Misc::StringUtils::lowerCaseInPlace(id.mId);
            const T* rec = store.get<T>().search(id.mId);
            if (rec)
                store.mImpl->mTES4RecordIds[rec->mFormId] = id.mId;

            if (n.toInt() == ESM::REC_DIAL4)
            {
                // TODO
                //dialogue = const_cast<ESM4::Dialogue*>(get<ESM4::Dialogue>().find(id.mId));
            }
            else
            {
                dialogue = nullptr;
            }
        }
    }
    static bool readGroup(ESMStore& store, ESM4::Reader& reader, ESM4::Dialogue* dialogue)
    {
        const ESM4::RecordHeader& header = reader.hdr();

        switch (static_cast<ESM4::GroupType>(header.group.type))
        {
            case ESM4::Grp_RecordType:
            case ESM4::Grp_InteriorCell:
            case ESM4::Grp_InteriorSubCell:
            case ESM4::Grp_ExteriorCell:
            case ESM4::Grp_ExteriorSubCell:
                reader.enterGroup();
                return readItem(store, reader, dialogue);
            case ESM4::Grp_WorldChild:
            case ESM4::Grp_CellChild:
            case ESM4::Grp_TopicChild:
            case ESM4::Grp_CellPersistentChild:
            case ESM4::Grp_CellTemporaryChild:
            case ESM4::Grp_CellVisibleDistChild:
                reader.adjustGRUPFormId();
                reader.enterGroup();
                if (!reader.hasMoreRecs())
                    return false;
                return readItem(store, reader, dialogue);
        }

        reader.skipGroup();

        return true;
    }
    static void readRecord(ESMStore& store, ESM4::Reader& reader, ESM4::Dialogue* dialogue)
    {
        ESM4::RecordTypes typeId = static_cast<ESM4::RecordTypes>(reader.hdr().record.typeId);
        switch (typeId)
        {
            case ESM4::REC_AACT: break;
            case ESM4::REC_ACHR: return readUnimplementedTypedRecord<ESM4::ActorCharacter>(store, reader, dialogue);
            case ESM4::REC_ACRE: return readUnimplementedTypedRecord<ESM4::ActorCreature>(store, reader, dialogue);
            case ESM4::REC_ACTI: return readImplementedTypedRecord<ESM4::Activator>(store, reader, ESM::NAME(typeId | ESM::esm4RecnameFlag), dialogue);
            case ESM4::REC_ADDN: break;
            case ESM4::REC_ALCH: return readUnimplementedTypedRecord<ESM4::Potion>(store, reader, dialogue);
            case ESM4::REC_ALOC: return readUnimplementedTypedRecord<ESM4::MediaLocationController>(store, reader, dialogue);
            case ESM4::REC_AMMO: return readUnimplementedTypedRecord<ESM4::Ammunition>(store, reader, dialogue);
            case ESM4::REC_ANIO: return readUnimplementedTypedRecord<ESM4::AnimObject>(store, reader, dialogue);
            case ESM4::REC_APPA: return readUnimplementedTypedRecord<ESM4::Apparatus>(store, reader, dialogue);
            case ESM4::REC_ARMA: return readUnimplementedTypedRecord<ESM4::ArmorAddon>(store, reader, dialogue);
            case ESM4::REC_ARMO: return readUnimplementedTypedRecord<ESM4::Armor>(store, reader, dialogue);
            case ESM4::REC_ARTO: break;
            case ESM4::REC_ASPC: return readUnimplementedTypedRecord<ESM4::AcousticSpace>(store, reader, dialogue);
            case ESM4::REC_ASTP: break;
            case ESM4::REC_AVIF: break;
            case ESM4::REC_BOOK: return readUnimplementedTypedRecord<ESM4::Book>(store, reader, dialogue);
            case ESM4::REC_BPTD: return readUnimplementedTypedRecord<ESM4::BodyPartData>(store, reader, dialogue);
            case ESM4::REC_CAMS: break;
            case ESM4::REC_CCRD: break;
            case ESM4::REC_CELL: return readImplementedTypedRecord<ESM4::Cell>(store, reader, ESM::NAME(typeId | ESM::esm4RecnameFlag), dialogue);
            case ESM4::REC_CLAS: return readUnimplementedTypedRecord<ESM4::Class>(store, reader, dialogue);
            case ESM4::REC_CLFM: return readUnimplementedTypedRecord<ESM4::Colour>(store, reader, dialogue);
            case ESM4::REC_CLMT: break;
            case ESM4::REC_CLOT: return readUnimplementedTypedRecord<ESM4::Clothing>(store, reader, dialogue);
            case ESM4::REC_CMNY: break;
            case ESM4::REC_COBJ: break;
            case ESM4::REC_COLL: break;
            case ESM4::REC_CONT: return readUnimplementedTypedRecord<ESM4::Container>(store, reader, dialogue);
            case ESM4::REC_CPTH: break;
            case ESM4::REC_CREA: return readUnimplementedTypedRecord<ESM4::Creature>(store, reader, dialogue);
            case ESM4::REC_CSTY: break;
            case ESM4::REC_DEBR: break;
            case ESM4::REC_DIAL: return readUnimplementedTypedRecord<ESM4::Dialogue>(store, reader, dialogue);
            case ESM4::REC_DLBR: break;
            case ESM4::REC_DLVW: break;
            case ESM4::REC_DOBJ: return readUnimplementedTypedRecord<ESM4::DefaultObj>(store, reader, dialogue);
            case ESM4::REC_DOOR: return readUnimplementedTypedRecord<ESM4::Door>(store, reader, dialogue);
            case ESM4::REC_DUAL: break;
            case ESM4::REC_ECZN: break;
            case ESM4::REC_EFSH: break;
            case ESM4::REC_ENCH: break;
            case ESM4::REC_EQUP: break;
            case ESM4::REC_EXPL: break;
            case ESM4::REC_EYES: return readUnimplementedTypedRecord<ESM4::Eyes>(store, reader, dialogue);
            case ESM4::REC_FACT: break;
            case ESM4::REC_FLOR: return readUnimplementedTypedRecord<ESM4::Flora>(store, reader, dialogue);
            case ESM4::REC_FLST: return readUnimplementedTypedRecord<ESM4::FormIdList>(store, reader, dialogue);
            case ESM4::REC_FSTP: break;
            case ESM4::REC_FSTS: break;
            case ESM4::REC_FURN: return readUnimplementedTypedRecord<ESM4::Furniture>(store, reader, dialogue);
            case ESM4::REC_GLOB: return readUnimplementedTypedRecord<ESM4::GlobalVariable>(store, reader, dialogue);
            case ESM4::REC_GMST: break;
            case ESM4::REC_GRAS: return readUnimplementedTypedRecord<ESM4::Grass>(store, reader, dialogue);
            case ESM4::REC_GRUP: break;
            case ESM4::REC_HAIR: return readUnimplementedTypedRecord<ESM4::Hair>(store, reader, dialogue);
            case ESM4::REC_HAZD: break;
            case ESM4::REC_HDPT: return readUnimplementedTypedRecord<ESM4::HeadPart>(store, reader, dialogue);
            case ESM4::REC_IDLE:
                // FIXME: ESM4::IdleAnimation::load does not work with Oblivion.esm
                // return readUnimplementedTypedRecord<ESM4::IdleAnimation>(store, reader, dialogue);
                break;
            case ESM4::REC_IDLM: return readUnimplementedTypedRecord<ESM4::IdleMarker>(store, reader, dialogue);
            case ESM4::REC_IMAD: break;
            case ESM4::REC_IMGS: break;
            case ESM4::REC_IMOD: return readUnimplementedTypedRecord<ESM4::ItemMod>(store, reader, dialogue);
            case ESM4::REC_INFO: return readUnimplementedTypedRecord<ESM4::DialogInfo>(store, reader, dialogue);
            case ESM4::REC_INGR: return readUnimplementedTypedRecord<ESM4::Ingredient>(store, reader, dialogue);
            case ESM4::REC_IPCT: break;
            case ESM4::REC_IPDS: break;
            case ESM4::REC_KEYM: return readUnimplementedTypedRecord<ESM4::Key>(store, reader, dialogue);
            case ESM4::REC_KYWD: break;
            case ESM4::REC_LAND: return readUnimplementedTypedRecord<ESM4::Land>(store, reader, dialogue);
            case ESM4::REC_LCRT: break;
            case ESM4::REC_LCTN: break;
            case ESM4::REC_LGTM: return readUnimplementedTypedRecord<ESM4::LightingTemplate>(store, reader, dialogue);
            case ESM4::REC_LIGH: return readUnimplementedTypedRecord<ESM4::Light>(store, reader, dialogue);
            case ESM4::REC_LSCR: break;
            case ESM4::REC_LTEX: return readUnimplementedTypedRecord<ESM4::LandTexture>(store, reader, dialogue);
            case ESM4::REC_LVLC: return readUnimplementedTypedRecord<ESM4::LevelledCreature>(store, reader, dialogue);
            case ESM4::REC_LVLI: return readUnimplementedTypedRecord<ESM4::LevelledItem>(store, reader, dialogue);
            case ESM4::REC_LVLN: return readUnimplementedTypedRecord<ESM4::LevelledNpc>(store, reader, dialogue);
            case ESM4::REC_LVSP: break;
            case ESM4::REC_MATO: return readUnimplementedTypedRecord<ESM4::Material>(store, reader, dialogue);
            case ESM4::REC_MATT: break;
            case ESM4::REC_MESG: break;
            case ESM4::REC_MGEF: break;
            case ESM4::REC_MISC: return readUnimplementedTypedRecord<ESM4::MiscItem>(store, reader, dialogue);
            case ESM4::REC_MOVT: break;
            case ESM4::REC_MSET: return readUnimplementedTypedRecord<ESM4::MediaSet>(store, reader, dialogue);
            case ESM4::REC_MSTT: return readUnimplementedTypedRecord<ESM4::MovableStatic>(store, reader, dialogue);
            case ESM4::REC_MUSC: return readUnimplementedTypedRecord<ESM4::Music>(store, reader, dialogue);
            case ESM4::REC_MUST: break;
            case ESM4::REC_NAVI: return readUnimplementedTypedRecord<ESM4::Navigation>(store, reader, dialogue);
            case ESM4::REC_NAVM: return readUnimplementedTypedRecord<ESM4::NavMesh>(store, reader, dialogue);
            case ESM4::REC_NOTE: return readUnimplementedTypedRecord<ESM4::Note>(store, reader, dialogue);
            case ESM4::REC_NPC_: return readUnimplementedTypedRecord<ESM4::Npc>(store, reader, dialogue);
            case ESM4::REC_OTFT: return readUnimplementedTypedRecord<ESM4::Outfit>(store, reader, dialogue);
            case ESM4::REC_PACK: return readUnimplementedTypedRecord<ESM4::AIPackage>(store, reader, dialogue);
            case ESM4::REC_PERK: break;
            case ESM4::REC_PGRD: return readUnimplementedTypedRecord<ESM4::Pathgrid>(store, reader, dialogue);
            case ESM4::REC_PGRE: return readUnimplementedTypedRecord<ESM4::PlacedGrenade>(store, reader, dialogue);
            case ESM4::REC_PHZD: break;
            case ESM4::REC_PROJ: break;
            case ESM4::REC_PWAT: return readUnimplementedTypedRecord<ESM4::PlaceableWater>(store, reader, dialogue);
            case ESM4::REC_QUST: return readUnimplementedTypedRecord<ESM4::Quest>(store, reader, dialogue);
            case ESM4::REC_RACE: return readUnimplementedTypedRecord<ESM4::Race>(store, reader, dialogue);
            case ESM4::REC_REFR: return readImplementedTypedRecord<ESM4::Reference>(store, reader, ESM::NAME(typeId | ESM::esm4RecnameFlag), dialogue);
            case ESM4::REC_REGN: return readUnimplementedTypedRecord<ESM4::Region>(store, reader, dialogue);
            case ESM4::REC_RELA: break;
            case ESM4::REC_REVB: break;
            case ESM4::REC_RFCT: break;
            case ESM4::REC_ROAD: return readUnimplementedTypedRecord<ESM4::Road>(store, reader, dialogue);
            case ESM4::REC_SBSP: return readUnimplementedTypedRecord<ESM4::SubSpace>(store, reader, dialogue);
            case ESM4::REC_SCEN: break;
            case ESM4::REC_SCOL: return readUnimplementedTypedRecord<ESM4::StaticCollection>(store, reader, dialogue);
            case ESM4::REC_SCPT: return readUnimplementedTypedRecord<ESM4::Script>(store, reader, dialogue);
            case ESM4::REC_SCRL: return readUnimplementedTypedRecord<ESM4::Scroll>(store, reader, dialogue);
            case ESM4::REC_SGST: return readUnimplementedTypedRecord<ESM4::SigilStone>(store, reader, dialogue);
            case ESM4::REC_SHOU: break;
            case ESM4::REC_SLGM: return readUnimplementedTypedRecord<ESM4::SoulGem>(store, reader, dialogue);
            case ESM4::REC_SMBN: break;
            case ESM4::REC_SMEN: break;
            case ESM4::REC_SMQN: break;
            case ESM4::REC_SNCT: break;
            case ESM4::REC_SNDR: return readUnimplementedTypedRecord<ESM4::SoundReference>(store, reader, dialogue);
            case ESM4::REC_SOPM: break;
            case ESM4::REC_SOUN: return readUnimplementedTypedRecord<ESM4::Sound>(store, reader, dialogue);
            case ESM4::REC_SPEL: break;
            case ESM4::REC_SPGD: break;
            case ESM4::REC_STAT: return readImplementedTypedRecord<ESM4::Static>(store, reader, ESM::NAME(typeId | ESM::esm4RecnameFlag), dialogue);
            case ESM4::REC_TACT: return readUnimplementedTypedRecord<ESM4::TalkingActivator>(store, reader, dialogue);
            case ESM4::REC_TERM: return readUnimplementedTypedRecord<ESM4::Terminal>(store, reader, dialogue);
            case ESM4::REC_TES4: return readUnimplementedTypedRecord<ESM4::Header>(store, reader, dialogue);
            case ESM4::REC_TREE: return readUnimplementedTypedRecord<ESM4::Tree>(store, reader, dialogue);
            case ESM4::REC_TXST: return readUnimplementedTypedRecord<ESM4::TextureSet>(store, reader, dialogue);
            case ESM4::REC_VTYP: break;
            case ESM4::REC_WATR: break;
            case ESM4::REC_WEAP: return readUnimplementedTypedRecord<ESM4::Weapon>(store, reader, dialogue);
            case ESM4::REC_WOOP: break;
            case ESM4::REC_WRLD: return readUnimplementedTypedRecord<ESM4::World>(store, reader, dialogue);
            case ESM4::REC_WTHR: break;
        }

        Log(Debug::Warning) << "Unsupported record: " << ESM::NAME(reader.hdr().record.typeId).toStringView();

        reader.skipRecordData();
    }
    static bool readItem(ESMStore& store, ESM4::Reader& reader, ESM4::Dialogue* dialogue)
    {
        if (!reader.getRecordHeader() || !reader.hasMoreRecs())
            return false;

        const ESM4::RecordHeader& header = reader.hdr();

        if (header.record.typeId == ESM4::REC_GRUP)
            return readGroup(store, reader, dialogue);

        readRecord(store, reader, dialogue);
        return true;
    }
};


void ESMStore::load(ESM4::Reader& esm, Loading::Listener* listener, ESM4::Dialogue*& dialogue)
{
    if (listener != nullptr)
        listener->setProgressRange(::EsmLoader::fileProgress);

    //// Land texture loading needs to use a separate internal store for each plugin.
    //// We set the number of plugins here so we can properly verify if valid plugin
    //// indices are being passed to the LandTexture Store retrieval methods.
    //mStores.get<ESM4::LandTexture>().resize(esm.getIndex() + 1);
    
    while (esm.hasMoreRecs())
    {
        esm.exitGroupCheck();
        if (!ESM4Reading::readItem(*this, esm, dialogue))
            break;

        
        if (listener != nullptr)
            listener->setProgress(::EsmLoader::fileProgress * esm.getFileOffset() / esm.getFileSize());
    }
    Log(Debug::Debug) << "Loaded ESM4 " << esm.getFileName();
}

ESM::LuaScriptsCfg ESMStore::getLuaScriptsCfg() const
{
    ESM::LuaScriptsCfg cfg;
    for (const LuaContent& c : mLuaContent)
    {
        if (std::holds_alternative<std::string>(c))
        {
            // *.omwscripts are intentionally reloaded every time when `getLuaScriptsCfg` is called.
            // It is important for the `reloadlua` console command.
            try
            {
                auto file = std::ifstream(std::get<std::string>(c));
                std::string fileContent(std::istreambuf_iterator<char>(file), {});
                LuaUtil::parseOMWScripts(cfg, fileContent);
            }
            catch (std::exception& e) { Log(Debug::Error) << e.what(); }
        }
        else
        {
            const ESM::LuaScriptsCfg& addition = std::get<ESM::LuaScriptsCfg>(c);
            cfg.mScripts.insert(cfg.mScripts.end(), addition.mScripts.begin(), addition.mScripts.end());
        }
    }
    return cfg;
}

int ESMStore::find(std::string_view id) const
{
    return mImpl->find(std::string(id));
}

int ESMStore::findStatic(std::string_view id) const
{
    return mImpl->findStatic(std::string(id));
}

ESMStore::ESMStore()
    : mDynamicCount(0)
{
    mImpl = std::make_unique<ESMStoreImpl>();
    iterateTuple(Types::sAll, [&](auto typeGetter)
    {
        using type = typename decltype(typeGetter)::type;
        mStores.put<type>(new Store<type>());
        if constexpr (type::sRecordId != ESM::REC_INTERNAL_PLAYER)
            mImpl->mRecordToStore[type::sRecordId] = &get<type>();
    });
    get<ESM::Pathgrid>().setCells(get<ESM::Cell>());
}

ESMStore::~ESMStore()
{
    mImpl->mIds.clear();
    mImpl->mStaticIds.clear();
    mImpl->mRecordToStore.clear();
    for (auto& store : mStores)
    {
        delete store.second;
    }
}

void ESMStore::setUp()
{
    mImpl->mIds.clear();

    ESMTypeMap::iterator storeIt = mStores.begin();
    for (; storeIt != mStores.end(); ++storeIt) {
        storeIt->second->setUp();

        if (isCacheableRecord(storeIt->second->getRecName()))
        {
            std::vector<std::string> identifiers;
            storeIt->second->listIdentifier(identifiers);

            for (std::vector<std::string>::const_iterator record = identifiers.begin(); record != identifiers.end(); ++record)
                mImpl->mIds[*record] = storeIt->second->getRecName();
        }
    }

    if (mImpl->mStaticIds.empty())
        for (const auto& [k, v] : mImpl->mIds)
            mImpl->mStaticIds.emplace(Misc::StringUtils::lowerCase(k), v);

    mImpl->mSkills.setUp();
    mImpl->mMagicEffects.setUp();
    mImpl->mAttributes.setUp();
    mStores.get<ESM::Dialogue>().setUp();
}

void ESMStore::validateRecords(ESM::ReadersCache& readers)
{
    validate();
    countAllCellRefs(readers);
}

void ESMStore::countAllCellRefs(ESM::ReadersCache& readers)
{
    // TODO: We currently need to read entire files here again.
    // We should consider consolidating or deferring this reading.
    if(!mRefCount.empty())
        return;
    std::vector<Ref> refs;
    std::vector<TES4Ref> refs4;
    std::vector<std::string> refIDs;
    for(auto it = get<ESM::Cell>().intBegin(); it != get<ESM::Cell>().intEnd(); ++it)
        readRefs(*it, refs, refIDs, readers);
    for(auto it = get<ESM::Cell>().extBegin(); it != get<ESM::Cell>().extEnd(); ++it)
        readRefs(*it, refs, refIDs, readers);
    /*for (auto it = get<ESM4::Cell>().intBegin(); it != get<ESM4::Cell>().intEnd(); ++it)
        readRefs(*it, refs4, refIDs, this);
    for (auto it = get<ESM4::Cell>().extBegin(); it != get<ESM4::Cell>().extEnd(); ++it)
        readRefs(*it, refs4, refIDs, this);*/
    const auto lessByRefNum = [](const Ref& l, const Ref& r)
    { return l.mRefNum < r.mRefNum; };
    const auto refrLess = [](const TES4Ref& l, const TES4Ref& r)
    { return false; }; // TODO
    std::stable_sort(refs.begin(), refs.end(), lessByRefNum);
    const auto equalByRefNum = [] (const Ref& l, const Ref& r) { return l.mRefNum == r.mRefNum; };
    const auto refrEqual = [](const TES4Ref& l, const TES4Ref& r)
    { return l.mRecord.mBaseObj == r.mRecord.mBaseObj; };
    const auto incrementRefCount = [&] (const Ref& value)
    {
        if (value.mRefID != deletedRefID)
        {
            std::string& refId = refIDs[value.mRefID];
            // We manually lower case IDs here for the time being to improve performance.
            Misc::StringUtils::lowerCaseInPlace(refId);
            ++mRefCount[std::move(refId)];
        }
    };
    const auto increment4RefCount = [&](const TES4Ref& value)
    {
        if (true)
        {
            std::string& refId = refIDs[value.mRecord.mBaseObj];
            // We manually lower case IDs here for the time being to improve performance.
            Misc::StringUtils::lowerCaseInPlace(refId);
            ++mRefCount[std::move(refId)];
        }
    };
    Misc::forEachUnique(refs.rbegin(), refs.rend(), equalByRefNum, incrementRefCount);
    //Misc::forEachUnique(refs4.rbegin(), refs4.rend(), refrEqual, increment4RefCount);
}

int ESMStore::getRefCount(std::string_view id) const
{
    const std::string lowerId = Misc::StringUtils::lowerCase(id);
    auto it = mRefCount.find(lowerId);
    if(it == mRefCount.end())
        return 0;
    return it->second;
}

void ESMStore::validate()
{
    std::vector<ESM::NPC> npcsToReplace = getNPCsToReplace(get<ESM::Faction>(), get<ESM::Class>(), get<ESM::NPC>().mStatic);

    for (const ESM::NPC &npc : npcsToReplace)
    {
        get<ESM::NPC>().eraseStatic(npc.mId);
        get<ESM::NPC>().insertStatic(npc);
    }

    // Validate spell effects for invalid arguments
    std::vector<ESM::Spell> spellsToReplace;
    for (ESM::Spell spell : get<ESM::Spell>())
    {
        if (spell.mEffects.mList.empty())
            continue;

        bool changed = false;
        auto iter = spell.mEffects.mList.begin();
        while (iter != spell.mEffects.mList.end())
        {
            const ESM::MagicEffect* mgef = get<ESM::MagicEffect>().search(iter->mEffectID);
            if (!mgef)
            {
                Log(Debug::Verbose) << "Spell '" << spell.mId << "' has an invalid effect (index " << iter->mEffectID << ") present. Dropping the effect.";
                iter = spell.mEffects.mList.erase(iter);
                changed = true;
                continue;
            }

            if (mgef->mData.mFlags & ESM::MagicEffect::TargetSkill)
            {
                if (iter->mAttribute != -1)
                {
                    iter->mAttribute = -1;
                    Log(Debug::Verbose) << ESM::MagicEffect::effectIdToString(iter->mEffectID) <<
                        " effect of spell '" << spell.mId << "' has an attribute argument present. Dropping the argument.";
                    changed = true;
                }
            }
            else if (mgef->mData.mFlags & ESM::MagicEffect::TargetAttribute)
            {
                if (iter->mSkill != -1)
                {
                    iter->mSkill = -1;
                    Log(Debug::Verbose) << ESM::MagicEffect::effectIdToString(iter->mEffectID) <<
                        " effect of spell '" << spell.mId << "' has a skill argument present. Dropping the argument.";
                    changed = true;
                }
            }
            else if (iter->mSkill != -1 || iter->mAttribute != -1)
            {
                iter->mSkill = -1;
                iter->mAttribute = -1;
                Log(Debug::Verbose) << ESM::MagicEffect::effectIdToString(iter->mEffectID) <<
                    " effect of spell '" << spell.mId << "' has argument(s) present. Dropping the argument(s).";
                changed = true;
            }

            ++iter;
        }

        if (changed)
            spellsToReplace.emplace_back(spell);
    }

    for (const ESM::Spell &spell : spellsToReplace)
    {
        get<ESM::Spell>().eraseStatic(spell.mId);
        get<ESM::Spell>().insertStatic(spell);
    }
}

void ESMStore::validateDynamic()
{
    std::vector<ESM::NPC> npcsToReplace = getNPCsToReplace(get<ESM::Faction>(), get<ESM::Class>(), get<ESM::NPC>().mDynamic);

    for (const ESM::NPC &npc : npcsToReplace)
        get<ESM::NPC>().insert(npc);

    removeMissingScripts(get<ESM::Script>(), get<ESM::Armor>().mDynamic);
    removeMissingScripts(get<ESM::Script>(), get<ESM::Book>().mDynamic);
    removeMissingScripts(get<ESM::Script>(), get<ESM::Clothing>().mDynamic);
    removeMissingScripts(get<ESM::Script>(), get<ESM::Weapon>().mDynamic);

    removeMissingObjects(get<ESM::CreatureLevList>());
    removeMissingObjects(get<ESM::ItemLevList>());
}

// Leveled lists can be modified by scripts. This removes items that no longer exist (presumably because the plugin was removed) from modified lists
template<class T>
void ESMStore::removeMissingObjects(Store<T>& store)
{
    for(auto& entry : store.mDynamic)
    {
        auto first = std::remove_if(entry.second.mList.begin(), entry.second.mList.end(), [&] (const auto& item)
        {
            if(!find(item.mId))
            {
                Log(Debug::Verbose) << "Leveled list '" << entry.first << "' has nonexistent object '" << item.mId << "', ignoring it.";
                return true;
            }
            return false;
        });
        entry.second.mList.erase(first, entry.second.mList.end());
    }
}

template <class T>
const T* ESMStore::insert(const T& x)
{
    const std::string id = "$dynamic" + std::to_string(mDynamicCount++);

    Store<T>& store = const_cast<Store<T>&>(get<T>());
    if (store.search(id) != nullptr)
    {
        const std::string msg = "Try to override existing record '" + id + "'";
        throw std::runtime_error(msg);
    }
    T record = x;

    record.mId = id;

    T* ptr = store.insert(record);
    for (iterator it = mStores.begin(); it != mStores.end(); ++it)
    {
        if (it->second == &store)
        {
            mImpl->mIds[ptr->mId] = it->second->getRecName();
        }
    }
    return ptr;
}

template <class T>
const T* ESMStore::overrideRecord(const T& x)
{
    Store<T>& store = const_cast<Store<T>&>(get<T>());

    T* ptr = store.insert(x);
    for (iterator it = mStores.begin(); it != mStores.end(); ++it)
    {
        if (it->second == &store)
        {
            mImpl->mIds[ptr->mId] = it->second->getRecName();
        }
    }
    return ptr;
}

template <class T>
const T* ESMStore::insertStatic(const T& x)
{
    Store<T>& store = const_cast<Store<T>&>(get<T>());
    if (store.search(x.mId) != nullptr)
    {
        const std::string msg = "Try to override existing record '" + x.mId + "'";
        throw std::runtime_error(msg);
    }

    T* ptr = store.insertStatic(x);
    for (iterator it = mStores.begin(); it != mStores.end(); ++it)
    {
        if (it->second == &store)
        {
            mImpl->mIds[ptr->mId] = it->second->getRecName();
        }
    }
    return ptr;
}

    int ESMStore::countSavedGameRecords() const
    {
        return 1 // DYNA (dynamic name counter)
            + get<ESM::Potion>().getDynamicSize()
            + get<ESM::Armor>().getDynamicSize()
            + get<ESM::Book>().getDynamicSize()
            + get<ESM::Class>().getDynamicSize()
            + get<ESM::Clothing>().getDynamicSize()
            + get<ESM::Enchantment>().getDynamicSize()
            + get<ESM::NPC>().getDynamicSize()
            + get<ESM::Spell>().getDynamicSize()
            + get<ESM::Weapon>().getDynamicSize()
            + get<ESM::CreatureLevList>().getDynamicSize()
            + get<ESM::ItemLevList>().getDynamicSize()
            + get<ESM::Creature>().getDynamicSize()
            + get<ESM::Container>().getDynamicSize();
    }

    void ESMStore::write (ESM::ESMWriter& writer, Loading::Listener& progress) const
    {
        writer.startRecord(ESM::REC_DYNA);
        writer.startSubRecord("COUN");
        writer.writeT(mDynamicCount);
        writer.endRecord("COUN");
        writer.endRecord(ESM::REC_DYNA);

        get<ESM::Potion>().write(writer, progress);
        get<ESM::Armor>().write(writer, progress);
        get<ESM::Book>().write(writer, progress);
        get<ESM::Class>().write(writer, progress);
        get<ESM::Clothing>().write(writer, progress);
        get<ESM::Enchantment>().write(writer, progress);
        get<ESM::NPC>().write(writer, progress);
        get<ESM::Spell>().write(writer, progress);
        get<ESM::Weapon>().write(writer, progress);
        get<ESM::CreatureLevList>().write(writer, progress);
        get<ESM::ItemLevList>().write(writer, progress);
        get<ESM::Creature>().write(writer, progress);
        get<ESM::Container>().write(writer, progress);
    }

    bool ESMStore::readRecord (ESM::ESMReader& reader, uint32_t type)
    {
        switch (type)
        {
            case ESM::REC_ALCH:
            case ESM::REC_ARMO:
            case ESM::REC_BOOK:
            case ESM::REC_CLAS:
            case ESM::REC_CLOT:
            case ESM::REC_ENCH:
            case ESM::REC_SPEL:
            case ESM::REC_WEAP:
            case ESM::REC_LEVI:
            case ESM::REC_LEVC:
                mImpl->mRecordToStore[ESM::RecNameInts(type)]->read (reader);
                return true;
            case ESM::REC_NPC_:
            case ESM::REC_CREA:
            case ESM::REC_CONT:
                mImpl->mRecordToStore[ESM::RecNameInts(type)]->read(reader, true);
                return true;

            case ESM::REC_DYNA:
                reader.getSubNameIs("COUN");
                reader.getHT(mDynamicCount);
                return true;

            default:

                return false;
        }
    }

    bool ESMStore::readRecord(ESM4::Reader& reader, uint32_t type)
    {
        switch (type)
        {
            case ESM::REC_ALCH4:
            case ESM::REC_ARMO4:
            case ESM::REC_BOOK4:
            case ESM::REC_CLAS4:
            case ESM::REC_CLOT4:
            case ESM::REC_ENCH4:
            case ESM::REC_SPEL4:
            case ESM::REC_WEAP4:
                mImpl->mRecordToStore[ESM::RecNameInts(type)]->read(reader);
                return true;
            case ESM::REC_NPC_4:
            case ESM::REC_CREA4:
            case ESM::REC_CONT4:
                mImpl->mRecordToStore[ESM::RecNameInts(type)]->read(reader, true);
                return true;

            default:

                return false;
        }
    }

    void ESMStore::checkPlayer()
    {
        setUp();

        const ESM::NPC *player = get<ESM::NPC>().find ("player");

        if (!get<ESM::Race>().find (player->mRace) || !get<ESM::Class>().find(player->mClass))
            throw std::runtime_error ("Invalid player record (race or class unavailable");
    }

    std::pair<std::shared_ptr<MWMechanics::SpellList>, bool> ESMStore::getSpellList(const std::string& id) const
    {
        auto result = mSpellListCache.find(id);
        std::shared_ptr<MWMechanics::SpellList> ptr;
        if (result != mSpellListCache.end())
            ptr = result->second.lock();
        if (!ptr)
        {
            int type = find(id);
            ptr = std::make_shared<MWMechanics::SpellList>(id, type);
            if (result != mSpellListCache.end())
                result->second = ptr;
            else
                mSpellListCache.insert({id, ptr});
            return {ptr, false};
        }
        return {ptr, true};
    }

    const std::string& ESMStore::getFormName(uint32_t formId) const
    {
        return mImpl->mTES4RecordIds[formId];
    }

    template <>
    const ESM::Book* ESMStore::insert<ESM::Book>(const ESM::Book& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Armor* ESMStore::insert<ESM::Armor>(const ESM::Armor& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Class* ESMStore::insert<ESM::Class>(const ESM::Class& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Enchantment* ESMStore::insert<ESM::Enchantment>(const ESM::Enchantment& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Potion* ESMStore::insert<ESM::Potion>(const ESM::Potion& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Weapon* ESMStore::insert<ESM::Weapon>(const ESM::Weapon& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Clothing* ESMStore::insert<ESM::Clothing>(const ESM::Clothing& toInsert) { return mImpl->insert(*this, toInsert); }
    template <>
    const ESM::Spell* ESMStore::insert<ESM::Spell>(const ESM::Spell& toInsert) { return mImpl->insert(*this, toInsert); }

    template <>
    const ESM::GameSetting* ESMStore::insertStatic<ESM::GameSetting>(const ESM::GameSetting& toInsert) { return mImpl->insertStatic(*this, toInsert); }
    template <>
    const ESM::Static* ESMStore::insertStatic<ESM::Static>(const ESM::Static& toInsert) { return mImpl->insertStatic(*this, toInsert); }
    template <>
    const ESM4::Static* ESMStore::insertStatic<ESM4::Static>(const ESM4::Static& toInsert) { return mImpl->insertStatic(*this, toInsert); }
    template <>
    const ESM::Door* ESMStore::insertStatic<ESM::Door>(const ESM::Door& toInsert) { return mImpl->insertStatic(*this, toInsert); }
    template <>
    const ESM::Global* ESMStore::insertStatic<ESM::Global>(const ESM::Global& toInsert) { return mImpl->insertStatic(*this, toInsert); }
    template <>
    const ESM::NPC* ESMStore::insertStatic<ESM::NPC>(const ESM::NPC& toInsert) { return mImpl->insertStatic(*this, toInsert); }

    template <>
    const ESM::Container* ESMStore::overrideRecord<ESM::Container>(const ESM::Container& toInsert) { return mImpl->overrideRecord(*this, toInsert); }
    template <>
    const ESM::Creature* ESMStore::overrideRecord<ESM::Creature>(const ESM::Creature& toInsert) { return mImpl->overrideRecord(*this, toInsert); }
    template <>
    const ESM::CreatureLevList* ESMStore::overrideRecord<ESM::CreatureLevList>(const ESM::CreatureLevList& toInsert) { return mImpl->overrideRecord(*this, toInsert); }
    template <>
    const ESM::Door* ESMStore::overrideRecord<ESM::Door>(const ESM::Door& toInsert) { return mImpl->overrideRecord(*this, toInsert); }
    template <>
    const ESM::ItemLevList* ESMStore::overrideRecord<ESM::ItemLevList>(const ESM::ItemLevList& toInsert) { return mImpl->overrideRecord(*this, toInsert); }
    template <>
    const ESM::NPC* ESMStore::overrideRecord<ESM::NPC>(const ESM::NPC& toInsert) { return mImpl->overrideRecord(*this, toInsert); }


    template <>
    const Store<ESM::Attribute>& ESMStore::get<ESM::Attribute>() const
    {
        return mImpl->mAttributes;
    }

    template <>
    const Store<ESM::MagicEffect>& ESMStore::get<ESM::MagicEffect>() const
    {
        return mImpl->mMagicEffects;
    }

    template <>
    const Store<ESM::Skill>& ESMStore::get<ESM::Skill>() const
    {
        return mImpl->mSkills;
    }

    template <>
    Store<ESM::Attribute>& ESMStore::get<ESM::Attribute>()
    {
        return mImpl->mAttributes;
    }

    template <>
    Store<ESM::MagicEffect>& ESMStore::get<ESM::MagicEffect>() 
    {
        return mImpl->mMagicEffects;
    }

    template <>
    Store<ESM::Skill>& ESMStore::get<ESM::Skill>()
    {
        return mImpl->mSkills;
    }

    template <>
    const ESM::NPC* ESMStore::insert<ESM::NPC>(const ESM::NPC& npc)
    {
        if (Misc::StringUtils::ciEqual(npc.mId, "player"))
        {
            return mStores.get<ESM::NPC>().insert(npc);
        }

        const std::string id = "$dynamic" + std::to_string(mDynamicCount++);
        if (mStores.get<ESM::NPC>().search(id) != nullptr)
        {
            const std::string msg = "Try to override existing record '" + id + "'";
            throw std::runtime_error(msg);
        }
        ESM::NPC record = npc;

        record.mId = id;

        ESM::NPC* ptr = mStores.get<ESM::NPC>().insert(record);
        mImpl->mIds[ptr->mId] = ESM::REC_NPC_;
        return ptr;
    }
} // end namespace
