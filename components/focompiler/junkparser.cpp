#include "junkparser.hpp"

#include "scanner.hpp"

FOCompiler::JunkParser::JunkParser (Compiler::ErrorHandler& errorHandler, const Compiler::Context& context,
    int ignoreKeyword)
: Parser (errorHandler, context), mIgnoreKeyword (ignoreKeyword)
{}

bool FOCompiler::JunkParser::parseInt (int value, const Compiler::TokenLoc& loc, Scanner& scanner)
{
    scanner.putbackInt (value, loc);
    return false;
}

bool FOCompiler::JunkParser::parseFloat (float value, const Compiler::TokenLoc& loc, Scanner& scanner)
{
    scanner.putbackFloat (value, loc);
    return false;
}

bool FOCompiler::JunkParser::parseName (const std::string& name, const Compiler::TokenLoc& loc,
    Scanner& scanner)
{
    scanner.putbackName (name, loc);
    return false;
}

bool FOCompiler::JunkParser::parseKeyword (int keyword, const Compiler::TokenLoc& loc, Scanner& scanner)
{
    if (keyword==mIgnoreKeyword)
        reportWarning ("found junk (ignoring it)", loc);
    else
        scanner.putbackKeyword (keyword, loc);

    return false;
}

bool FOCompiler::JunkParser::parseSpecial (int code, const Compiler::TokenLoc& loc, Scanner& scanner)
{
    //if (code==Scanner::S_member)
        //reportWarning ("found junk (ignoring it)", loc);
    //else
        scanner.putbackSpecial (code, loc);

    return false;
}
