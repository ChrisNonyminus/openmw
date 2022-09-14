#ifndef FOCOMPILER_EXTENSIONS0_H
#define FOCOMPILER_EXTENSIONS0_H

#include "usings.hpp"

#include <components/esm4/script.hpp>

namespace FOCompiler
{
    extern std::map<ESM4::FunctionIndices, std::string> sFunctionIndices;
    void registerExtensions (Extensions& extensions, bool consoleOnly = false);

    namespace Ai
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Animation
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Cell
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Console
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Container
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Control
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Dialogue
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Gui
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Misc
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Sky
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Sound
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Stats
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace Transformation
    {
        void registerExtensions (Extensions& extensions);
    }

    namespace User
    {
        void registerExtensions (Extensions& extensions);
    }
} // namespace FOCompiler


#endif
