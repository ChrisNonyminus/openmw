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
#include "fgtri.hpp"

//#include <stdexcept>
#include <iostream> // FIXME: for debugging only
#include <iomanip>  // FIXME: for debugging only

#include "fgstream.hpp"

namespace FgLib
{
    FgTri::FgTri(const std::string triPath, const VFS::Manager* manager)
    {
        if (!manager->exists(triPath))
        {
            mNeedsNifVertices = true;
        }
        else
        {
            FgStream tri(triPath, manager);

            tri.read(mFileType); // FIXME: assert that it is "FRTRI003"
            tri.read(mNumVertices);
            tri.read(mNumTriangles);
            tri.read(mNumQuads);
            tri.read(mNumLabelledVertices);
            tri.read(mNumLabelledSurfacePoints);
            tri.read(mNumTexCoords);
            tri.read(mExtensionInfo);
            tri.read(mNumLabelledDiffMorphs);
            tri.read(mNumLabelledStatMorphs);
            tri.read(mNumTotalStatMorphVertices);

            tri.skip(16); // Reserved

            // NOTE: StatMorphVertices are also read into mVertices
            boost::scoped_array<float> vertices(new float[3 * (mNumVertices + mNumTotalStatMorphVertices)]);
            for (std::size_t i = 0; i < 3 * (mNumVertices + mNumTotalStatMorphVertices); ++i)
                tri.read(vertices[i]);

            mVertices.swap(vertices);

            boost::scoped_array<std::int32_t> triangleIndices(new std::int32_t[3 * mNumTriangles]);
            for (std::size_t i = 0; i < 3 * mNumTriangles; ++i)
                tri.read(triangleIndices[i]);

            mTriangleIndices.swap(triangleIndices);

            boost::scoped_array<std::int32_t> quadIndices(new std::int32_t[3 * mNumQuads]);
            for (std::size_t i = 0; i < 3 * mNumQuads; ++i)
                tri.read(quadIndices[i]);

            mQuadIndices.swap(quadIndices);

            //std::cout << "TRI: " << name << std::endl; // FIXME: for testing only

            for (std::size_t i = 0; i < mNumLabelledVertices; ++i)
            {
                std::int32_t stringIndex;
                tri.read(stringIndex); // FIXME: not sure if there is an int here

                std::string label = tri.readString();
                std::cout << "LV: " << label << std::endl; // FIXME: for testing only
            }

            // FIXME the rest of the file (dummy reads)

            for (std::size_t i = 0; i < mNumLabelledSurfacePoints; ++i)
            {
                std::int32_t stringIndex;
                tri.read(stringIndex);

                float dummy;
                tri.read(dummy);
                tri.read(dummy);
                tri.read(dummy);

                std::string label = tri.readString();
                std::cout << "LS: " << label << std::endl; // FIXME: for testing only
            }

            if (mNumTexCoords == 0 && (mExtensionInfo & 0x01) != 0)
            {
                for (std::size_t i = 0; i < mNumVertices; ++i)
                {
                    float dummy;
                    // OpenGL texture coordinates
                    tri.read(dummy); // U
                    tri.read(dummy); // V
                }
            }
            else if (mNumTexCoords > 0 && (mExtensionInfo & 0x01) != 0)
            {
                for (std::size_t i = 0; i < mNumTexCoords; ++i)
                {
                    float dummy;
                    // OpenGL texture coordinates
                    tri.read(dummy); // U
                    tri.read(dummy); // V
                }

                for (std::size_t i = 0; i < mNumTriangles; ++i)
                {
                    std::int32_t dummy;
                    tri.read(dummy);
                    tri.read(dummy);
                    tri.read(dummy);
                }

                for (std::size_t i = 0; i < mNumQuads; ++i)
                {
                    std::int32_t dummy;
                    tri.read(dummy);
                    tri.read(dummy);
                    tri.read(dummy);
                    tri.read(dummy);
                }
            }

            // anger, fear, happy, sad, etc
            std::vector<std::int16_t> diffMorphs; // TODO: array instead?
            for (std::size_t md = 0; md < mNumLabelledDiffMorphs; ++md)
            {
                std::string label = tri.readString();

                float scale;
                tri.read(scale);
                //std::cout << "\"" << label << "\" " <<
                //std::fixed << std::setprecision(6) << scale << std::endl; // FIXME: for testing only

                diffMorphs.clear();
                diffMorphs.resize(3 * mNumVertices);
                for (std::size_t i = 0; i < 3 * mNumVertices; ++i)
                    tri.read(diffMorphs[i]);

                mLabelledDiffMorphs.push_back(label);
                mLabelledDiffMorphsMap.insert(std::make_pair(label, // name of the diff morph
                    std::make_pair(scale, // for the diff morphs
                        std::move(diffMorphs))));
            }

            // blink, look up/down, squint
            for (std::size_t ms = 0; ms < mNumLabelledStatMorphs; ++ms)
            {
                std::string label = tri.readString();
                std::int32_t numAffectedVertices;
                tri.read(numAffectedVertices); // sum of numAffectedVertices == mNumTotalStatMorphVertices

                //std::cout << "\"" << label << "\" " << numAffectedVertices << std::endl; // FIXME
                for (std::size_t i = 0; i < numAffectedVertices; ++i)
                {
                    std::int32_t dummy;
                    tri.read(dummy); // vertex indices, should be < mNumVertices
                }
            }

// FIXME: for testing only
#if 0
            size_t pos = name.find_last_of(".");
            std::string mesh = name.substr(0, pos + 1)+"nif";
            NiModelPtr model = NiBtOgre::NiModelManager::getSingleton().getOrLoadByName(mesh, "General");
            const std::vector<Ogre::Vector3>& nifVerts = model->fgVertices();
            for (std::size_t i = 0; i < mNumVertices; ++i) // NOTE: can't check stat morph vertices
            {
                if (mVertices[3*i + 0] != nifVerts[i].x ||
                    mVertices[3*i + 1] != nifVerts[i].y ||
                    mVertices[3*i + 2] != nifVerts[i].z)
                {
                    // It turns out that they are indeed different, e.g. Characters\Imperial\HeadHuman.TRI
                    // Which one is the correct one to use for FaceGen SSM?
                    std::cout << name << " TRI and NIF vertices differ at index " << i << std::endl;
                }
            }
#endif
        }
    }
    FgTri::FgTri(const std::vector<osg::Vec3f>& nifVerts)
    {
        // Some meshes don't have an associated TRI file.  E.g. all the ears and some hair:
        //
        // Characters\Hair\Blindfold.NIF
        // Characters\Hair\Emperor.NIF
        // Characters\Hair\KhajiitEarrings.NIF
        // Characters\Hair\Style07.NIF

        mFileType = 0;
        mNumVertices = (std::uint32_t)nifVerts.size();
        mNumTriangles = 0;
        mNumQuads = 0;
        mNumLabelledVertices = 0;
        mNumLabelledSurfacePoints = 0;
        mNumTexCoords = 0;
        mExtensionInfo = 0;
        mNumLabelledDiffMorphs = 0;
        mNumLabelledStatMorphs = 0;
        mNumTotalStatMorphVertices = 0;

        boost::scoped_array<float> vertices(new float[3 * (mNumVertices + mNumTotalStatMorphVertices)]);
        for (std::size_t i = 0; i < mNumVertices; ++i)
        {
            vertices[3 * i + 0] = nifVerts[i].x();
            vertices[3 * i + 1] = nifVerts[i].y();
            vertices[3 * i + 2] = nifVerts[i].z();
        }
        mVertices.swap(vertices);
    }
    FgTri::~FgTri()
    {
    }
    const std::pair<float, std::vector<std::int16_t>>& FgTri::diffMorphVertices(const std::string& label) const
    {
#if 0
        std::map<std::string, std::pair<float, std::vector<std::int16_t> > >::const_iterator lb
            = mLabelledDiffMorphsMap.lower_bound(label);

        if (lb != mLabelledDiffMorphsMap.end() && !(mLabelledDiffMorphsMap.key_comp()(label, lb->first)))
        {
            return lb->second;
        }
        else // none found
            throw std::runtime_error("FgTri: not found label " + label);
#else
        std::map<std::string, std::pair<float, std::vector<std::int16_t>>>::const_iterator it
            = mLabelledDiffMorphsMap.find(label);

        if (it != mLabelledDiffMorphsMap.end())
        {
            return it->second;
        }
        else // none found
            throw std::runtime_error("FgTri: not found label " + label);
#endif
    }
    bool FgTri::hasDiffMorph(const std::string& label) const
    {
        return mLabelledDiffMorphsMap.find(label) != mLabelledDiffMorphsMap.end();
    }
}
