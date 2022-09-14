#include "loadmesg.hpp"

#include <stdexcept>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Message::load(ESM4::Reader& reader)
{
    mFormId = reader.hdr().record.id;
    reader.adjustFormId(mFormId);
    mFlags  = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            // todo: buttons (how do I parse collections?)
            case ESM4::SUB_EDID: reader.getZString(mEditorId); break;
            case ESM4::SUB_DESC: reader.getZString(mDescription); break;
            case ESM4::SUB_FULL: reader.getZString(mFullName); break;
            case ESM4::SUB_INAM: reader.get(mIcon); break;
            case ESM4::SUB_DNAM: reader.get(mMsgFlags); break;
            case ESM4::SUB_TNAM: reader.get(mDisplayTime); break;
            case ESM4::SUB_ITXT:
            case ESM4::SUB_CTDA:
            case ESM4::SUB_NAM0:
            case ESM4::SUB_NAM1:
            case ESM4::SUB_NAM2:
            case ESM4::SUB_NAM3:
            case ESM4::SUB_NAM4:
            case ESM4::SUB_NAM5:
            case ESM4::SUB_NAM6:
            case ESM4::SUB_NAM7:
            case ESM4::SUB_NAM8:
            case ESM4::SUB_NAM9:
            {
                reader.skipSubRecordData();
                break;
            }
            default:
                throw std::runtime_error("ESM4::MESG::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}
