#ifndef GAME_BASE_ENVIRONMENT_H
#define GAME_BASE_ENVIRONMENT_H

#include <components/misc/notnullptr.hpp>

#include <memory>

namespace osg
{
    class Stats;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWBase
{
    class World;
    class ScriptManager;
    class DialogueManager;
    class Journal;
    class TopiclessJournal;
    class SoundManager;
    class MechanicsManager;
    class InputManager;
    class WindowManager;
    class StateManager;
    class LuaManager;

    /// \brief Central hub for mw-subsystems
    ///
    /// This class allows each mw-subsystem to access any others subsystem's top-level manager class.
    ///
    class Environment
    {
            static Environment *sThis;

            World* mWorld = nullptr;
            SoundManager* mSoundManager = nullptr;
            ScriptManager* mScriptManager = nullptr;
            ScriptManager* mTES4ScriptManager = nullptr; // have a separate script manager for tes4/fo3/fnv scripts
            WindowManager* mWindowManager = nullptr;
            MechanicsManager* mMechanicsManager = nullptr;
            std::map<std::string, MechanicsManager*> mOtherMechanicsManagers; // string map for getting mechanics managers from other games (I hate this)
            DialogueManager* mDialogueManager = nullptr;
            std::map<std::string, DialogueManager*> mOtherDialogueManagers; // string map for getting dialogue managers from other games (I hate this)
            std::map<std::string, TopiclessJournal*> mTopiclessJournals;
            Journal* mJournal = nullptr;
            InputManager* mInputManager = nullptr;
            StateManager* mStateManager = nullptr;
            LuaManager* mLuaManager = nullptr;
            Resource::ResourceSystem* mResourceSystem = nullptr;
            float mFrameRateLimit = 0;
            float mFrameDuration = 0;

        public:

            Environment();

            ~Environment();

            Environment(const Environment&) = delete;

            Environment& operator=(const Environment&) = delete;

            void setWorld(World& value) { mWorld = &value; }

            void setSoundManager(SoundManager& value) { mSoundManager = &value; }

            void setScriptManager(ScriptManager& value) { mScriptManager = &value; }

            void setTes4ScriptManager(ScriptManager& value) { mTES4ScriptManager = &value; }

            void setWindowManager(WindowManager& value) { mWindowManager = &value; }

            void setMechanicsManager(MechanicsManager& value) { mMechanicsManager = &value; }

            void setMechanicsManager(const std::string& game, MechanicsManager& value) { mOtherMechanicsManagers[game] = &value; }

            void setDialogueManager(DialogueManager& value) { mDialogueManager = &value; }

            void setDialogueManager(const std::string& game, DialogueManager& value) { mOtherDialogueManagers[game] = &value; }

            void setJournal(Journal& value) { mJournal = &value; }

            void setJournal(const std::string& game, TopiclessJournal& journal) { mTopiclessJournals[game] = &journal; }

            void setInputManager(InputManager& value) { mInputManager = &value; }

            void setStateManager(StateManager& value) { mStateManager = &value; }

            void setLuaManager(LuaManager& value) { mLuaManager = &value; }

            void setResourceSystem(Resource::ResourceSystem& value) { mResourceSystem = &value; }

            Misc::NotNullPtr<World> getWorld() const { return mWorld; }

            Misc::NotNullPtr<SoundManager> getSoundManager() const { return mSoundManager; }

            Misc::NotNullPtr<ScriptManager> getScriptManager() const { return mScriptManager; }

            Misc::NotNullPtr<ScriptManager> getTes4ScriptManager() const { return mTES4ScriptManager; }

            Misc::NotNullPtr<WindowManager> getWindowManager() const { return mWindowManager; }

            Misc::NotNullPtr<MechanicsManager> getMechanicsManager() const { return mMechanicsManager; }

            Misc::NotNullPtr<MechanicsManager> getMechanicsManager(const std::string& game) const { return mOtherMechanicsManagers.at(game); }

            Misc::NotNullPtr<DialogueManager> getDialogueManager() const { return mDialogueManager; }

            Misc::NotNullPtr<DialogueManager> getDialogueManager(const std::string& game) const { return mOtherDialogueManagers.at(game); }

            Misc::NotNullPtr<Journal> getJournal() const { return mJournal; }

            Misc::NotNullPtr<TopiclessJournal> getTopiclessJournal(const std::string& game) const { return mTopiclessJournals.at(game); }

            Misc::NotNullPtr<InputManager> getInputManager() const { return mInputManager; }

            Misc::NotNullPtr<StateManager> getStateManager() const { return mStateManager; }

            Misc::NotNullPtr<LuaManager> getLuaManager() const { return mLuaManager; }

            Misc::NotNullPtr<Resource::ResourceSystem> getResourceSystem() const { return mResourceSystem; }

            float getFrameRateLimit() const { return mFrameRateLimit; }

            void setFrameRateLimit(float value) { mFrameRateLimit = value; }

            float getFrameDuration() const { return mFrameDuration; }

            void setFrameDuration(float value) { mFrameDuration = value; }

            /// Return instance of this class.
            static const Environment& get()
            {
                assert(sThis != nullptr);
                return *sThis;
            }

            void reportStats(unsigned int frameNumber, osg::Stats& stats) const;
    };
}

#endif
