#include "globalscripts.hpp"

#include <components/debug/debuglog.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/globalscript.hpp>
#include <components/esm3/loadsscr.hpp>
#include <components/esm3/loadscpt.hpp>

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/scriptmanager.hpp"

#include "interpretercontext.hpp"



namespace
{
    struct ScriptCreatingVisitor
    {
        ESM::GlobalScript operator()(const MWWorld::Ptr &ptr) const
        {
            ESM::GlobalScript script;
            script.mTargetRef.unset();
            script.mRunning = false;
            if (!ptr.isEmpty())
            {
                if (ptr.getCellRef().hasContentFile())
                {
                    script.mTargetId = ptr.getCellRef().getRefId();
                    script.mTargetRef = ptr.getCellRef().getRefNum();
                }
                else if (MWBase::Environment::get().getWorld()->getPlayerPtr() == ptr)
                    script.mTargetId = ptr.getCellRef().getRefId();
            }
            return script;
        }

        ESM::GlobalScript operator()(const std::pair<ESM4::FormId, std::string>& pair) const
        {
            ESM::GlobalScript script;
            // TODO
            return script;
        }
    };

    struct PtrGettingVisitor
    {
        const MWWorld::Ptr* operator()(const MWWorld::Ptr &ptr) const
        {
            return &ptr;
        }

        const MWWorld::Ptr* operator()(const std::pair<ESM4::FormId, std::string> &pair) const
        {
            return nullptr;
        }
    };

    struct PtrResolvingVisitor
    {
        MWWorld::Ptr operator()(const MWWorld::Ptr &ptr) const
        {
            return ptr;
        }

        MWWorld::Ptr operator()(const std::pair<ESM4::FormId, std::string> &pair) const
        {
            if (pair.second.empty())
                return MWWorld::Ptr();
            else if(pair.first)
                return MWBase::Environment::get().getWorld()->searchPtrViaFormId(pair.first);
            return MWBase::Environment::get().getWorld()->searchPtr(pair.second, false);
        }
    };

    class MatchPtrVisitor
    {
        const MWWorld::Ptr& mPtr;
    public:
        MatchPtrVisitor(const MWWorld::Ptr& ptr) : mPtr(ptr) {}

        bool operator()(const MWWorld::Ptr &ptr) const
        {
            return ptr == mPtr;
        }

        bool operator()(const std::pair<ESM4::FormId, std::string>& pair) const
        {
            return false;
        }
    };

    struct IdGettingVisitor
    {
        std::string_view operator()(const MWWorld::Ptr& ptr) const
        {
            if(ptr.isEmpty())
                return {};
            return ptr.mRef->mRef.getRefId();
        }

        std::string_view operator()(const std::pair<ESM4::FormId, std::string>& pair) const
        {
            return pair.second;
        }
    };
}

namespace FOScript
{
    GlobalScriptDesc::GlobalScriptDesc() : mRunning (false) {}

    const MWWorld::Ptr* GlobalScriptDesc::getPtrIfPresent() const
    {
        return std::visit(PtrGettingVisitor(), mTarget);
    }

    MWWorld::Ptr GlobalScriptDesc::getPtr()
    {
        MWWorld::Ptr ptr = std::visit(PtrResolvingVisitor {}, mTarget);
        mTarget = ptr;
        return ptr;
    }

    std::string_view GlobalScriptDesc::getId() const
    {
        return std::visit(IdGettingVisitor {}, mTarget);
    }


    GlobalScripts::GlobalScripts (const MWWorld::ESMStore& store)
    : mStore (store)
    {}

    void GlobalScripts::addScript(std::string_view name, const MWWorld::Ptr& target)
    {
        std::string lowerName = ::Misc::StringUtils::lowerCase(name);
        const auto iter = mScripts.find(lowerName);

        if (iter==mScripts.end())
        {
            if (const ESM4::Script *script = mStore.get<ESM4::Script>().search(lowerName))
            {
                auto desc = std::make_shared<GlobalScriptDesc>();
                MWWorld::Ptr ptr = target;
                desc->mTarget = ptr;
                desc->mRunning = true;
                desc->mLocals.configure (*script);
                mScripts.insert (std::make_pair(lowerName, desc));
            }
            else
            {
                Log(Debug::Error) << "Failed to add global script " << name << ": script record not found";
            }
        }
        else if (!iter->second->mRunning)
        {
            iter->second->mRunning = true;
            MWWorld::Ptr ptr = target;
            iter->second->mTarget = ptr;
        }
    }

    void GlobalScripts::removeScript (std::string_view name)
    {
        const auto iter = mScripts.find (::Misc::StringUtils::lowerCase (name));

        if (iter!=mScripts.end())
            iter->second->mRunning = false;
    }

    bool GlobalScripts::isRunning (std::string_view name) const
    {
        const auto iter = mScripts.find (::Misc::StringUtils::lowerCase (name));

        if (iter==mScripts.end())
            return false;

        return iter->second->mRunning;
    }

    void GlobalScripts::run()
    {
        for (const auto& script : mScripts)
        {
            if (script.second->mRunning)
            {
                FOScript::InterpreterContext context(script.second);
                if (!MWBase::Environment::get().getTes4ScriptManager()->run(script.first, context))
                    script.second->mRunning = false;
            }
        }
    }

    void GlobalScripts::clear()
    {
        mScripts.clear();
    }

    void GlobalScripts::addStartup()
    {
        // make list of global scripts to be added
        std::vector<std::string> scripts;

        scripts.emplace_back("main");

        for (MWWorld::Store<ESM::StartScript>::iterator iter =
            mStore.get<ESM::StartScript>().begin();
            iter != mStore.get<ESM::StartScript>().end(); ++iter)
        {
            scripts.push_back (iter->mId);
        }

        // add scripts
        for (std::vector<std::string>::const_iterator iter (scripts.begin());
            iter!=scripts.end(); ++iter)
        {
            try
            {
                addScript (*iter);
            }
            catch (const std::exception& exception)
            {
                Log(Debug::Error)
                    << "Failed to add start script " << *iter << " because an exception has "
                    << "been thrown: " << exception.what();
            }
        }
    }

    int GlobalScripts::countSavedGameRecords() const
    {
        return mScripts.size();
    }

    Locals& GlobalScripts::getLocals(std::string_view name)
    {
        auto iter = mScripts.find(name);

        if (iter==mScripts.end())
        {
            const ESM4::Script *script = mStore.get<ESM4::Script>().find(name);

            auto desc = std::make_shared<GlobalScriptDesc>();
            desc->mLocals.configure (*script);

            iter = mScripts.emplace(name, desc).first;
        }

        return iter->second->mLocals;
    }

    const Locals* GlobalScripts::getLocalsIfPresent(std::string_view name) const
    {
        auto iter = mScripts.find(name);
        if (iter==mScripts.end())
            return nullptr;
        return &iter->second->mLocals;
    }

    void GlobalScripts::updatePtrs(const MWWorld::Ptr& base, const MWWorld::Ptr& updated)
    {
        MatchPtrVisitor visitor(base);
        for (const auto& script : mScripts)
        {
            if (std::visit (visitor, script.second->mTarget))
                script.second->mTarget = updated;
        }
    }
}
