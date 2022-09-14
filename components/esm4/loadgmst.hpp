#ifndef ESM4_GMST_H
#define ESM4_GMST_H


#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>

#include "formid.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct GameSetting
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_GMST4;
        static std::string getRecordType() { return "Game Setting (TES4)"; }

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        enum class ValueType
        {
            Int,
            Float,
            String
        };

        std::string mEditorId;

        ValueType mType;
        
        std::string mStringValue;
        float mFloatValue;
        int32_t mIntValue;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& writer) const;

        //void blank();
    };
}

#endif
