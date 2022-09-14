#ifndef GAME_F3MECHANICS_USINGS_H
#define GAME_F3MECHANICS_USINGS_H

// mechanics stuff in mwmechanics that's abstracted enough that we don't need to redefine
// instead we just use the "using" keyword and include the necessary headers

// todo: make these abstract classes instead

#include "../mwmechanics/movement.hpp"
#include "../mwmechanics/steering.hpp"
#include "../mwmechanics/objects.hpp"
#include "../mwmechanics/obstacle.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/actor.hpp"
#include "../mwmechanics/aitimer.hpp"
#include "../mwmechanics/aitemporarybase.hpp"

namespace F3Mechanics
{
    using Movement = MWMechanics::Movement;
    using Objects = MWMechanics::Objects; // todo: it seems the objects class does use some morrowind-specific code. What do I do regarding that?
    using ObstacleCheck = MWMechanics::ObstacleCheck;
    using Actor = MWMechanics::Actor;
    using CharacterController = MWMechanics::CharacterController;
    using AiReactionTimer = MWMechanics::AiReactionTimer;
    using AiTemporaryBase = MWMechanics::AiTemporaryBase;
}

#endif
