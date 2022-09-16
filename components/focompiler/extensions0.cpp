#include "extensions0.hpp"

#include "opcodes.hpp"

namespace FOCompiler
{
    std::map<ESM4::FunctionIndices, std::string> sFunctionIndices;
    void registerExtensions (Extensions& extensions, bool consoleOnly)
    {
        Ai::registerExtensions (extensions);
        Animation::registerExtensions (extensions);
        Cell::registerExtensions (extensions);
        Container::registerExtensions (extensions);
        Control::registerExtensions (extensions);
        Dialogue::registerExtensions (extensions);
        Gui::registerExtensions (extensions);
        Misc::registerExtensions (extensions);
        Sky::registerExtensions (extensions);
        Sound::registerExtensions (extensions);
        Stats::registerExtensions (extensions);
        Transformation::registerExtensions (extensions);

        if (consoleOnly)
        {
            Console::registerExtensions (extensions);
            User::registerExtensions (extensions);
        }
    }

    namespace Ai
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerInstruction("startcombat", "c", opcodeStartCombat);
            extensions.registerFunction("isincombat", 'l', "", opcodeIsInCombat);
            extensions.registerFunction("getiscurrentpackage", 'l', "c", opcodeGetIsCurrentPackage, opcodeGetIsCurrentPackageExplicit);
            extensions.registerFunction("getinfaction", 'l', "c", opcodeGetInFaction, opcodeGetInFactionExplicit);
            sFunctionIndices[ESM4::FUN_IsInCombat] = "isincombat";
            sFunctionIndices[ESM4::FUN_GetIsCurrentPackage] = "getiscurrentpackage";
            sFunctionIndices[ESM4::FUN_GetInFaction] = "getinfaction";
        }
    }

    namespace Animation
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Cell
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Console
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Container
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Control
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Dialogue
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Gui
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerInstruction("showmessage", "c/f/f", opcodeShowMessage);
        }
    }

    namespace Misc
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerFunction("getisid", 'l', "c", opcodeGetIsID, opcodeGetIsIDExplicit);
            sFunctionIndices[ESM4::FUN_GetIsID] = "getisid";
            extensions.registerFunction("getquestvariable", 'l', "cc", opcodeGetQuestVariable);
            sFunctionIndices[ESM4::FUN_GetQuestVariable] = "getquestvariable";
            extensions.registerFunction("gamemode", 'l', "", opcodeGameMode);
            extensions.registerFunction("onload", 'l', "", opcodeOnLoad);
            extensions.registerInstruction("setquestobject", "cl", opcodeSetQuestObject);
            extensions.registerInstruction("setstage", "cl", opcodeSetStage);
            extensions.registerInstruction("modifyfacegen", "cll", opcodeModifyFaceGen, opcodeModifyFaceGenExplicit);
            extensions.registerInstruction("mfg", "cll", opcodeModifyFaceGen, opcodeModifyFaceGenExplicit);
        }
    }

    namespace Sky
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }

    namespace Sound
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerInstruction("sayto", "cc/l/l", opcodeSayTo, opcodeSayToExplicit);
            extensions.registerFunction("getisvoicetype", 'l', "c", opcodeGetVoiceType, opcodeGetVoiceTypeExplicit);
            sFunctionIndices[ESM4::FUN_GetIsVoiceType] = "getisvoicetype";
        }
    }

    namespace Stats
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerFunction("getdead", 'l', "", opcodeGetDead);
            sFunctionIndices[ESM4::FUN_GetDead] = "getdead";
            extensions.registerFunction("getactorvalue", 'l', "c", opcodeGetActorValue, opcodeGetActorValueExplicit);
            extensions.registerFunction("getav", 'l', "c", opcodeGetActorValue, opcodeGetActorValueExplicit);
            sFunctionIndices[ESM4::FUN_GetActorValue] = "getactorvalue";
        }
    }

    namespace Transformation
    {
        void registerExtensions (Extensions& extensions)
        {
            extensions.registerInstruction("movetomarker", "c/f/f/f", opcodeMoveToMarker);
            extensions.registerInstruction("moveto", "c/f/f/f", opcodeMoveToMarker);
        }
    }

    namespace User
    {
        void registerExtensions (Extensions& extensions)
        {
        }
    }
}
