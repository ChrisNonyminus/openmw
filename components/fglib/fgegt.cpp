/*
  Copyright (C) 2019-2020 cc9cii

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
#include "fgegt.hpp"

#include <stdexcept>

#include <boost/scoped_array.hpp>

#include "fgstream.hpp"

FgLib::FgEgt::FgEgt(const std::string& name, const VFS::Manager* manager)
{
    FgStream egt(name, manager);

    egt.read(mFileType); // FIXME: assert that it is "FREGM002"
    egt.read(mNumRows);
    egt.read(mNumColumns);
    egt.read(mNumSymTextureModes);
    if (mNumSymTextureModes != 50)
        throw std::runtime_error("EGT: Number of Symmetric Texture Modes is not 50");
    egt.read(mNumAsymTextureModes);
    if (mNumAsymTextureModes != 0)
        throw std::runtime_error("EGT: Number of Asymmetric Texture Modes is not 0");
    egt.read(mTextureBasisVersion);

    egt.skip(36); // Reserved

    char r, g, b;
    std::size_t index = 0;
    std::size_t imgSize = mNumRows * mNumColumns;
    std::size_t size = imgSize * 50 /*mNumSymTextureModes*/;
    boost::scoped_array<char> rgb(new char[3 * size]);

    // NOTE: the EGT image is row-major but starting at the bottom left
    //  c.f. Ogre::PixelBox addressing starts at top left, so a conversion is needed
    //
    // RANT: It took countless cycles of trial and error just to identify that the issues might
    //       be related to EGT (and not how the two detailed maps should be applied and using
    //       which method) and then wasted so much time figuring out that the image pixel order
    //       was causing the problem :-(
    //
    // EGT: top left pixel     = (mNumRows - 1) * mNumColumns
    //      top right pixel    = mNumRows*mNumColumns - 1
    //      bottom left pixel  = 0
    //      bottom right pixel = mNumColumns - 1
    //
    // FIXME: do this once rather than for each EGT instance?
    boost::scoped_array<std::size_t> flip(new size_t[imgSize]);
    for (std::size_t r = 0; r < mNumRows; ++r)
        for (std::size_t c = 0; c < mNumColumns; ++c)
            flip[index++] = (mNumRows - 1 - r) * mNumColumns + c;

    mSymTextureModeScales.resize(mNumSymTextureModes);
    mSymTextureModes.resize(size);

    for (std::size_t j = 0; j < mNumSymTextureModes; ++j)
    {
        egt.read(mSymTextureModeScales.at(j)); // scale for symmetric texture mode j

        for (std::size_t v = 0; v < 3 * imgSize; ++v) // (R image + G image + B image) for each mode j
            egt.read(rgb[j * 3 * imgSize + v]);

        // reorganise to have all modes for the same pixel location grouped together
        // FIXME: prob. premature optimisation
        for (std::size_t i = 0; i < imgSize; ++i)
        {
            //   rgb[a] | r r r r .... r g g g g .... g b b b b .... b r r r r
            //          | ^                                            ^
            //          | |                                            |
            //       a  | 0 1 2 3 ....                                 3*imgSize+0
            //  mode j  | 0 0 0 0 ....                                 1 1 1 1
            // pixel i  | 0 1 2 3 ....                                 0 1 2 3
            //
            // i.e. the same pixel locations for each mode are 3*imgSize apart
            //      r and g are 1*imgSize apart
            //      r and b are 2*imgSize apart
            r = rgb[j * 3 * imgSize + i + 0 * imgSize];
            g = rgb[j * 3 * imgSize + i + 1 * imgSize];
            b = rgb[j * 3 * imgSize + i + 2 * imgSize];

            // convert i to row-major order starting top left
            mSymTextureModes[j + flip[i] * mNumSymTextureModes]
                = osg::Vec3f(float(r), float(g), float(b)) * mSymTextureModeScales[j];
        }
    }

    // FIXME: asym texture modes
}

FgLib::FgEgt::~FgEgt()
{
}
