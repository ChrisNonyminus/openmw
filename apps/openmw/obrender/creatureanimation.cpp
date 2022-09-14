#include "creatureanimation.hpp"

#include <osg/MatrixTransform>

#include <components/esm3/loadcrea.hpp>
#include <components/debug/debuglog.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/attach.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/settings/settings.hpp>
#include <components/vfs/manager.hpp>

#include <components/nif/niffile.hpp>
#include <components/nif/extra.hpp>
#include <components/files/constrainedfilestream.hpp>

#include <components/esm4/loadcrea.hpp>

#include <components/misc/rng.hpp>

#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/inventorystore.hpp" // TODO: make and include "../obworld/inventorystore.hpp"

#include "../mwclass/creature.hpp" // TODO: make and include "../obclass/creature.hpp"
#include <components/misc/pathhelpers.hpp>


namespace OBRender
{
    void CreatureAnimation::hideDismember(int part)
    {
        auto& partPtr = mObjectParts[part];

        
    }
    void CreatureAnimation::addAnimSource(const std::string& model, const std::string& kfname)
    {
        addSingleAnimSource(kfname, model); // TODO
    }
    CreatureAnimation::CreatureAnimation(const MWWorld::Ptr& ptr,
        const std::string& skeletonmodel, Resource::ResourceSystem* resourceSystem)
        : ActorAnimation(ptr, osg::ref_ptr<osg::Group>(ptr.getRefData().getBaseNode()), resourceSystem)
    {
        MWWorld::LiveCellRef<ESM4::Creature>* ref = mPtr.get<ESM4::Creature>();

        

        if (!skeletonmodel.empty())
        {
            setObjectRoot(skeletonmodel, true, false, true);
            addAnimSource(skeletonmodel);
        }

        size_t pos = skeletonmodel.find_last_of('\\');
        std::string path = skeletonmodel.substr(0, pos + 1); // +1 for '\\'

        for (size_t i = 0; i < ref->mBase->mNif.size(); ++i)
        {
            std::string meshName = path + ref->mBase->mNif[i];
            if (ref->mBase->mNif[i].empty())
                continue;

            
            mObjectParts.push_back(std::make_unique<MWRender::PartHolder>(this->attach(meshName, false)));
            hideDismember(i);
            std::shared_ptr<SceneUtil::ControllerSource> src = std::make_shared<SceneUtil::FrameTimeSource>();
            SceneUtil::AssignControllerSourcesVisitor assignVisitor(src);
            mObjectParts[i]->getNode()->accept(assignVisitor);
        }
    }
    void CreatureAnimation::addAnimSource(const std::string& skeletonName)
    {
        size_t pos = skeletonName.find_last_of('\\');
        std::string path = skeletonName.substr(0, pos + 1); // +1 for '\\'

        MWWorld::LiveCellRef<ESM4::Creature>* ref = mPtr.get<ESM4::Creature>();

        // fixme: this doesn't completely work
        /*for (const auto& name : mResourceSystem->getVFS()->getRecursiveDirectoryIterator(path))
        {
            if (Misc::getFileExtension(name) == "kf")
            {
                try
                {
                    addAnimSource(skeletonName, name);
                }
                catch (std::exception e)
                {
                    Log(Debug::Error) << e.what();
                }
            }
        }*/
        std::string animName;
        for (unsigned int i = 0; i < ref->mBase->mKf.size(); ++i)
        {
            animName = path + "idleanims\\" + ref->mBase->mKf[i];
            addAnimSource(skeletonName, animName);
            animName = path + "specialanims\\" + ref->mBase->mKf[i];
            addAnimSource(skeletonName, animName);
            animName = path + ref->mBase->mKf[i];
            addAnimSource(skeletonName, animName);
        }
        addAnimSource(skeletonName, path + "mtbackward.kf");
        addAnimSource(skeletonName, path + "mtfastforward.kf");
        addAnimSource(skeletonName, path + "mtidle.kf");
        addAnimSource(skeletonName, path + "mtleft.kf");
        addAnimSource(skeletonName, path + "mtright.kf");
        addAnimSource(skeletonName, path + "mtturnleft.kf");
        addAnimSource(skeletonName, path + "mtturnright.kf");
    }
    void CreatureAnimation::play(std::string_view groupname, const AnimPriority& priority, int blendMask, bool autodisable, float speedmult, std::string_view start, std::string_view stop, float startpoint, size_t loops, bool loopfallback)
    {
        MWWorld::LiveCellRef<ESM4::Creature>* ref = mPtr.get<ESM4::Creature>();
        Log(Debug::Debug) << ref->mBase->mEditorId << " is playing animation " << groupname;
        Animation::play(groupname, priority, blendMask, autodisable, speedmult, start, stop, startpoint, loops, loopfallback);
    }
}
