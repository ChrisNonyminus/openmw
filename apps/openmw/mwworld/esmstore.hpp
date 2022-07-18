#ifndef OPENMW_MWWORLD_ESMSTORE_H
#define OPENMW_MWWORLD_ESMSTORE_H

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <components/esm/luascripts.hpp>
#include "store.hpp"

namespace Loading
{
    class Listener;
}

namespace MWMechanics
{
    class SpellList;
}

namespace ESM
{
    class ReadersCache;
}

namespace MWWorld
{
    struct ESMStoreImp;
    template<typename T> struct SRecordType
    {
        static const int recordId;
        static int getId() { return recordId; };
    };
    #define GetRecordTypeId(__Type) SRecordType<__Type>::recordId

    class ESMStore
    {
        friend struct ESMStoreImp; //This allows StoreImp to extend esmstore without beeing included everywhere
		std::unique_ptr<ESMStoreImp> mStoreImp;

        // Lookup of all IDs. Makes looking up references faster. Just
        // maps the id name to the record type.
        using IDMap = std::unordered_map<std::string, int, Misc::StringUtils::CiHash, Misc::StringUtils::CiEqual>;
        IDMap mIds;
        std::unordered_map<std::string, int> mStaticIds;

        std::unordered_map<std::string, int> mRefCount;

        std::vector<std::unique_ptr< StoreBase >> mStores;

        unsigned int mDynamicCount;

        mutable std::unordered_map<std::string, std::weak_ptr<MWMechanics::SpellList>, Misc::StringUtils::CiHash, Misc::StringUtils::CiEqual> mSpellListCache;

        template <class T>
        Store<T>& getWritable() {return static_cast<Store<T>&>(*mStores[GetRecordTypeId(T)]);};

        /// Validate entries in store after setup
        void validate();

        void countAllCellRefs(ESM::ReadersCache& readers);

        template<class T>
        void removeMissingObjects(Store<T>& store);

        using LuaContent = std::variant<
            ESM::LuaScriptsCfg,  // data from an omwaddon
            std::string>;  // path to an omwscripts file
        std::vector<LuaContent> mLuaContent;

    public:
        void addOMWScripts(std::string filePath) { mLuaContent.push_back(std::move(filePath)); }
        ESM::LuaScriptsCfg getLuaScriptsCfg() const;

        /// \todo replace with SharedIterator<StoreBase>
        typedef std::vector<std::unique_ptr< StoreBase>>::const_iterator iterator;

        iterator begin() const {
            return mStores.begin();
        }

        iterator end() const {
            return mStores.end();
        }

        /// Look up the given ID in 'all'. Returns 0 if not found.
        int find(std::string_view id) const
        {
            IDMap::const_iterator it = mIds.find(id);
            if (it == mIds.end()) {
                return 0;
            }
            return it->second;
        }
        int findStatic(const std::string &id) const
        {
            IDMap::const_iterator it = mStaticIds.find(id);
            if (it == mStaticIds.end()) {
                return 0;
            }
            return it->second;
        }

        ESMStore();
        ~ESMStore();

        void clearDynamic();

        void movePlayerRecord();

        /// Validate entries in store after loading a save
        void validateDynamic();

        void load(ESM::ESMReader &esm, Loading::Listener* listener, ESM::Dialogue*& dialogue);

        template <class T>
        const Store<T>& get() const {return static_cast<const Store<T>&>(*mStores[GetRecordTypeId(T)]);}

        /// Insert a custom record (i.e. with a generated ID that will not clash will pre-existing records)
        template <class T>
        const T* insert(const T& x);

        /// Insert a record with set ID, and allow it to override a pre-existing static record.
        template <class T>
        const T *overrideRecord(const T &x);

        template <class T>
        const T *insertStatic(const T &x);

        // This method must be called once, after loading all master/plugin files. This can only be done
        //  from the outside, so it must be public.
        void setUp();
        void validateRecords(ESM::ReadersCache& readers);

        int countSavedGameRecords() const;

        void write (ESM::ESMWriter& writer, Loading::Listener& progress) const;

        bool readRecord (ESM::ESMReader& reader, uint32_t type);
        ///< \return Known type?

        // To be called when we are done with dynamic record loading
        void checkPlayer();

        /// @return The number of instances defined in the base files. Excludes changes from the save file.
        int getRefCount(std::string_view id) const;

        /// Actors with the same ID share spells, abilities, etc.
        /// @return The shared spell list to use for this actor and whether or not it has already been initialized.
        std::pair<std::shared_ptr<MWMechanics::SpellList>, bool> getSpellList(const std::string& id) const;
    };

    //Special cases these aren't StoreBase, but IndexedStore
    template <> const Store<ESM::MagicEffect>& ESMStore::get<ESM::MagicEffect>() const;
    template <> Store<ESM::MagicEffect>& ESMStore::getWritable<ESM::MagicEffect>();

    template <> const Store<ESM::Skill>& ESMStore::get<ESM::Skill>() const;
    template <> Store<ESM::Skill>& ESMStore::getWritable<ESM::Skill>();

    template <> const Store<ESM::Attribute>& ESMStore::get<ESM::Attribute>() const;
    template <> Store<ESM::Attribute>& ESMStore::getWritable<ESM::Attribute>();

}

#endif
