#include "dialutils.hpp"

namespace F3Dialogue
{
    std::string getDialogueFile(const VFS::Manager* vfs, std::string esmName, std::string voiceType, 
                                      ESM4::FormId infoId, std::string extension)
    {
         Misc::StringUtils::lowerCaseInPlace(esmName);
         Misc::StringUtils::lowerCaseInPlace(voiceType);
         std::string idString = Misc::StringUtils::lowerCase(ESM4::formIdToString(infoId));
         
         boost::filesystem::path dialFolder = boost::filesystem::path("sound") / "voice" / esmName / voiceType;
         

         const auto& filePath = vfs->getRecursiveDirectoryIterator(dialFolder.string());

         for (const auto& name : filePath)
         {
            std::string lower = Misc::StringUtils::lowerCase(name);
            if (lower.find(idString) != std::string::npos &&
                lower.find(extension) != std::string::npos)
            {
                return lower;
            }
         }
         return "";
    }


}
