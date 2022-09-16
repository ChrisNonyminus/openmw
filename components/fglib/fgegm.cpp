/*
  Copyright (C) 2019, 2020 cc9cii

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

  Based on the cryptic notes on https://facegen.com/dl/sdk/doc/manual/indepth.html
  and https://facegen.com/dl/sdk/doc/manual/fileformats.html.

*/
#include "fgegm.hpp"

#include <stdexcept>

#include "fgstream.hpp"

namespace FgLib
{
    FgEgm::FgEgm(const std::string& name, const VFS::Manager* manager)
    {
        FgStream egm(name, manager);

        egm.read(mFileType); // FIXME: assert that it is "FREGM002"
        egm.read(mNumVertices); // NOTE: this is validated later in FgSam
        egm.read(mNumSymMorphModes);
        if (mNumSymMorphModes != 50)
            throw std::runtime_error("EGM: Number of Symmetric Morph Modes is not 50");
        egm.read(mNumAsymMorphModes);
        if (mNumAsymMorphModes != 30)
            throw std::runtime_error("EGM: Number of Asymmetric Morph Modes is not 30");
        egm.read(mGeometryBasisVersion);

        egm.skip(40); // Reserved

        std::size_t index;
        boost::scoped_array<float> symMorphModeScales(new float[50]);
        boost::scoped_array<std::int16_t> symMorphModes((new std::int16_t[3 * mNumVertices * 50]));
        for (std::size_t j = 0; j < 50; ++j)
        {
            egm.read(symMorphModeScales[j]);
            index = 3 * mNumVertices * j;
            for (std::size_t v = 0; v < 3 * mNumVertices; ++v)
                egm.read(symMorphModes[index + v]);
        }
        mSymMorphModeScales.swap(symMorphModeScales);
        mSymMorphModes.swap(symMorphModes);

        boost::scoped_array<float> asymMorphModeScales(new float[30]);
        boost::scoped_array<std::int16_t> asymMorphModes((new std::int16_t[3 * mNumVertices * 30]));
        for (std::size_t k = 0; k < 30; ++k)
        {
            egm.read(asymMorphModeScales[k]);
            index = 3 * mNumVertices * k; // WARN: index reused
            for (std::size_t v = 0; v < 3 * mNumVertices; ++v)
                egm.read(asymMorphModes[index + v]);
        }
        mAsymMorphModeScales.swap(asymMorphModeScales);
        mAsymMorphModes.swap(asymMorphModes);
    }
    FgEgm::~FgEgm()
    {
    }
}
