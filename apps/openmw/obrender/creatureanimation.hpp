#ifndef GAME_TES4RENDER_NPCANIMATION_H
#define GAME_TES4RENDER_NPCANIMATION_H

#include "../mwrender/actoranimation.hpp"
#include "../mwrender/weaponanimation.hpp"
#include "../mwworld/inventorystore.hpp"

namespace MWWorld
{
    class Ptr;
}

namespace OBRender
{
    class CreatureAnimation : public MWRender::ActorAnimation
    {
        std::vector<MWRender::PartHolderPtr> mObjectParts;
        void hideDismember(int part);
        void addAnimSource(const std::string& model, const std::string& animName);

    public:
        CreatureAnimation(const MWWorld::Ptr& ptr, const std::string& skeletonmodel, Resource::ResourceSystem* resourceSystem);
        virtual ~CreatureAnimation() {}
        void addAnimSource(const std::string& skeletonModel);
        void play(std::string_view groupname, const AnimPriority& priority, int blendMask, bool autodisable,
            float speedmult, std::string_view start, std::string_view stop,
            float startpoint, size_t loops, bool loopfallback = false);
    };
}

#endif
