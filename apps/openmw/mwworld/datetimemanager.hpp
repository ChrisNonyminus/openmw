#ifndef GAME_MWWORLD_DATETIMEMANAGER_H
#define GAME_MWWORLD_DATETIMEMANAGER_H

#include <string_view>

#include "globalvariablename.hpp"

namespace ESM
{
    struct EpochTimeStamp;
}

namespace MWWorld
{
    class Globals;
    class TimeStamp;

    class DateTimeManager
    {
        int mDaysPassed = 0;
        int mDay = 0;
        int mMonth = 0;
        int mYear = 0;
        float mGameHour = 0.f;
        float mTimeScale = 0.f;

        void setHour(double hour);
        void setDay(int day);
        void setMonth(int month);

    public:
        std::string_view getMonthName(int month) const;
        TimeStamp getTimeStamp() const;
        ESM::EpochTimeStamp getEpochTimeStamp() const;
        float getTimeScaleFactor() const;

        void advanceTime(double hours, Globals& globalVariables);

        void setup(Globals& globalVariables);
        bool updateGlobalInt(GlobalVariableName name, int value);
        bool updateGlobalFloat(GlobalVariableName name, float value);
    };
}

#endif
