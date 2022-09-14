#ifndef ESMLOADER_HPP
#define ESMLOADER_HPP

#include <optional>

#include "contentloader.hpp"

namespace ToUTF8
{
  class Utf8Encoder;
    class StatelessUtf8Encoder;
}

namespace ESM
{
    class ReadersCache;
    struct Dialogue;
}

namespace ESM4
{
    struct Dialogue;
}

namespace MWWorld
{

class ESMStore;

struct EsmLoader : public ContentLoader
{
    explicit EsmLoader(MWWorld::ESMStore& store, ESM::ReadersCache& readers, ToUTF8::Utf8Encoder* encoder, ToUTF8::StatelessUtf8Encoder* stateless, std::vector<int>& esmVersions);

    std::optional<int> getMasterFileFormat() const { return mMasterFileFormat; }

    void load(const boost::filesystem::path& filepath, int& index, Loading::Listener* listener) override;

    private:
        ESM::ReadersCache& mReaders;
        MWWorld::ESMStore& mStore;
        ToUTF8::Utf8Encoder* mEncoder;
        ToUTF8::StatelessUtf8Encoder* mStatelessEncoder;
        ESM::Dialogue* mDialogue;
        ESM4::Dialogue* mDialogue4;
        std::optional<int> mMasterFileFormat;
        std::vector<int>& mESMVersions;
};

} /* namespace MWWorld */

#endif // ESMLOADER_HPP
