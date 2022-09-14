#ifndef FOSCRIPT_LOCALS_H
#define FOSCRIPT_LOCALS_H

#include <string>
#include <string_view>
#include <vector>

#include <components/interpreter/types.hpp>

#include <components/esm4/script.hpp>
#include <components/esm4/loadscpt.hpp>

namespace MWWorld
{
    class Ptr;
}

namespace FOScript
{
    class Locals
    {
            bool mInitialised;

            void ensure(std::string_view scriptName);

        public:
            std::vector<Interpreter::Type_Short> mShorts;
            std::vector<Interpreter::Type_Integer> mLongs;
            std::vector<Interpreter::Type_Float> mFloats;
            std::vector<MWWorld::Ptr> mRefs;

            Locals();

            /// Are there any locals?
            ///
            /// \note Will return false, if locals have not been configured yet.
            bool isEmpty() const;

            /// \return Did the state of *this change from uninitialised to initialised?
            bool configure (const ESM4::Script& script);

            /// @note var needs to be in lowercase
            ///
            /// \note Locals will be automatically configured first, if necessary
            bool setVarByInt(std::string_view script, std::string_view var, int val);

            /// \note Locals will be automatically configured first, if necessary
            //
            // \note If it can not be determined if the variable exists, the error will be
            // ignored and false will be returned.
            bool hasVar(std::string_view script, std::string_view var);

            /// if var does not exist, returns 0
            /// @note var needs to be in lowercase
            ///
            /// \note Locals will be automatically configured first, if necessary
            int getIntVar(std::string_view script, std::string_view var);

            /// if var does not exist, returns 0
            /// @note var needs to be in lowercase
            ///
            /// \note Locals will be automatically configured first, if necessary
            float getFloatVar(std::string_view script, std::string_view var);
    };
}

#endif
