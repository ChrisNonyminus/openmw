#include "loadfact.hpp"
#include "reader.hpp"

void ESM4::Faction::load(ESM4::Reader &reader)
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
                //reader.get(mData); // not working for some reason; some data subrecords for FACT records seem to have varying sizes
                reader.skipSubRecordData();
                break;
            }
            case ESM4::SUB_FULL: reader.getZString(mFullName); break;
            case ESM4::SUB_WMI1: reader.get(mReputation); break;
            case ESM4::SUB_XNAM:
            {
                Relation rel;
                reader.get(rel);
                mRelations.push_back(rel);
                break;
            }
            case ESM4::SUB_RNAM:
            {
                int32_t num;
                reader.get(num);
                rankNumbers.push_back(num);
                break;
            }
            case ESM4::SUB_MNAM:
            {
                std::string subData;
                reader.getZString(subData);
                maleRankNames.push_back(subData);
                break;
            }
            case ESM4::SUB_FNAM:
            {
                std::string subData;
                reader.getZString(subData);
                femaleRankNames.push_back(subData);
                break;
            }
            case ESM4::SUB_INAM:
            {
                std::string subData;
                reader.getZString(subData);
                rankInsignias.push_back(subData);
                break;
            }
            case ESM4::SUB_CNAM: // unused
            {
                reader.skipSubRecordData();
                break;
            }
            default:
                throw std::runtime_error("ESM4::FACT::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}
