#ifndef GAME_F3MECHANICS_TYPEDAIPACKAGE_H
#define GAME_F3MECHANICS_TYPEDAIPACKAGE_H

#include "aipackage.hpp"

namespace F3Mechanics
{
    template <class T>
    struct TypedAiPackage : public AiPackage
    {
        TypedAiPackage(std::string name = "") :
            AiPackage(T::getTypeId(), T::makeDefaultOptions(), name) {}

        TypedAiPackage(bool repeat, std::string name = "") :
            AiPackage(T::getTypeId(), T::makeDefaultOptions().withRepeat(repeat), name) {}

        TypedAiPackage(const Options& options, std::string name = "") :
            AiPackage(T::getTypeId(), options) {}

        template <class Derived>
        TypedAiPackage(Derived*) :
            AiPackage(Derived::getTypeId(), Derived::makeDefaultOptions()) {}

        std::unique_ptr<AiPackage> clone() const override
        {
            return std::make_unique<T>(*static_cast<const T*>(this));
        }
    };
}

#endif
