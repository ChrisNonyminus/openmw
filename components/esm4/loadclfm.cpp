/*
  Copyright (C) 2020-2021 cc9cii

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
#include "loadclfm.hpp"

#include <iostream> // FIXME: for debugging only
#include <stdexcept>

#include "reader.hpp"
//#include "writer.hpp"

void ESM4::Colour::load(ESM4::Reader& reader)
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
            case ESM4::SUB_CNAM:
            {
                reader.get(mColour.red);
                reader.get(mColour.green);
                reader.get(mColour.blue);
                reader.get(mColour.custom);

                break;
            }
            case ESM4::SUB_FNAM:
            {
                reader.get(mPlayable);

                break;
            }
            default:
                // std::cout << "CLFM " << ESM::printName(subHdr.typeId) << " skipping..."
                //<< subHdr.dataSize << std::endl;
                // reader.skipSubRecordData();
                throw std::runtime_error("ESM4::CLFM::load - Unknown subrecord " + ESM::printName(subHdr.typeId));
        }
    }
}

// void ESM4::Colour::save(ESM4::Writer& writer) const
//{
// }

// void ESM4::Colour::blank()
//{
// }
