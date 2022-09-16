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
#ifndef FGLIB_FGSTREAM_H
#define FGLIB_FGSTREAM_H

#include <vector>
#include <cstddef>

#include <components/vfs/manager.hpp>

#include <osg/Referenced>
#include <osg/ref_ptr>
#include <osg/Vec3f>
#include <osg/Quat>
#include <osg/Matrixf>

#include <components/files/constrainedfilestream.hpp>

namespace FgLib
{
    class FgStream
    {
        Files::IStreamPtr mStream;

        // default, copy and assignment not allowed
        FgStream();
        FgStream(const FgStream& other);
        FgStream& operator=(const FgStream& other);

    public:

        FgStream(const std::string& name, const VFS::Manager* manager);
        ~FgStream();

        //size_t tell() { return mStream->tell(); } // FIXME: debugging only
        //const std::string& getName() const { return mStream->getName(); } // FIXME: debugging only

        std::string readString();
        void readSizedString(std::string& str);

        template<typename T>
        inline void read(T& t) {
            mStream->read((char*)&t, sizeof(T));
        }

        template<typename T>
        inline T read() {
            T value;
            mStream->read((char*)&value, sizeof(T));
            return value;
        }

        inline void skip(unsigned long length) { mStream->ignore(length); }

        template<typename T>
        inline void readVector(std::vector<T>& data, std::uint32_t size) {
            data.resize(size);
            for (std::uint32_t i = 0; i < size; ++i)
                read(data.at(i));
        }

        template<typename T>
        inline void readVector(std::vector<T>& data) {
            std::uint32_t size = 0;
            mStream->read(&size, sizeof(std::uint32_t));

            data.resize(size);
            for (std::uint32_t i = 0; i < size; ++i)
                read(data.at(i));
        }
    };

    template<>
    void FgStream::read<osg::Vec3f>(osg::Vec3f& value);

    template<>
    void FgStream::read<osg::Quat>(osg::Quat& value);

    template<>
    void FgStream::read<osg::Matrix3>(osg::Matrix3& value);
}

#endif // NIBTOGRE_FGSTREAM_H
