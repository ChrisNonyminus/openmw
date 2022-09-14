/*
  Copyright (C) 2016, 2018, 2020 cc9cii

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  cc9cii cc9c@iinet.net.au

  Much of the information on the data structures are based on the information
  from Tes4Mod:Mod_File_Format and Tes5Mod:File_Formats but also refined by
  trial & error.  See http://en.uesp.net/wiki for details.

*/
#ifndef ESM4_CLAS_H
#define ESM4_CLAS_H

#include <cstdint>
#include <string>

#include <components/esm/defs.hpp>

#include "formid.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Class
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_CLAS4;
        static std::string getRecordType() { return "Class (TES4)"; }
        struct Data
        {
            std::uint32_t attr;
        };

        struct FOAttributes
        {
            uint8_t strength;
            uint8_t perception;
            uint8_t endurance;
            uint8_t charisma;
            uint8_t intelligence;
            uint8_t agility;
            uint8_t luck;
        };

        enum FO_Services : uint32_t
        {
            FOServices_Weapons = 1 << 0,
            FOServices_Armor = 1 << 1,
            FOServices_Alcohol = 1 << 2,
            FOServices_Books = 1 << 3,
            FOServices_Food = 1 << 4,
            FOServices_Chems = 1 << 5,
            FOServices_Stimpaks = 1 << 6,
            FOServices_Lights = 1 << 7,
            FOServices_Misc = 1 << 10,
            FOServices_Potions = 1 << 13,
            FOServices_Training = 1 << 14,
            FOServices_Recharge = 1 << 16,
            FOServices_Repair = 1 << 17,
        };

        enum FOSkillEnum : int8_t
        {
            Skill_None = -1,
            Skill_Barter,
            Skill_BigGuns,
            Skill_EnergyWeapons,
            Skill_Explosives,
            Skill_Lockpick,
            Skill_Medicine,
            Skill_MeleeWeapons,
            Skill_Repair,
            Skill_Science,
            Skill_GunsSmallGuns,
            Skill_Sneak,
            Skill_Speech,
            Skill_SurvivalThrowing,
            Skill_Unarmed,
            Skill_MAX
        };

        enum class FO_ActorValues : int32_t
        {
            None = -1,

            // ai status
            Aggression,
            Confidence,
            Energy,
            Responsibility,
            Mood,

            // SPECIAL
            Strength,
            Perception,
            Endurance,
            Charisma,
            Intelligence,
            Agility,
            Luck,

            // derived
            ActionPoints,
            CarryWeight,
            CriticalChance,
            HealRate,
            Health,
            MeleeDamage,
            DamageResistance,
            PoisonResistance,
            RadResistance,
            SpeedMultiplier,
            Fatigue,
            Karma,
            XP,
            PerceptionCond,
            EnduranceCond,
            LeftAttackCond,
            RightAttackCond,
            LeftMobilityCond,
            RightMobilityCond,
            BrainCond,

            // skills
            Barter,
            BigGuns,
            EnergyWeapons,
            Explosives,
            Lockpick,
            Medicine,
            MeleeWeapons,
            Repair,
            Science,
            Guns_and_SmallGuns,
            Sneak,
            Speech,
            Survival_and_Throwing,
            Unarmed,

            // status?
            InventoryWeight,
            Paralysis,
            Invisibility,
            Chameleon,
            NightEye,
            Turbo,
            FireResistance,
            WaterBreathing,
            RadLevel,
            BloodyMess,
            UnarmedDamage,
            Assistance,
            ElectricResistance,
            FrostResistance,
            EnergyResistance,
            EMPResistance,

            IgnoreCrippledLimbs = 72,
            Dehydration,
            Hunger,
            SleepDeprivation,
            Damage
        };

#pragma pack(push, 1)
        struct FO_Data
        {
            FO_ActorValues mTagSkill1;
            FO_ActorValues mTagSkill2;
            FO_ActorValues mTagSkill3;
            FO_ActorValues mTagSkill4;
            uint32_t flags; // 1: playable | 2: guard
            FO_Services mBuysSellsAndServices;
            FOSkillEnum mTeaches;
            uint8_t mMaxTrainingLevel;
            uint8_t unused[2];
        };
#pragma pack(pop)
        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        std::string mFullName;
        std::string mDesc;
        std::string mIcon;
        Data mData;
        FOAttributes mSpecial;

        FO_Data mFOData;

        void load(ESM4::Reader& reader);
        //void save(ESM4::Writer& reader) const;

        //void blank();
    };
}

#endif // ESM4_CLAS_H
