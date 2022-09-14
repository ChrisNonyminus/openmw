#include "activator.hpp"

#include <MyGUI_TextIterator.h>

#include <components/esm3/loadacti.hpp>
#include <components/misc/rng.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/cellstore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/action.hpp"
#include "../mwworld/failedaction.hpp"
#include "../mwworld/nullaction.hpp"

#include "../mwphysics/physicssystem.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwgui/tooltips.hpp"
#include "../mwgui/ustring.hpp"

#include "../mwmechanics/npcstats.hpp"

#include "../f3mechanics/stats.hpp"

#include "../f3script/scriptmanagerimp.hpp"

#include "classmodel.hpp"

namespace MWClass
{
    Activator::Activator()
        : MWWorld::RegisteredClass<Activator>(ESM::Activator::sRecordId)
    {
    }

    void Activator::insertObjectRendering (const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model, true);
            ptr.getRefData().getBaseNode()->setNodeMask(MWRender::Mask_Static);
        }
    }

    void Activator::insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void Activator::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, model, rotation, MWPhysics::CollisionType_World);
    }

    std::string Activator::getModel(const MWWorld::ConstPtr &ptr) const
    {
        return getClassModel<ESM::Activator>(ptr);
    }

    bool Activator::isActivator() const
    {
        return true;
    }

    bool Activator::useAnim() const
    {
        return true;
    }

    std::string_view Activator::getName(const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Activator> *ref = ptr.get<ESM::Activator>();

        return ref->mBase->mName;
    }

    std::string_view Activator::getScript(const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Activator> *ref =
            ptr.get<ESM::Activator>();

        return ref->mBase->mScript;
    }

    bool Activator::hasToolTip (const MWWorld::ConstPtr& ptr) const
    {
        return !getName(ptr).empty();
    }

    MWGui::ToolTipInfo Activator::getToolTipInfo (const MWWorld::ConstPtr& ptr, int count) const
    {
        const MWWorld::LiveCellRef<ESM::Activator> *ref = ptr.get<ESM::Activator>();

        MWGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MWGui::toUString(name)) + MWGui::ToolTips::getCountString(count);

        std::string text;
        if (MWBase::Environment::get().getWindowManager()->getFullHelp())
        {
            text += MWGui::ToolTips::getCellRefString(ptr.getCellRef());
            text += MWGui::ToolTips::getMiscString(ref->mBase->mScript, "Script");
        }
        info.text = text;

        return info;
    }

    std::unique_ptr<MWWorld::Action> Activator::activate(const MWWorld::Ptr &ptr, const MWWorld::Ptr &actor) const
    {
        if(actor.getClass().isNpc() && actor.getClass().getNpcStats(actor).isWerewolf())
        {
            const MWWorld::ESMStore &store = MWBase::Environment::get().getWorld()->getStore();
            auto& prng = MWBase::Environment::get().getWorld()->getPrng();
            const ESM::Sound *sound = store.get<ESM::Sound>().searchRandom("WolfActivator", prng);

            std::unique_ptr<MWWorld::Action> action = std::make_unique<MWWorld::FailedAction>("#{sWerewolfRefusal}");
            if(sound) action->setSound(sound->mId);

            return action;
        }
        return std::make_unique<MWWorld::NullAction>();
    }


    MWWorld::Ptr Activator::copyToCellImpl(const MWWorld::ConstPtr &ptr, MWWorld::CellStore &cell) const
    {
        const MWWorld::LiveCellRef<ESM::Activator> *ref = ptr.get<ESM::Activator>();

        return MWWorld::Ptr(cell.insert(ref), &cell);
    }

    std::string_view Activator::getSoundIdFromSndGen(const MWWorld::Ptr& ptr, std::string_view name) const
    {
        const std::string model = getModel(ptr); // Assume it's not empty, since we wouldn't have gotten the soundgen otherwise
        const MWWorld::ESMStore &store = MWBase::Environment::get().getWorld()->getStore(); 
        std::string_view creatureId;
        const VFS::Manager* const vfs = MWBase::Environment::get().getResourceSystem()->getVFS();

        for (const ESM::Creature &iter : store.get<ESM::Creature>())
        {
            if (!iter.mModel.empty() && Misc::StringUtils::ciEqual(model,
                Misc::ResourceHelpers::correctMeshPath(iter.mModel, vfs)))
            {
                creatureId = !iter.mOriginal.empty() ? iter.mOriginal : iter.mId;
                break;
            }
        }

        int type = getSndGenTypeFromName(name);

        std::vector<const ESM::SoundGenerator*> fallbacksounds;
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        if (!creatureId.empty())
        {
            std::vector<const ESM::SoundGenerator*> sounds;
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end(); ++sound)
            {
                if (type == sound->mType && !sound->mCreature.empty() && (Misc::StringUtils::ciEqual(creatureId, sound->mCreature)))
                    sounds.push_back(&*sound);
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);
            }

            if (!sounds.empty())
                return sounds[Misc::Rng::rollDice(sounds.size(), prng)]->mSound;
            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }
        else
        {
            // The activator doesn't have a corresponding creature ID, but we can try to use the defaults
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end(); ++sound)
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);

            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }

        return {};
    }

    int Activator::getSndGenTypeFromName(std::string_view name)
    {
        if (name == "left")
            return ESM::SoundGenerator::LeftFoot;
        if (name == "right")
            return ESM::SoundGenerator::RightFoot;
        if (name == "swimleft")
            return ESM::SoundGenerator::SwimLeft;
        if (name == "swimright")
            return ESM::SoundGenerator::SwimRight;
        if (name == "moan")
            return ESM::SoundGenerator::Moan;
        if (name == "roar")
            return ESM::SoundGenerator::Roar;
        if (name == "scream")
            return ESM::SoundGenerator::Scream;
        if (name == "land")
            return ESM::SoundGenerator::Land;

        throw std::runtime_error("Unexpected soundgen type: " + std::string(name));
    }

    

    TES4Activator::TES4Activator()
        : MWWorld::RegisteredClass<TES4Activator>(ESM::RecNameInts::REC_ACTI4)
    {
    }

    void TES4Activator::insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model, true);
            ptr.getRefData().getBaseNode()->setNodeMask(MWRender::Mask_Static);
        }
    }

    void TES4Activator::insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void TES4Activator::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, model, rotation, MWPhysics::CollisionType_World);
    }

    std::string TES4Activator::getModel(const MWWorld::ConstPtr& ptr) const
    {
        return getClassModel<ESM4::Activator>(ptr);
    }

    bool TES4Activator::isActivator() const
    {
        return true;
    }

    bool TES4Activator::useAnim() const
    {
        return true;
    }

    std::string_view TES4Activator::getName(const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM4::Activator>* ref = ptr.get<ESM4::Activator>();

        return ref->mBase->mFullName;
    }

    std::string_view TES4Activator::getId(const MWWorld::Ptr& ptr) const
    {
        return ptr.get<ESM4::Activator>()->mBase->mEditorId;
    }

    ESM4::FormId TES4Activator::getFormId(const MWWorld::Ptr& ptr) const
    {
        return ptr.get<ESM4::Activator>()->mBase->mFormId;
    }

    std::string_view TES4Activator::getScript(const MWWorld::ConstPtr& ptr) const
    {
        const auto& esmStore = MWBase::Environment::get().getWorld()->getStore();
        const MWWorld::LiveCellRef<ESM4::Activator>* ref = ptr.get<ESM4::Activator>();
        if (ref->mBase->mScriptId && esmStore.get<ESM4::Script>().search(ref->mBase->mScriptId))
            return esmStore.get<ESM4::Script>().find(ref->mBase->mScriptId)->mEditorId;
        return "";
    }

    bool TES4Activator::hasToolTip(const MWWorld::ConstPtr& ptr) const
    {
        return !getName(ptr).empty();
    }

    MWGui::ToolTipInfo TES4Activator::getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const
    {
        const MWWorld::LiveCellRef<ESM4::Activator>* ref = ptr.get<ESM4::Activator>();

        MWGui::ToolTipInfo info;
        std::string_view name = getName(ptr);
        info.caption = MyGUI::TextIterator::toTagsString(MWGui::toUString(name)) + MWGui::ToolTips::getCountString(count);

        std::string text;
        if (MWBase::Environment::get().getWindowManager()->getFullHelp())
        {
            std::stringstream ss;
            ss << std::hex << ref->mBase->mScriptId;
            text += MWGui::ToolTips::getCellRefString(ptr.getCellRef());
            text += MWGui::ToolTips::getMiscString(ss.str(), "Script");
        }
        info.text = text;

        return info;
    }

    std::unique_ptr<MWWorld::Action> TES4Activator::activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const
    {
        // TODO
        /*const auto* scpt = MWBase::Environment().get().getWorld()->getStore().get<ESM4::Script>().search(getScript(ptr));
        if (scpt)
        {
            
        }*/
        return std::make_unique<MWWorld::NullAction>();
    }

    MWWorld::Ptr TES4Activator::copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const
    {
        const MWWorld::LiveCellRef<ESM4::Activator>* ref = ptr.get<ESM4::Activator>();

        return MWWorld::Ptr(cell.insert(ref), &cell);
    }

    std::string_view TES4Activator::getSoundIdFromSndGen(const MWWorld::Ptr& ptr, std::string_view name) const
    {
        const std::string model = getModel(ptr); // Assume it's not empty, since we wouldn't have gotten the soundgen otherwise
        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
        std::string_view creatureId;
        const VFS::Manager* const vfs = MWBase::Environment::get().getResourceSystem()->getVFS();

        for (const ESM::Creature& iter : store.get<ESM::Creature>())
        {
            if (!iter.mModel.empty() && Misc::StringUtils::ciEqual(model, Misc::ResourceHelpers::correctMeshPath(iter.mModel, vfs)))
            {
                creatureId = !iter.mOriginal.empty() ? iter.mOriginal : iter.mId;
                break;
            }
        }

        int type = getSndGenTypeFromName(name);

        std::vector<const ESM::SoundGenerator*> fallbacksounds;
        auto& prng = MWBase::Environment::get().getWorld()->getPrng();
        if (!creatureId.empty())
        {
            std::vector<const ESM::SoundGenerator*> sounds;
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end(); ++sound)
            {
                if (type == sound->mType && !sound->mCreature.empty() && (Misc::StringUtils::ciEqual(creatureId, sound->mCreature)))
                    sounds.push_back(&*sound);
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);
            }

            if (!sounds.empty())
                return sounds[Misc::Rng::rollDice(sounds.size(), prng)]->mSound;
            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }
        else
        {
            // The activator doesn't have a corresponding creature ID, but we can try to use the defaults
            for (auto sound = store.get<ESM::SoundGenerator>().begin(); sound != store.get<ESM::SoundGenerator>().end(); ++sound)
                if (type == sound->mType && sound->mCreature.empty())
                    fallbacksounds.push_back(&*sound);

            if (!fallbacksounds.empty())
                return fallbacksounds[Misc::Rng::rollDice(fallbacksounds.size(), prng)]->mSound;
        }

        return {};
    }

    int TES4Activator::getSndGenTypeFromName(std::string_view name)
    {
        if (name == "left")
            return ESM::SoundGenerator::LeftFoot;
        if (name == "right")
            return ESM::SoundGenerator::RightFoot;
        if (name == "swimleft")
            return ESM::SoundGenerator::SwimLeft;
        if (name == "swimright")
            return ESM::SoundGenerator::SwimRight;
        if (name == "moan")
            return ESM::SoundGenerator::Moan;
        if (name == "roar")
            return ESM::SoundGenerator::Roar;
        if (name == "scream")
            return ESM::SoundGenerator::Scream;
        if (name == "land")
            return ESM::SoundGenerator::Land;

        throw std::runtime_error("Unexpected soundgen type: " + std::string(name));
    }
}
