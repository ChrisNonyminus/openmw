#ifndef F3DIALOGUE_DIALINFO_H
#define F3DIALOGUE_DIALINFO_H

#include <string>
#include <sstream>

#include <boost/filesystem.hpp>

#include <components/vfs/manager.hpp>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadinfo.hpp>

namespace F3Dialogue
{
    std::string getDialogueFile(const VFS::Manager* vfs, std::string esmName, std::string voiceType, 
                                      ESM4::FormId infoId, std::string extension);


}

#endif
