#include "loadwthr.hpp"

#ifdef NDEBUG // FIXME: debuggigng only
#undef NDEBUG
#endif

#include <cassert>
#include <stdexcept>

#include <iostream> // FIXME: debug only

#include "reader.hpp"


void ESM4::Weather::load(ESM4::Reader& reader)
{
    mFormId = reader.hdr().record.id;
    reader.adjustFormId(mFormId);
    mFlags  = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM4::SUB_EDID:
            {
                reader.getString(mEditorId);
                break;
            }
            case ESM4::SUB_DNAM:
            {
                reader.getString(mCloudTextures[0]);
                break;
            }
            case ESM4::SUB_CNAM:
            {
                reader.getString(mCloudTextures[1]);
                break;
            }
            case ESM4::SUB_ANAM:
            {
                reader.getString(mCloudTextures[2]);
                break;
            }
            case ESM4::SUB_BNAM:
            {
                reader.getString(mCloudTextures[3]);
                break;
            }
            default:
                throw std::runtime_error("ESM4::WTHR::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
    
}
