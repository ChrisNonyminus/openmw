#include "esmwriter.hpp"

#include <cassert>
#include <fstream>
#include <stdexcept>

#include <components/to_utf8/to_utf8.hpp>

namespace ESM
{
    ESMWriter::ESMWriter()
        : mRecords()
        , mStream(nullptr)
        , mHeaderPos()
        , mEncoder(nullptr)
        , mRecordCount(0)
        , mCounting(true)
        , mHeader()
    {
    }

    unsigned int ESMWriter::getVersion() const
    {
        return mHeader.mData.version;
    }

    void ESMWriter::setVersion(unsigned int ver)
    {
        mHeader.mData.version = ver;
    }

    void ESMWriter::setType(int type)
    {
        mHeader.mData.type = type;
    }

    void ESMWriter::setAuthor(const std::string& auth)
    {
        mHeader.mData.author.assign(auth);
    }

    void ESMWriter::setDescription(const std::string& desc)
    {
        mHeader.mData.desc.assign(desc);
    }

    void ESMWriter::setRecordCount(int count)
    {
        mHeader.mData.records = count;
    }

    void ESMWriter::setFormatVersion(FormatVersion value)
    {
        mHeader.mFormatVersion = value;
    }

    void ESMWriter::clearMaster()
    {
        mHeader.mMaster.clear();
    }

    void ESMWriter::addMaster(const std::string& name, uint64_t size)
    {
        Header::MasterData d;
        d.name = name;
        d.size = size;
        mHeader.mMaster.push_back(d);
    }

    void ESMWriter::save(std::ostream& file)
    {
        mRecordCount = 0;
        mRecords.clear();
        mCounting = true;
        mStream = &file;

        startRecord("TES3", 0);

        mHeader.save(*this);

        endRecord("TES3");
    }

    void ESMWriter::close()
    {
        if (!mRecords.empty())
            throw std::runtime_error("Unclosed record remaining");
    }

    void ESMWriter::startRecord(NAME name, uint32_t flags)
    {
        mRecordCount++;

        writeName(name);
        RecordData rec;
        rec.name = name;
        rec.position = mStream->tellp();
        rec.size = 0;
        writeT<uint32_t>(0); // Size goes here
        writeT<uint32_t>(0); // Unused header?
        writeT(flags);
        mRecords.push_back(rec);

        assert(mRecords.back().size == 0);
    }

    void ESMWriter::startRecord(uint32_t name, uint32_t flags)
    {
        startRecord(NAME(name), flags);
    }

    void ESMWriter::startSubRecord(NAME name)
    {
        // Sub-record hierarchies are not properly supported in ESMReader. This should be fixed later.
        assert(mRecords.size() <= 1);

        writeName(name);
        RecordData rec;
        rec.name = name;
        rec.position = mStream->tellp();
        rec.size = 0;
        writeT<uint32_t>(0); // Size goes here
        mRecords.push_back(rec);

        assert(mRecords.back().size == 0);
    }

    void ESMWriter::endRecord(NAME name)
    {
        RecordData rec = mRecords.back();
        assert(rec.name == name);
        mRecords.pop_back();

        mStream->seekp(rec.position);

        mCounting = false;
        write(reinterpret_cast<const char*>(&rec.size), sizeof(uint32_t));
        mCounting = true;

        mStream->seekp(0, std::ios::end);
    }

    void ESMWriter::endRecord(uint32_t name)
    {
        endRecord(NAME(name));
    }

    void ESMWriter::writeHNString(NAME name, const std::string& data)
    {
        startSubRecord(name);
        writeHString(data);
        endRecord(name);
    }

    void ESMWriter::writeHNString(NAME name, const std::string& data, size_t size)
    {
        assert(data.size() <= size);
        startSubRecord(name);
        writeHString(data);

        if (data.size() < size)
        {
            for (size_t i = data.size(); i < size; ++i)
                write("\0", 1);
        }

        endRecord(name);
    }

    void ESMWriter::writeHNRefId(NAME name, const RefId& value)
    {
        writeHNString(name, value.getRefIdString());
    }

    void ESMWriter::writeHNRefId(NAME name, const RefId& value, std::size_t size)
    {
        writeHNString(name, value.getRefIdString(), size);
    }

    void ESMWriter::writeMaybeFixedSizeString(const std::string& data, std::size_t size)
    {
        std::string string;
        if (!data.empty())
            string = mEncoder ? mEncoder->getLegacyEnc(data) : data;
        if (mHeader.mFormatVersion <= MaxLimitedSizeStringsFormatVersion)
        {
            if (string.size() > size)
                throw std::runtime_error("Fixed string data is too long: \"" + string + "\" ("
                    + std::to_string(string.size()) + " > " + std::to_string(size) + ")");
            string.resize(size);
        }
        else
        {
            constexpr StringSizeType maxSize = std::numeric_limits<StringSizeType>::max();
            if (string.size() > maxSize)
                throw std::runtime_error("String size is too long: \"" + string.substr(0, 64) + "<...>\" ("
                    + std::to_string(string.size()) + " > " + std::to_string(maxSize) + ")");
            writeT(static_cast<StringSizeType>(string.size()));
        }
        write(string.c_str(), string.size());
    }

    void ESMWriter::writeHString(const std::string& data)
    {
        if (data.size() == 0)
            write("\0", 1);
        else
        {
            // Convert to UTF8 and return
            const std::string_view string = mEncoder != nullptr ? mEncoder->getLegacyEnc(data) : data;

            write(string.data(), string.size());
        }
    }

    void ESMWriter::writeHCString(const std::string& data)
    {
        writeHString(data);
        if (data.size() > 0 && data[data.size() - 1] != '\0')
            write("\0", 1);
    }

    void ESMWriter::writeMaybeFixedSizeRefId(const RefId& value, std::size_t size)
    {
        writeMaybeFixedSizeString(value.getRefIdString(), size);
    }

    void ESMWriter::writeHRefId(const RefId& value)
    {
        writeHString(value.getRefIdString());
    }

    void ESMWriter::writeHCRefId(const RefId& value)
    {
        writeHCString(value.getRefIdString());
    }

    void ESMWriter::writeName(NAME name)
    {
        write(name.mData, NAME::sCapacity);
    }

    void ESMWriter::write(const char* data, size_t size)
    {
        if (mCounting && !mRecords.empty())
        {
            for (std::list<RecordData>::iterator it = mRecords.begin(); it != mRecords.end(); ++it)
                it->size += static_cast<uint32_t>(size);
        }

        mStream->write(data, size);
    }

    void ESMWriter::setEncoder(ToUTF8::Utf8Encoder* encoder)
    {
        mEncoder = encoder;
    }
}
