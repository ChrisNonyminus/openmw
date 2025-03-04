/*
  Copyright (C) 2019-2021 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#include "loadterm.hpp"

#include <stdexcept>
//#include <iostream> // FIXME: testing only

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Terminal::load(ESM4::Reader& reader)
{
    mFormId = reader.hdr().record.id;
    reader.adjustFormId(mFormId);
    mFlags = reader.hdr().record.flags;

    while (reader.getSubRecordHeader())
    {
        const ESM4::SubRecordHeader& subHdr = reader.subRecordHeader();
        switch (subHdr.typeId)
        {
            case ESM4::SUB_EDID:
                reader.getZString(mEditorId);
                break;
            case ESM4::SUB_FULL:
                reader.getLocalizedString(mFullName);
                break;
            case ESM4::SUB_DESC:
                reader.getLocalizedString(mText);
                break;
            case ESM4::SUB_SCRI:
                reader.getFormId(mScriptId);
                break;
            case ESM4::SUB_PNAM:
                reader.getFormId(mPasswordNote);
                break;
            case ESM4::SUB_SNAM:
                reader.getFormId(mSound);
                break;
            case ESM4::SUB_MODL:
                reader.getZString(mModel);
                break;
            case ESM4::SUB_RNAM:
                reader.getZString(mResultText);
                break;
            case ESM4::SUB_DNAM: // difficulty
            case ESM4::SUB_ANAM: // flags
            case ESM4::SUB_CTDA:
            case ESM4::SUB_INAM:
            case ESM4::SUB_ITXT:
            case ESM4::SUB_MODT: // texture hash?
            case ESM4::SUB_SCDA:
            case ESM4::SUB_SCHR:
            case ESM4::SUB_SCRO:
            case ESM4::SUB_SCRV:
            case ESM4::SUB_SCTX:
            case ESM4::SUB_SCVR:
            case ESM4::SUB_SLSD:
            case ESM4::SUB_TNAM:
            case ESM4::SUB_OBND:
            case ESM4::SUB_MODS: // FONV
            {
                // std::cout << "TERM " << ESM::printName(subHdr.typeId) << " skipping..." << std::endl;
                reader.skipSubRecordData();
                break;
            }
            default:
                throw std::runtime_error("ESM4::TERM::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}

// void ESM4::Terminal::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Terminal::blank()
//{
// }
