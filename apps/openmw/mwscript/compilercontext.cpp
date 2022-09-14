#include "compilercontext.hpp"

#include "../mwworld/esmstore.hpp"

#include <components/esm3/loaddial.hpp>

#include <components/compiler/locals.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/scriptmanager.hpp"

#include "../mwworld/ptr.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/manualref.hpp"

namespace MWScript
{
    CompilerContext::CompilerContext (Type type)
    : mType (type)
    {}

    bool CompilerContext::canDeclareLocals() const
    {
        return mType==Type_Full;
    }

    char CompilerContext::getGlobalType (const std::string& name) const
    {
        return MWBase::Environment::get().getWorld()->getGlobalVariableType (name);
    }

    std::pair<char, bool> CompilerContext::getMemberType (const std::string& name,
        const std::string& id) const
    {
        std::string_view script;
        bool reference = false;

        char type = ' ';

        if (const ESM::Script *scriptRecord =
            MWBase::Environment::get().getWorld()->getStore().get<ESM::Script>().search (id))
        {
            script = scriptRecord->mId;

            if (!script.empty())
                type = MWBase::Environment::get().getScriptManager()->getLocals(script).getType(
                    Misc::StringUtils::lowerCase(name));
        }
        else if (const ESM4::Script* tes4scriptRecord = 
            MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().search(id))
        {
            script = tes4scriptRecord->mEditorId;

            if (!script.empty())
                type = MWBase::Environment::get().getScriptManager()->getLocals(script).getType(
                    Misc::StringUtils::lowerCase(name));
        }
        else if (const ESM4::Quest* qust4 = 
            MWBase::Environment::get().getWorld()->getStore().get<ESM4::Quest>().search(id))
        {
            script = MWBase::Environment::get().getWorld()->getStore().get<ESM4::Script>().find(qust4->mQuestScript)->mEditorId;

            if (!script.empty())
                type = MWBase::Environment::get().getTes4ScriptManager()->getLocals(script).getType(
                    Misc::StringUtils::lowerCase(name));
        }
        else
        {
            MWWorld::ManualRef ref (MWBase::Environment::get().getWorld()->getStore(), id);

            script = ref.getPtr().getClass().getScript (ref.getPtr());
            reference = true;

            if (!script.empty())
                type = MWBase::Environment::get().getScriptManager()->getLocals(script).getType(
                    Misc::StringUtils::lowerCase(name));
        }

        return std::make_pair (type, reference);
    }

    bool CompilerContext::isId (const std::string& name) const
    {
        const MWWorld::ESMStore &store =
            MWBase::Environment::get().getWorld()->getStore();

        return
            store.get<ESM::Activator>().search (name) ||
            store.get<ESM::Potion>().search (name) ||
            store.get<ESM::Apparatus>().search (name) ||
            store.get<ESM::Armor>().search (name) ||
            store.get<ESM::Book>().search (name) ||
            store.get<ESM::Clothing>().search (name) ||
            store.get<ESM::Container>().search (name) ||
            store.get<ESM::Creature>().search (name) ||
            store.get<ESM::Door>().search (name) ||
            store.get<ESM::Ingredient>().search (name) ||
            store.get<ESM::CreatureLevList>().search (name) ||
            store.get<ESM::ItemLevList>().search (name) ||
            store.get<ESM::Light>().search (name) ||
            store.get<ESM::Lockpick>().search (name) ||
            store.get<ESM::Miscellaneous>().search (name) ||
            store.get<ESM::NPC>().search (name) ||
            store.get<ESM::Probe>().search (name) ||
            store.get<ESM::Repair>().search (name) ||
            store.get<ESM::Static>().search (name) ||
            store.get<ESM::Weapon>().search (name) ||
            store.get<ESM::Script>().search (name) ||

            store.get<ESM4::Creature>().search(name);
    }

    ESM4::FormId CompilerContext::getReference (const std::string& lowerEditorId) const
    {
        if (lowerEditorId == "player" || lowerEditorId == "playerref")
            return 0x00000014;

        // first search the active cells
        MWWorld::Ptr ptr
            = MWBase::Environment::get().getWorld()->searchPtrViaEditorId(lowerEditorId, true/*activeOnly*/);

        if (ptr)
            return ptr.getClass().getFormId(ptr);

        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();

        /*const ESM4::ActorCharacter *achr = store.get<ESM4::ActorCharacter>().search(lowerEditorId);
        if (achr)
            return achr->mFormId;*/ // todo: add ACHR to store

        const ESM4::Reference *ref = store.get<ESM4::Reference>().search(lowerEditorId);
        if (ref)
            return ref->mFormId;

        /*const ESM4::ActorCreature *acre = store.get<ESM4::ActorCreature>().search(lowerEditorId);
        if (acre)
            return acre->mFormId;*/ // todo: add ACRE to store

        const ESM4::Quest *quest = store.get<ESM4::Quest>().search (lowerEditorId);
        if (quest && quest->mQuestScript)
        {
            const ESM4::Script *script = store.get<ESM4::Script>().search(quest->mQuestScript);
            if (script)
                return script->mFormId;
        }

        return 0;
    }

    int32_t CompilerContext::getAIPackage (const std::string& lowerEditorId) const
    {
        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();

        const ESM4::AIPackage *pack = store.get<ESM4::AIPackage>().search(lowerEditorId);
        if (pack)
            return pack->mData.type;

        return 0;
    }
}
