#include "loadgmst.hpp"
#include "reader.hpp"

void ESM4::GameSetting::load(ESM4::Reader &reader)
{
    mFormId = reader.hdr().record.id;
    reader.adjustFormId(mFormId);
    mFlags  = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM4::SUB_EDID: reader.getZString(mEditorId); break;
            case ESM4::SUB_DATA:
            {
                switch (mEditorId[0])
                {
                    case 's': reader.getZString(mStringValue); mType = ValueType::String; break;
                    case 'f': reader.get(mFloatValue); mType = ValueType::Float; break;
                    default: reader.get(mIntValue); mType = ValueType::Int; break;
                }
                break;
            }
            default:
                throw std::runtime_error("ESM4::GMST::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}
