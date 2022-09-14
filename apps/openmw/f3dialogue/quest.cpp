#include "quest.hpp"
#include "dialoguemanagerimp.hpp"

#include "../mwworld/worldimp.hpp"
#include "../mwbase/environment.hpp"
#include "../mwworld/store.hpp"
#include "../mwworld/esmstore.hpp"

#include "../f3script/scriptmanagerimp.hpp"
#include "../f3script/interpretercontext.hpp"

namespace F3Dialogue
{
    Quest::Quest(const ESM4::Quest* qust)
    {
        mName = qust->mQuestName;
        mStage = 0;
        mState = QuestState::Inactive;
        mForm = qust;
        mScript = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().search(qust->mQuestScript);
    }
    std::string_view Quest::getName() const
    {
        return mName;
    }
    QuestState Quest::getState() const
    {
        return mState;
    }
    int Quest::getStage() const
    {
        return mStage;
    }
    void Quest::setStage(int stage) const
    {
        if (mState == QuestState::Inactive)
            mState = QuestState::Active;
        mStage = stage;
        for (auto& qst : mForm->mStages)
        {
            if (qst.mIndex == stage)
            {
                for (auto& entry : qst.mEntries)
                {
                    if ((entry.mStageFlags & 1) != 0)
                    {
                        mState = QuestState::Completed;
                    }
                    if ((entry.mStageFlags & 2) != 0)
                    {
                        mState = QuestState::Failed;
                    }
                    ESM4::Script scpt;
                    scpt.mScript = entry.mEmbeddedScript;
                    const ESM4::Script* rec = MWBase::Environment::get().getWorld()->createRecord(scpt);
                    FOScript::Locals scriptLocals;
                    scriptLocals.configure(*rec);
                    FOScript::InterpreterContext interpreterContext(&scriptLocals, MWWorld::Ptr());
                    MWBase::Environment::get().getTes4ScriptManager()->run(rec->mEditorId, interpreterContext);
                }
                break;
            }
        }
    }
    const ESM4::Quest* Quest::getRecord() const
    {
        return mForm;
    }
    std::vector<ESM4::QuestStage::LogEntry>::const_iterator Quest::logEntriesBegin() const
    {
        for (auto& stage : mForm->mStages)
        {
            if (stage.mIndex == getStage())
            {
                return stage.mEntries.begin();
            }
        }
        throw std::runtime_error(std::string("Could not get log entry iterator for '") + std::string(getName()) + "'. Stage index could not be resolved.");
    }
    std::vector<ESM4::QuestStage::LogEntry>::const_iterator Quest::logEntriesEnd() const
    {
        for (auto& stage : mForm->mStages)
        {
            if (stage.mIndex == getStage())
            {
                return stage.mEntries.end();
            }
        }
        throw std::runtime_error(std::string("Could not get log entry iterator for '") + std::string(getName()) + "'. Stage index could not be resolved.");
    }
    std::vector<ESM4::QuestObjective>::const_iterator Quest::objectivesBegin() const
    {
        return mForm->mObjectives.begin();
    }
    std::vector<ESM4::QuestObjective>::const_iterator Quest::objectivesEnd() const
    {
        return mForm->mObjectives.end();
    }
}
