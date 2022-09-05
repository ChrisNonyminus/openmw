#ifndef ESM4_WTHR_H
#define ESM4_WTHR_H

#include <cstdint>
#include <vector>
#include <string>

#include <components/esm/defs.hpp>

#include "common.hpp"

namespace ESM4
{
    class Reader;
    class Writer;

    struct Weather
    {
        static constexpr ESM::RecNameInts sRecordId = ESM::REC_WTHR4;
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

        };
    };
}


#endif
