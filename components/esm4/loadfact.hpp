#ifndef ESM4_FACT_H
#define ESM4_FACT_H


#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>

#include "formid.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Faction
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_FACT4;
        static std::string getRecordType() { return "Faction (TES4)"; }

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

#pragma pack(push, 1)
        enum FactionFlags1 : uint8_t
        {
            FF1_HiddenFromPC = 1 << 0,
            FF1_Evil = 1 << 1,
            FF1_SpecialCombat = 1 << 2,
        };
        enum FactionFlags2 : uint8_t
        {
            FF2_TrackCrime = 1 << 0,
            FF2_AllowSell = 1 << 1,
        };
        struct Data
        {
            FactionFlags1 flags1;
            FactionFlags2 flags2;
            //uint8_t unused[2];
        };
        enum GroupCombatReaction :uint32_t
        {
            GCR_Neutral,
            GCR_Enemy,
            GCR_Ally,
            GCR_Friend
        };
        struct Relation
        {
            FormId target; // race or faction the faction has a relation with
            int32_t modifier; // negative values are a bad relation, positive is good
            GroupCombatReaction reaction; // how members of the faction react when this target is in combat
        };
#pragma pack(pop)

        std::string mEditorId;
        std::string mFullName;

        FormId mReputation; // FONV

        Data mData; // FO3/NV
        std::vector<int32_t> rankNumbers;
        std::vector<std::string> maleRankNames;
        std::vector<std::string> femaleRankNames;
        std::vector<std::string> rankInsignias;

        std::vector<Relation> mRelations;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& writer) const;

        //void blank();
    };
}

#endif
