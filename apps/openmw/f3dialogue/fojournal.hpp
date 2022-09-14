#ifndef GAME_F3DIALOG_JOURNAL_H
#define GAME_F3DIALOG_JOURNAL_H

#include "../mwbase/journal.hpp"

#include "quest.hpp"

namespace F3Dialogue
{
    class TopiclessJournal : public MWBase::TopiclessJournal
    {
        TQuestContainer mQuests;

    public:
        TopiclessJournal();

        TQuestIter begin() override;

        TQuestIter end() override;

        TQuestConstIter begin() const override;
        TQuestConstIter end() const override;

        const F3Dialogue::Quest& getQuest(const std::string& id) const override;
        const F3Dialogue::Quest& getQuest(ESM4::FormId id) const override;

        std::vector<ESM4::FormId> revealObjectives(ESM4::FormId questId, const std::vector<uint32_t>& objectives) override;
    };
}

#endif
