#ifndef FOSCRIPT_EXTENSIONS_H
#define FOSCRIPT_EXTENSIONS_H

#include "usings.hpp"

#include <map>

#include <components/esm4/script.hpp>

namespace Interpreter
{
    class Interpreter;
}

namespace FOScript
{
    namespace Ai
    {
        void installOpcodes(Interpreter::Interpreter& interpreter);
    }
    namespace Misc
    {
        void installOpcodes(Interpreter::Interpreter& interpreter);
    }
    namespace Stats
    {
        void installOpcodes(Interpreter::Interpreter& interpreter);
    }
    namespace Sound
    {
        void installOpcodes(Interpreter::Interpreter& interpreter);
    }
    void installOpcodes (Interpreter::Interpreter& interpreter, bool consoleOnly = false);
    ///< \param consoleOnly include console only opcodes
}

#endif
