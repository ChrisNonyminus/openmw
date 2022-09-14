#include "scriptparser.hpp"

#include <iostream> // FIXME

#include "scanner.hpp"
#include "skipparser.hpp"
#include "../compiler/errorhandler.hpp"

namespace FOCompiler
{
    ScriptParser::ScriptParser (Compiler::ErrorHandler& errorHandler, const Compiler::Context& context,
        Compiler::Locals& locals, bool end)
    : Parser (errorHandler, context), mOutput (locals),
      mLineParser (errorHandler, context, locals, mOutput.getLiterals(), mOutput.getCode()),
      mControlParser (errorHandler, context, locals, mOutput.getLiterals()),
      mEnd (end)
    {}

    void ScriptParser::getCode (std::vector<Interpreter::Type_Code>& code) const
    {
        mOutput.getCode (code);
    }

    bool ScriptParser::parseName (const std::string& name, const Compiler::TokenLoc& loc,
        Scanner& scanner)
    {
        mLineParser.reset();
        if (mLineParser.parseName (name, loc, scanner))
            scanner.scan (mLineParser);

        return true;
    }

    bool ScriptParser::parseKeyword (int keyword, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (keyword==Scanner::K_while || keyword==Scanner::K_if || keyword==Scanner::K_elseif)
        {
            mControlParser.reset();
            if (mControlParser.parseKeyword (keyword, loc, scanner))
                scanner.scan (mControlParser);

            mControlParser.appendCode (mOutput.getCode());

            return true;
        }

        /// \todo add an option to disable this nonsense
        if (keyword==Scanner::K_endif)
        {
            // surplus endif
            // e.g. Dark18MotherScript: near the end one endif is not commented out
            //      MG18Script: looks like the main if condition line was deleted
            //      RufioDie_Script: some if conditions commented but not endif
            //      MQ14Script: an if condition missing
            //      Dark05AssassinatedScript: an if condition missing
            //      MS13Script: not clear if elseif is meant to be if
            //      MG09Script: an if condition missing
            //      ArenaGrandChampionMatchScript: an if condition missing (more than once)
            //      MGMageConversationFollowScript: a mess
            getErrorHandler().warning ("endif without matching if/elseif", loc);

            SkipParser skip (getErrorHandler(), getContext());
            scanner.scan (skip);
            return true;
        }

        if ((keyword == Scanner::K_end || keyword == Scanner::K_en) && mEnd)
        {
            return false;
        }

        mLineParser.reset();
        if (mLineParser.parseKeyword (keyword, loc, scanner))
            scanner.scan (mLineParser);

        return true;
    }

    bool ScriptParser::parseSpecial (int code, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (code==Scanner::S_newline) // empty line
            return true;

        if (code==Scanner::S_open) /// \todo Option to switch this off
        {
            scanner.putbackSpecial (code, loc);
            return parseKeyword (Scanner::K_if, loc, scanner);
        }

        mLineParser.reset();
        if (mLineParser.parseSpecial (code, loc, scanner))
            scanner.scan (mLineParser);

        return true;
    }

    void ScriptParser::parseEOF (Scanner& scanner)
    {
        if (mEnd)
            Parser::parseEOF (scanner);
    }

    void ScriptParser::reset(bool keepLocals)
    {
        mLineParser.reset();
        mOutput.clear(keepLocals);
    }

    LineParser& ScriptParser::getLineParser()
    {
        return mLineParser;
    }

    Compiler::Literals& ScriptParser::getLiterals()
    {
        return mOutput.getLiterals();
    }

    const std::vector<Interpreter::Type_Code>& ScriptParser::getCode() const
    {
        return mOutput.getCode();
    }
}
