#ifndef OPENMW_MWWORLD_ESMSTORE_H
#define OPENMW_MWWORLD_ESMSTORE_H

#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <components/esm/luascripts.hpp>
#include <components/esm/records.hpp>

#include <components/esm4/reader.hpp>

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
    struct ESMStoreImpl;
    struct ESM4Reading;

    class ESMStore
    {

        // https://gpfault.net/posts/mapping-types-to-values.txt.html
        class ESMTypeMap
        {
        private:
            typedef std::map<int, StoreBase*> InternalMap;
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
            iterator find() {return mMap.find(getTypeId<Key>()); }

            template <class Key>
            const_iterator find() const {return mMap.find(getTypeId<Key>()); }

            template <class Key>
            Store<Key>& get() { return *static_cast<Store<Key>*>(find<Key>()->second); }

            template <class Key>
            const Store<Key>& get() const { return *static_cast<Store<Key>*>(find<Key>()->second); }

            template <class Key>
            void put(Store<Key>*&& value)
            {
                mMap[getTypeId<Key>()] = std::forward<Store<Key>*>(value);
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

        std::unordered_map<std::string, int> mRefCount;
        ESMTypeMap mStores;
        unsigned int mDynamicCount;

        std::unique_ptr<ESMStoreImpl> mImpl;

        mutable std::unordered_map<std::string, std::weak_ptr<MWMechanics::SpellList>, Misc::StringUtils::CiHash, Misc::StringUtils::CiEqual> mSpellListCache;

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
        typedef ESMTypeMap::const_iterator iterator;

        iterator begin() const {
            return mStores.begin();
        }

        iterator end() const {
            return mStores.end();
        }

        /// Look up the given ID in 'all'. Returns 0 if not found.
        int find(std::string_view id) const;
        int findStatic(std::string_view id) const;

        friend struct ESMStoreImpl;
        friend struct ESM4Reading;

        ESMStore();
        ~ESMStore();

        void clearDynamic ()
        {
            for (ESMTypeMap::iterator it = mStores.begin(); it != mStores.end(); ++it)
                it->second->clearDynamic();

            movePlayerRecord();
        }

        void movePlayerRecord ()
        {
            auto player = mStores.get<ESM::NPC>().find("player");
            mStores.get<ESM::NPC>().insert(*player);
        }

        /// Validate entries in store after loading a save
        void validateDynamic();

        void load(ESM::ESMReader &esm, Loading::Listener* listener, ESM::Dialogue*& dialogue);

        void load(ESM4::Reader &esm, Loading::Listener* listener, ESM4::Dialogue*& dialogue);

        template <class T>
        const Store<T> &get() const {
            return mStores.get<T>();
        }

        template <class T>
        Store<T>& get()
        {
            return mStores.get<T>();
        }

        /// Insert a custom record (i.e. with a generated ID that will not clash will pre-existing records)
        template <class T>
        const T* insert(const T& x);

        /// Insert a record with set ID, and allow it to override a pre-existing static record.
        template <class T>
        const T *overrideRecord(const T &x) ;

        template <class T>
        const T* insertStatic(const T& x);

        // This method must be called once, after loading all master/plugin files. This can only be done
        //  from the outside, so it must be public.
        void setUp();
        void validateRecords(ESM::ReadersCache& readers);

        int countSavedGameRecords() const;

        void write (ESM::ESMWriter& writer, Loading::Listener& progress) const;

        bool readRecord (ESM::ESMReader& reader, uint32_t type);

        bool readRecord (ESM4::Reader& reader, uint32_t type);
        ///< \return Known type?

        // To be called when we are done with dynamic record loading
        void checkPlayer();

        /// @return The number of instances defined in the base files. Excludes changes from the save file.
        int getRefCount(std::string_view id) const;

        /// Actors with the same ID share spells, abilities, etc.
        /// @return The shared spell list to use for this actor and whether or not it has already been initialized.
        std::pair<std::shared_ptr<MWMechanics::SpellList>, bool> getSpellList(const std::string& id) const;

        const std::string& getFormName(uint32_t formId) const;

        template <>
        const Store<ESM::Attribute>& get<ESM::Attribute>() const;
        template <>
        const Store<ESM::MagicEffect>& get<ESM::MagicEffect>() const;
        template <>
        const Store<ESM::Skill>& get<ESM::Skill>() const;
        template <>
        Store<ESM::Attribute>& get<ESM::Attribute>();
        template <>
        Store<ESM::MagicEffect>& get<ESM::MagicEffect>();
        template <>
        Store<ESM::Skill>& get<ESM::Skill>();
    };

    template <>
    inline const ESM::Cell *ESMStore::insert<ESM::Cell>(const ESM::Cell &cell) {
        return mStores.get<ESM::Cell>().insert(cell);
    }

    template <>
    inline const ESM4::Cell* ESMStore::insert<ESM4::Cell>(const ESM4::Cell& cell)
    {
        return mStores.get<ESM4::Cell>().insert(cell);
    }
}

#endif
