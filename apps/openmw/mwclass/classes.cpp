#include "classes.hpp"

#include "activator.hpp"
#include "creature.hpp"
#include "npc.hpp"
#include "weapon.hpp"
#include "armor.hpp"
#include "potion.hpp"
#include "apparatus.hpp"
#include "book.hpp"
#include "clothing.hpp"
#include "container.hpp"
#include "door.hpp"
#include "ingredient.hpp"
#include "creaturelevlist.hpp"
#include "itemlevlist.hpp"
#include "light.hpp"
#include "lockpick.hpp"
#include "misc.hpp"
#include "probe.hpp"
#include "repair.hpp"
#include "static.hpp"
#include "bodypart.hpp"

// todo: move TES4Sound to its own file and thus the following includes as well

#include <components/esm4/loadsoun.hpp>

#include "../mwgui/tooltips.hpp"
#include "../mwworld/action.hpp"
#include "../mwworld/nullaction.hpp"
#include "../mwworld/cellstore.hpp"

namespace MWClass
{
    class TES4Sound final : public MWWorld::RegisteredClass<TES4Sound>
    {
        // temporary dummy class to let SOUN refs load
        friend MWWorld::RegisteredClass<TES4Sound>;

        TES4Sound()
            : MWWorld::RegisteredClass<TES4Sound>(ESM4::Sound::sRecordId)
        {
        }

        MWWorld::Ptr copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const override 
        {
            const MWWorld::LiveCellRef<ESM4::Sound>* ref = ptr.get<ESM4::Sound>();

            return MWWorld::Ptr(cell.insert(ref), &cell);
        }

    public:
        void insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const override {}
        ///< Add reference into a cell for rendering

        void insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const override {}

        void insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const override {}

        std::string_view getName(const MWWorld::ConstPtr& ptr) const override { return ""; }
        ///< \return name or ID; can return an empty string.

        bool hasToolTip(const MWWorld::ConstPtr& ptr) const override { return false; }
        ///< @return true if this object has a tooltip when focused (default implementation: true)

        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override
        {
            return MWGui::ToolTipInfo();
        }
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        std::string_view getScript(const MWWorld::ConstPtr& ptr) const override { return ""; }
        ///< Return name of the script attached to ptr

        std::unique_ptr<MWWorld::Action> activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override { return std::make_unique<MWWorld::NullAction>(); }
        ///< Generate action for activation

        std::string getModel(const MWWorld::ConstPtr& ptr) const override { return ""; }

        bool useAnim() const override { return false; }
        ///< Whether or not to use animated variant of model (default false)

        bool isActivator() const override { return false; }

        std::string_view getSoundIdFromSndGen(const MWWorld::Ptr& ptr, std::string_view name) const override
        {
            return "";
        }
    };
    void registerClasses()
    {
        Activator::registerSelf();
        Creature::registerSelf();
        Npc::registerSelf();
        Weapon::registerSelf();
        Armor::registerSelf();
        Potion::registerSelf();
        Apparatus::registerSelf();
        Book::registerSelf();
        Clothing::registerSelf();
        Container::registerSelf();
        Door::registerSelf();
        Ingredient::registerSelf();
        CreatureLevList::registerSelf();
        ItemLevList::registerSelf();
        Light::registerSelf();
        Lockpick::registerSelf();
        Miscellaneous::registerSelf();
        Probe::registerSelf();
        Repair::registerSelf();
        Static::registerSelf();
        BodyPart::registerSelf();

        TES4Activator::registerSelf();
        TES4Static::registerSelf();
        TES4Light::registerSelf();
        TES4Sound::registerSelf();
        ESM4Creature::registerSelf();
    }
}
