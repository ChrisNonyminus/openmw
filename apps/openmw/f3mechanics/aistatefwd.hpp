#ifndef OPENMW_F3MECHANICS_AISTATEFWD_H
#define OPENMW_F3MECHANICS_AISTATEFWD_H

#include "usings.hpp"

namespace F3Mechanics
{
    template <class Base>
    class DerivedClassStorage;

    /// \brief Container for AI package status.
    using AiState = DerivedClassStorage<AiTemporaryBase>;
}

#endif
