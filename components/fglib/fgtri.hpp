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

*/
#ifndef FGLIB_FGTRI_H
#define FGLIB_FGTRI_H

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <osg/Vec3>

#include <components/vfs/manager.hpp>

#include <boost/scoped_array.hpp>

#include <components/sceneutil/keyframe.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/textkeymap.hpp>

#include <osg/Referenced>
#include <osg/ref_ptr>

namespace osg
{
    class Node;
}

namespace FgLib
{
    class FgTri
    {
        uint64_t mFileType;
        int32_t mNumVertices;
        int32_t mNumTriangles;
        int32_t mNumQuads;
        int32_t mNumLabelledVertices;
        int32_t mNumLabelledSurfacePoints;
        int32_t mNumTexCoords;
        int32_t mExtensionInfo; // 0x01: There are texture coordinates | 0x02: surface point labels are 16-bit chars.
        int32_t mNumLabelledDiffMorphs;
        int32_t mNumLabelledStatMorphs;
        int32_t mNumTotalStatMorphVertices;
        // char[16] reserved

        boost::scoped_array<float> mVertices;
        boost::scoped_array<std::int32_t> mTriangleIndices;
        boost::scoped_array<std::int32_t> mQuadIndices;

        std::vector<std::string> mLabelledDiffMorphs; // names
        std::map<std::string, std::pair<float, std::vector<std::int16_t>>> mLabelledDiffMorphsMap;

        bool mNeedsNifVertices;

        FgTri();
        FgTri(const FgTri& other);
        FgTri& operator=(const FgTri& other);

    public:
        FgTri(const std::string triPath, const VFS::Manager* manager);
        FgTri(const std::vector<osg::Vec3f>& nifVerts); // for creating a dummy
        ~FgTri();

        // used by FgSam
        inline std::uint32_t numVertices() const { return mNumVertices; }
        inline const boost::scoped_array<float>& vertices() const { return mVertices; }
        inline std::uint32_t numStatMorphVertices() const { return mNumTotalStatMorphVertices; }

        // simply means that the TRI file corresponding to the NIF was not found
        inline bool needsNifVertices() const { return mNeedsNifVertices; }

        // number of emotions, lip sync
        inline std::uint32_t numDiffMorphs() const { return mNumLabelledDiffMorphs; }
        inline const std::vector<std::string>& diffMorphs() const { return mLabelledDiffMorphs; }
        const std::pair<float, std::vector<std::int16_t> >& diffMorphVertices(const std::string& label) const;

        bool hasDiffMorph(const std::string& label) const;
    };
}

#endif
