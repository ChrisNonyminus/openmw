#include "fojournal.hpp"

#include <iterator>

#include <components/misc/strings/algorithm.hpp>

#include <components/debug/debuglog.hpp>

#include <components/focompiler/scanner.hpp>

#include <components/compiler/exception.hpp>

#include "../mwworld/esmstore.hpp"
#include "../mwworld/class.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../f3script/scriptmanagerimp.hpp"
#include "../f3script/interpretercontext.hpp"
#include "../f3script/extensions.hpp"
#include "../mwscript/compilercontext.hpp"

namespace F3Dialogue
{
    bool compile(const std::string& cmd, std::vector<ESM4::FormId>& code, const MWWorld::Ptr& actor)
    {
        bool success = true;
        try
        {
            MWScript::CompilerContext context(MWScript::CompilerContext::Type_Full);
            context.setExtensions(&MWBase::Environment::get().getTes4ScriptManager()->getExtensions());
            Compiler::StreamErrorHandler errorHandler;
            errorHandler.setContext("CTDA checking script");

            std::istringstream input(cmd + "\n");
            FOCompiler::Scanner scanner(errorHandler, input, context.getExtensions());

            Compiler::Locals locals;

            std::string_view actorScript = actor.getClass().getScript(actor);

            if (!actorScript.empty())
            {
                locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(actorScript);
            }

            FOCompiler::ScriptParser parser(errorHandler, context, locals, false);

            scanner.scan(parser);

            if (!errorHandler.isGood())
                success = false;

            if (success)
                parser.getCode(code);
        }
        catch (const Compiler::SourceException& /* error */)
        {
            // error has already been reported via error handler
            success = false;
        }
        catch (const std::exception& error)
        {
            Log(Debug::Error) << std::string("Condition check error: An exception has been thrown: ") + error.what();
            success = false;
        }

        if (!success)
        {
            Log(Debug::Error) << "Error: compiling failed (condition script): \n"
                              << cmd << "\n";
        }

        return success;
    }
    bool targetMeetsCondition(const ESM4::Quest* quest, uint32_t objIdx, int targetIdx)
    {
        for (auto& qobj : quest->mObjectives)
        {
            if (qobj.mIndex != objIdx)
                continue;
            bool condsMet = false;
            for (auto& cond : qobj.mTargets[targetIdx].mConditions)
            {
                if (!FOCompiler::sFunctionIndices.count(ESM4::FunctionIndices(cond.functionIndex)))
                {
                    std::stringstream err;
                    err << "quest objective '" << qobj.mDescription << "' had a condition with an unhandled function: " << cond.functionIndex;
                    Log(Debug::Error) << err.str();
                    return false;
                }
                std::vector<Interpreter::Type_Code> code;
                std::stringstream script;
                if (cond.runOn != 2 || cond.runOn != 0)
                {
                    Log(Debug::Error) << ("Unhandled runOn in quest objective '" + qobj.mDescription + "': ") << cond.runOn; return false;
                }
                MWWorld::Ptr ptr = cond.runOn == 2 ? MWBase::Environment::get().getWorld()->searchPtrViaFormId(cond.reference) : MWBase::Environment::get().getWorld()->searchPtrViaFormId(qobj.mTargets[targetIdx].mTarget.target);
                if (ptr.isEmpty())
                    return false;
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
                ESM4::Script scpt;
                scpt.mScript.scriptSource = script.str();
                const ESM4::Script* rec = MWBase::Environment::get().getWorld()->createRecord(scpt);
                FOScript::Locals scriptLocals;
                scriptLocals.configure(*rec);
                if (compile(script.str(), code, ptr))
                {
                    try
                    {
                        FOScript::Locals locals;
                        locals.mFloats.resize(1);
                        FOScript::InterpreterContext interpreterContext(&locals, ptr);
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
                        Log(Debug::Error) << std::string("Condition script error: An exception has been thrown: ") + error.what();
                        condsMet = false;
                    }
                }
            }
        }
        return false;
    }
    const F3Dialogue::Quest& TopiclessJournal::getQuest(const std::string& id) const
    {
        for (auto& quest : *this)
        {
            if (quest.second.getRecord()->mEditorId == id)
                return quest.second;
        }
        /*const ESM4::Quest* qust = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().search(id);
        if (qust)
        {
            Quest q(qust);
            mQuests[id] = q;
            return mQuests[id];
        }*/
        throw std::runtime_error("Quest not found: " + (id));
    }
    const F3Dialogue::Quest& TopiclessJournal::getQuest(ESM4::FormId id) const
    {
        for (auto& quest : *this)
        {
            if (quest.first == id)
                return quest.second;
        }
        /*const ESM4::Quest* qust = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().search(id);
        if (qust)
        {
            Quest q(qust);
            mQuests[id] = q;
            return mQuests[id];
        }*/
        throw std::runtime_error("Quest not found: " + ESM4::formIdToString(id));
    }
    TopiclessJournal::TopiclessJournal()
    {
        for (auto& qust : MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>())
        {
            Quest q(&qust);
            mQuests.emplace(qust.mFormId, std::move(q));
        }
    }
    TopiclessJournal::TQuestIter TopiclessJournal::begin()
    {
        return mQuests.begin();
    }
    TopiclessJournal::TQuestIter TopiclessJournal::end()
    {
        return mQuests.end();
    }
    TopiclessJournal::TQuestConstIter TopiclessJournal::begin() const
    {
        return mQuests.begin();
    }
    TopiclessJournal::TQuestConstIter TopiclessJournal::end() const
    {
        return mQuests.end();
    }
    std::vector<ESM4::FormId> TopiclessJournal::revealObjectives(ESM4::FormId questId, const std::vector<uint32_t>& objectives)
    {
        std::vector<ESM4::FormId> targets;
        const ESM4::Quest* qust = getQuest(questId).getRecord();
        for (const auto& obj : objectives)
        {
            for (auto& qobj : qust->mObjectives)
            {
                if (qobj.mIndex == obj)
                {
                    for (int i = 0; i < qobj.mTargets.size(); i++)
                    {
                        if (targetMeetsCondition(qust, obj, i))
                        {
                            targets.push_back(qobj.mTargets[i].mTarget.target);
                        }
                    }
                }
            }
        }
        return targets;
    }
}
