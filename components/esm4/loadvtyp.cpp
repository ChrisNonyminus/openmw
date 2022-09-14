#include "loadvtyp.hpp"
#include "reader.hpp"

void ESM4::VoiceType::load(ESM4::Reader &reader)
{
    mFormId = reader.hdr().record.id;
    reader.adjustFormId(mFormId);
    mFlags  = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case SUB_EDID: reader.getZString(mEditorId); break;
            case SUB_DNAM:
            {
                reader.skipSubRecordData();
                break;
            }
            default:
                throw std::runtime_error("ESM4::VTYP::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}
