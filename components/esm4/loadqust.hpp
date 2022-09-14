/*
  Copyright (C) 2020-2021 cc9cii

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

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#ifndef ESM4_QUST_H
#define ESM4_QUST_H

#include <cstdint>

#include <components/esm/defs.hpp>

#include "formid.hpp"
#include "script.hpp" // TargetCondition, ScriptDefinition

namespace ESM4
{
    class Reader;
    class Writer;

#pragma pack(push, 1)
    struct QuestData
    {
        std::uint8_t flags;    // Quest_Flags
        std::uint8_t priority;
        std::uint16_t padding; // FO3
        float questDelay;      // FO3
    };
    struct QuestTarget
    {
        FormId target;
        uint8_t flags; // 1: compass marker ignores locks
        uint8_t unused[3];
    };
#pragma pack(pop)

    struct QuestStage
    {
        int16_t mIndex;
        struct LogEntry
        {
            uint8_t mStageFlags; // 1: completed | 2: failed
            std::vector<TargetCondition> mConditions;
            std::string mEntry;
            ScriptDefinition mEmbeddedScript;
            FormId mNextQuest;
            void clear()
            {
                mStageFlags = 0;
                mConditions.clear();
                mEntry.clear();
                mEmbeddedScript.scriptSource.clear();
                mEmbeddedScript.localVarData.clear();
                mNextQuest = 0;
            }
        };
        std::vector<LogEntry> mEntries;
        void clear()
        {
            mIndex = 0; 
            mEntries.clear();
        }
    };

    struct QuestObjective
    {
        int32_t mIndex;
        std::string mDescription;

        struct Target
        {
            QuestTarget mTarget;
            std::vector<TargetCondition> mConditions;
            void clear()
            {
                mConditions.clear();
            }
        };
        std::vector<Target> mTargets;
        void clear()
        {
            mIndex = 0;
            mDescription.clear();
            mTargets.clear();
        }
    };

    struct Quest
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_QUST4;
        static std::string getRecordType() { return "Quest (TES4)"; }
        // NOTE: these values are for TES4
        enum Quest_Flags
        {
            Flag_StartGameEnabled     = 0x01,
            Flag_AllowRepeatConvTopic = 0x04,
            Flag_AllowRepeatStages    = 0x08
        };

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        std::string mQuestName;
        std::string mFileName; // texture file
        FormId mQuestScript;

        QuestData mData;

        std::vector<TargetCondition> mTargetConditions;

        std::vector<QuestStage> mStages;
        std::vector<QuestObjective> mObjectives;

        ScriptDefinition mScript;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& writer) const;

        //void blank();
    };
}

#endif // ESM4_QUST_H
