#include "interpretercontext.hpp"

#include <cmath>
#include <sstream>

#include <components/compiler/locals.hpp>
#include <components/esm4/records.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/inputmanager.hpp"
#include "../mwbase/luamanager.hpp"

#include "../mwworld/action.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwworld/containerstore.hpp"

#include "../mwmechanics/npcstats.hpp"

#include "locals.hpp"
#include "globalscripts.hpp"

namespace FOScript
{
    const MWWorld::Ptr InterpreterContext::getReferenceImp(std::string_view id, bool activeOnly, bool doThrow) const
    {
        if (!id.empty())
        {
            return MWBase::Environment::get().getWorld()->getPtr (id, activeOnly);
        }
        else
        {
            if (mReference.isEmpty() && mGlobalScriptDesc)
                mReference = MWBase::Environment::get().getWorld()->searchPtrViaEditorId(mTargetId, false /*activeOnly*/);

            if (mReference.isEmpty() && doThrow)
                throw MissingImplicitRefError();

            return mReference;
        }
    }

    const Locals& InterpreterContext::getMemberLocals(std::string_view& id, bool global)
        const
    {
        if (global)
        {
            return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().
                getLocals (id);
        }
        else
        {
            const MWWorld::Ptr ptr = getReferenceImp (id, false);

            id = ptr.getClass().getScript (ptr);

            ptr.getRefData().setLocals (
                *MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find (id));

            return ptr.getRefData().getFOLocals();
        }
    }

    Locals& InterpreterContext::getMemberLocals(std::string_view& id, bool global)
    {
        if (global)
        {
            return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().
                getLocals (id);
        }
        else
        {
            const MWWorld::Ptr ptr = getReferenceImp (id, false);

            id = ptr.getClass().getScript (ptr);

            ptr.getRefData().setLocals (
                *MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find (id));

            return ptr.getRefData().getFOLocals();
        }
    }

    const Locals& InterpreterContext::getScriptMemberLocals(std::string& id, bool global) const
    {
        if (global)
        {
            //return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);

            const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();

            const ESM4::Quest* quest = store.get<ESM4::Quest>().search(id);
            if (quest && quest->mQuestScript)
            {
                const ESM4::Script* script = store.get<ESM4::Script>().search(quest->mQuestScript);
                if (script)
                    id = script->mEditorId;

                return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);
            }
            return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);
        }
        else
        {
            const MWWorld::Ptr ptr = getReferenceImp(id, false);
            id = ptr.getClass().getScript(ptr);

            ptr.getRefData().setLocals(*MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find(id));
            return ptr.getRefData().getFOLocals();
        }
    }

    Locals& InterpreterContext::getScriptMemberLocals(std::string& id, bool global)
    {
        if (global)
        {
            //return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);

            const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();

            const ESM4::Quest* quest = store.get<ESM4::Quest>().search(id);
            if (quest && quest->mQuestScript)
            {
                const ESM4::Script* script = store.get<ESM4::Script>().search(quest->mQuestScript);
                if (script)
                    id = script->mEditorId;

                return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);
            }
            return MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocals(id);
        }
        else
        {
            const MWWorld::Ptr ptr = getReferenceImp(id, false);
            id = ptr.getClass().getScript(ptr);

            ptr.getRefData().setLocals(*MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().search(id));
            return ptr.getRefData().getFOLocals();
        }
    }

    MissingImplicitRefError::MissingImplicitRefError() : std::runtime_error("no implicit reference") {}

    int InterpreterContext::findLocalVariableIndex(std::string_view scriptId, std::string_view name, char type) const
    {
        int index = MWBase::Environment::get().getTes4ScriptManager()->getLocals (scriptId).
            searchIndex (type, name);

        if (index!=-1)
            return index;

        std::ostringstream stream;

        stream << "Failed to access ";

        switch (type)
        {
            case 's': stream << "short"; break;
            case 'l': stream << "long"; break;
            case 'f': stream << "float"; break;
            case 'r': stream << "ref"; break;
        }

        stream << " member variable " << name << " in script " << scriptId;

        throw std::runtime_error (stream.str().c_str());
    }

    InterpreterContext::InterpreterContext (Locals *locals, const MWWorld::Ptr& reference)
    : mLocals (locals), mReference (reference)
    {
        if (!reference.isEmpty())
            mTargetId = reference.getClass().getId(reference);

        if (!reference.isEmpty())
            mTargetFormId = reference.getClass().getFormId(reference);
    }

    float InterpreterContext::getDistanceToRef(const std::string& name, const std::string& id) const
    {
        // NOTE: id may be empty, indicating an implicit reference

        MWWorld::Ptr ref2;

        if (id.empty())
            ref2 = getReferenceImp();
        else
            ref2 = MWBase::Environment::get().getWorld()->getPtr(id, false);

        if (ref2.getContainerStore()) // is the object contained?
        {
            MWWorld::Ptr container = MWBase::Environment::get().getWorld()->findContainer(ref2);

            if (!container.isEmpty())
                ref2 = container;
            else
                throw std::runtime_error("failed to find container ptr");
        }

        const MWWorld::Ptr ref = MWBase::Environment::get().getWorld()->getPtr(name, false);

        // If the objects are in different worldspaces, return a large value (just like vanilla)
        if (ref.getCell()->getCell4()->mParent != ref2.getCell()->getCell4()->mParent)
            return std::numeric_limits<float>::max();

        double diff[3];

        const float* const pos1 = ref.getRefData().getPosition().pos;
        const float* const pos2 = ref2.getRefData().getPosition().pos;
        for (int i = 0; i < 3; ++i)
            diff[i] = pos1[i] - pos2[i];

        return static_cast<float>(std::sqrt(diff[0] * diff[0] + diff[1] * diff[1] + diff[2] * diff[2]));
    }

    InterpreterContext::InterpreterContext (std::shared_ptr<GlobalScriptDesc> globalScriptDesc)
    : mLocals (&(globalScriptDesc->mLocals))
    {
        const MWWorld::Ptr* ptr = globalScriptDesc->getPtrIfPresent();
        // A nullptr here signifies that the script's target has not yet been resolved after loading the game.
        // Script targets are lazily resolved to MWWorld::Ptrs (which can, upon resolution, be empty)
        // because scripts started through dialogue often don't use their implicit target.
        if (ptr)
            mReference = *ptr;
        else
            mGlobalScriptDesc = globalScriptDesc;
    }

    std::string_view InterpreterContext::getTarget() const
    {
        if(!mReference.isEmpty())
            return mReference.mRef->mRef.getRefId();
        else if(mGlobalScriptDesc)
            return mGlobalScriptDesc->getId();
        return {};
    }

    int InterpreterContext::getLocalShort (int index) const
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        return mLocals->mShorts.at (index);
    }

    int InterpreterContext::getLocalLong (int index) const
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        return mLocals->mLongs.at (index);
    }

    float InterpreterContext::getLocalFloat (int index) const
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        return mLocals->mFloats.at (index);
    }

    void InterpreterContext::setLocalShort (int index, int value)
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        mLocals->mShorts.at (index) = value;
    }

    void InterpreterContext::setLocalLong (int index, int value)
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        mLocals->mLongs.at (index) = value;
    }

    void InterpreterContext::setLocalFloat (int index, float value)
    {
        if (!mLocals)
            throw std::runtime_error ("local variables not available in this context");

        mLocals->mFloats.at (index) = value;
    }

    void InterpreterContext::messageBox(std::string_view message,
        const std::vector<std::string>& buttons)
    {
        if (buttons.empty())
            MWBase::Environment::get().getWindowManager()->messageBox (message);
        else
            MWBase::Environment::get().getWindowManager()->interactiveMessageBox(message, buttons);
    }

    void InterpreterContext::report (const std::string& message)
    {
    }

    int InterpreterContext::getGlobalShort(std::string_view name) const
    {
        return MWBase::Environment::get().getWorld()->getGlobalInt (name);
    }

    int InterpreterContext::getGlobalLong(std::string_view name) const
    {
        // a global long is internally a float.
        return MWBase::Environment::get().getWorld()->getGlobalInt (name);
    }

    float InterpreterContext::getGlobalFloat(std::string_view name) const
    {
        return MWBase::Environment::get().getWorld()->getGlobalFloat (name);
    }

    void InterpreterContext::setGlobalShort(std::string_view name, int value)
    {
        MWBase::Environment::get().getWorld()->setGlobalInt (name, value);
    }

    void InterpreterContext::setGlobalLong(std::string_view name, int value)
    {
        MWBase::Environment::get().getWorld()->setGlobalInt (name, value);
    }

    void InterpreterContext::setGlobalFloat(std::string_view name, float value)
    {
        MWBase::Environment::get().getWorld()->setGlobalFloat (name, value);
    }

    std::vector<std::string> InterpreterContext::getGlobals() const
    {
        const MWWorld::Store<ESM::Global>& globals =
            MWBase::Environment::get().getWorld()->getStore().get<ESM::Global>();

        std::vector<std::string> ids;
        for (const auto& globalVariable : globals)
        {
            ids.emplace_back(globalVariable.mId);
        }

        return ids;
    }

    char InterpreterContext::getGlobalType(std::string_view name) const
    {
        MWBase::World *world = MWBase::Environment::get().getWorld();
        return world->getGlobalVariableType(name);
    }

    std::string InterpreterContext::getActionBinding(std::string_view targetAction) const
    {
        MWBase::InputManager* input = MWBase::Environment::get().getInputManager();
        const auto& actions = input->getActionKeySorting();
        for (const int action : actions)
        {
            std::string_view desc = input->getActionDescription(action);
            if(desc.empty())
                continue;

            if(desc == targetAction)
            {
                if(input->joystickLastUsed())
                    return input->getActionControllerBindingName(action);
                else
                    return input->getActionKeyBindingName(action);
            }
        }

        return "None";
    }

    std::string_view InterpreterContext::getActorName() const
    {
        const MWWorld::Ptr& ptr = getReferenceImp();
        if (ptr.getClass().isNpc())
        {
            /*const ESM4::Npc* npc = ptr.get<ESM4::Npc>()->mBase;
            return npc->mFullName;*/
        }

        const ESM4::Creature* creature = ptr.get<ESM4::Creature>()->mBase;
        return creature->mFullName;
    }

    std::string_view InterpreterContext::getNPCRace() const
    {
        //const ESM4::Npc* npc = getReferenceImp().get<ESM4::Npc>()->mBase;
        throw std::runtime_error("getNPCRace not implemented yet for OBScript/FOScript");
        /*const ESM4::Race* race = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Race>().search(npc->mRace);
        return race->mFullName;*/
    }

    std::string_view InterpreterContext::getNPCClass() const
    {
        //const ESM4::Npc* npc = getReferenceImp().get<ESM4::Npc>()->mBase;
        throw std::runtime_error("getNPCClass not implemented yet for OBScript/FOScript");
        /*const ESM4::Class* class_ = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Class>().search(npc->mClass);
        return class_->mFullName;*/
    }

    std::string_view InterpreterContext::getNPCFaction() const
    {
        //const ESM4::Npc* npc = getReferenceImp().get<ESM4::Npc>()->mBase;
        throw std::runtime_error("getNPCFaction not implemented yet for OBScript/FOScript");
        // const ESM4::faction* faction = MWBase::Environment::get().getWorld()->getStore().get<ESM4::ActorFaction>().find(npc->mFaction);
        // return faction->mFU;
    }

    std::string_view InterpreterContext::getNPCRank() const
    {
        //const ESM4::Npc* npc = getReferenceImp().get<ESM4::Npc>()->mBase;
        throw std::runtime_error("getNPCFaction not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getPCName() const
    {
        /*MWBase::World *world = MWBase::Environment::get().getWorld();
        return world->getPlayerPtr().get<ESM4::Npc>()->mBase->mFullName;*/
        throw std::runtime_error("getPCName not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getPCRace() const
    {
        throw std::runtime_error("getNPCRace not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getPCClass() const
    {
        throw std::runtime_error("getNPCClass not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getPCRank() const
    {
        throw std::runtime_error("getNPCFaction not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getPCNextRank() const
    {
        throw std::runtime_error("getNPCFaction not implemented yet for OBScript/FOScript");
    }

    int InterpreterContext::getPCBounty() const
    {
        throw std::runtime_error("getPCBounty not implemented yet for OBScript/FOScript");
    }

    std::string_view InterpreterContext::getCurrentCellName() const
    {
        return MWBase::Environment::get().getWorld()->getCellName();
    }

    void InterpreterContext::executeActivation(const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor)
    {
        MWBase::Environment::get().getLuaManager()->objectActivated(ptr, actor);
        std::unique_ptr<MWWorld::Action> action = (ptr.getClass().activate(ptr, actor));
        action->execute (actor);
        if (action->getTarget() != MWWorld::Ptr() && action->getTarget() != ptr)
        {
            updatePtr(ptr, action->getTarget());
        }
    }

    int InterpreterContext::getMemberShort(std::string_view id, std::string_view name,
        bool global) const
    {
        const Locals& locals = getMemberLocals(id, global);

        return locals.mShorts[findLocalVariableIndex(id, name, 's')];
    }

    int InterpreterContext::getMemberLong(std::string_view id, std::string_view name,
        bool global) const
    {
        const Locals& locals = getMemberLocals(id, global);

        return locals.mLongs[findLocalVariableIndex(id, name, 'l')];
    }

    float InterpreterContext::getMemberFloat(std::string_view id, std::string_view name,
        bool global) const
    {
        const Locals& locals = getMemberLocals(id, global);

        return locals.mFloats[findLocalVariableIndex(id, name, 'f')];
    }

    void InterpreterContext::setMemberShort(std::string_view id, std::string_view name,
        int value, bool global)
    {
        Locals& locals = getMemberLocals(id, global);

        locals.mShorts[findLocalVariableIndex(id, name, 's')] = value;
    }

    void InterpreterContext::setMemberLong(std::string_view id, std::string_view name, int value, bool global)
    {
        Locals& locals = getMemberLocals(id, global);

        locals.mLongs[findLocalVariableIndex(id, name, 'l')] = value;
    }

    void InterpreterContext::setMemberFloat(std::string_view id, std::string_view name, float value, bool global)
    {
        Locals& locals = getMemberLocals(id, global);

        locals.mFloats[findLocalVariableIndex(id, name, 'f')] = value;
    }

    Locals& InterpreterContext::getLocals()
    {
        return *mLocals;
    }

    int InterpreterContext::getScriptMemberShort(const std::string& id, const std::string& name,
        bool global) const
    {
        std::string scriptId(id);

        const Locals& locals = getScriptMemberLocals(scriptId, global);

        return locals.mShorts[findLocalVariableIndex(scriptId, name, 's')];
    }

    int InterpreterContext::getScriptMemberLong(const std::string& id, const std::string& name,
        bool global) const
    {
        std::string scriptId(id);

        const Locals& locals = getScriptMemberLocals(scriptId, global);

        return locals.mLongs[findLocalVariableIndex(scriptId, name, 'l')];
    }

    float InterpreterContext::getScriptMemberFloat(const std::string& id, const std::string& name,
        bool global) const
    {
        std::string scriptId(id);

        const Locals& locals = getScriptMemberLocals(scriptId, global);

        return locals.mFloats[findLocalVariableIndex(scriptId, name, 'f')];
    }

    uint32_t InterpreterContext::getScriptMemberRef(const std::string& id, const std::string& name,
        bool global) const
    {
        std::string scriptId(id);

        const Locals& locals = getScriptMemberLocals(scriptId, global);
        MWWorld::Ptr ptr = locals.mRefs[findLocalVariableIndex(scriptId, name, 'r')];
        return ptr.getClass().getFormId(ptr);
    }

    void InterpreterContext::setScriptMemberShort(const std::string& id, const std::string& name,
        int value, bool global)
    {
        std::string scriptId(id);

        Locals& locals = getScriptMemberLocals(scriptId, global);

        locals.mShorts[findLocalVariableIndex(scriptId, name, 's')] = value;
    }

    void InterpreterContext::setScriptMemberLong(const std::string& id, const std::string& name, int value, bool global)
    {
        std::string scriptId(id);

        Locals& locals = getScriptMemberLocals(scriptId, global);

        locals.mLongs[findLocalVariableIndex(scriptId, name, 'l')] = value;
    }

    void InterpreterContext::setScriptMemberFloat(const std::string& id, const std::string& name, float value, bool global)
    {
        std::string scriptId(id);

        Locals& locals = getScriptMemberLocals(scriptId, global);

        locals.mFloats[findLocalVariableIndex(scriptId, name, 'f')] = value;
    }

    void InterpreterContext::setScriptMemberRef(const std::string& id, const std::string& name, uint32_t value, bool global)
    {
        std::string scriptId(id);

        Locals& locals = getScriptMemberLocals(scriptId, global);

        locals.mRefs[findLocalVariableIndex(scriptId, name, 'r')] = MWBase::Environment::get().getWorld()->searchPtrViaFormId(value);
    }

    MWWorld::Ptr InterpreterContext::getReference(bool required) const
    {
        return getReferenceImp("", true, required);
    }

    std::string InterpreterContext::getTargetId() const
    {
        return mTargetId;
    }

    ESM4::FormId InterpreterContext::getTargetFormId() const
    {
        return mTargetFormId;
    }

    void InterpreterContext::updatePtr(const MWWorld::Ptr& base, const MWWorld::Ptr& updated)
    {
        if (!mReference.isEmpty() && base == mReference)
        {
            mReference = updated;
            if (mLocals == &base.getRefData().getFOLocals())
                mLocals = &mReference.getRefData().getFOLocals();
        }
    }
}
