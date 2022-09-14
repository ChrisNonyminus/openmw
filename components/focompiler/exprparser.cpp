#include "exprparser.hpp"

#include <stdexcept>
#include <cassert>
#include <algorithm>
#include <stack>
#include <iterator>
#include <iostream> // FIXME

#include <components/esm4/formid.hpp>

#include <components/misc/strings/algorithm.hpp>

#include "generator.hpp"
#include "scanner.hpp"
#include "../compiler/errorhandler.hpp"
#include "../compiler/locals.hpp"
#include "stringparser.hpp"
#include "../compiler/extensions.hpp"
#include "../compiler/context.hpp"
#include "discardparser.hpp"
#include "junkparser.hpp"

namespace FOCompiler
{
    int ExprParser::getPriority (char op) const
    {
        switch (op)
        {
            case '(':
                return 0;

            case '|': // FIXME: need more testing to confirm the priority
                return 1;

            case '&': // FIXME: need more testing to confirm the priority
                return 2;

            case 'e': // ==
            case 'n': // !=
            case 'l': // <
            case 'L': // <=
            case 'g': // <
            case 'G': // >=

                return 3;

            case '+':
            case '-':

                return 4;

            case '*':
            case '/':

                return 5;

            case 'm':

                return 6;
        }

        return 0;
    }

    char ExprParser::getOperandType (int Index) const
    {
        assert (!mOperands.empty());
        assert (Index>=0);
        assert (Index<static_cast<int> (mOperands.size()));
        return mOperands[mOperands.size()-1-Index];
    }

    char ExprParser::getOperator() const
    {
        assert (!mOperators.empty());
        return mOperators[mOperators.size()-1];
    }

    bool ExprParser::isOpen() const
    {
        return std::find (mOperators.begin(), mOperators.end(), '(')!=mOperators.end();
    }

    void ExprParser::popOperator()
    {
        assert (!mOperators.empty());
        mOperators.resize (mOperators.size()-1);
    }

    void ExprParser::popOperand()
    {
        assert (!mOperands.empty());
        mOperands.resize (mOperands.size()-1);
    }

    void ExprParser::replaceBinaryOperands()
    {
        char t1 = getOperandType (1);
        char t2 = getOperandType();

        popOperand();
        popOperand();

        if (t1==t2)
            mOperands.push_back (t1);
        else if (t1=='f' || t2=='f')
            mOperands.push_back ('f');
        else
            throw std::logic_error ("failed to determine result operand type");
    }

    void ExprParser::pop()
    {
        char op = getOperator();

        switch (op)
        {
            case 'm':

                Generator::negate (mCode, getOperandType());
                popOperator();
                break;

            case '+':

                Generator::add (mCode, getOperandType (1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            case '-':

                Generator::sub (mCode, getOperandType (1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            case '*':

                Generator::mul (mCode, getOperandType (1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            case '/':

                Generator::div (mCode, getOperandType (1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            case 'e':
            case 'n':
            case 'l':
            case 'L':
            case 'g':
            case 'G':

                Generator::compare (mCode, op, getOperandType (1), getOperandType());
                popOperator();
                popOperand();
                popOperand();
                mOperands.push_back ('l');
                break;

            case '&':

                Generator::boolean_and (mCode, getOperandType(1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            case '|':

                Generator::boolean_or (mCode, getOperandType(1), getOperandType());
                popOperator();
                replaceBinaryOperands();
                break;

            default:

                throw std::logic_error ("unknown operator");
        }
    }

    void ExprParser::pushIntegerLiteral (int value)
    {
        mNextOperand = false;
        mOperands.push_back ('l');
        Generator::pushInt (mCode, mLiterals, value);
    }

    void ExprParser::pushFloatLiteral (float value)
    {
        mNextOperand = false;
        mOperands.push_back ('f');
        Generator::pushFloat (mCode, mLiterals, value);
    }

    void ExprParser::pushBinaryOperator (char c)
    {
        while (!mOperators.empty() && getPriority (getOperator())>=getPriority (c))
            pop();

        mOperators.push_back (c);
        mNextOperand = true;
    }

    void ExprParser::close()
    {
        while (getOperator()!='(')
            pop();

        popOperator();
    }

    int ExprParser::parseArguments (const std::string& arguments, Scanner& scanner)
    {
        return parseArguments (arguments, scanner, mCode);
    }

    bool ExprParser::handleMemberAccess (const std::string& name)
    {
        mMemberOp = false;
        mRefOp = false; // FIXME: what a mess

        std::string name2 = Misc::StringUtils::lowerCase (name);
        std::string id = Misc::StringUtils::lowerCase (mExplicit);
        std::string scriptId = "";

        std::pair<char, bool> type = getContext().getMemberType (name2, id);

        if (type.first!=' ')
        {
            if (scriptId.empty())
                Generator::fetchMember (mCode, mLiterals, type.first, name2, id, !type.second);
            else
                Generator::fetchMember (mCode, mLiterals, type.first, name2, scriptId, !type.second);

            mNextOperand = false;
            mExplicit.clear();
            mOperands.push_back (type.first=='f' ? 'f' : 'l');
            return true;
        }

        return false;
    }

    ExprParser::ExprParser (Compiler::ErrorHandler& errorHandler, const Compiler::Context& context, Compiler::Locals& locals,
        Compiler::Literals& literals, bool argument)
    : Parser (errorHandler, context), mLocals (locals), mLiterals (literals),
      mNextOperand (true), mFirst (true), mArgument (argument), mExplicit(""), mRefOp (false), mMemberOp (false),
      mPotentialExplicit(""), mPotentialReference(0), mPotentialAIPackage(-1)
    {}

    bool ExprParser::parseInt (int value, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (!mExplicit.empty())
            return Parser::parseInt (value, loc, scanner);

        mFirst = false;

        if (mNextOperand)
        {
            start();

            pushIntegerLiteral (value);
            mTokenLoc = loc;
            return true;
        }
        else
        {
            // no comma was used between arguments
            scanner.putbackInt (value, loc);
            return false;
        }
    }

    bool ExprParser::parseFloat (float value, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (!mExplicit.empty())
            return Parser::parseFloat (value, loc, scanner);

        mFirst = false;

        if (mNextOperand)
        {
            start();

            pushFloatLiteral (value);
            mTokenLoc = loc;
            return true;
        }
        else
        {
            // no comma was used between arguments
            scanner.putbackFloat (value, loc);
            return false;
        }
    }

    bool ExprParser::parseName (const std::string& name, const Compiler::TokenLoc& loc,
        Scanner& scanner)
    {
        if (!mExplicit.empty())
        {
            // FIXME: if we reach here must have not found any extension "keyword", so mRefOp is really mMemberOp
            // but the logic is hard to follow so needs a cleanup
            if ((mMemberOp || mRefOp) && handleMemberAccess (name))
                return true;

            return Parser::parseName (name, loc, scanner);
        }

        mFirst = false;

        if (mNextOperand)
        {
            start();

            std::string name2 = Misc::StringUtils::lowerCase (name);

            char type = mLocals.getType (name2);

            if (type != ' ')
            {
                Generator::fetchLocal (mCode, type, mLocals.getIndex (name2));
                mNextOperand = false;
                mOperands.push_back (type=='f' ? 'f' : 'l');

                if (type == 'r')
                    mPotentialExplicit = name;

                return true;
            }

            type = getContext().getGlobalType (name2);

            if (type!=' ')
            {
                Generator::fetchGlobal (mCode, mLiterals, type, name2);
                mNextOperand = false;
                mOperands.push_back (type=='f' ? 'f' : 'l');
                return true;
            }
#if 0
            // die in a fire, Morrowind script compiler!
            if (const Compiler::Extensions *extensions = getContext().getExtensions())
            {
                if (getContext().isJournalId (name2))
                {
                    // JournalID used as an argument. Use the index of that JournalID
                    Generator::pushString (mCode, mLiterals, name2);
                    int keyword = extensions->searchKeyword ("getjournalindex");
                    extensions->generateFunctionCode (keyword, mCode, mLiterals, mExplicit, 0);
                    mNextOperand = false;
                    mOperands.push_back ('l');

                    return true;
                }
            }
#endif
            if (ESM4::FormId formId = getContext().getReference(name2))
            {
                mPotentialExplicit = name; // to make parseSpecial check for S_ref
                mPotentialReference = formId;

                return true;
            }
            else
            {
                int32_t packId = getContext().getAIPackage(name2);
                if (packId != -1)
                {
                    mPotentialAIPackage = packId;

                    return true;
                }
            }
        }
        else
        {
            // no comma was used between arguments
            scanner.putbackName (name, loc);
            return false;
        }

        return Parser::parseName (name, loc, scanner);
    }

    bool ExprParser::parseKeyword (int keyword, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (const Compiler::Extensions *extensions = getContext().getExtensions())
        {
            std::string argumentType; // ignored
            bool hasExplicit = false; // ignored
            if (extensions->isInstruction (keyword, argumentType, hasExplicit))
            {
                // pretend this is not a keyword
                std::string name = loc.mLiteral;
                if (name.size()>=2 && name[0]=='"' && name[name.size()-1]=='"')
                    name = name.substr (1, name.size()-2);
                return parseName (name, loc, scanner);
            }
        }

        // FIXME: not sure if K_int needs to be added
        // FIXME: not sure if K_endif_broken needs to be added
        if (keyword==Scanner::K_end || keyword==Scanner::K_begin ||
            keyword==Scanner::K_en ||
            keyword==Scanner::K_short || keyword==Scanner::K_long ||
            keyword==Scanner::K_float || keyword==Scanner::K_if ||
            keyword==Scanner::K_endif || keyword==Scanner::K_else ||
            keyword==Scanner::K_elseif || keyword==Scanner::K_while ||
            keyword==Scanner::K_endwhile || keyword==Scanner::K_return ||
            keyword==Scanner::K_messagebox || keyword==Scanner::K_set ||
            keyword==Scanner::K_message || keyword==Scanner::K_ref ||
            keyword==Scanner::K_to || keyword==Scanner::K_startscript ||
            keyword==Scanner::K_stopscript || keyword==Scanner::K_enable ||
            keyword==Scanner::K_disable)
        {
            return parseName (loc.mLiteral, loc, scanner);
        }

        mFirst = false;

        if (!mExplicit.empty())
        {
            if (mRefOp && mNextOperand)
            {
                if (keyword==Scanner::K_getdisabled)
                {
                    start();

                    mTokenLoc = loc;

                    Generator::getDisabled (mCode, mLiterals, mExplicit);
                    mOperands.push_back ('l');
                    mExplicit.clear();
                    mRefOp = false;

                    std::vector<Interpreter::Type_Code> ignore;
                    parseArguments ("x", scanner, ignore);

                    mNextOperand = false;
                    return true;
                }
                else if (keyword==Scanner::K_getdistance)
                {
                    start();

                    mTokenLoc = loc;
                    parseArguments ("c", scanner);

                    Generator::getDistance (mCode, mLiterals, mExplicit);
                    mOperands.push_back ('f');
                    mExplicit.clear();
                    mRefOp = false;

                    mNextOperand = false;
                    return true;
                }
                else if (keyword==Scanner::K_scriptrunning)
                {
                    start();

                    mTokenLoc = loc;
                    parseArguments ("c", scanner);

                    Generator::scriptRunning (mCode);
                    mOperands.push_back ('l');

                    mExplicit.clear();
                    mRefOp = false;
                    mNextOperand = false;
                    return true;
                }

                // check for custom extensions
                if (const Compiler::Extensions *extensions = getContext().getExtensions())
                {
                    char returnType;
                    std::string argumentType;

                    bool hasExplicit = true;
                    if (extensions->isFunction (keyword, returnType, argumentType, hasExplicit))
                    {
                        if (!hasExplicit)
                        {
                            getErrorHandler().warning ("stray explicit reference (ignoring it)", loc);
                            mExplicit.clear();
                        }

                        start();

                        mTokenLoc = loc;
                        int optionals = parseArguments (argumentType, scanner);

                        int localIndex = -1;
                        if (hasExplicit && !mExplicit.empty())
                            localIndex = mLocals.getIndex(mExplicit);

                        extensions->generateFunctionCode (keyword, mCode, mLiterals, mExplicit, optionals);
                        mOperands.push_back (returnType);
                        mExplicit.clear();
                        mRefOp = false;

                        mNextOperand = false;
                        return true;
                    }

                    if (extensions->isInstruction (keyword, argumentType, hasExplicit))
                    {
                        if (!hasExplicit)
                        {
                            getErrorHandler().warning ("stray explicit reference (ignoring it)", loc);
                            mExplicit.clear();
                        }

                        start();

                        int optionals = parseArguments (argumentType, scanner);

                        int localIndex = -1;
                        if (hasExplicit && !mExplicit.empty())
                            localIndex = mLocals.getIndex(mExplicit);

                        extensions->generateInstructionCode(keyword, mCode, mLiterals, mExplicit, optionals);
                        mOperands.push_back('l'); // FIXME: ?? is this needed for an instruction?
                        mExplicit.clear();
                        mRefOp = false;

                        mNextOperand = false;
                        return true;
                    }
                }
            }

            return Parser::parseKeyword (keyword, loc, scanner);
        }

        if (mNextOperand)
        {
            if (keyword==Scanner::K_getsquareroot)
            {
                start();

                mTokenLoc = loc;
                parseArguments ("f", scanner);

                Generator::squareRoot (mCode);
                mOperands.push_back ('f');

                mNextOperand = false;
                return true;
            }
#if 0  // handled as a function
            else if (keyword==Scanner::K_menumode)
            {
                start();

                mTokenLoc = loc;

                Generator::menuMode (mCode);
                mOperands.push_back ('l');

                mNextOperand = false;
                return true;
            }
#endif
            else if (keyword==Scanner::K_random)
            {
                start();

                mTokenLoc = loc;
                parseArguments ("l", scanner);

                Generator::random (mCode);
                mOperands.push_back ('l');

                mNextOperand = false;
                return true;
            }
            else if (keyword==Scanner::K_scriptrunning)
            {
                start();

                mTokenLoc = loc;
                parseArguments ("c", scanner);

                Generator::scriptRunning (mCode);
                mOperands.push_back ('l');

                mNextOperand = false;
                return true;
            }
            else if (keyword==Scanner::K_getdistance)
            {
                start();

                mTokenLoc = loc;
                parseArguments ("c", scanner);

                Generator::getDistance (mCode, mLiterals, "");
                mOperands.push_back ('f');

                mNextOperand = false;
                return true;
            }
            else if (keyword==Scanner::K_getsecondspassed)
            {
                start();

                mTokenLoc = loc;

                Generator::getSecondsPassed (mCode);
                mOperands.push_back ('f');

                mNextOperand = false;
                return true;
            }
            else if (keyword==Scanner::K_getdisabled)
            {
                start();

                mTokenLoc = loc;

                Generator::getDisabled (mCode, mLiterals, "");
                mOperands.push_back ('l');

                std::vector<Interpreter::Type_Code> ignore;
                parseArguments ("x", scanner, ignore);

                mNextOperand = false;
                return true;
            }
            else
            {
                // check for custom extensions
                if (const Compiler::Extensions *extensions = getContext().getExtensions())
                {
                    start();

                    char returnType;
                    std::string argumentType;

                    bool hasExplicit = false;

                    if (extensions->isFunction (keyword, returnType, argumentType, hasExplicit))
                    {
                        mTokenLoc = loc;

                        //std::cout << "function " << loc.mLiteral << std::endl; // FIXME: temp testing

                        int optionals = parseArguments (argumentType, scanner);

                        extensions->generateFunctionCode (keyword, mCode, mLiterals, "", optionals);
                        mOperands.push_back (returnType);

                        mNextOperand = false;
                        return true;
                    }
                }
            }
        }
        else
        {
            // no comma was used between arguments
            scanner.putbackKeyword (keyword, loc);
            return false;
        }

        return Parser::parseKeyword (keyword, loc, scanner);
    }

    bool ExprParser::parseSpecial (int code, const Compiler::TokenLoc& loc, Scanner& scanner)
    {
        if (!mExplicit.empty())
        {
            if (mRefOp && code==Scanner::S_open)
            {
                /// \todo add option to disable this workaround
                mOperators.push_back ('(');
                mTokenLoc = loc;
                return true;
            }

            if (!mRefOp && code==Scanner::S_ref_or_member)
            {
                mRefOp = true;
                return true;
            }
#if 0
            if (!mMemberOp && code==Scanner::S_ref_or_member)
            {
                mMemberOp = true;
                return true;
            }
#endif
            return Parser::parseSpecial (code, loc, scanner);
        }

        if (code==Scanner::S_comma)
        {
            mTokenLoc = loc;

            if (mFirst)
            {
                // leading comma
                mFirst = false;
                return true;
            }

            // end marker
            scanner.putbackSpecial (code, loc);
            return false;
        }

        mFirst = false;

        if (!mRefOp && code == Scanner::S_ref_or_member)
        {
            mExplicit = mPotentialExplicit; // FIXME: convert to lowercase?
            mPotentialExplicit.clear();
            mNextOperand = true;
            mRefOp = true;
            return true;
        }

        // FIXME: tidy up logic (do this in parseName()?)
        if (!mPotentialExplicit.empty())
        {
            mPotentialExplicit.clear();
            if (mPotentialReference != 0)
            {
                pushIntegerLiteral(mPotentialReference); // FIXME: formId or ai package type
                mPotentialReference = 0;
            }
        }

        // See SE05QuestScript
        //     if ( HerdirTarget.GetCurrentAIPackage != SE05TortureHoldPosition )
        if (mPotentialAIPackage != -1)
        {
            pushIntegerLiteral(mPotentialAIPackage); // ai package type
            mPotentialAIPackage = -1;
        }
        // falls through

        if (code==Scanner::S_newline)
        {
            // end marker
            mTokenLoc = loc;
            scanner.putbackSpecial (code, loc);
            return false;
        }

        if (code==Scanner::S_minus && mNextOperand)
        {
            // unary
            mOperators.push_back ('m');
            mTokenLoc = loc;
            return true;
        }

        if (code ==Scanner::S_plus && mNextOperand)
        {
            // Also unary, but +, just ignore it
            mTokenLoc = loc;
            return true;
        }

        if (code==Scanner::S_open)
        {
            if (mNextOperand)
            {
                mOperators.push_back ('(');
                mTokenLoc = loc;
                return true;
            }
            else
            {
                // no comma was used between arguments
                scanner.putbackSpecial (code, loc);
                return false;
            }
        }

        if (code==Scanner::S_close && !mNextOperand)
        {
            if (isOpen())
            {
                close();
                return true;
            }

            mTokenLoc = loc;
            scanner.putbackSpecial (code, loc);
            return false;
        }

        if (!mNextOperand)
        {
            mTokenLoc = loc;
            char c = 0; // comparison

            switch (code)
            {
                case Scanner::S_plus: c = '+'; break;
                case Scanner::S_minus: c = '-'; break;
                case Scanner::S_mult: pushBinaryOperator ('*'); return true;
                case Scanner::S_div: pushBinaryOperator ('/'); return true;
                case Scanner::S_and: pushBinaryOperator('&'); return true;
                case Scanner::S_or: pushBinaryOperator('|'); return true;
                case Scanner::S_cmpEQ: c = 'e'; break;
                case Scanner::S_cmpNE: c = 'n'; break;
                case Scanner::S_cmpLT: c = 'l'; break;
                case Scanner::S_cmpLE: c = 'L'; break;
                case Scanner::S_cmpGT: c = 'g'; break;
                case Scanner::S_cmpGE: c = 'G'; break;
            }

            if (c)
            {
                if (mArgument && !isOpen())
                {
                    // expression ends here
                    // Thank you Morrowind for this rotten syntax :(
                    scanner.putbackSpecial (code, loc);
                    return false;
                }

                pushBinaryOperator (c);
                return true;
            }
        }

        return Parser::parseSpecial (code, loc, scanner);
    }

    void ExprParser::reset()
    {
        mOperands.clear();
        mOperators.clear();
        mNextOperand = true;
        mCode.clear();
        mFirst = true;
        mExplicit.clear();
        mRefOp = false;
        mMemberOp = false;
        Parser::reset();
    }

    char ExprParser::append (std::vector<Interpreter::Type_Code>& code)
    {
        if (mOperands.empty() && mOperators.empty())
        {
            getErrorHandler().error ("missing expression", mTokenLoc);
            return 'l';
        }

        if (mNextOperand || mOperands.empty())
        {
            getErrorHandler().error ("syntax error in expression", mTokenLoc);
            return 'l';
        }

        while (!mOperators.empty())
            pop();

        std::copy (mCode.begin(), mCode.end(), std::back_inserter (code));

        //assert (mOperands.size()==1);
        return mOperands[0];
    }

    int ExprParser::parseArguments (const std::string& arguments, Scanner& scanner,
        std::vector<Interpreter::Type_Code>& code, int ignoreKeyword)
    {
        bool optional = false;
        int optionalCount = 0;

        ExprParser parser (getErrorHandler(), getContext(), mLocals, mLiterals, true);
        StringParser stringParser (getErrorHandler(), getContext(), mLocals, mLiterals);
        DiscardParser discardParser (getErrorHandler(), getContext());
        JunkParser junkParser (getErrorHandler(), getContext(), ignoreKeyword);

        std::stack<std::vector<Interpreter::Type_Code> > stack;

        for (std::string::const_iterator iter (arguments.begin()); iter!=arguments.end();
            ++iter)
        {
            if (*iter=='/')
            {
                optional = true;
            }
            else if (*iter=='S' || *iter=='c' || *iter=='x')
            {
                stringParser.reset();

                if (optional || *iter=='x')
                    stringParser.setOptional (true);

                if (*iter=='c') stringParser.smashCase();
                scanner.scan (stringParser);

                if (optional && stringParser.isEmpty())
                    break;

                if (*iter!='x')
                {
                    std::vector<Interpreter::Type_Code> tmp;
                    stringParser.append (tmp);

                    stack.push (tmp);

                    if (optional)
                        ++optionalCount;
                }
            }
            else if (*iter=='X')
            {
                parser.reset();

                parser.setOptional (true);

                scanner.scan (parser);

                if (parser.isEmpty())
                    break;
            }
            else if (*iter=='z')
            {
                discardParser.reset();
                discardParser.setOptional (true);

                scanner.scan (discardParser);

                if (discardParser.isEmpty())
                    break;
            }
            else if (*iter=='j')
            {
                /// \todo disable this when operating in strict mode
                junkParser.reset();

                scanner.scan (junkParser);
            }
            else
            {
                parser.reset();

                if (optional)
                    parser.setOptional (true);

                scanner.scan (parser);

                if (optional && parser.isEmpty())
                    break;

                std::vector<Interpreter::Type_Code> tmp;

                char type = parser.append (tmp);

                if (type!=*iter)
                    Generator::convert (tmp, type, *iter);

                stack.push (tmp);

                if (optional)
                    ++optionalCount;
            }
        }

        while (!stack.empty())
        {
            std::vector<Interpreter::Type_Code>& tmp = stack.top();

            std::copy (tmp.begin(), tmp.end(), std::back_inserter (code));

            stack.pop();
        }

        return optionalCount;
    }
}
