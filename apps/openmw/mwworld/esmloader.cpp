#include "esmloader.hpp"
#include "esmstore.hpp"

#include <components/esm3/esmreader.hpp>
#include <components/esm3/readerscache.hpp>

#include <components/esm4/reader.hpp>
#include <components/esm4/records.hpp>

namespace MWWorld
{

EsmLoader::EsmLoader(MWWorld::ESMStore& store, ESM::ReadersCache& readers, ToUTF8::Utf8Encoder* encoder, std::vector<int>& esmVersions)
    : mReaders(readers)
    , mStore(store)
    , mEncoder(encoder)
    , mDialogue(nullptr) // A content file containing INFO records without a DIAL record appends them to the previous file's dialogue
    , mESMVersions(esmVersions)
    , mDialogue4(nullptr) // A content file containing INFO records without a DIAL record appends them to the previous file's dialogue
{
}

void EsmLoader::load(const boost::filesystem::path& filepath, int& index, Loading::Listener* listener)
{

    try
    {
        const ESM::ReadersCache::BusyItem reader = mReaders.get(static_cast<std::size_t>(index));
        reader->setEncoder(mEncoder);
        reader->setIndex(index);
        reader->open(filepath.string());
        reader->resolveParentFileIndices(mReaders);

        assert(reader->getGameFiles().size() == reader->getParentFileIndices().size());
        for (std::size_t i = 0, n = reader->getParentFileIndices().size(); i < n; ++i)
            if (i == static_cast<std::size_t>(reader->getIndex()))
                throw std::runtime_error("File " + reader->getName() + " asks for parent file "
                    + reader->getGameFiles()[i].name
                    + ", but it is not available or has been loaded in the wrong order. "
                      "Please run the launcher to fix this issue.");

        mESMVersions[index] = reader->getVer();
        mStore.load(*reader, listener, mDialogue);

        if (!mMasterFileFormat.has_value() && (Misc::StringUtils::ciEndsWith(reader->getName(), ".esm") || Misc::StringUtils::ciEndsWith(reader->getName(), ".omwgame")))
            mMasterFileFormat = reader->getFormat();
    }
    catch (std::exception e)
    {
        // loading ESM3 failed, it might be an esm4, try again
        ESM4::Reader::sReadersCache[index] = ESM::Reader::getReader(filepath.string());
        ESM4::Reader& esm4 = *(static_cast<ESM4::Reader*>(ESM4::Reader::sReadersCache[index]));
        esm4.setEncoder(mEncoder);
        esm4.setIndex(index);
        esm4.updateModIndices(ESM4::Reader::sFilenames);

        assert(esm4.getContext().parentFileIndices.size() == esm4.getGameFiles().size());
        for (std::size_t i = 0, n = esm4.getGameFiles().size(); i < n; ++i)
            if (i == static_cast<std::size_t>(esm4.getIndex()))
                throw std::runtime_error("File " + esm4.getFileName() + " asks for parent file "
                    + esm4.getGameFiles()[i].name
                    + ", but it is not available or has been loaded in the wrong order. "
                      "Please run the launcher to fix this issue.");
        mStore.load(esm4, listener, mDialogue4);

        if (!mMasterFileFormat.has_value() && (Misc::StringUtils::ciEndsWith(esm4.getFileName(), ".esm") || Misc::StringUtils::ciEndsWith(esm4.getFileName(), ".omwgame")))
            mMasterFileFormat = esm4.getFormat();
    }
}

} /* namespace MWWorld */
