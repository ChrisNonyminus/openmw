#include <sstream>

#include <components/focompiler/scanner.hpp>
#include <components/compiler/context.hpp>
#include <components/compiler/locals.hpp>
#include <components/compiler/exception.hpp>

#include <components/debug/debuglog.hpp>

#include <components/focompiler/extensions0.hpp>

#include <components/resource/resourcesystem.hpp>
#include <components/vfs/manager.hpp>

#include <components/esm4/records.hpp>

#include "dialoguemanagerimp.hpp"

#include "../f3script/scriptmanagerimp.hpp"
#include "../f3script/interpretercontext.hpp"
#include "../f3script/extensions.hpp"

#include "../f3mechanics/stats.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/soundmanager.hpp"

#include "../mwworld/esmstore.hpp"
#include "../mwworld/store.hpp"

#include "../mwworld/class.hpp"

#include "dialutils.hpp"


std::vector<ESM4::FormId> F3Dialogue::DialogueManager::parseInfoIdsFromDialChoice(const ESM4::Dialogue *dial)
{
    std::vector<ESM4::FormId> infos;
    const auto& store = MWBase::Environment::get().getWorld()->getStore().get<ESM4::DialogInfo>();

    for (auto& info : store)
    {
        if (info.mParentTopic == dial->mFormId)
            infos.push_back(info.mFormId);
    }
    return infos;
}

void F3Dialogue::DialogueManager::addTopicsFromInfo(const ESM4::DialogInfo *dial)
{
    updateActorKnownTopics();
    for (const auto& topicId : dial->mChoices)
    {
        if (mActorKnownTopics.count(topicId))
            mDialogues.insert(topicId);
    }
}

bool actorHasTopic(const MWWorld::Ptr& actor, const ESM4::DialogInfo* info)
{
    ESM4::FormId formId = actor.getClass().getFormId(actor);
    if (info->mSpeaker == formId)
    {
        return true;
    }
    const auto* quest = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().find(info->mQuest);
    if (actor.getClass().isNpc())
    {
        if (const ESM4::Npc* asNpc = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Npc>().search(formId))
        {
            for (auto& cond : quest->mTargetConditions)
            {
                if (cond.functionIndex == ESM4::FUN_GetIsVoiceType)
                {
                    if (cond.condition == ESM4::ConditionTypeAndFlag::CTF_EqualTo)
                    {
                        if (asNpc->mVoice == cond.param1)
                        {
                            return true;
                        }
                    }
                }

            }
        }
    }
    else
    {
        if (const ESM4::Creature* asCreature = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Creature>().search(formId))
        {
            for (auto& cond : quest->mTargetConditions)
            {
                if (cond.functionIndex == ESM4::FUN_GetIsVoiceType)
                {
                    if (cond.condition == ESM4::ConditionTypeAndFlag::CTF_EqualTo)
                    {
                        if (asCreature->mVoice == cond.param1)
                        {
                            return true;
                        }
                    }
                }
            }
        } 
    }
    return false;
}



std::string getVoiceType(const MWWorld::Ptr& actor, const ESM4::DialogInfo* info)
{
    ESM4::FormId formId = actor.getClass().getFormId(actor);
    if (actor.getClass().isNpc())
    {
        if (const ESM4::Npc* asNpc = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Npc>().search(formId))
        {
            return MWBase::Environment::get().getWorld()->getStore().get<ESM4::VoiceType>().find(asNpc->mVoice)->mEditorId;
        }
    }
    else
    {
        if (const ESM4::Creature* asCreature = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Creature>().search(formId))
        {
            return MWBase::Environment::get().getWorld()->getStore().get<ESM4::VoiceType>().find(asCreature->mVoice)->mEditorId;
        } 
    }
    return "";
}

void F3Dialogue::DialogueManager::updateActorKnownTopics()
{
    updateGlobals();

    mActorKnownTopics.clear();

    const auto& dialogs = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Dialogue>();
    const auto& infos = MWBase::Environment::get().getWorld()->getStore().get<ESM4::DialogInfo>();

    for (const auto& dialog : dialogs)
    {
        if (dialog.mDialType == ESM4::DialType::DTYP_Topic || dialog.mDialType == ESM4::DTYP_Persuation)
        {
            for (const auto& response : parseInfoIdsFromDialChoice(&dialog))
            {
                if (const auto* info = infos.search(response))
                {
                    if (actorHasTopic(mActor, info))
                    {
                        if (!mActorKnownTopics.count(dialog.mFormId))
                        {
                            mActorKnownTopics[dialog.mFormId] = std::vector<TopicInfo>();
                        }
                        mActorKnownTopics[dialog.mFormId].push_back(TopicInfo{0, info});
                    }
                }
            }
        }
    }
}

void F3Dialogue::DialogueManager::updateGlobals()
{
    MWBase::Environment::get().getWorld()->updateDialogueGlobals();
}

bool F3Dialogue::DialogueManager::compile(const std::string& cmd, std::vector<ESM4::FormId>& code, const MWWorld::Ptr& actor, bool localsOverride)
{
    bool success = true;
    try 
    {
        mErrorHandler.reset();
        mErrorHandler.setContext("[dialogue script]");

        std::istringstream input (cmd + "\n");

        FOCompiler::Scanner scanner(mErrorHandler, input, mCompilerContext.getExtensions());

        Compiler::Locals locals;

        std::string_view actorScript = actor.getClass().getScript(actor);

        if(!actorScript.empty())
        {
            locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(actorScript);
        }

        FOCompiler::ScriptParser parser(mErrorHandler, mCompilerContext, locals, false);

        scanner.scan(parser);

        if (!mErrorHandler.isGood())
            success = false;

        if (success)
            parser.getCode (code);
    }
    catch (const Compiler::SourceException& /* error */)
    {
        // error has already been reported via error handler
        success = false;
    }
    catch (const std::exception& error)
    {
        Log(Debug::Error) << std::string ("Dialogue error: An exception has been thrown: ") + error.what();
        success = false;
    }

    if (!success)
    {
        Log(Debug::Error) << "Error: compiling failed (dialogue script): \n" << cmd << "\n";
    }

    return success;
}

void F3Dialogue::DialogueManager::executeScript(const std::string &script, const MWWorld::Ptr &actor)
{
    std::vector<Interpreter::Type_Code> code;
    if(compile(script, code, actor))
    {
        ESM4::Script scpt;
        scpt.mScript.scriptSource = script;
        const ESM4::Script* dialScript = MWBase::Environment::get().getWorld()->createRecord(scpt); // todo: remove the script record once it's done being used
        try
        {
            FOScript::Locals scriptLocals;
            scriptLocals.configure(*dialScript);
            FOScript::InterpreterContext interpreterContext(&scriptLocals, actor);
            Interpreter::Interpreter interpreter;
            FOScript::installOpcodes (interpreter);
            interpreter.run (code.data(), code.size(), interpreterContext);
        }
        catch (const std::exception& error)
        {
            Log(Debug::Error) << std::string ("Dialogue error: An exception has been thrown: ") + error.what();
        }
    }
}

bool F3Dialogue::DialogueManager::infoMeetsCriteria(const ESM4::DialogInfo* info, const MWWorld::Ptr & subject, const MWWorld::Ptr& target)
{
    ESM4::FormId formId = subject.getCellRef().getRefr().mBaseObj;
    const auto* quest = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().find(info->mQuest);

    bool condsMet = false;
    
    if (/*Misc::StringUtils::ciStartsWith(quest->mEditorId, "dialogue") || Misc::StringUtils::ciStartsWith(quest->mEditorId, "conv") || Misc::StringUtils::ciStartsWith(quest->mEditorId, "generic") || Misc::StringUtils::ciStartsWith(quest->mEditorId, "radio") || Misc::StringUtils::ciStartsWith(quest->mEditorId, "daddialogue")*/true) // if it's a dialogue/conversation/voice/radio-handling quest, check its conditions
    {
        for (auto& cond : quest->mTargetConditions)
        {
            if (!FOCompiler::sFunctionIndices.count(ESM4::FunctionIndices(cond.functionIndex)))
            {
                std::stringstream err;
                err << "dialogue info '" << info->mEditorId << "' had a condition with an unhandled function: " << cond.functionIndex;
                Log(Debug::Error) << err.str();
                return false;
            }
            std::vector<Interpreter::Type_Code> code;
            std::stringstream script;
            bool runOnTarget = false;
            switch (cond.runOn)
            {
                case 0: break;
                case 1: runOnTarget = true; break;
                default: Log(Debug::Error) << ("Unhandled runOn in dialogue info '" + info->mEditorId + "': ") << cond.runOn; return false;
            }
            script << "float ConditionReturnValue\n";
            script << "set ConditionReturnValue to " << FOCompiler::sFunctionIndices[ESM4::FunctionIndices(cond.functionIndex)];
            if (cond.param1)
            {
                try
                {
                    script << " " << ESM4::formIdToString(cond.param1);
                }
                catch (std::exception e)
                {
                    // probably not a form id, but we don't know the parameter type :(
                    script << cond.param1;
                }
            }
            if (cond.param2)
            {
                if (cond.functionIndex == ESM4::FUN_GetQuestVariable)
                    script << " " << (MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find(quest->mQuestScript)->mScript.localVarData[cond.param2]).variableName;
                else
                {
                    try
                    {
                        script << " " << ESM4::formIdToString(cond.param2);
                    }
                    catch (std::exception e)
                    {
                        // probably not a form id, but we don't know the parameter type :(

                        script << cond.param2;
                    }
                }
            }
            if (compile(script.str(), code, (runOnTarget ? target : subject), true))
            {
                try
                {
                    FOScript::Locals locals;
                    locals.mFloats.resize(1);
                    FOScript::InterpreterContext interpreterContext(&locals, (runOnTarget ? target : subject));
                    Interpreter::Interpreter interpreter;
                    FOScript::installOpcodes(interpreter);
                    interpreter.run(code.data(), code.size(), interpreterContext);
                    float value = interpreterContext.getLocalFloat(0);
                    /*Log(Debug::Info) << "Dialogue Info '" << info->mEditorId << "' had the following condition:\n\tCondition: " << cond.condition
                                     << "\n\tComparison Value: " << cond.comparison << "\n\tFunction: \n\n"
                                     << script.str() << "\n\n\tReturned value: " << value;*/
                    switch ((cond.condition & ~0x01))
                    {
                        case ESM4::CTF_EqualTo:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value == cond.comparison);
                            else condsMet &= (value == cond.comparison);
                            break;
                        }
                        case ESM4::CTF_NotEqualTo:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value != cond.comparison);
                            else
                                condsMet &= (value != cond.comparison);
                            break;
                        }
                        case ESM4::CTF_GreaterThan:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value > cond.comparison);
                            else
                                condsMet &= (value > cond.comparison);
                            break;
                        }
                        case ESM4::CTF_GrThOrEqTo:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value >= cond.comparison);
                            else
                                condsMet &= (value >= cond.comparison);
                            break;
                        }
                        case ESM4::CTF_LessThan:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value < cond.comparison);
                            else
                                condsMet &= (value < cond.comparison);
                            break;
                        }
                        case ESM4::CTF_LeThOrEqTo:
                        {
                            if ((cond.condition & ESM4::CTF_Combine) != 0)
                                condsMet |= (value <= cond.comparison);
                            else
                                condsMet &= (value <= cond.comparison);
                            break;
                        }
                        default:
                            condsMet = false;
                            break;
                    }
                    if ((cond.condition & ESM4::CTF_Combine) != 0 && condsMet)
                    {
                        break;
                    }
                }
                catch (const std::exception& error)
                {
                    Log(Debug::Error) << std::string("Dialogue error: An exception has been thrown: ") + error.what();
                    condsMet = false;
                }
            }
        }
    
    }

    if (!condsMet)
        return false;
    for (auto& cond : info->mTargetConditions)
    {
        if (!FOCompiler::sFunctionIndices.count(ESM4::FunctionIndices(cond.functionIndex)))
        {
            std::stringstream err;
            err << "dialogue info '" << info->mEditorId << "' had a condition with an unhandled function: " << cond.functionIndex;
            Log(Debug::Error) << err.str();
            return false;
        }
        std::vector<Interpreter::Type_Code> code;
        std::stringstream script;
        bool runOnTarget = false;
        switch (cond.runOn)
        {
            case 0: break;
            case 1: runOnTarget = true; break;
            default: Log(Debug::Error) << ("Unhandled runOn in dialogue info '" + info->mEditorId + "': ") << cond.runOn; return false;
        }
        script << "float ConditionReturnValue\n";
        script << "set ConditionReturnValue to " << FOCompiler::sFunctionIndices[ESM4::FunctionIndices(cond.functionIndex)];
        if (cond.param1)
        {
            try
            {
                script << " " << ESM4::formIdToString(cond.param1);
            }
            catch (std::exception e)
            {
                // probably not a form id, but we don't know the parameter type :(
                script << cond.param1;
            }
        }
        if (cond.param2)
        {
            if (cond.functionIndex == ESM4::FUN_GetQuestVariable)
                script << " " << (MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find((MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().find(cond.param1))->mQuestScript)->mScript.localVarData[cond.param2]).variableName;
            else
            {
                try
                {
                    script << " " << ESM4::formIdToString(cond.param2);
                }
                catch (std::exception e)
                {
                    // probably not a form id, but we don't know the parameter type :(
                    script << cond.param2;
                }
            }
        }
        if (compile(script.str(), code, (runOnTarget ? target : subject)))
        {
            try
            {
                FOScript::Locals locals;
                locals.mFloats.resize(1);
                FOScript::InterpreterContext interpreterContext(&locals, (runOnTarget ? target : subject));
                Interpreter::Interpreter interpreter;
                FOScript::installOpcodes (interpreter);
                interpreter.run(code.data(), code.size(), interpreterContext);
                float value = interpreterContext.getLocalFloat(0);
                /*Log(Debug::Info) << "Dialogue Info '" << info->mEditorId << "' had the following condition:\n\tCondition: " << cond.condition
                                 << "\n\tComparison Value: " << cond.comparison << "\n\tFunction: \n\n"
                                 << script.str() << "\n\n\tReturned value: " << value;*/
                switch ((cond.condition & ~0x01))
                {
                    case ESM4::CTF_EqualTo:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value == cond.comparison);
                        else
                            condsMet &= (value == cond.comparison);
                        break;
                    }
                    case ESM4::CTF_NotEqualTo:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value != cond.comparison);
                        else
                            condsMet &= (value != cond.comparison);
                        break;
                    }
                    case ESM4::CTF_GreaterThan:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value > cond.comparison);
                        else
                            condsMet &= (value > cond.comparison);
                        break;
                    }
                    case ESM4::CTF_GrThOrEqTo:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value >= cond.comparison);
                        else
                            condsMet &= (value >= cond.comparison);
                        break;
                    }
                    case ESM4::CTF_LessThan:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value < cond.comparison);
                        else
                            condsMet &= (value < cond.comparison);
                        break;
                    }
                    case ESM4::CTF_LeThOrEqTo:
                    {
                        if ((cond.condition & ESM4::CTF_Combine) != 0)
                            condsMet |= (value <= cond.comparison);
                        else
                            condsMet &= (value <= cond.comparison);
                        break;
                    }
                    default:
                        condsMet = false;
                        break;
                }
                if ((cond.condition & ESM4::CTF_Combine) != 0 && condsMet)
                {
                    break;
                }
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << std::string ("Dialogue error: An exception has been thrown: ") + error.what();
                condsMet = false;
            }
        }
    }
    return condsMet;
}

void F3Dialogue::DialogueManager::executeTopic(ESM4::FormId topic, MWBase::DialogueManager::ResponseCallback *callback)
{
    const MWWorld::Store<ESM4::Dialogue>& dialogues = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Dialogue>();

    const ESM4::Dialogue& dialogue = *dialogues.find(topic);

    const ESM4::DialogInfo* info = nullptr;
    for (auto& topicinfo : mActorKnownTopics[topic])
    {
        if (infoMeetsCriteria(info, mActor, MWBase::Environment::get().getWorld()->getPlayerPtr()))
        {
            info = topicinfo.mInfo;
        }
    }
    if (!info)
    {
        return;
    }

    FOScript::InterpreterContext interpreterContext(&mActor.getRefData().getFOLocals(), mActor);
    callback->addResponse(dialogue.mFormId, info->mFormId);

    if (dialogue.mDialType == ESM4::DTYP_Topic)
    {
        // todo: add new topic to journal
    }

    mLastTopic = topic;

    executeScript(info->mScript.scriptSource, mActor);

    addTopicsFromInfo(info);
}

F3Dialogue::DialogueManager::DialogueManager(const Compiler::Extensions &extensions, Translation::Storage &translationDataStorage) :
    mTranslationDataStorage(translationDataStorage)
    , mCompilerContext (MWScript::CompilerContext::Type_Full)
    , mErrorHandler()
    , mTalkedTo(false)
    , mOriginalDisposition(0)
    , mCurrentDisposition(0)
    , mPermanentDispositionChange(0)
{
    mChoice = -1;
    mIsInChoice = false;
    mGoodbye = false;
    mCompilerContext.setExtensions (&extensions);
}

void F3Dialogue::DialogueManager::clear()
{
    mDialogues.clear();
    mTalkedTo = false;
    mOriginalDisposition = 0;
    mCurrentDisposition = 0;
    mPermanentDispositionChange = 0;
}

bool F3Dialogue::DialogueManager::isInChoice() const
{
    return mIsInChoice;
}

bool F3Dialogue::DialogueManager::startDialogue(const MWWorld::Ptr &actor, MWBase::DialogueManager::ResponseCallback *callback)
{
    updateGlobals();
    if (actor.getClass().getFOStats(actor).mDead)
        return false;
    mLastTopic = 0;

    mChoice = -1;
    mIsInChoice = false;
    mGoodbye = false;
    mChoices.clear();

    mActor = actor;

    F3Mechanics::Stats& stats = actor.getClass().getFOStats(actor);
    mTalkedTo = stats.mTalkedTo;

    mActorKnownTopics.clear();

    // greeting
    const MWWorld::Store<ESM4::Dialogue>& dialogs = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Dialogue>();

    updateActorKnownTopics();

    for (const auto& dialog : dialogs)
    {
        if (dialog.mTopicName == "greeting" && mActorKnownTopics.count(dialog.mFormId))
        {
            stats.mTalkedTo = true;
            const auto* info = mActorKnownTopics[dialog.mFormId][0].mInfo;
            FOScript::InterpreterContext interpreterContext(&mActor.getRefData().getFOLocals(), mActor);
            callback->addResponse(dialog.mFormId, info->mFormId);
            executeScript(info->mScript.scriptSource, mActor);
            mLastTopic = dialog.mFormId;

            addTopicsFromInfo(info);
            return true;
        }
    }

    return false;
}

std::list<std::string> F3Dialogue::DialogueManager::getAvailableTopics()
{
    updateActorKnownTopics();

    std::list<std::string> topicList;
    for (const auto& [topic, topicInfos] : mActorKnownTopics)
    {
        if (mDialogues.count(topic))
            topicList.push_back(ESM4::formIdToString(topic));
    }

    topicList.sort(Misc::StringUtils::ciLess);
    return topicList;
}

int F3Dialogue::DialogueManager::getTopicFlag(const std::string &topicId)
{
    return mActorKnownTopics[ESM4::stringToFormId(topicId)][0].mFlags;
}

void F3Dialogue::DialogueManager::sayTo(const MWWorld::Ptr &actor, const MWWorld::Ptr& target, const std::string &topic)
{
    MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
    if (sndMgr->sayActive(actor))
    {
        // actor is already saying something.
        return;
    }

    if (actor.getClass().isNpc() && MWBase::Environment::get().getWorld()->isSwimming(actor))
    {
        // npcs don't talk while submerged
        return;
    }

    if (actor.getClass().getFOStats(actor).mKnockdown)
    {
        // unconscious actors cannot speak
        return;
    }

    const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
    const ESM4::Dialogue* dial = store.get<ESM4::Dialogue>().find(topic);

    const F3Mechanics::Stats& stats = actor.getClass().getFOStats(actor);
    for (const auto& info : store.get<ESM4::DialogInfo>())
    {
        if (info.mParentTopic == dial->mFormId &&
            infoMeetsCriteria(&info, actor, target))
        {
            MWBase::WindowManager* winMgr = MWBase::Environment::get().getWindowManager();
            const auto* vfs = MWBase::Environment::get().getResourceSystem()->getVFS();
            std::string soundPath = getDialogueFile(vfs, info.mSrcEsm, getVoiceType(actor, &info), info.mFormId, "ogg");
            const ESM4::Sound* responseSound = store.get<ESM4::Sound>().search(info.mResponseData.sound);
            if (winMgr->getSubtitlesEnabled())
                winMgr->messageBox(info.mResponse);
            if (soundPath != "")
                sndMgr->say(actor, soundPath);
            if (responseSound)
                sndMgr->say(actor, responseSound->mSoundFile);
            if (!info.mScript.scriptSource.empty())
                executeScript(info.mScript.scriptSource, actor);
            return;
        }
    }
}

void F3Dialogue::DialogueManager::say(const MWWorld::Ptr &actor, const std::string &topic)
{
    return sayTo(actor, MWBase::Environment::get().getWorld()->getPlayerPtr(), topic);
}
