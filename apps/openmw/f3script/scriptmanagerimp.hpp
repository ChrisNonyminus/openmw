#ifndef FOSCRIPT_SCRIPTMANAGER_H
#define FOSCRIPT_SCRIPTMANAGER_H

#include <map>
#include <set>
#include <string>

#include <components/compiler/streamerrorhandler.hpp>
#include <components/focompiler/fileparser.hpp>
#include <components/focompiler/controlparser.hpp>

#include <components/focompiler/extensions0.hpp>

#include <components/interpreter/interpreter.hpp>
#include <components/interpreter/types.hpp>

#include "../mwbase/scriptmanager.hpp"

#include "globalscripts.hpp"

namespace MWWorld
{
    class ESMStore;
}

namespace Compiler
{
    class Context;
}

namespace Interpreter
{
    class Context;
    class Interpreter;
}

namespace FOScript
{
    class ScriptManager : public MWBase::ScriptManager
    {
            Compiler::StreamErrorHandler mErrorHandler;
            const MWWorld::ESMStore& mStore;
            Compiler::Context& mCompilerContext;
            FOCompiler::FileParser mParser;
            Interpreter::Interpreter mInterpreter;
            bool mOpcodesInstalled;

            typedef std::map<std::string, std::pair<FOCompiler::ControlParser::Codes, Compiler::Locals>> CompiledScriptCollection;
            std::map<std::string, CompiledScriptCollection, ::Misc::StringUtils::CiComp> mScripts;
            GlobalScripts mGlobalScripts;
            std::unordered_map<std::string, Compiler::Locals, ::Misc::StringUtils::CiHash, ::Misc::StringUtils::CiEqual> mOtherLocals;
            std::vector<std::string> mScriptBlacklist;

        public:

            ScriptManager (const MWWorld::ESMStore& store,
                Compiler::Context& compilerContext, int warningsMode,
                const std::vector<std::string>& scriptBlacklist);

            void clear() override;

            bool run(std::string_view name, Interpreter::Context& interpreterContext, const std::string& blockType = "") override;
            ///< Run the script with the given name (compile first, if not compiled yet)

            bool compile(std::string_view name) override;
            ///< Compile script with the given namen
            /// \return Success?

            std::pair<int, int> compileAll() override;
            ///< Compile all scripts
            /// \return count, success

            const Compiler::Locals& getLocals(std::string_view name) override;
            ///< Return locals for script \a name.

            GlobalScripts& getFOGlobalScripts() override;

            MWScript::GlobalScripts& getGlobalScripts() override
            {
                throw std::runtime_error("Tried to get mw global scripts from foscript manager");
            }

            const Compiler::Extensions& getExtensions() const override;
    };
}

#endif
