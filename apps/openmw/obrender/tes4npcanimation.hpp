#ifndef OBRENDER_NPCANIMATION_H
#define OBRENDER_NPCANIMATION_H

#include "../mwrender/actoranimation.hpp"
#include "../mwrender/weaponanimation.hpp"

#include "../mwworld/inventorystore.hpp"

#include "../mwsound/sound.hpp"

namespace ESM4
{
    struct Npc;
    struct Race;
    struct Armor;
    struct Clothing;
}

namespace MWWorld
{
    class ESMStore;
}

namespace OBRender
{
    class RotateController;
    class Tes4HeadAnimationTime;
    class TES4NpcAnimation : public MWRender::ActorAnimation, public MWRender::WeaponAnimation, public MWWorld::InventoryStoreListener
    {
    public:
        void equipmentChanged() override;

    public:
        typedef std::map<ESM::PartReferenceType, std::string> PartBoneMap;

        enum ViewMode
        {
            VM_Normal,
            VM_FirstPerson,
            VM_HeadOnly
        };

        void setViewMode(ViewMode viewMode);

    private:
        static const PartBoneMap sPartList;

        bool mListenerDisabled;

        float mPoseDuration;
        float mStartTimer;
        std::string mCurrentAnim;

        // Bounded Parts
        std::vector<MWRender::PartHolderPtr> mHeadParts;
        std::map<uint32_t, MWRender::PartHolderPtr> mObjectParts;
        std::array<MWSound::Sound*, ESM::PRT_Count> mSounds;

        const ESM4::Npc* mNpc;
        const ESM4::Race* mRace;

        std::string mHeadModel;
        std::string mHairModel;
        ViewMode mViewMode;

        bool mShowWeapons;
        bool mShowCarriedLeft;

        bool mIsTes4;
        bool mIsFONV;
        bool mIsTES5;
        bool mIsFO3;

        enum NpcType
        {
            Type_Normal,
            Type_Werewolf,
            Type_Vampire
        };
        NpcType mNpcType;

        int mVisibilityFlags;

        int mPartslots[ESM::PRT_Count]; // Each part slot is taken by clothing, armor, or is empty
        int mPartPriorities[ESM::PRT_Count];

        osg::Vec3f mFirstPersonOffset;
        // Field of view to use when rendering first person meshes
        float mFirstPersonFieldOfView;

        std::shared_ptr<Tes4HeadAnimationTime> mHeadAnimationTime;
        std::shared_ptr<MWRender::WeaponAnimationTime> mWeaponAnimationTime;

        bool mSoundsDisabled;

        bool mAccurateAiming;
        float mAimingFactor;

        MWRender::PartHolderPtr mHeadObject;

        void updateTES4NpcBase();
        void updateFO3NpcBase();
        void updateTES5NpcBase();

        std::string getSkeletonModel(const MWWorld::ESMStore& store) const;

        MWRender::PartHolderPtr createSkinnedObject(const std::string& meshName,
             SceneUtil::Skeleton* skeletonModel);

        // must return a holder of a SceneUtil::MorphGeometry
        MWRender::PartHolderPtr createMorphedObject(const std::string& meshName,
            SceneUtil::Skeleton* skeletonModel, const std::string& texture = "", const std::string& boneName = "Bip01");

        MWRender::PartHolderPtr createObject(const std::string& meshName,
             SceneUtil::Skeleton* skeletonModel, const std::string& texture = "");

        bool equipArmor(const ESM4::Armor* armor, bool isFemale);
        bool equipClothes(const ESM4::Clothing* cloth, bool isFemale);
        void hideDismember(MWRender::PartHolder& scene);
        void deleteClonedMaterials();

        //MWRender::PartHolderPtr insertBoundedPart(const std::string& model, int group, const std::string& bonename,
        //    const std::string& bonefilter,
        //    bool enchantedGlow, osg::Vec4f* glowColor = NULL);

        void removeParts(ESM::PartReferenceType type);
        void removeIndividualPart(ESM::PartReferenceType type);
        //void reserveIndividualPart(ESM::PartReferenceType type, int group, int priority);

        //bool addOrReplaceIndividualPart(ESM::PartReferenceType type, int group, int priority, const std::string& mesh,
        //    bool enchantedGlow = false, osg::Vec4f* glowColor = NULL);
        //void removePartGroup(int group);
        //void addPartGroup(int group, int priority, const std::vector<ESM::PartReference>& parts,
        //    bool enchantedGlow = false, osg::Vec4f* glowColor = NULL);

        //void applyAlpha(float alpha, MWRender::PartHolderPtr node);

        
        void replaceMeshTexture(osg::ref_ptr<osg::Node> model, osg::ref_ptr<osg::Texture2D> texture);

    public:
        TES4NpcAnimation(const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem,
            bool disableSounds = false, ViewMode viewMode = VM_Normal, float firstPersonFieldOfView = 75.f);
        virtual ~TES4NpcAnimation();

        virtual void addAnimSource(const std::string& model);
        void addTes4AnimSource(const std::string& model, const std::string& anim);

        /// Rebuilds the NPC, updating their root model, animation sources, and equipment.
        void rebuild();

        void applyFaceMorphs(std::map<std::string, float>
                morphDiffs);

        osg::Group* getArrowBone() override;
        osg::Node* getWeaponNode() override;
        Resource::ResourceSystem* getResourceSystem() override;
        void showWeapon(bool) override;
    };
}

#endif
