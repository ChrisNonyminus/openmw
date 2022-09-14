#ifndef GAME_F3DIALOG_DIALOGUEMANAGERIMP_H
#define GAME_F3DIALOG_DIALOGUEMANAGERIMP_H

#include "../mwbase/dialoguemanager.hpp"

#include <map>
#include <set>
#include <unordered_map>

#include <components/compiler/streamerrorhandler.hpp>
#include <components/translation/translation.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/esm4/loaddial.hpp>
#include <components/esm4/loadinfo.hpp>

#include "../mwworld/ptr.hpp"

#include "../mwscript/compilercontext.hpp"

namespace F3Dialogue
{
    class DialogueManager : public MWBase::DialogueManager
    {
        struct TopicInfo
        {
            int mFlags;
            const ESM4::DialogInfo* mInfo;
        };

        std::set<ESM4::FormId> mDialogues; // these are the topics the player knows.

        std::map<ESM4::FormId, std::vector<TopicInfo>> mActorKnownTopics;

        Translation::Storage& mTranslationDataStorage;
        MWScript::CompilerContext mCompilerContext;
        Compiler::StreamErrorHandler mErrorHandler;

        MWWorld::Ptr mActor;
        bool mTalkedTo;

        int mChoice;
        ESM4::FormId mLastTopic; // last topic id
        bool mIsInChoice;
        bool mGoodbye;

        std::vector<std::pair<ESM4::FormId, int>> mChoices;

        int mOriginalDisposition;
        int mCurrentDisposition;
        int mPermanentDispositionChange;

        std::vector<ESM4::FormId> parseInfoIdsFromDialChoice(const ESM4::Dialogue* dial);
        void addTopicsFromInfo(const ESM4::DialogInfo* dial);

        void updateActorKnownTopics();
        void updateGlobals();

        bool compile(const std::string& cmd, std::vector<Interpreter::Type_Code>& code, const MWWorld::Ptr& actor, bool localsOverride = false);
        void executeScript(const std::string& script, const MWWorld::Ptr& actor);

        void executeTopic (ESM4::FormId topic, ResponseCallback* callback);

        bool infoMeetsCriteria(const ESM4::DialogInfo* info, const MWWorld::Ptr & subject, const MWWorld::Ptr& target);

        //void updateOriginalDisposition(); // todo

    public:

            DialogueManager (const Compiler::Extensions& extensions, Translation::Storage& translationDataStorage);

            void clear() override;

            bool isInChoice() const override;

            bool startDialogue (const MWWorld::Ptr& actor, ResponseCallback* callback) override;

            std::list<std::string> getAvailableTopics() override;
            int getTopicFlag(const std::string& topicId) override;

            bool inJournal (const std::string& topicId, const std::string& infoId) override
            {
                return false; //todo
            }

            void addTopic(std::string_view topic) override {} // todo

            void addChoice(std::string_view text,int choice) override {} // todo
            const std::vector<std::pair<std::string, int> >& getChoices() override
            {
                throw std::runtime_error("F3Dialogue::DialogueManager::getChoices() unimplemented");
            } //todo

            bool isGoodbye() override
            {
                return false; //todo
            }

            void goodbye() override
            {
                // todo
            }

            bool checkServiceRefused (ResponseCallback* callback, ServiceType service = ServiceType::Any) override
            {
                return false; // todo
            }

            void say(const MWWorld::Ptr &actor, const std::string &topic) override;

            void sayTo(const MWWorld::Ptr &actor, const MWWorld::Ptr& target, const std::string &topic) override;

            //calbacks for the GUI
            void keywordSelected(const std::string& keyword, ResponseCallback* callback) override {} // todo
            void goodbyeSelected() override {} // todo
            void questionAnswered(int answer, ResponseCallback* callback) override {} // todo

            void persuade (int type, ResponseCallback* callback) override
            {
                // todo
            }

            /// @note Controlled by an option, gets discarded when dialogue ends by default
            void applyBarterDispositionChange (int delta) override
            {
                // todo
            }

            int countSavedGameRecords() const override
            {
                return 0; // todo
            }

            void write (ESM::ESMWriter& writer, Loading::Listener& progress) const override
            {
                // esm3 not supported for esm4 dialogue manager
            }

            void readRecord (ESM::ESMReader& reader, uint32_t type) override
            {
                // esm3 not supported for esm4 dialogue manager
            }

            /// Changes faction1's opinion of faction2 by \a diff.
            void modFactionReaction (std::string_view faction1, std::string_view faction2, int diff) override
            {
                // todo
            }

            void setFactionReaction (std::string_view faction1, std::string_view faction2, int absolute) override
            {
                // todo
            }

            /// @return faction1's opinion of faction2
            int getFactionReaction (std::string_view faction1, std::string_view faction2) const override
            {
                return 0; // todo
            }

            /// Removes the last added topic response for the given actor from the journal
            void clearInfoActor (const MWWorld::Ptr& actor) const override
            {
                // todo
            }
    };
}

#endif
