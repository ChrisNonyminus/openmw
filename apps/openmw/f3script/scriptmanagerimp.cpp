#include "scriptmanagerimp.hpp"

#include <cassert>
#include <sstream>
#include <exception>
#include <algorithm>

#include <components/debug/debuglog.hpp>

#include <components/esm3/loadscpt.hpp>

#include <components/misc/strings/lower.hpp>

#include <components/focompiler/scanner.hpp>
#include <components/focompiler/controlparser.hpp>
#include <components/compiler/context.hpp>
#include <components/compiler/exception.hpp>
#include <components/focompiler/quickfileparser.hpp>

#include "../mwworld/esmstore.hpp"

#include "extensions.hpp"
#include "interpretercontext.hpp"

namespace FOScript
{
    ScriptManager::ScriptManager (const MWWorld::ESMStore& store,
        Compiler::Context& compilerContext, int warningsMode,
        const std::vector<std::string>& scriptBlacklist)
    : mErrorHandler(), mStore (store),
      mCompilerContext (compilerContext), mParser (mErrorHandler, mCompilerContext),
      mOpcodesInstalled (false), mGlobalScripts (store)
    {
        mErrorHandler.setWarningsMode (warningsMode);

        mScriptBlacklist.resize (scriptBlacklist.size());

        std::transform (scriptBlacklist.begin(), scriptBlacklist.end(),
            mScriptBlacklist.begin(), ::Misc::StringUtils::lowerCase);
        std::sort (mScriptBlacklist.begin(), mScriptBlacklist.end());
    }

    bool ScriptManager::compile(std::string_view name)
    {
        mParser.reset();
        mErrorHandler.reset();

        if (const ESM4::Script *script = mStore.get<ESM4::Script>().search (name))
        {
            mErrorHandler.setContext(script->mEditorId);

            bool Success = true;
            try
            {
                std::istringstream input (script->mScript.scriptSource);

                FOCompiler::Scanner scanner(mErrorHandler, input, mCompilerContext.getExtensions());

                scanner.scan (mParser);

                if (!mErrorHandler.isGood())
                    Success = false;
            }
            catch (const Compiler::SourceException&)
            {
                // error has already been reported via error handler
                Success = false;
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "Error: An exception has been thrown: " << error.what();
                Success = false;
            }

            if (!Success)
            {
                Log(Debug::Error) << "Error: script compiling failed: " << name;
            }

            if (Success)
            {
                CompiledScriptCollection script;
                mParser.getCode(script);
                mScripts[std::string(name)] = std::move(script); 
                return true;
            }
        }

        return false;
    }

    bool ScriptManager::run(std::string_view name, Interpreter::Context& interpreterContext, const std::string& blockType)
    {
        // compile script
        auto iter = mScripts.find(std::string(name));

        if (iter==mScripts.end())
        {
            if (!compile (name))
            {
                // failed -> ignore script from now on.
                CompiledScriptCollection empty;
                mScripts.emplace(name, empty);
                return false;
            }

            iter = mScripts.find (std::string(name));
            assert (iter!=mScripts.end());
        }

        // execute script

        std::string blockName = "gamemode";
        if (!blockType.empty())
            blockName = blockType;

        /*if (blockName == "gamemode" && interpreterContext.menuMode())
            return;*/ // todo

        auto iter2 = iter->second.find(blockName);

        if (iter2 != iter->second.end() && !iter2->second.first.empty())
        {
            try 
            {
                if (!mOpcodesInstalled)
                {
                    installOpcodes(mInterpreter);
                    mOpcodesInstalled = true;
                }

                mInterpreter.run(&iter2->second.first[0], iter2->second.first.size(), interpreterContext);

                return true;
            }
            catch (const std::exception& e)
            {
                Log(Debug::Error) << "Execution of TES4/FO3/FNV script known as '" << name << "' failed.\n\tReason: " << e.what();
                iter2->second.first.clear();
            }
        }
        return false;
    }

    void ScriptManager::clear()
    {
        // for (auto& script : mScripts)
        // {
        //     //script.second.mInactive.clear();
        // }
        mScripts.clear();

        mGlobalScripts.clear();
    }

    std::pair<int, int> ScriptManager::compileAll()
    {
        int count = 0;
        int success = 0;

        for (auto& script : mStore.get<ESM4::Script>())
        {
            if (!std::binary_search (mScriptBlacklist.begin(), mScriptBlacklist.end(),
                ::Misc::StringUtils::lowerCase(script.mEditorId)))
            {
                ++count;

                if (compile(script.mEditorId))
                    ++success;
            }
        }

        return std::make_pair (count, success);
    }

    const Compiler::Locals& ScriptManager::getLocals(std::string_view name)
    {
        {
            auto iter = mScripts.find(std::string(name));

            if (iter!=mScripts.end() && iter->second.size() > 0)
                return iter->second.begin()->second.second; // todo: this seems wrong.
        }

        {
            auto iter = mOtherLocals.find(std::string(name));

            if (iter!=mOtherLocals.end())
                return iter->second;
        }

        if (const ESM4::Script* script = mStore.get<ESM4::Script>().search(name))
        {
            Compiler::Locals locals;

            const Compiler::ContextOverride override(mErrorHandler, std::string{name} + "[local variables]");

            std::istringstream stream (script->mScript.scriptSource);
            FOCompiler::QuickFileParser parser (mErrorHandler, mCompilerContext, locals);
            FOCompiler::Scanner scanner(mErrorHandler, stream, mCompilerContext.getExtensions());
            try
            {
                scanner.scan (parser);
            }
            catch (const Compiler::SourceException&)
            {
                // error has already been reported via error handler
                locals.clear();
            }
            catch (const std::exception& error)
            {
                Log(Debug::Error) << "Error: An exception has been thrown: " << error.what();
                locals.clear();
            }

            auto iter = mOtherLocals.emplace(name, locals).first;

            return iter->second;
        }

        throw std::logic_error("script " + std::string{name} + " does not exist");
    }

    GlobalScripts& ScriptManager::getFOGlobalScripts()
    {
        return mGlobalScripts;
    }

    const Compiler::Extensions& ScriptManager::getExtensions() const
    {
        return *mCompilerContext.getExtensions();
    }
}
