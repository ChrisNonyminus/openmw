#ifndef F3DIALOGUE_QUEST_H
#define F3DIALOGUE_QUEST_H

#include <string>
#include <sstream>

#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>

#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadinfo.hpp>
#include <components/esm4/loadqust.hpp>
#include <components/esm4/loadscpt.hpp>


namespace F3Dialogue
{
    class Journal;
    enum class QuestState
    {
        Inactive, // not running
        Active, // running; mStage will handle what part of the quest we're in
        Completed, // done!
        Failed, // womp womp
    };
    class Quest
    {
        mutable QuestState mState; // the state of the quest
        mutable int mStage; // the individual stage of the quest when it's active

        std::string mName; // name of the quest

        const ESM4::Quest* mForm; // esm4 form for the source quest
        const ESM4::Script* mScript; // esm4 form for the source quest's script
    public:
        Quest(const ESM4::Quest* qust);

        std::string_view getName() const;

        QuestState getState() const;

        int getStage() const;
        void setStage(int stage) const;
        
        const ESM4::Quest* getRecord() const;

        std::vector<ESM4::QuestStage::LogEntry>::const_iterator logEntriesBegin() const;
        std::vector<ESM4::QuestStage::LogEntry>::const_iterator logEntriesEnd() const;

        std::vector<ESM4::QuestObjective>::const_iterator objectivesBegin() const;
        std::vector<ESM4::QuestObjective>::const_iterator objectivesEnd() const;
    };
}

#endif
