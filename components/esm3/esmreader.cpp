#include "esmreader.hpp"

#include "readerscache.hpp"

#include <components/files/conversion.hpp>
#include <components/files/openfile.hpp>
#include <components/misc/strings/algorithm.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace ESM
{

    ESM_Context ESMReader::getContext()
    {
        // Update the file position before returning
        mCtx.filePos = mEsm->tellg();
        return mCtx;
    }

    ESMReader::ESMReader()
        : mRecordFlags(0)
        , mBuffer(50 * 1024)
        , mEncoder(nullptr)
        , mFileSize(0)
    {
        clearCtx();
        mCtx.index = 0;
    }

    void ESMReader::restoreContext(const ESM_Context& rc)
    {
        // Reopen the file if necessary
        if (mCtx.filename != rc.filename)
            openRaw(rc.filename);

        // Copy the data
        mCtx = rc;

        // Make sure we seek to the right place
        mEsm->seekg(mCtx.filePos);
    }

    void ESMReader::close()
    {
        mEsm.reset();
        clearCtx();
        mHeader.blank();
    }

    void ESMReader::clearCtx()
    {
        mCtx.filename.clear();
        mCtx.leftFile = 0;
        mCtx.leftRec = 0;
        mCtx.leftSub = 0;
        mCtx.subCached = false;
        mCtx.recName.clear();
        mCtx.subName.clear();
    }

    void ESMReader::resolveParentFileIndices(ReadersCache& readers)
    {
        mCtx.parentFileIndices.clear();
        for (const Header::MasterData& mast : getGameFiles())
        {
            const std::string& fname = mast.name;
            int index = getIndex();
            for (int i = 0; i < getIndex(); i++)
            {
                const ESM::ReadersCache::BusyItem reader = readers.get(static_cast<std::size_t>(i));
                if (reader->getFileSize() == 0)
                    continue; // Content file in non-ESM format
                const auto fnamecandidate = Files::pathToUnicodeString(reader->getName().filename());
                if (Misc::StringUtils::ciEqual(fname, fnamecandidate))
                {
                    index = i;
                    break;
                }
            }
            mCtx.parentFileIndices.push_back(index);
        }
    }

    void ESMReader::openRaw(std::unique_ptr<std::istream>&& stream, const std::filesystem::path& name)
    {
        close();
        mEsm = std::move(stream);
        mCtx.filename = name;
        mEsm->seekg(0, mEsm->end);
        mCtx.leftFile = mFileSize = mEsm->tellg();
        mEsm->seekg(0, mEsm->beg);
    }

    void ESMReader::openRaw(const std::filesystem::path& filename)
    {
        openRaw(Files::openBinaryInputFileStream(filename), filename);
    }

    void ESMReader::open(std::unique_ptr<std::istream>&& stream, const std::filesystem::path& name)
    {
        openRaw(std::move(stream), name);

        if (getRecName() != "TES3")
            fail("Not a valid Morrowind file");

        getRecHeader();

        mHeader.load(*this);
    }

    void ESMReader::open(const std::filesystem::path& file)
    {
        open(Files::openBinaryInputFileStream(file), file);
    }

    std::string ESMReader::getHNOString(NAME name)
    {
        if (isNextSub(name))
            return getHString();
        return "";
    }

    ESM::RefId ESMReader::getHNORefId(NAME name)
    {
        if (isNextSub(name))
            return getRefId();
        return ESM::RefId::sEmpty;
    }

    void ESMReader::skipHNORefId(NAME name)
    {
        if (isNextSub(name))
            skipHRefId();
    }

    std::string ESMReader::getHNString(NAME name)
    {
        getSubNameIs(name);
        return getHString();
    }

    RefId ESMReader::getHNRefId(NAME name)
    {
        getSubNameIs(name);
        return getRefId();
    }

    std::string ESMReader::getHString()
    {
        return std::string(getHStringView());
    }

    std::string_view ESMReader::getHStringView()
    {
        getSubHeader();

        // Hack to make MultiMark.esp load. Zero-length strings do not
        // occur in any of the official mods, but MultiMark makes use of
        // them. For some reason, they break the rules, and contain a byte
        // (value 0) even if the header says there is no data. If
        // Morrowind accepts it, so should we.
        if (mCtx.leftSub == 0 && hasMoreSubs() && !mEsm->peek())
        {
            // Skip the following zero byte
            mCtx.leftRec--;
            char c;
            getT(c);
            return std::string_view();
        }

        return getStringView(mCtx.leftSub);
    }

    RefId ESMReader::getRefId()
    {
        return ESM::RefId::stringRefId(getHStringView());
    }

    void ESMReader::skipHString()
    {
        getSubHeader();

        // Hack to make MultiMark.esp load. Zero-length strings do not
        // occur in any of the official mods, but MultiMark makes use of
        // them. For some reason, they break the rules, and contain a byte
        // (value 0) even if the header says there is no data. If
        // Morrowind accepts it, so should we.
        if (mCtx.leftSub == 0 && hasMoreSubs() && !mEsm->peek())
        {
            // Skip the following zero byte
            mCtx.leftRec--;
            skipT<char>();
            return;
        }

        skip(mCtx.leftSub);
    }

    void ESMReader::skipHRefId()
    {
        skipHString();
    }

    void ESMReader::getHExact(void* p, int size)
    {
        getSubHeader();
        if (size != static_cast<int>(mCtx.leftSub))
            reportSubSizeMismatch(size, mCtx.leftSub);
        getExact(p, size);
    }

    // Read the given number of bytes from a named subrecord
    void ESMReader::getHNExact(void* p, int size, NAME name)
    {
        getSubNameIs(name);
        getHExact(p, size);
    }

    // Get the next subrecord name and check if it matches the parameter
    void ESMReader::getSubNameIs(NAME name)
    {
        getSubName();
        if (mCtx.subName != name)
            fail("Expected subrecord " + name.toString() + " but got " + mCtx.subName.toString());
    }

    bool ESMReader::isNextSub(NAME name)
    {
        if (!hasMoreSubs())
            return false;

        getSubName();

        // If the name didn't match, then mark the it as 'cached' so it's
        // available for the next call to getSubName.
        mCtx.subCached = (mCtx.subName != name);

        // If subCached is false, then subName == name.
        return !mCtx.subCached;
    }

    bool ESMReader::peekNextSub(NAME name)
    {
        if (!hasMoreSubs())
            return false;

        getSubName();

        mCtx.subCached = true;
        return mCtx.subName == name;
    }

    // Read subrecord name. This gets called a LOT, so I've optimized it
    // slightly.
    void ESMReader::getSubName()
    {
        // If the name has already been read, do nothing
        if (mCtx.subCached)
        {
            mCtx.subCached = false;
            return;
        }

        // reading the subrecord data anyway.
        const std::size_t subNameSize = decltype(mCtx.subName)::sCapacity;
        getExact(mCtx.subName.mData, static_cast<int>(subNameSize));
        mCtx.leftRec -= static_cast<std::uint32_t>(subNameSize);
    }

    void ESMReader::skipHSub()
    {
        getSubHeader();
        skip(mCtx.leftSub);
    }

    void ESMReader::skipHSubSize(int size)
    {
        skipHSub();
        if (static_cast<int>(mCtx.leftSub) != size)
            reportSubSizeMismatch(mCtx.leftSub, size);
    }

    void ESMReader::skipHSubUntil(NAME name)
    {
        while (hasMoreSubs() && !isNextSub(name))
        {
            mCtx.subCached = false;
            skipHSub();
        }
        if (hasMoreSubs())
            mCtx.subCached = true;
    }

    void ESMReader::getSubHeader()
    {
        if (mCtx.leftRec < static_cast<std::streamsize>(sizeof(mCtx.leftSub)))
            fail("End of record while reading sub-record header: " + std::to_string(mCtx.leftRec) + " < "
                + std::to_string(sizeof(mCtx.leftSub)));

        // Get subrecord size
        getUint(mCtx.leftSub);
        mCtx.leftRec -= sizeof(mCtx.leftSub);

        // Adjust number of record bytes left
        if (mCtx.leftRec < mCtx.leftSub)
            fail("Record size is larger than rest of file: " + std::to_string(mCtx.leftRec) + " < "
                + std::to_string(mCtx.leftSub));
        mCtx.leftRec -= mCtx.leftSub;
    }

    NAME ESMReader::getRecName()
    {
        if (!hasMoreRecs())
            fail("No more records, getRecName() failed");
        getName(mCtx.recName);
        mCtx.leftFile -= decltype(mCtx.recName)::sCapacity;

        // Make sure we don't carry over any old cached subrecord
        // names. This can happen in some cases when we skip parts of a
        // record.
        mCtx.subCached = false;

        return mCtx.recName;
    }

    void ESMReader::skipRecord()
    {
        skip(mCtx.leftRec);
        mCtx.leftRec = 0;
        mCtx.subCached = false;
    }

    void ESMReader::getRecHeader(uint32_t& flags)
    {
        // General error checking
        if (mCtx.leftFile < static_cast<std::streamsize>(3 * sizeof(uint32_t)))
            fail("End of file while reading record header");
        if (mCtx.leftRec)
            fail("Previous record contains unread bytes");

        std::uint32_t leftRec = 0;
        getUint(leftRec);
        mCtx.leftRec = static_cast<std::streamsize>(leftRec);
        getUint(flags); // This header entry is always zero
        getUint(flags);
        mCtx.leftFile -= 3 * sizeof(uint32_t);

        // Check that sizes add up
        if (mCtx.leftFile < mCtx.leftRec)
            reportSubSizeMismatch(mCtx.leftFile, mCtx.leftRec);

        // Adjust number of bytes mCtx.left in file
        mCtx.leftFile -= mCtx.leftRec;
    }

    /*************************************************************************
     *
     *  Lowest level data reading and misc methods
     *
     *************************************************************************/

    std::string ESMReader::getMaybeFixedStringSize(std::size_t size)
    {
        if (mHeader.mFormatVersion > MaxLimitedSizeStringsFormatVersion)
        {
            StringSizeType storedSize = 0;
            getT(storedSize);
            if (storedSize > mCtx.leftSub)
                fail("String does not fit subrecord (" + std::to_string(storedSize) + " > "
                    + std::to_string(mCtx.leftSub) + ")");
            size = static_cast<std::size_t>(storedSize);
        }

        return std::string(getStringView(size));
    }

    std::string_view ESMReader::getStringView(std::size_t size)
    {
        if (mBuffer.size() <= size)
            // Add some extra padding to reduce the chance of having to resize
            // again later.
            mBuffer.resize(3 * size);

        // And make sure the string is zero terminated
        mBuffer[size] = 0;

        // read ESM data
        char* ptr = mBuffer.data();
        getExact(ptr, size);

        size = strnlen(ptr, size);

        // Convert to UTF8 and return
        if (mEncoder != nullptr)
            return mEncoder->getUtf8(std::string_view(ptr, size));

        return std::string_view(ptr, size);
    }

    RefId ESMReader::getRefId(std::size_t size)
    {
        return RefId::stringRefId(getStringView(size));
    }

    [[noreturn]] void ESMReader::fail(const std::string& msg)
    {
        std::stringstream ss;

        ss << "ESM Error: " << msg;
        ss << "\n  File: " << Files::pathToUnicodeString(mCtx.filename);
        ss << "\n  Record: " << mCtx.recName.toStringView();
        ss << "\n  Subrecord: " << mCtx.subName.toStringView();
        if (mEsm.get())
            ss << "\n  Offset: 0x" << std::hex << mEsm->tellg();
        throw std::runtime_error(ss.str());
    }

}
