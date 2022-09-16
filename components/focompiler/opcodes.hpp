#ifndef FOCOMPILER_OPCODES_H
#define FOCOMPILER_OPCODES_H

// fallout 3 and new vegas use a different script bytecode than morrowind
namespace FOCompiler
{
    namespace Ai
    {
        const unsigned int opcodeStartCombat = 0x2000322;
        const unsigned int opcodeIsInCombat = 0x2000323;
        const unsigned int opcodeGetIsCurrentPackage = 0x200032D;
        const unsigned int opcodeGetIsCurrentPackageExplicit = 0x200032E;
        const unsigned int opcodeGetInFaction = 0x200032F;
        const unsigned int opcodeGetInFactionExplicit = 0x2000330;
    }

    namespace Gui
    {
        const unsigned int opcodeShowMessage = 0x20031;
    }
    
    namespace Sound
    {
        const unsigned int opcodeSayTo = 0x20032;
        const unsigned int opcodeSayToExplicit = 0x20034;
        const unsigned int opcodeGetVoiceType = 0x200032B;
        const unsigned int opcodeGetVoiceTypeExplicit = 0x200032C;
    }

    namespace Transformation
    {
        const unsigned int opcodeMoveToMarker = 0x20033; // alias: MoveTo
    }

    namespace Stats
    {
        const unsigned int opcodeGetDead = 0x2000327;
        const unsigned int opcodeGetActorValue = 0x2000331;
        const unsigned int opcodeGetActorValueExplicit = 0x2000332;
    }

    namespace Misc
    {
        const unsigned int opcodeGetIsID = 0x2000328;
        const unsigned int opcodeGetIsIDExplicit = 0x2000329;
        const unsigned int opcodeGetQuestVariable = 0x200032A;
        const unsigned int opcodeGameMode = 0x2000333;
        const unsigned int opcodeSetQuestObject = 0x2000334;
        const unsigned int opcodeSetStage = 0x2000335;
        const unsigned int opcodeOnLoad = 0x2000336;
        const unsigned int opcodeModifyFaceGen = 0x2000337;
        const unsigned int opcodeModifyFaceGenExplicit
            = 0x2000338;
    }
}

#endif
