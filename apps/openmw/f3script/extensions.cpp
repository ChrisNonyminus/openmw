#include <string>

#include "extensions.hpp"

#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/installopcodes.hpp>
#include <components/interpreter/runtime.hpp>
#include <components/interpreter/opcodes.hpp>

#include <components/debug/debuglog.hpp>

#include <components/esm4/loadqust.hpp>

#include <components/focompiler/opcodes.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/inventorystore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/store.hpp"
#include "../mwworld/localscripts.hpp"

#include "guiextensions.hpp"

#include "interpretercontext.hpp"
#include "usings.hpp"

#include "../f3dialogue/dialoguemanagerimp.hpp"
#include "../f3dialogue/fojournal.hpp"

#include "scriptmanagerimp.hpp"

#include "../f3mechanics/aisequence.hpp"
#include "../f3mechanics/aipackage.hpp"
#include "../f3mechanics/stats.hpp"

#include "../mwmechanics/stat.hpp"


namespace FOScript
{
    namespace Ai
    {
        template <class R>
        class OpGetIsCurrentPackage : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                MWWorld::Ptr ptr = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view packageId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                const ESM4::AIPackage* pack = MWBase::Environment::get().getWorld()->getStore().get<ESM4::AIPackage>().search(packageId);
                if (!pack)
                    pack = MWBase::Environment::get().getWorld()->getStore().get<ESM4::AIPackage>().search(ESM4::stringToFormId(std::string(packageId)));

                if (pack)
                {
                    auto& aisequence = ptr.getClass().getFOStats(ptr).mAiSequence;
                    for (auto& ai : aisequence)
                    {
                        if (ai->getName() == pack->mEditorId)
                        {
                            runtime.push(1);
                            return;
                        }
                    }
                }
                runtime.push(0);
            }
        };
        template <class R>
        class GetInFaction : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                MWWorld::Ptr ptr = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view factId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                const ESM4::Faction* fact = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(factId);
                if (!fact)
                    fact = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Faction>().search(ESM4::stringToFormId(std::string(factId)));

                if (fact)
                {
                    for (auto& actorFaction : ptr.getClass().getFOStats(ptr).mFactionRanks)
                    {
                        if (actorFaction.first == fact->mFormId && actorFaction.second >= 0)
                        {
                            runtime.push(1);
                            return;
                        }
                    }
                }
                runtime.push(0);
            }
        };
        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment5<OpGetIsCurrentPackage<ImplicitRef>>(FOCompiler::Ai::opcodeGetIsCurrentPackage);
            interpreter.installSegment5<OpGetIsCurrentPackage<ExplicitRef>>(FOCompiler::Ai::opcodeGetIsCurrentPackageExplicit);
            interpreter.installSegment5<GetInFaction<ImplicitRef>>(FOCompiler::Ai::opcodeGetInFaction);
            interpreter.installSegment5<GetInFaction<ExplicitRef>>(FOCompiler::Ai::opcodeGetInFactionExplicit);
        }
    }
    namespace Misc
    {
        template <class R>
        class OpGetIsID : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                MWWorld::Ptr ptr = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view compare = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                if (compare == ptr.getClass().getId(ptr))
                    runtime.push(1);
                else 
                {
                    try 
                    {
                        ESM4::FormId id = ESM4::stringToFormId(std::string(compare));
                        if (ptr.getCellRef().getRefr().mBaseObj == id || ptr.getClass().getFormId(ptr) == id)
                        {
                            runtime.push(1);
                            return;
                        }
                    }
                    catch (std::exception e)
                    {
                        runtime.push(0);
                        return;
                    }
                    runtime.push(0);
                }
            }
        };
        class OpGetQuestVariable : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view questId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                std::string_view varName = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                const ESM4::Quest* quest = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().search(questId);

                if (!quest)
                {
                    if (questId.size() == 8)
                        quest = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().find(ESM4::stringToFormId(std::string(questId)));
                    else
                    {
                        runtime.push(0);
                        return;
                    }
                }

                const ESM4::Script* scpt = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find(quest->mQuestScript);

                const auto& locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(scpt->mEditorId);
                int idx = locals.getIndex(varName);
                switch (locals.getType(varName))
                {
                    case 'l':
                    {
                        runtime.push(atoi(locals.get(locals.getType(varName))[idx].c_str()));
                        return;
                    }
                    case 'f':
                    {
                        runtime.push(static_cast<float>(atof(locals.get(locals.getType(varName))[idx].c_str())));
                        return;
                    }
                    case 'r':
                    case 's':
                    {
                        //runtime.push(locals.get(locals.getType(varName))[idx]);
                        runtime.push(1); // todo
                        return;
                    }
                    default: break;
                }

                runtime.push(0);

            }
        };
        class OpGameMode : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                bool gameMode = !MWBase::Environment::get().getWindowManager()->isGuiMode();
                runtime.push(gameMode);
            }
        };
        class OpOnLoad : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());
                runtime.push(context.getReference().isInCell());
            }
        };
        class OpSetQuestObject : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {

                std::string_view editorId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                int flag = (runtime[0].mInteger);
                runtime.pop();
                

                MWWorld::Ptr found = MWBase::Environment::get().getWorld()->searchPtrViaEditorId(std::string(editorId), false); // FIXME: this might be the wrong thing to do. Could it be possible that a ptr a script wants to set as a quest item could be unloaded and then unavailable
                if (!found.isEmpty())
                {
                    found.getRefData().setIsQuestItem(flag != 0);
                }
            }
        };
        class OpSetStage : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {

                std::string_view editorId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                int stage = (runtime[0].mInteger);
                runtime.pop();
                const auto& quest = MWBase::Environment::get().getTopiclessJournal("FO3")->getQuest(std::string(editorId));
                bool questAdded = quest.getState() == F3Dialogue::QuestState::Inactive;
                quest.setStage(stage);
                if (questAdded)
                {
                    auto winMgr = MWBase::Environment::get().getWindowManager();
                    std::stringstream ss;
                    ss << "Quest added: " << quest.getName();
                    winMgr->messageBox(ss.str());
                }
            }
        };
        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment5<OpGetIsID<ImplicitRef>>(FOCompiler::Misc::opcodeGetIsID);
            interpreter.installSegment5<OpGetIsID<ExplicitRef>>(FOCompiler::Misc::opcodeGetIsIDExplicit);
            interpreter.installSegment5<OpGetQuestVariable>(FOCompiler::Misc::opcodeGetQuestVariable);
            interpreter.installSegment5<OpGameMode>(FOCompiler::Misc::opcodeGameMode);
            interpreter.installSegment5<OpSetStage>(FOCompiler::Misc::opcodeSetStage);
            interpreter.installSegment5<OpOnLoad>(FOCompiler::Misc::opcodeOnLoad);
        }
    }
    namespace Sound
    {
        template <class R>
        class OpSayTo : public Interpreter::Opcode1
        {
        public:
            void execute(Interpreter::Runtime& runtime, unsigned int arg0) override
            {
                MWWorld::Ptr ptr = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view target_ref = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                std::string_view topicId = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                // note: these two arguments are optional (TODO: how do I handle optional arguments?)
                if (arg0 > 0)
                {
                    int forceSubtitleFlag = runtime[0].mInteger;
                    runtime.pop();
                }
                if (arg0 > 1)
                {
                    int noTargetLookFlag = runtime[0].mInteger;
                    runtime.pop();
                }

                MWWorld::Ptr target = MWBase::Environment::get().getWorld()->getPtr(target_ref, true);

                MWBase::Environment::get().getDialogueManager("FO3")->sayTo(ptr, target, std::string(topicId));
            }
        };
        template <class R>
        class OpGetIsVoiceType : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                MWWorld::Ptr actor = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view voiceTypeRef = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                ESM4::FormId formId = actor.getClass().getFormId(actor);

                const ESM4::VoiceType* vtyp = MWBase::Environment::get().getWorld()->getStore().get<ESM4::VoiceType>().search(voiceTypeRef);
                if (!vtyp)
                {
                    vtyp = MWBase::Environment::get().getWorld()->getStore().get<ESM4::VoiceType>().search(ESM4::stringToFormId(std::string(voiceTypeRef)));
                }

                if (vtyp)
                {
                    if (actor.getClass().isNpc())
                    {
                        if (const ESM4::Npc* asNpc = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Npc>().search(formId))
                        {
                            if (vtyp->mFormId == asNpc->mVoice)
                            {
                                runtime.push(1);
                                return;
                            }
                        }
                    }
                    else
                    {
                        if (const ESM4::Creature* asCreature = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Creature>().search(formId))
                        {
                            if (vtyp->mFormId == asCreature->mVoice)
                            {
                                runtime.push(1);
                                return;
                            }
                        }
                    }
                }

                runtime.push(0);
            }
        };
        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment3<OpSayTo<ImplicitRef>>(FOCompiler::Sound::opcodeSayTo);
            interpreter.installSegment3<OpSayTo<ExplicitRef>>(FOCompiler::Sound::opcodeSayToExplicit);
            interpreter.installSegment5<OpGetIsVoiceType<ImplicitRef>>(FOCompiler::Sound::opcodeGetVoiceType);
            interpreter.installSegment5<OpGetIsVoiceType<ExplicitRef>>(FOCompiler::Sound::opcodeGetVoiceTypeExplicit);
        }
    }
    namespace Stats
    {
        template <class R>
        class OpGetActorValue : public Interpreter::Opcode0
        {
        public:
            void execute(Interpreter::Runtime& runtime) override
            {
                MWWorld::Ptr actor = R()(runtime);

                InterpreterContext& context
                    = static_cast<InterpreterContext&>(runtime.getContext());

                std::string_view actorValue = runtime.getStringLiteral(runtime[0].mInteger);
                runtime.pop();

                // todo: tes4 stats

                auto& stats = actor.getClass().getFOStats(actor);

                // fallout skills
                static std::map<std::string, F3Mechanics::Skill, ::Misc::StringUtils::CiComp> skillsByName = // ugh, some fo3 scripts access the skills as actor values
                    // we don't use the skills as actor values, so I have to get creative
                {
                        { "barter", F3Mechanics::Skill_Barter },
                        { "bigguns", F3Mechanics::Skill_BigGuns },
                        { "energyweapons", F3Mechanics::Skill_EnergyWeapons },
                        { "explosives", F3Mechanics::Skill_Explosives },
                        { "lockpick", F3Mechanics::Skill_Lockpick },
                        { "medicine", F3Mechanics::Skill_Medicine },
                        { "meleeweapons", F3Mechanics::Skill_MeleeWeapons },
                        { "repair", F3Mechanics::Skill_Repair },
                        { "science", F3Mechanics::Skill_Science },
                        { "guns", F3Mechanics::Skill_Guns },
                        { "smallguns", F3Mechanics::Skill_Guns },
                        { "sneak", F3Mechanics::Skill_Sneak },
                        { "speech", F3Mechanics::Skill_Speech },
                        { "survival", F3Mechanics::Skill_Survival },
                        { "throwing", F3Mechanics::Skill_Survival },
                        { "unarmed", F3Mechanics::Skill_Unarmed }
                };

                if (skillsByName.count(std::string(actorValue)))
                {
                    int ret = stats.mSkills[skillsByName[std::string(actorValue)]].getModified();
                    runtime.push(ret);
                    return;
                }

                if (stats.mActorValues.count(std::string(actorValue)))
                {
                    int ret = stats.mActorValues[std::string(actorValue)].getModified();
                    runtime.push(ret);
                    return;
                }

                runtime.push(0);
            }
        };
        class OpGetDistanceToRef : public Interpreter::Opcode0
        {
        public:
            virtual void execute(Interpreter::Runtime& runtime)
            {
                std::string_view name = runtime.getStringLiteral(runtime[0].mInteger); // 1 mandatory argument

                Interpreter::Type_Float distance = runtime.getContext().getDistanceToRef(std::string(name));

                runtime.push(distance);
            }
        };

        class OpGetDistanceToRefExplicit : public Interpreter::Opcode0
        {
        public:
            virtual void execute(Interpreter::Runtime& runtime)
            {
                int index = runtime[0].mInteger;
                runtime.pop();
                std::string_view id = runtime.getStringLiteral(index); // explicit ref editor id (formid better?)

                std::string_view name = runtime.getStringLiteral(runtime[0].mInteger); // 1 mandatory argument

                Interpreter::Type_Float distance = runtime.getContext().getDistanceToRef(std::string(name), std::string(id));

                runtime.push(distance);
            }
        };
        void installOpcodes(Interpreter::Interpreter& interpreter)
        {
            interpreter.installSegment5<OpGetActorValue<ImplicitRef>>(FOCompiler::Stats::opcodeGetActorValue);
            interpreter.installSegment5<OpGetActorValue<ExplicitRef>>(FOCompiler::Stats::opcodeGetActorValueExplicit);
            interpreter.installSegment5<OpGetDistanceToRef>(92);
            interpreter.installSegment5<OpGetDistanceToRefExplicit>(93);
        }
    }
    void installOpcodes (Interpreter::Interpreter& interpreter, bool consoleOnly)
    {
        Interpreter::installOpcodes (interpreter);
        Ai::installOpcodes(interpreter);
        Gui::installOpcodes (interpreter);
        Sound::installOpcodes(interpreter);
        Misc::installOpcodes(interpreter);
        Stats::installOpcodes(interpreter);

        if (consoleOnly)
        {
            // Console::installOpcodes (interpreter);
            // User::installOpcodes (interpreter);
        }
    }
}
