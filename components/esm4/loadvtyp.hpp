#ifndef ESM4_VTYP_H
#define ESM4_VTYP_H


#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>

#include "formid.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct VoiceType
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_VTYP4;
        static std::string getRecordType() { return "Voice Type (TES4)"; }

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& writer) const;

        //void blank();
    };
}

#endif
