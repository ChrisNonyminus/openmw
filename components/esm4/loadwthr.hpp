#ifndef ESM4_WTHR_H
#define ESM4_WTHR_H

#include <cstdint>
#include <string>
#include <vector>

#include <components/esm/defs.hpp>

#include "common.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Weather
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_WTHR4;
#pragma pack(push, 1)
        struct TimeOfDayColors
        {
            uint32_t sunrise;
            uint32_t day;
            uint32_t sunset;
            uint32_t night;
            uint32_t highnoon;
            uint32_t midnight;
        };
        struct NAM0
        {
            TimeOfDayColors skyUpper;
            TimeOfDayColors fog;
            TimeOfDayColors unused1;
            TimeOfDayColors ambient;
            TimeOfDayColors sunlight;
            TimeOfDayColors sun;
            TimeOfDayColors stars;
            TimeOfDayColors skyLower;
            TimeOfDayColors horizon;
            TimeOfDayColors unused2;
        };
        struct FNAM
        {
            float dayNear;
            float dayFar;
            float nightNear;
            float nightFar;
            float dayPower;
            float nightPower;
        };
        struct WeatherData
        {
            uint8_t windSpeed;
            uint8_t cloudSpeedLower;
            uint8_t cloudSpeedUpper;
            uint8_t transitionData;
            uint8_t sunGlare;
            uint8_t sunDamage;
            uint8_t precipitationBeginFadeIn;
            uint8_t precipitationEndFadeOut;
            uint8_t lightningBeginFadeIn;
            uint8_t lightningEndFadeOut;
            uint8_t lightningFrequency;
            uint8_t weatherClass; // 0 - none; 1 - pleasant; 2 - cloudy; 4 - rainy; 8 - snow
            uint8_t lightningColorR;
            uint8_t lightningColorG;
            uint8_t lightningColorB;
        };
        struct SNAM
        {
            FormId sound;
            uint32_t type; // 0: default | 1: precip | 2: wind | 3: thunder
        };
#pragma pack(pop)

        FormId mFormId;       // from the header
        std::uint32_t mFlags; // from the header, see enum type RecordFlag for details

        std::string mEditorId;
        
        std::string mCloudTextures[4];

        std::string mModelPath;

        void load(Reader& reader);

        static std::string getRecordType()
        {
            return "Weather (TES4)";
        }
    };
}

#endif
