#ifndef GAME_MWCLASS_BOOK_H
#define GAME_MWCLASS_BOOK_H

#include "../mwworld/registeredclass.hpp"

namespace MWClass
{
    class Book : public MWWorld::RegisteredClass<Book>
    {
        friend MWWorld::RegisteredClass<Book>;

        Book();

        MWWorld::Ptr copyToCellImpl(const MWWorld::ConstPtr& ptr, MWWorld::CellStore& cell) const override;

    public:
        void insertObjectRendering(const MWWorld::Ptr& ptr, const std::string& model,
            MWRender::RenderingInterface& renderingInterface) const override;
        ///< Add reference into a cell for rendering

        std::string_view getName(const MWWorld::ConstPtr& ptr) const override;
        ///< \return name or ID; can return an empty string.

        std::unique_ptr<MWWorld::Action> activate(const MWWorld::Ptr& ptr, const MWWorld::Ptr& actor) const override;
        ///< Generate action for activation

        const ESM::RefId& getScript(const MWWorld::ConstPtr& ptr) const override;
        ///< Return name of the script attached to ptr

        MWGui::ToolTipInfo getToolTipInfo(const MWWorld::ConstPtr& ptr, int count) const override;
        ///< @return the content of the tool tip to be displayed. raises exception if the object has no tooltip.

        int getValue(const MWWorld::ConstPtr& ptr) const override;
        ///< Return trade value of the object. Throws an exception, if the object can't be traded.

        const ESM::RefId& getUpSoundId(const MWWorld::ConstPtr& ptr) const override;
        ///< Return the pick up sound Id

        const ESM::RefId& getDownSoundId(const MWWorld::ConstPtr& ptr) const override;
        ///< Return the put down sound Id

        const std::string& getInventoryIcon(const MWWorld::ConstPtr& ptr) const override;
        ///< Return name of inventory icon.

        const ESM::RefId& getEnchantment(const MWWorld::ConstPtr& ptr) const override;
        ///< @return the enchantment ID if the object is enchanted, otherwise an empty string

        const ESM::RefId& applyEnchantment(const MWWorld::ConstPtr& ptr, const ESM::RefId& enchId, int enchCharge,
            const std::string& newName) const override;
        ///< Creates a new record using \a ptr as template, with the given name and the given enchantment applied to it.

        std::unique_ptr<MWWorld::Action> use(const MWWorld::Ptr& ptr, bool force = false) const override;
        ///< Generate action for using via inventory menu

        std::string getModel(const MWWorld::ConstPtr& ptr) const override;

        int getEnchantmentPoints(const MWWorld::ConstPtr& ptr) const override;

        float getWeight(const MWWorld::ConstPtr& ptr) const override;

        bool canSell(const MWWorld::ConstPtr& item, int npcServices) const override;
    };
}

#endif
