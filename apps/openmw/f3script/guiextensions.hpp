#ifndef FOSCRIPT_GUIEXTENSIONS_H
#define FOSCRIPT_GUIEXTENSIONS_H

#include "usings.hpp"

namespace Interpreter
{
    class Interpreter;
}

namespace FOScript
{
    /// \brief GUI-related script functionality
    namespace Gui
    {        
        void installOpcodes (Interpreter::Interpreter& interpreter);
    }
}

#endif
