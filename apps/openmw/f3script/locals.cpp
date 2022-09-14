#include "locals.hpp"
#include "globalscripts.hpp"

#include <components/compiler/locals.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/scriptmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/esmstore.hpp"

#include "../mwworld/class.hpp"

#include "../mwworld/ptr.hpp"


namespace FOScript
{
    void Locals::ensure(std::string_view scriptName)
    {
        if (!mInitialised)
        {
            const ESM4::Script *script = MWBase::Environment::get().getWorld()->getStore().
                get<ESM4::Script>().find(scriptName);

            configure (*script);
        }
    }

    Locals::Locals() : mInitialised (false) {}

    bool Locals::configure (const ESM4::Script& script)
    {
        if (mInitialised)
            return false;

        const Locals* global = MWBase::Environment::get().getTes4ScriptManager()->getFOGlobalScripts().getLocalsIfPresent(script.mEditorId);
        if(global)
        {
            mShorts = global->mShorts;
            mLongs = global->mLongs;
            mFloats = global->mFloats;
            mRefs = global->mRefs;
        }
        else
        {
            const Compiler::Locals& locals =
                MWBase::Environment::get().getTes4ScriptManager()->getLocals (script.mEditorId);

            mShorts.clear();
            mShorts.resize (locals.get ('s').size(), 0);
            mLongs.clear();
            mLongs.resize(locals.get('l').size(), 0);
            mFloats.clear();
            mFloats.resize(locals.get('f').size(), 0);
            mRefs.clear();
            mRefs.resize(locals.get('r').size(), 0);
        }

        mInitialised = true;
        return true;
    }

    bool Locals::isEmpty() const
    {
        return (mShorts.empty() && mLongs.empty() && mFloats.empty() && mRefs.empty());
    }

    bool Locals::hasVar(std::string_view script, std::string_view var)
    {
        ensure (script);

        const Compiler::Locals& locals =
            MWBase::Environment::get().getTes4ScriptManager()->getLocals(script);
        int index = locals.getIndex(var);
        return (index != -1);
    }

    int Locals::getIntVar(std::string_view script, std::string_view var)
    {
        ensure (script);

        const Compiler::Locals& locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(script);
        int index = locals.getIndex(var);
        char type = locals.getType(var);
        if(index != -1)
        {
            switch(type)
            {
                case 's':
                    return mShorts.at (index);

                case 'l':
                    return mLongs.at (index);

                case 'f':
                    return static_cast<int>(mFloats.at(index));

                case 'r':
                    return static_cast<int>(mRefs.at(index).getClass().getFormId(mRefs.at(index)));

                default:
                    return 0;
            }
        }
        return 0;
    }

    float Locals::getFloatVar(std::string_view script, std::string_view var)
    {
        ensure (script);

        const Compiler::Locals& locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(script);
        int index = locals.getIndex(var);
        char type = locals.getType(var);
        if(index != -1)
        {
            switch(type)
            {
                case 's':
                    return mShorts.at (index);

                case 'l':
                    return mLongs.at (index);

                case 'f':
                    return mFloats.at(index);
                default:
                    return 0;
            }
        }
        return 0;
    }

    bool Locals::setVarByInt(std::string_view script, std::string_view var, int val)
    {
        ensure (script);

        const Compiler::Locals& locals = MWBase::Environment::get().getTes4ScriptManager()->getLocals(script);
        int index = locals.getIndex(var);
        char type = locals.getType(var);
        if(index != -1)
        {
            switch(type)
            {
                case 's':
                    mShorts.at (index) = val; break;

                case 'l':
                    mLongs.at (index) = val; break;

                case 'f':
                    mFloats.at(index) = static_cast<float>(val); break;

                case 'r':
                    mRefs.at(index) = MWBase::Environment::get().getWorld()->searchPtrViaFormId(static_cast<uint32_t>(val));
                    break;
            }
            return true;
        }
        return false;
    }
}
