#ifndef ESM4_MESG_H
#define ESM4_MESG_H


#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>

#include "formid.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Message
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_MESG4;
        static std::string getRecordType() { return "Message (TES4)"; }
#pragma pack(push, 1)
        struct CTDA // Note: values depend on whether the game is FO3 or FNV
        {
            uint8_t type; // type of condition
            uint8_t unused[3];
            union {
                float f; // used if id is invalid
                FormId id; // form id to compare against
            } val;
            uint32_t functionIdx; // index of the function to compare using.
            uint8_t param1[4]; // parameters to pass to the function
            uint8_t param2[4];
            uint32_t runon;
            FormId ref; // reference to apply the function to
        };
        struct MenuButton
        {
            std::string text;
            CTDA condition;
        };
#pragma pack(pop)

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        std::string mDescription;
        std::string mFullName;
        FormId mIcon;
        
        uint32_t mMsgFlags; // 1: message box | 2: auto display
        uint32_t mDisplayTime;

        std::vector<MenuButton> mButtons;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& writer) const;

        //void blank();
    };
}

#endif
