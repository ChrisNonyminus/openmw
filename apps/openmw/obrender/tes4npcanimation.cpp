#include "tes4npcanimation.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"

#include "../f3mechanics/fomechanicsmanager.hpp"

#include <osg/Array>
#include <osg/Geode>

#include <components/debug/debuglog.hpp>

#include <components/fglib/fgctl.hpp>
#include <components/fglib/fgegm.hpp>
#include <components/fglib/fgegt.hpp>
#include <components/fglib/fgfile.hpp>
#include <components/fglib/fgsam.hpp>
#include <components/fglib/fgstream.hpp>
#include <components/fglib/fgtri.hpp>

#include <components/sceneutil/morphgeometry.hpp>

#include <components/resource/imagemanager.hpp>

namespace
{

}

namespace OBRender
{
    class Tes4HeadAnimationTime : public SceneUtil::ControllerSource
    {
    private:
        MWWorld::Ptr mReference;
        float mTalkStart;
        float mTalkStop;
        float mBlinkStart;
        float mBlinkStop;

        float mBlinkTimer;

        bool mEnabled;

        float mValue;

    private:
        void resetBlinkTimer();

    public:
        Tes4HeadAnimationTime(const MWWorld::Ptr& reference);

        void updatePtr(const MWWorld::Ptr& updated);

        void update(float dt);

        void setEnabled(bool enabled);

        void setTalkStart(float value);
        void setTalkStop(float value);
        void setBlinkStart(float value);
        void setBlinkStop(float value);

        float getValue(osg::NodeVisitor* nv) override;
    };

    void Tes4HeadAnimationTime::resetBlinkTimer()
    {
        mBlinkTimer = -(2.0f + Misc::Rng::rollDice(6));
    }

    Tes4HeadAnimationTime::Tes4HeadAnimationTime(const MWWorld::Ptr& reference)
        : mReference(reference), mTalkStart(0), mTalkStop(0), mBlinkStart(0), mBlinkStop(0), mEnabled(true), mValue(0)
    {
    }

    void Tes4HeadAnimationTime::updatePtr(const MWWorld::Ptr& updated)
    {
        mReference = updated;
    }

    void Tes4HeadAnimationTime::update(float dt)
    {
        if (!mEnabled)
            return;

        if (MWBase::Environment::get().getSoundManager()->sayDone(mReference))
        {
            mBlinkTimer += dt;

            float duration = mBlinkStop - mBlinkStart;

            if (mBlinkTimer >= 0 && mBlinkTimer <= duration)
            {
                mValue = mBlinkStart + mBlinkTimer;
            }
            else
                mValue = mBlinkStop;

            if (mBlinkTimer > duration)
                resetBlinkTimer();
        }
        else
        {
            mValue = mTalkStart + (mTalkStop - mTalkStart) * std::min(1.f, MWBase::Environment::get().getSoundManager()->getSaySoundLoudness(mReference) * 2); // Rescale a bit (most voices are not very loud)
        }
    }

    void Tes4HeadAnimationTime::setEnabled(bool enabled)
    {
        mEnabled = enabled;
    }

    void Tes4HeadAnimationTime::setTalkStart(float value)
    {
        mTalkStart = value;
    }

    void Tes4HeadAnimationTime::setTalkStop(float value)
    {
        mTalkStop = value;
    }

    void Tes4HeadAnimationTime::setBlinkStart(float value)
    {
        mBlinkStart = value;
    }

    void Tes4HeadAnimationTime::setBlinkStop(float value)
    {
        mBlinkStop = value;
    }

    float Tes4HeadAnimationTime::getValue(osg::NodeVisitor*)
    {
        return mValue;
    }

    static TES4NpcAnimation::PartBoneMap createPartListMap()
    {
        TES4NpcAnimation::PartBoneMap result;
        result.insert(std::make_pair(ESM::PRT_Head, "Head"));
        result.insert(std::make_pair(ESM::PRT_Hair, "Head")); // note it uses "Head" as attach bone, but "Hair" as filter
        result.insert(std::make_pair(ESM::PRT_Neck, "Neck"));
        result.insert(std::make_pair(ESM::PRT_Cuirass, "Chest"));
        result.insert(std::make_pair(ESM::PRT_Groin, "Groin"));
        result.insert(std::make_pair(ESM::PRT_Skirt, "Groin"));
        result.insert(std::make_pair(ESM::PRT_RHand, "Right Hand"));
        result.insert(std::make_pair(ESM::PRT_LHand, "Left Hand"));
        result.insert(std::make_pair(ESM::PRT_RWrist, "Right Wrist"));
        result.insert(std::make_pair(ESM::PRT_LWrist, "Left Wrist"));
        result.insert(std::make_pair(ESM::PRT_Shield, "Shield Bone"));
        result.insert(std::make_pair(ESM::PRT_RForearm, "Right Forearm"));
        result.insert(std::make_pair(ESM::PRT_LForearm, "Left Forearm"));
        result.insert(std::make_pair(ESM::PRT_RUpperarm, "Right Upper Arm"));
        result.insert(std::make_pair(ESM::PRT_LUpperarm, "Left Upper Arm"));
        result.insert(std::make_pair(ESM::PRT_RFoot, "Right Foot"));
        result.insert(std::make_pair(ESM::PRT_LFoot, "Left Foot"));
        result.insert(std::make_pair(ESM::PRT_RAnkle, "Right Ankle"));
        result.insert(std::make_pair(ESM::PRT_LAnkle, "Left Ankle"));
        result.insert(std::make_pair(ESM::PRT_RKnee, "Right Knee"));
        result.insert(std::make_pair(ESM::PRT_LKnee, "Left Knee"));
        result.insert(std::make_pair(ESM::PRT_RLeg, "Right Upper Leg"));
        result.insert(std::make_pair(ESM::PRT_LLeg, "Left Upper Leg"));
        result.insert(std::make_pair(ESM::PRT_RPauldron, "Right Clavicle")); // used for ear in TES4
        result.insert(std::make_pair(ESM::PRT_LPauldron, "Left Clavicle")); // used for eye in TES4
        result.insert(std::make_pair(ESM::PRT_Weapon, "Weapon Bone"));
        result.insert(std::make_pair(ESM::PRT_Tail, "Tail")); // used for tail in TES4
        return result;
    }
    const TES4NpcAnimation::PartBoneMap TES4NpcAnimation::sPartList = createPartListMap();

    class TextureVisitor : public osg::NodeVisitor
    {
    public:
        TextureVisitor(osg::ref_ptr<osg::Texture2D> tex) : NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN)
        {
            mTex = tex;
        }

        virtual ~TextureVisitor() {}

        virtual void apply(osg::Node& node)
        {
            if (node.getStateSet())
            {
                node.getStateSet()->setTextureAttributeAndModes(0, mTex, osg::StateAttribute::ON);
            }
            traverse(node);
        }

    private:

        osg::ref_ptr<osg::Texture2D> mTex;
    };
    void TES4NpcAnimation::replaceMeshTexture(osg::ref_ptr<osg::Node> model, osg::ref_ptr<osg::Texture2D> texture)
    {
        /*osg::StateSet* stateset = model->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, texture, osg::StateAttribute::ON);
        model->setStateSet(stateset);*/
        TextureVisitor texVisitor(texture);
        //model->accept(texVisitor);
    }
    TES4NpcAnimation::TES4NpcAnimation(const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem, bool disableSounds, ViewMode viewMode, float firstPersonFieldOfView)
        : ActorAnimation(ptr, parentNode, resourceSystem),
          mViewMode(viewMode),
          mShowWeapons(false),
          mShowCarriedLeft(true),
          mNpcType(Type_Normal),
          mFirstPersonFieldOfView(firstPersonFieldOfView),
          mSoundsDisabled(disableSounds),
          mAccurateAiming(false),
          mAimingFactor(0.f)
    {
        mNpc = mPtr.get<ESM4::Npc>()->mBase;
        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
        mRace = store.get<ESM4::Race>().search(mNpc->mRace); // WARN: might be nul

        mHeadAnimationTime = std::make_shared<Tes4HeadAnimationTime>(mPtr);
        mWeaponAnimationTime = std::make_shared<MWRender::WeaponAnimationTime>(this);

        // FIXME for foreign
        for (size_t i = 0; i < ESM::PRT_Count; i++)
        {
            mPartslots[i] = -1;
            mPartPriorities[i] = 0;
        }

        mHeadParts.clear();
        mStartTimer = 3.f + Misc::Rng::rollDice(15); // make the vertex pose demo somewhat random

        mIsTes4 = mNpc->mIsTES4;
        mIsFO3 = mIsFONV = mNpc->mIsFONV;
        mIsTES5 = !mIsTes4 && !mIsFO3;

        if (mIsTes4)
            updateTES4NpcBase();
        else if (mIsFO3)
            updateFO3NpcBase();
        else
            updateTES5NpcBase();

        mPtr.getClass().getInventoryStore(mPtr).setInvListener(this, mPtr);
    }
    TES4NpcAnimation::~TES4NpcAnimation()
    {
        deleteClonedMaterials();

        if (!mListenerDisabled && mPtr.getRefData().getCustomData())
            mPtr.getClass().getInventoryStore(mPtr).setInvListener(NULL, mPtr);
    }
    // FIXME: these animations need to be added to the skeleton just once, not everytime a character is created!
    void TES4NpcAnimation::addAnimSource(const std::string& model)
    {
        // First find the kf file.  For TES3 the kf file has the same name as the nif file.
        // For TES4, different animations (e.g. idle, block) have different kf files.
        size_t pos = model.find("skeleton.nif");
        if (pos == std::string::npos)
        {
            pos = model.find("skeletonbeast.nif");
            if (pos == std::string::npos && !mIsTES5) // FIXME: TES5 female has skeleton_female.nif
                return; // FIXME: should throw here
            // TODO: skeletonsesheogorath
        }
        std::string skeletonPath = model.substr(0, pos);

        // FIXME: for testing just load idle
        std::string animName = skeletonPath + "locomotion\\mtidle.kf";
        for (auto& kf : mNpc->mKf)
        {
            addTes4AnimSource(model, skeletonPath + kf);
        }
    }
    void TES4NpcAnimation::addTes4AnimSource(const std::string& model, const std::string& anim)
    {
        addSingleAnimSource(anim, model); // TODO
    }
    void TES4NpcAnimation::rebuild()
    {
        if (mIsTes4)
            updateTES4NpcBase();
        else if (mIsFO3)
            updateFO3NpcBase();
        else
            updateTES5NpcBase();
        MWBase::Environment::get().getMechanicsManager("FO3")->forceStateUpdate(mPtr);
    }
    void TES4NpcAnimation::applyFaceMorphs(std::map<std::string, float> morphDiffs)
    {
        SceneUtil::MorphGeometry* morph = dynamic_cast<SceneUtil::MorphGeometry*>(mHeadObject->getNode().get());

        std::vector<std::string> keys;
        for (auto& diff : morphDiffs)
        {
            morph->getMorphTarget(diff.first).setWeight(diff.second);
            keys.push_back(diff.first);
        }

        morph->mixMorphTargets(keys);
    }
    osg::Group* TES4NpcAnimation::getArrowBone()
    {
        return nullptr; // todo
    }
    osg::Node* TES4NpcAnimation::getWeaponNode()
    {
        return nullptr; // todo
    }
    Resource::ResourceSystem* TES4NpcAnimation::getResourceSystem()
    {
        return mResourceSystem;
    }
    void TES4NpcAnimation::showWeapon(bool)
    {
        // todo
    }
    void TES4NpcAnimation::equipmentChanged()
    {
        rebuild(); // todo
    }
    void TES4NpcAnimation::setViewMode(ViewMode viewMode)
    {
        if (mViewMode == viewMode)
            return;
        mViewMode = viewMode;
        rebuild();
    }
    void TES4NpcAnimation::updateTES4NpcBase()
    {
        // create and store morphed parts here
        // * head is always needed
        // * upper body & lower body textures may be used for exposed skin areas of clothes/armor
        // * ears' textures need to be morphed
        //
        // eyes/hands/feet textures are not morphed - just use those specified in the RACE records
        //
        // hair textures?
        //
        // remember to destroy any cloned materials

        if (!mNpc || !mRace)
            return;

        clearAnimSources(); // clears *all* animations

        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
        std::string skeletonModel = getSkeletonModel(store);
        if (skeletonModel.empty())
        {
            throw std::runtime_error("Empty skeleton model!");
        }

        Misc::StringUtils::lowerCaseInPlace(skeletonModel);
        size_t pos = skeletonModel.find_last_of(skeletonModel);
        pos = skeletonModel.find_last_of('\\');
        if (pos == std::string::npos)
            throw std::runtime_error(mNpc->mEditorId + " NPC skeleton.nif path could not be derived.");

        std::string skeletonPath = skeletonModel.substr(0, pos + 1); // +1 is for '\\'

        bool isFemale = false;
        if (mIsTes4)
            isFemale = (mNpc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0;
        else if (mIsFO3)
            isFemale = (mNpc->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0;
        else if (mIsTES5)
            isFemale = (mNpc->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female)
                != 0;

        setObjectRoot(skeletonModel, true, true, false);

        if (mViewMode != VM_FirstPerson)
            addAnimSource(skeletonModel);

        std::string modelName;
        std::string meshName;
        std::string textureName;

        // todo: tes4 inventory store
        /*MWWorld::InventoryStoreTES4& inv = static_cast<const MWClass::ForeignNpc&>(mPtr.getClass()).getInventoryStoreTES4(mPtr);
        MWWorld::ContainerStoreIterator invHeadGear = inv.getSlot(MWWorld::InventoryStoreTES4::Slot_TES4_Hair);
        if (invHeadGear == inv.end()) 
        {*/
        const ESM4::Hair* hair = store.get<ESM4::Hair>().search(mNpc->mHair);
        if (!hair)
        {
            // try to use the race defaults
            hair = store.get<ESM4::Hair>().search(mRace->mDefaultHair[isFemale ? 1 : 0]);
            if (!hair)
            {
                // NOTE "TG05Dremora" does not have a hair record nor race defaults
                // FIXME: we should remember our random selection?
                int hairChoice = Misc::Rng::rollDice(int(mRace->mHairChoices.size() - 1));
                hair = store.get<ESM4::Hair>().search(mRace->mHairChoices[hairChoice]);

                if (!hair)
                    throw std::runtime_error(mNpc->mEditorId + " - cannot find the hair.");
            }
        }
        meshName = "meshes\\" + hair->mModel;
        textureName = "textures\\" + hair->mIcon;
        mObjectParts.emplace(ESM4::Armor::TES4_Hair, std::make_unique<MWRender::PartHolder>(this->attach(meshName, false, "bip01 head")));
        for (int index = ESM4::Race::Head; index < ESM4::Race::NumHeadParts; ++index)
        {
            // skip 2 if male, 1 if female (ears)
            if ((isFemale && index == ESM4::Race::EarMale) || (!isFemale && index == ESM4::Race::EarFemale))
                continue;

            // FIXME: ears need texture morphing
            // skip ears if wearing a helmet - check for the head slot
            if ((index == ESM4::Race::EarMale || index == ESM4::Race::EarFemale) /*&& (invHeadGear != inv.end())*/)
                continue;

            // FIXME: mouth, lower teeth and tongue can have FaceGen emotions e.g. Surprise, Fear

            // FIXME: we do head elsewhere for now
            // FIXME: if we retrieve a morphed head the morphing is no longer present!
            if (index == ESM4::Race::Head)
                continue;

            if (mRace->mHeadParts[index].mesh == "") // FIXME: horrible workaround
            {
                std::string missing;
                switch (index)
                {
                    case 1: missing = "Ear Male"; break;
                    case 2: missing = "Ear Female"; break;
                    default: break;
                }
                Log(Debug::Error) << mNpc->mEditorId << ", a " << (isFemale ? "female," : "male,")
                                  << " does not have headpart \"" << missing << "\".";

                continue;
            }

            // Get mesh and texture from RACE except eye textures which are specified in Npc::mEyes
            // (NOTE: Oblivion.esm NPC_ records all have valid mEyes formid if one exists)

            meshName = "meshes\\" + mRace->mHeadParts[index].mesh;

            if (index == ESM4::Race::EyeLeft || index == ESM4::Race::EyeRight)
            {
                const ESM4::Eyes* eyes = store.get<ESM4::Eyes>().search(mNpc->mEyes);
                if (!eyes) // "ClaudettePerrick" does not have an eye record
                {
                    // FIXME: how to remember our random selection?
                    int eyeChoice = Misc::Rng::rollDice(int(mRace->mEyeChoices.size() - 1));
                    eyes = store.get<ESM4::Eyes>().search(mRace->mEyeChoices[eyeChoice]);

                    if (!eyes)
                        throw std::runtime_error(mNpc->mEditorId + " - cannot find the eye texture.");
                }

                textureName = "textures\\" + eyes->mIcon;
            }
            else
                textureName = "textures\\" + mRace->mHeadParts[index].texture;

            // TODO: if the texture doesn't exist, then grab it from the mesh (but shouldn't happen, so
            // log an error before proceeding with the fallback)

            // FaceGen:
            // dependency: model and texture files found
            //
            // Find the corresponding EGM file in the same directory as the NIF file. If it doesn't
            // exist, log an error and abandon any morphs for this mesh.
            //
            // Find the corresponding TRI file in the same directory as the NIF file. In a few cases
            // they don't exist so construct a dummy one from the NIF file.
            //
            // Morph the vertices in the TRI file using the RACE and NPC_ morph coefficients and the EGM
            // file.
            //
            // Find the corresponding EGT file in same directory as the NIF file. If it doesn't
            // exist, log an error and abandon any morphs for this texture.
            //
            // detail modulation: need to find the age from NPC_ symmetric morph coefficients.
            //
            // Morph the texture using the NPC_ morph coefficients, detail modulation and the EGT file.
            //
            // Find the detai map texture from "textures\\faces\\oblivion.esm\\" for the Npc::mFormId.
            //
            // Create the object using the morphed vertices, morphed texture and the detail map.
            //
            // TODO: to save unnecessary searches for the resources, these info should be persisted

            if (index == 1 || index == 2) // ears use morphed textures
            {
                // FIXME: Khajiit ears have FaceGen emotions - so we should build the poses
                mHeadParts.push_back(
                    createMorphedObject(meshName, mSkeleton));

                FgLib::FgSam sam;
                auto textureEars = sam.getMorphedTexture(meshName, textureName, mNpc->mEditorId,
                    mRace->mSymTextureModeCoefficients,
                    mNpc->mSymTextureModeCoefficients, mResourceSystem);
                if (textureEars.get())
                {
                    replaceMeshTexture(mHeadParts.back()->getNode(), textureEars);
                }
            }
            else
            {
                mHeadParts.push_back(createMorphedObject(meshName, mSkeleton, textureName));
            }
        }
        const std::vector<ESM4::Race::BodyPart>& bodyParts = (isFemale ? mRace->mBodyPartsFemale : mRace->mBodyPartsMale);
        for (int index = ESM4::Race::UpperBody; index < ESM4::Race::NumBodyParts; ++index)
        {
            meshName = bodyParts[index].mesh;
            textureName = "textures\\" + bodyParts[index].texture; // TODO: can it be empty string?

            // need a mapping of body parts to equipment slot, which are different for each game
            // FIXME: for now just implement TES4
            if (!mIsTes4)
                throw std::runtime_error("ForeignNpcAnimation: not TES4");
            int type = 0;
            // int invSlot = MWWorld::InventoryStoreTES4::TES4_Slots;
            switch (index)
            {
                case (ESM4::Race::UpperBody):
                {
                    // fixme: human upperbody need texture morphing
                    type = ESM4::Armor::TES4_UpperBody;
                    //invSlot = MWWorld::InventoryStoreTES4::Slot_TES4_UpperBody;
                    meshName = skeletonPath + (isFemale ? "female" : "") + "upperbody.nif";
                    break;
                }
                case (ESM4::Race::LowerBody):
                {
                    // FIXME: human lowerbody need texture morphing
                    type = ESM4::Armor::TES4_LowerBody;
                    //invSlot = MWWorld::InventoryStoreTES4::Slot_TES4_LowerBody;
                    meshName = skeletonPath + (isFemale ? "female" : "") + "lowerbody.nif";
                    break;
                }
                case (ESM4::Race::Hands):
                {
                    type = ESM4::Armor::TES4_Hands;
                    //invSlot = MWWorld::InventoryStoreTES4::Slot_TES4_Hands;
                    meshName = skeletonPath + (isFemale ? "female" : "") + "hand.nif";
                    break;
                }
                case (ESM4::Race::Feet):
                {
                    type = ESM4::Armor::TES4_Feet;
                    //invSlot = MWWorld::InventoryStoreTES4::Slot_TES4_Feet;
                    meshName = skeletonPath + (isFemale ? "female" : "") + "foot.nif";
                    break;
                }
                case (ESM4::Race::Tail):
                {
                    type = ESM4::Armor::TES4_Tail;
                    //invSlot = MWWorld::InventoryStoreTES4::Slot_TES4_Tail;
                    if (meshName != "")
                        meshName = "meshes\\" + meshName;
                    break;
                }
                default: break;
            }

            // MWWorld::ContainerStoreIterator invChest
            // = static_cast<MWWorld::InventoryStoreTES4&>(inv).getSlot(invSlot);
            if (index == ESM4::Race::UpperBody || index == ESM4::Race::LowerBody) // build always
            {
                removeIndividualPart(static_cast<ESM::PartReferenceType>(type));

                std::string headMeshName = mRace->mHeadParts[ESM4::Race::Head].mesh;
                Misc::StringUtils::lowerCaseInPlace(headMeshName);
                if (headMeshName.find("headhuman") != std::string::npos)
                {
                    mObjectParts[type] = createObject(meshName, mSkeleton);

                    FgLib::FgSam sam;
                    std::string npcTextureName;
                    auto bodyTexturePtr = sam.getMorphedTES4BodyTexture(meshName, textureName, mNpc->mEditorId,
                        mRace->mSymTextureModeCoefficients,
                        mNpc->mSymTextureModeCoefficients, mResourceSystem);
                    if (bodyTexturePtr.get())
                    {
                        replaceMeshTexture(mObjectParts[type]->getNode(), bodyTexturePtr);
                    }
                }
                else // probably argonian or khajiit
                {
                    mObjectParts[type] = createObject(meshName, mSkeleton, textureName);
                }
            }
            else if (/*invChest == inv.end() &&*/ meshName != "")
            {
                removeIndividualPart(static_cast<ESM::PartReferenceType>(type));

                mObjectParts[type] = createObject(meshName, mSkeleton, textureName);
            }
        }

        meshName = "meshes\\" + mRace->mHeadParts[ESM4::Race::Head].mesh;

        if (meshName.empty())
        {
            if (mRace->mEditorId == "Imperial" || mRace->mEditorId == "Nord" || mRace->mEditorId == "Breton" || mRace->mEditorId == "Redguard" || mRace->mEditorId == "HighElf" || mRace->mEditorId == "DarkElf" || mRace->mEditorId == "WoodElf" || mRace->mEditorId == "Dremora")
                meshName = "meshes\\Characters\\Imperial\\headhuman.nif";
            else if (mRace->mEditorId == "Argonian")
                meshName = "meshes\\Characters\\Argonian\\headargonian.nif";
            else if (mRace->mEditorId == "Orc")
                meshName = "meshes\\Characters\\Orc\\headorc.nif";
            else if (mRace->mEditorId == "Khajiit")
                meshName = "meshes\\Characters\\Khajiit\\headkhajiit.nif";
            else if (0) // FO3
            {
                mIsTes4 = false;
                isFemale = (mNpc->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0;

                // FIXME: can be female, ghoul, child, old, etc
                if (mRace->mEditorId.find("Old") != std::string::npos)
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headold.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headoldfemale.nif";
                }
                else if (mRace->mEditorId.find("Child") != std::string::npos)
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headchildfemale.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headchild.nif";
                }
                else
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headhumanfemale.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headhuman.nif";
                }

                //std::cout << "missing head " << mRace->mEditorId << std::endl;
                //else
                //return;
            }
        }

        textureName = "textures\\" + mRace->mHeadParts[ESM4::Race::Head].texture;

        // deprecated
        const std::vector<float>& sRaceCoeff = mRace->mSymShapeModeCoefficients;
        const std::vector<float>& sRaceTCoeff = mRace->mSymTextureModeCoefficients;
        const std::vector<float>& sCoeff = mNpc->mSymShapeModeCoefficients;
        const std::vector<float>& sTCoeff = mNpc->mSymTextureModeCoefficients;

        FgLib::FgSam sam;
        osg::Vec3f sym;

        // aged texture only for humans which we detect by whether we're using headhuman.nif
        std::string headMeshName = mRace->mHeadParts[ESM4::Race::Head].mesh;
        Misc::StringUtils::lowerCaseInPlace(headMeshName);
        bool hasAgedTexture = headMeshName.find("headhuman") != std::string::npos;
        std::string ageTextureFile;
        if (hasAgedTexture)
        {
            ageTextureFile
                = sam.getHeadHumanDetailTexture(meshName, sam.getAge(sRaceCoeff, sCoeff, mResourceSystem->getVFS()), isFemale);

            // find the corresponding normal texture
            /*std::size_t*/ pos = ageTextureFile.find_last_of(".");
            if (pos == std::string::npos)
                return; // FIXME: should throw

            // FIXME: use the normal
        }

        std::string faceDetailFile
            = sam.getTES4NpcDetailTexture_0("oblivion.esm", ESM4::formIdToString(mNpc->mFormId), const_cast<VFS::Manager*>(mResourceSystem->getVFS())); // fixme: get source esm filename

        auto object = createMorphedObject(meshName, mSkeleton, textureName);
        if (object.get())
        {
            FgLib::FgFile<FgLib::FgTri> triFile;
            const FgLib::FgTri* tri = triFile.getOrLoadByMeshName(meshName, mResourceSystem->getVFS());

            if (!tri || tri->numDiffMorphs() == 0)
                throw std::runtime_error(mNpc->mEditorId + " missing head mesh poses.");
            // todo: apply facegen morphs to nif osg node. I don't know how.
            SceneUtil::MorphGeometry* morph = dynamic_cast<SceneUtil::MorphGeometry*>(object->getNode().get());
            assert(morph->getSourceGeometry()->getVertexArray()->getDataType() == osg::Array::Vec3ArrayType);
            auto* srcVerts = static_cast<osg::Vec3Array*>(morph->getSourceGeometry()->getVertexArray());
            osg::ref_ptr<osg::Vec3Array> baseverts = new osg::Vec3Array();
            for (size_t v = 0; v < srcVerts->size(); v++)
            {
                //baseverts->push_back(osg::Vec3f(0, 0, 0));
                baseverts->push_back(srcVerts->at(v)); // todo: which is right?
            }
            morph->cacheMorphTarget(baseverts, "Base");
            for (size_t i = 0; i < tri->diffMorphs().size(); i++)
            {
                osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array();
                const auto& diffMorphVertices = tri->diffMorphVertices(tri->diffMorphs()[i]);
                if (srcVerts->size() * 3 != diffMorphVertices.second.size())
                    throw std::runtime_error("number of vertices in a pose differ to the MorphGeometry!");
                for (size_t v = 0; v < srcVerts->getNumElements(); ++v)
                {
                    float scale = diffMorphVertices.first;
                    osg::Vec3f delta(scale * diffMorphVertices.second[v * 3 + 0], scale * diffMorphVertices.second[v * 3 + 1], scale * diffMorphVertices.second[v * 3 + 2]);
                    verts->push_back(delta);
                }
                morph->cacheMorphTarget(verts, tri->diffMorphs()[i]);
            }
        }
        mHeadObject = std::move(object);
        // don't get egts for now

        //}

        // debugging
#if 1
        meshName = "meshes\\clutter\\food\\apple02.nif";
        attachMesh(meshName, "Bip01");
        attachMesh(meshName, "Bip01 L Thigh");
        attachMesh(meshName, "Bip01 R Thigh");
        attachMesh(meshName, "Bip01 L Calf");
        attachMesh(meshName, "Bip01 R Calf");
        attachMesh(meshName, "Bip01 L Foot");
        attachMesh(meshName, "Bip01 R Foot");
        attachMesh(meshName, "Bip01 Spine1");
        attachMesh(meshName, "Bip01 Neck");
        attachMesh(meshName, "Bip01 L Forearm");
        attachMesh(meshName, "Bip01 R Forearm");
        attachMesh(meshName, "Bip01 L ForeTwist");
        attachMesh(meshName, "Bip01 R ForeTwist");
        attachMesh(meshName, "Bip01 L Hand");
        attachMesh(meshName, "Bip01 L Thumb1");
        attachMesh(meshName, "Bip01 L Finger1");
        attachMesh(meshName, "Bip01 L Finger2");
        attachMesh(meshName, "Bip01 L Finger3");
        attachMesh(meshName, "Bip01 L Finger4");
        attachMesh(meshName, "Bip01 R Hand");
        attachMesh(meshName, "Bip01 R Thumb1");
        attachMesh(meshName, "Bip01 R Finger1");
        attachMesh(meshName, "Bip01 R Finger2");
        attachMesh(meshName, "Bip01 R Finger3");
        attachMesh(meshName, "Bip01 R Finger4");
#endif

        // FIXME: this section below should go to updateParts()
        std::vector<const ESM4::Clothing*> invCloth;
        std::vector<const ESM4::Armor*> invArmor;
        std::vector<const ESM4::Weapon*> invWeap;

        // check inventory
        MWWorld::InventoryStore& inv = mPtr.getClass().getInventoryStore(mPtr);
        for (size_t i = 0; i < 19; ++i)
        {
            MWWorld::ContainerStoreIterator store = inv.getSlot(int(i)); // compiler warning

            if (store == inv.end())
                continue;

            // TODO: this method of handling inventory doen't suit TES4 very well because it is possible
            //       for one part to occupy more than one slot; for now as a workaround just loop
            //       through the slots to gather all the equipped parts and process them afterwards

            if (store->getTypeDescription() == ESM4::Clothing::getRecordType())
            {
                const ESM4::Clothing* cloth = store->get<ESM4::Clothing>()->mBase;
                if (std::find(invCloth.begin(), invCloth.end(), cloth) == invCloth.end())
                    invCloth.push_back(cloth);
            }
            else if (store->getTypeDescription() == ESM4::Armor::getRecordType())
            {
                const ESM4::Armor* armor = store->get<ESM4::Armor>()->mBase;
                if (std::find(invArmor.begin(), invArmor.end(), armor) == invArmor.end())
                    invArmor.push_back(armor);
            }
            else if (store->getTypeDescription() == ESM4::Weapon::getRecordType())
            {
                const ESM4::Weapon* weap = store->get<ESM4::Weapon>()->mBase;
                if (std::find(invWeap.begin(), invWeap.end(), weap) == invWeap.end())
                    invWeap.push_back(weap);
            }
        }

        for (std::size_t i = 0; i < invCloth.size(); ++i)
            equipClothes(invCloth[i], isFemale);

        for (std::size_t i = 0; i < invArmor.size(); ++i)
            equipArmor(invArmor[i], isFemale);

        for (std::size_t i = 0; i < invWeap.size(); ++i)
        {
            std::string meshName;

            meshName = "meshes\\" + invWeap[i]->mModel;

            int type = ESM4::Armor::TES4_Weapon;

            removeIndividualPart((ESM::PartReferenceType)type);

            // FIXME: group "General"
            // FIXME: prob wrap this with a try/catch block
            mObjectParts[type] = createObject(meshName, mSkeleton);
        }

        mWeaponAnimationTime->updateStartTime();
    }
    class ColorVisitor : public osg::NodeVisitor
    {
    public:
        ColorVisitor(const osg::Vec4f& color) : NodeVisitor(NodeVisitor::TRAVERSE_ALL_CHILDREN)
        {
            mColor = color;
            mColorArrays = new osg::Vec4Array;
            mColorArrays->push_back(mColor);
        }

        virtual ~ColorVisitor() {}

        virtual void apply(osg::Node& node)
        {
            traverse(node);
        }

        virtual void apply(osg::Geometry& geom)
        {
            osg::Vec4Array* colorArrays = dynamic_cast<osg::Vec4Array*>(geom.getColorArray());

            if (colorArrays)
            {
                for (size_t c = 0; c < colorArrays->size(); c++)
                {
                    osg::Vec4* color = &colorArrays->at(c);

                    *color = mColor;
                }
            }
            else
            {
                geom.setColorArray(mColorArrays.get());
                geom.setColorBinding(osg::Geometry::BIND_OVERALL);
            }
            traverse(geom);
        }

        virtual void apply(osg::Geode& geode)
        {
            osg::StateSet* state = geode.getStateSet();

            for (size_t i = 0; i < geode.getNumDrawables(); i++)
            {
                osg::Geometry* geom = geode.getDrawable(i)->asGeometry();
                if (geom)
                {
                    osg::Vec4Array* colorArrays = dynamic_cast<osg::Vec4Array*>(geom->getColorArray());

                    if (colorArrays)
                    {
                        for (size_t c = 0; c < colorArrays->size(); c++)
                        {
                            osg::Vec4* color = &colorArrays->at(c);

                            *color = mColor;
                        }
                    }
                    else
                    {
                        geom->setColorArray(mColorArrays.get());
                        geom->setColorBinding(osg::Geometry::BIND_OVERALL);
                    }
                }
            }
            traverse(geode);
        }

    private:
        osg::Vec4 mColor;

        osg::ref_ptr<osg::Vec4Array> mColorArrays;
    };
    void TES4NpcAnimation::updateFO3NpcBase()
    {
        // create and store morphed parts here
        // * head is always needed
        // * upper body & lower body textures may be used for exposed skin areas of clothes/armor
        // * ears' textures need to be morphed
        //
        // eyes/hands/feet textures are not morphed - just use those specified in the RACE records
        //
        // hair textures?
        //
        // remember to destroy any cloned materials

        if (!mNpc || !mRace)
            return;

        clearAnimSources(); // clears *all* animations

        const MWWorld::ESMStore& store = MWBase::Environment::get().getWorld()->getStore();
        std::string skeletonModel = getSkeletonModel(store);
        if (skeletonModel.empty())
        {
            throw std::runtime_error("Empty skeleton model!");
        }

        Misc::StringUtils::lowerCaseInPlace(skeletonModel);
        size_t pos = skeletonModel.find_last_of('\\');
        if (pos == std::string::npos)
            throw std::runtime_error(mNpc->mEditorId + " NPC skeleton.nif path could not be derived.");

        std::string skeletonPath = skeletonModel.substr(0, pos + 1); // +1 is for '\\'

        bool isFemale = false;
        if (mIsTes4)
            isFemale = (mNpc->mBaseConfig.tes4.flags & ESM4::Npc::TES4_Female) != 0;
        else if (mIsFO3)
            isFemale = (mNpc->mBaseConfig.fo3.flags & ESM4::Npc::FO3_Female) != 0;
        else if (mIsTES5)
            isFemale = (mNpc->mBaseConfig.tes5.flags & ESM4::Npc::TES5_Female)
                != 0;

        setObjectRoot(skeletonModel, true, true, false);

        if (mViewMode != VM_FirstPerson)
            addAnimSource(skeletonModel);

        std::string modelName;
        std::string meshName;
        std::string textureName;

        // todo: fo3 inventory store
        /*MWWorld::InventoryStoreFO3& inv = static_cast<const MWClass::ForeignNpc&>(mPtr.getClass()).getInventoryStoreFO3(mPtr);
        MWWorld::ContainerStoreIterator invHeadGear = inv.getSlot(MWWorld::InventoryStoreTES4::Slot_TES4_Hair);
        if (invHeadGear == inv.end()) 
        {*/
        const ESM4::Hair* hair = store.get<ESM4::Hair>().search(mNpc->mHair);
        if (!hair)
        {
            // try to use the race defaults
            hair = store.get<ESM4::Hair>().search(mRace->mDefaultHair[isFemale ? 1 : 0]);
            if (!hair)
            {
                // NOTE "TG05Dremora" does not have a hair record nor race defaults
                // FIXME: we should remember our random selection?
                int hairChoice = Misc::Rng::rollDice(int(mRace->mHairChoices.size() - 1));
                hair = store.get<ESM4::Hair>().search(mRace->mHairChoices[hairChoice]);

                if (!hair)
                    throw std::runtime_error(mNpc->mEditorId + " - cannot find the hair.");
            }
        }
        meshName = "meshes\\" + hair->mModel;
        textureName = "textures\\" + hair->mIcon;
        mObjectParts.emplace(ESM4::Armor::FO3_Hair, std::make_unique<MWRender::PartHolder>(this->attach(meshName, false, "Bip01 Head")));
        osg::Vec4f hairColor;
        hairColor.r() = static_cast<float>(mNpc->mHairColour.red) / 256.f;
        hairColor.g() = static_cast<float>(mNpc->mHairColour.green) / 256.f;
        hairColor.b() = static_cast<float>(mNpc->mHairColour.blue) / 256.f;
        hairColor.a() = 1.f;
        ColorVisitor colorVis(hairColor);
        mObjectParts[ESM4::Armor::FO3_Hair]->getNode()->accept(colorVis);
        auto& inv = mPtr.getClass().getInventoryStore(mPtr);
        const std::vector<ESM4::Race::BodyPart>& headParts = (isFemale ? mRace->mHeadPartsFemale : mRace->mHeadParts);
        MWWorld::ContainerStoreIterator invHeadGear = inv./*getSlot(MWWorld::InventoryStoreTES4::Slot_TES4_Hair)*/ getSlot(MWWorld::InventoryStore::Slot_Helmet);
        for (int index = ESM4::Race::Head; index < 8; ++index) // FIXME: 8 head parts in FO3/FONV
        {
            // FIXME: ears need texture morphing
            // skip ears if wearing a helmet - check for the head slot
            if (index == 1 /*Ears*/ && (invHeadGear != inv.end()))
                continue;

            // mouth, lower teeth and tongue can have FaceGen emotions e.g. Surprise, Fear

            // FIXME: we do head elsewhere for now
            // FIXME: if we retrieve a morphed head the morphing is no longer present!
            if (index == ESM4::Race::Head)
                continue;

            if (headParts[index].mesh == "") // FIXME: horrible workaround
            {
                std::string missing;
                switch (index)
                {
                    //case 1: missing = "Ears"; break; // FIXME: only texture?
                    case 2: missing = "Mouth"; break;
                    case 3: missing = "Teeth Lower"; break;
                    case 4: missing = "Teeth Upper"; break;
                    case 5: missing = "Tongue"; break;
                    case 6: missing = "Left Eye"; break;
                    case 7: missing = "Right Eye"; break;
                    default: break;
                }
                if (index != 1) // only texture for ears
                    Log(Debug::Error) << mNpc->mEditorId << ", a " << (isFemale ? "female," : "male,")
                                      << " does not have headpart \"" << missing << "\".";

                continue;
            }

            // Get mesh and texture from RACE except eye textures which are specified in Npc::mEyes
            // (NOTE: Oblivion.esm NPC_ records all have valid mEyes formid if one exists)

            meshName = "meshes\\" + headParts[index].mesh;

            if (index == /*ESM4::Race::EyeLeft*/ 6 || index == /*ESM4::Race::EyeRight*/ 7)
            {
                const ESM4::Eyes* eyes = store.get<ESM4::Eyes>().search(mNpc->mEyes);
                if (!eyes) // "ClaudettePerrick" does not have an eye record
                {
                    // FIXME: how to remember our random selection?
                    int eyeChoice = Misc::Rng::rollDice(int(mRace->mEyeChoices.size() - 1));
                    eyes = store.get<ESM4::Eyes>().search(mRace->mEyeChoices[eyeChoice]);

                    if (!eyes)
                        throw std::runtime_error(mNpc->mEditorId + " - cannot find the eye texture.");
                }

                textureName = "textures\\" + eyes->mIcon;
            }
            else
                textureName = "textures\\" + headParts[index].texture;

            // TODO: if the texture doesn't exist, then grab it from the mesh (but shouldn't happen, so
            // log an error before proceeding with the fallback)

            if (index == 1) // there doesn't seem to be any ear mesh in FO3 (so why specify textures?)
                continue;

            // FaceGen:
            // dependency: model and texture files found
            //
            // Find the corresponding EGM file in the same directory as the NIF file. If it doesn't
            // exist, log an error and abandon any morphs for this mesh.
            //
            // Find the corresponding TRI file in the same directory as the NIF file. In a few cases
            // they don't exist so construct a dummy one from the NIF file.
            //
            // Morph the vertices in the TRI file using the RACE and NPC_ morph coefficients and the EGM
            // file.
            //
            // Find the corresponding EGT file in same directory as the NIF file. If it doesn't
            // exist, log an error and abandon any morphs for this texture.
            //
            // detail modulation: need to find the age from NPC_ symmetric morph coefficients.
            //
            // Morph the texture using the NPC_ morph coefficients, detail modulation and the EGT file.
            //
            // Find the detai map texture from "textures\\faces\\oblivion.esm\\" for the Npc::mFormId.
            //
            // Create the object using the morphed vertices, morphed texture and the detail map.
            //
            // TODO: to save unnecessary searches for the resources, these info should be persisted

            mHeadParts.push_back(createMorphedObject(meshName, mSkeleton, textureName));
        }
        const std::vector<ESM4::Race::BodyPart>& bodyParts = (isFemale ? mRace->mBodyPartsFemale : mRace->mBodyPartsMale);
        std::string bodyMeshName = "";
        std::string bodyTextureName = "";
        for (int index = ESM4::Race::UpperBody; index < 4; ++index)
        {
            meshName = "meshes\\" + bodyParts[index].mesh;
            textureName = "textures\\" + bodyParts[index].texture; // TODO: can it be empty string?

            int type = 0;
            // int invSlot = MWWorld::InventoryStoreFO3::FO3_Slots;
            switch (index)
            {
                case (ESM4::Race::UpperBody):
                {
                    type = ESM4::Armor::FO3_UpperBody;
                    //invSlot = MWWorld::InventoryStoreFO3::Slot_FO3_UpperBody;
                    bodyMeshName = meshName; // save for later
                    bodyTextureName = textureName; // save for later
                    //break;
                    continue; //  wait till we get EGT detail at index 3
                }
                case (1):
                {
                    type = ESM4::Armor::FO3_LeftHand;
                    //invSlot = MWWorld::InventoryStoreFO3::Slot_FO3_LeftHand;
                    break;
                }
                case (2):
                {
                    type = ESM4::Armor::FO3_RightHand;
                    //invSlot = MWWorld::InventoryStoreFO3::Slot_FO3_RightHand;
                    break;
                }
                case (3):
                {
                    // [0x00000003] = {mesh="Characters\\_Male\\UpperBodyHumanMale.egt" texture="" }
                    //invSlot = MWWorld::InventoryStoreFO3::Slot_FO3_UpperBody;
                    type = ESM4::Armor::FO3_UpperBody;
                    break;
                }
                default: break;
            }

            // MWWorld::ContainerStoreIterator invChest
            // = static_cast<MWWorld::InventoryStoreFO3&>(inv).getSlot(invSlot);
            if (index == 3) // build always
            {
                removeIndividualPart(static_cast<ESM::PartReferenceType>(ESM4::Armor::FO3_UpperBody));

                mObjectParts[type] = createObject(bodyMeshName, mSkeleton, bodyTextureName);
                hideDismember(*mObjectParts[type]);
                FgLib::FgSam sam;
                FgLib::FgFile<FgLib::FgEgt> egtFile;
                const FgLib::FgEgt* egt = egtFile.getOrLoadByMeshName(meshName, mResourceSystem->getVFS());

                if (egt == nullptr)
                    continue; // texutre morph not possible (should throw);

                auto texture = sam.getMorphedTexture(egt, bodyTextureName, mNpc->mEditorId, (isFemale ? mRace->mSymTextureModeCoeffFemale : mRace->mSymTextureModeCoefficients),
                    mNpc->mSymTextureModeCoefficients, mResourceSystem);
                if (texture.get())
                {
                    replaceMeshTexture(mObjectParts[type]->getNode(), texture);
                }
            }
            else if (/*invChest == inv.end() &&*/ meshName != "")
            {
                removeIndividualPart(static_cast<ESM::PartReferenceType>(type));

                mObjectParts[type] = createObject(meshName, mSkeleton, textureName);
                hideDismember(*mObjectParts[type]);
            }
        }

        meshName = "meshes\\" + headParts[ESM4::Race::Head].mesh;

        if (meshName.empty())
        {
            {

                // FIXME: can be female, ghoul, child, old, etc
                if (mRace->mEditorId.find("Old") != std::string::npos)
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headold.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headoldfemale.nif";
                }
                else if (mRace->mEditorId.find("Child") != std::string::npos)
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headchildfemale.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headchild.nif";
                }
                else
                {
                    if (isFemale)
                        meshName = "meshes\\Characters\\head\\headhumanfemale.nif";
                    else
                        meshName = "meshes\\Characters\\head\\headhuman.nif";
                }
                // FO3 races
                // CaucasianOldAged
                // AfricanAmericanOldAged
                // AsianOldAged
                // HispanicOldAged
                // AfricanAmericanRaider
                // AsianRaider
                // HispanicRaider
                // CaucasianRaider
                // TestQACaucasian
                // HispanicOld
                // HispanicChild
                // CaucasianOld
                // CaucasianChild
                // AsianOld
                // AsianChild
                // AfricanAmericanOld
                // AfricanAmericanChild
                // AfricanAmerican
                // Ghoul
                // Asian
                // Hispanic
                // Caucasian

                //std::cout << "missing head " << mRace->mEditorId << std::endl;
                //else
                //return;
            }
        }

        textureName = "textures\\" + headParts[ESM4::Race::Head].texture;

        // deprecated
        const std::vector<float>& sRaceCoeff = mRace->mSymShapeModeCoefficients;
        const std::vector<float>& sRaceTCoeff = mRace->mSymTextureModeCoefficients;
        const std::vector<float>& sCoeff = mNpc->mSymShapeModeCoefficients;
        const std::vector<float>& sTCoeff = mNpc->mSymTextureModeCoefficients;

        FgLib::FgSam sam;
        osg::Vec3f sym;

        // FO3 doesn't seem to have aged texture?

        std::string faceDetailFile
            = sam.getFO3NpcDetailTexture_0("falloutnv.esm", ESM4::formIdToString(mNpc->mFormId), const_cast<VFS::Manager*>(mResourceSystem->getVFS())); // fixme: get source esm filename. this is especially problematic for switching between fallout 3 and NV

        auto object = createMorphedObject(meshName, mSkeleton, faceDetailFile != "" ? faceDetailFile : textureName, "Bip01 Head");
        if (object.get())
        {
            FgLib::FgFile<FgLib::FgTri> triFile;
            const FgLib::FgTri* tri = triFile.getOrLoadByMeshName(meshName, mResourceSystem->getVFS());

            if (!tri || tri->numDiffMorphs() == 0)
                throw std::runtime_error(mNpc->mEditorId + " missing head mesh poses.");
            // todo: apply facegen morphs to nif osg node. I don't know how.
            SceneUtil::MorphGeometry* morph = dynamic_cast<SceneUtil::MorphGeometry*>(object->getNode().get());
            assert(morph->getSourceGeometry()->getVertexArray()->getDataType() == osg::Array::Vec3ArrayType);
            auto* srcVerts = static_cast<osg::Vec3Array*>(morph->getSourceGeometry()->getVertexArray());
            osg::ref_ptr<osg::Vec3Array> baseverts = new osg::Vec3Array();
            for (size_t v = 0; v < srcVerts->getNumElements(); v++)
            {
                baseverts->push_back(srcVerts->at(v));
            }
            morph->cacheMorphTarget(baseverts, "Base");
            for (size_t i = 0; i < tri->diffMorphs().size(); i++)
            {
                osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array();
                const auto& diffMorphVertices = tri->diffMorphVertices(tri->diffMorphs()[i]);
                if (srcVerts->getNumElements() * 3 != diffMorphVertices.second.size())
                    throw std::runtime_error("number of vertices in a pose differ to the MorphGeometry!");
                float scale = diffMorphVertices.first;
                for (size_t v = 0; v < srcVerts->getNumElements(); ++v)
                {
                    osg::Vec3f delta(scale * diffMorphVertices.second[v * 3 + 0], scale * diffMorphVertices.second[v * 3 + 1], scale * diffMorphVertices.second[v * 3 + 2]);
                    verts->push_back(delta);
                }
                morph->cacheMorphTarget(verts, tri->diffMorphs()[i]);
            }
        }
        mHeadObject = std::move(object);

        // don't get egts for now

        //}

        // debugging
#if 1
        meshName = "meshes\\clutter\\food\\apple02.nif";
        attachMesh(meshName, "Bip01");
        attachMesh(meshName, "Bip01 Head");
        attachMesh(meshName, "Bip01 L Thigh");
        attachMesh(meshName, "Bip01 R Thigh");
        attachMesh(meshName, "Bip01 L Calf");
        attachMesh(meshName, "Bip01 R Calf");
        attachMesh(meshName, "Bip01 L Foot");
        attachMesh(meshName, "Bip01 R Foot");
        attachMesh(meshName, "Bip01 Spine1");
        attachMesh(meshName, "Bip01 Neck");
        attachMesh(meshName, "Bip01 L Forearm");
        attachMesh(meshName, "Bip01 R Forearm");
        attachMesh(meshName, "Bip01 L ForeTwist");
        attachMesh(meshName, "Bip01 R ForeTwist");
        attachMesh(meshName, "Bip01 L Hand");
        attachMesh(meshName, "Bip01 L Thumb1");
        attachMesh(meshName, "Bip01 L Finger1");
        attachMesh(meshName, "Bip01 L Finger2");
        attachMesh(meshName, "Bip01 L Finger3");
        attachMesh(meshName, "Bip01 L Finger4");
        attachMesh(meshName, "Bip01 R Hand");
        attachMesh(meshName, "Bip01 R Thumb1");
        attachMesh(meshName, "Bip01 R Finger1");
        attachMesh(meshName, "Bip01 R Finger2");
        attachMesh(meshName, "Bip01 R Finger3");
        attachMesh(meshName, "Bip01 R Finger4");
#endif

        // FIXME: this section below should go to updateParts()
        std::vector<const ESM4::Armor*> invArmor;
        std::vector<const ESM4::Weapon*> invWeap;

        // check inventory
        for (size_t i = 0; i < 19; ++i)
        {
            MWWorld::ContainerStoreIterator store = inv.getSlot(int(i)); // compiler warning

            if (store == inv.end())
                continue;

            // TODO: this method of handling inventory doen't suit FO3 very well because it is possible
            //       for one part to occupy more than one slot; for now as a workaround just loop
            //       through the slots to gather all the equipped parts and process them afterwards

            if (store->getTypeDescription() == ESM4::Armor::getRecordType())
            {
                const ESM4::Armor* armor = store->get<ESM4::Armor>()->mBase;
                if (std::find(invArmor.begin(), invArmor.end(), armor) == invArmor.end())
                    invArmor.push_back(armor);
            }
            else if (store->getTypeDescription() == ESM4::Weapon::getRecordType())
            {
                const ESM4::Weapon* weap = store->get<ESM4::Weapon>()->mBase;
                if (std::find(invWeap.begin(), invWeap.end(), weap) == invWeap.end())
                    invWeap.push_back(weap);
            }
            else if (store->getTypeDescription() == ESM4::Weapon::getRecordType())
                throw std::runtime_error("Found CLOT in FO3/FONV");
        }

        for (std::size_t i = 0; i < invArmor.size(); ++i)
            equipArmor(invArmor[i], isFemale);

        for (std::size_t i = 0; i < invWeap.size(); ++i)
        {
            std::string meshName;

            if (invWeap[i]->mModel != "")
                return;

            meshName = "meshes\\" + invWeap[i]->mModel;

            int type = ESM4::Armor::TES4_Weapon;

            removeIndividualPart((ESM::PartReferenceType)type);

            // FIXME: group "General"
            // FIXME: prob wrap this with a try/catch block
            mObjectParts[type] = createObject(meshName, mSkeleton);
        }

        mWeaponAnimationTime->updateStartTime();
    }
    void TES4NpcAnimation::updateTES5NpcBase()
    {
        // todo: skyrim
    }
    std::string TES4NpcAnimation::getSkeletonModel(const MWWorld::ESMStore& store) const
    {
        std::string skeletonModel;
        if (!mNpc->mModel.empty()) // TES4/FO3/FONV
        {
            // FIXME: FO3/FONV some NPCs have mModel = marker_creature.nif
            if (mNpc->mModel == "marker_creature.nif")
                return ""; // FIXME FO3/FONV

            // Characters\_Male\skeleton.nif
            // Characters\_Male\skeletonbeast.nif
            // Characters\_Male\skeletonsesheogorath.nif
            return "meshes\\" + mNpc->mModel;
        }
        else
            return ""; // shouldn't happen
    }
    MWRender::PartHolderPtr TES4NpcAnimation::createSkinnedObject(const std::string& meshName, SceneUtil::Skeleton* skeletonModel)
    {
        MWRender::PartHolderPtr ptr = std::make_unique<MWRender::PartHolder>(this->attach(meshName, false));
        return ptr;
    }
    MWRender::PartHolderPtr TES4NpcAnimation::createMorphedObject(const std::string& meshName, SceneUtil::Skeleton* skeletonModel, const std::string& texture, const std::string& boneName)
    {
        auto attached = this->attachAndMorph(meshName, false, boneName);
        osg::ref_ptr<osg::Image> img = mResourceSystem->getImageManager()->getImage(texture);
        osg::ref_ptr<osg::Texture2D> image = new osg::Texture2D(img);
        image->setTextureSize(img->s(), img->t());

        replaceMeshTexture(attached, image);
        return std::make_unique<MWRender::PartHolder>(attached);
        // todo: set texture
    }
    MWRender::PartHolderPtr TES4NpcAnimation::createObject(const std::string& meshName, SceneUtil::Skeleton* skeletonModel, const std::string& texture)
    {
        MWRender::PartHolderPtr ptr = std::make_unique<MWRender::PartHolder>(this->attach(meshName, false));
        osg::ref_ptr<osg::Image> img = mResourceSystem->getImageManager()->getImage(texture);
        osg::ref_ptr<osg::Texture2D> image = new osg::Texture2D(img);
        image->setTextureSize(img->s(), img->t());

        replaceMeshTexture(ptr->getNode(), image);
        return ptr;
        // todo: set texture
    }
    bool TES4NpcAnimation::equipArmor(const ESM4::Armor* armor, bool isFemale)
    {
        std::string meshName;

        if (isFemale && !armor->mModelFemale.empty())
            meshName = "meshes\\" + armor->mModelFemale;
        else
            meshName = "meshes\\" + armor->mModelMale;

        int type = armor->mArmorFlags;

        if ((armor->mGeneralFlags & ESM4::Armor::TYPE_TES4) != 0)
            type = armor->mArmorFlags & 0xffff; // remove general flags high bits

        const std::vector<ESM4::Race::BodyPart>& bodyParts
            = (isFemale ? mRace->mBodyPartsFemale : mRace->mBodyPartsMale);

        int index = -1;
        std::string raceTexture = "";
        if (mIsTes4)
        {
            if ((armor->mArmorFlags & ESM4::Armor::TES4_UpperBody) != 0)
                index = ESM4::Race::UpperBody;
            else if ((armor->mArmorFlags & ESM4::Armor::TES4_LowerBody) != 0)
                index = ESM4::Race::LowerBody;
            else if ((armor->mArmorFlags & ESM4::Armor::TES4_Hands) != 0)
                index = ESM4::Race::Hands;
            else if ((armor->mArmorFlags & ESM4::Armor::TES4_Feet) != 0)
                index = ESM4::Race::Feet;
        }
        else if (mIsFO3 || mIsFONV) // FIXME: the visible skin is done with shaders
        {
            if ((armor->mArmorFlags & ESM4::Armor::FO3_UpperBody) != 0)
                index = 0;
            else if ((armor->mArmorFlags & ESM4::Armor::FO3_LeftHand) != 0)
                index = 1;
            else if ((armor->mArmorFlags & ESM4::Armor::FO3_RightHand) != 0)
                index = 2;
        }

        if (index != -1)
            raceTexture = "textures\\" + bodyParts[index].texture;

        removeParts((ESM::PartReferenceType)type);
        //removeIndividualPart((ESM::PartReferenceType)type);

        // FIXME: group "General"
        if (mIsTes4 && ((armor->mArmorFlags & ESM4::Armor::TES4_Hair) != 0)) // Hair slot
        {
            mObjectParts[type] = createMorphedObject(meshName, mSkeleton);
        }
        else if (mIsTes4 && (index == ESM4::Race::UpperBody || index == ESM4::Race::LowerBody))
        {
            //auto scene = createObject(meshName, mSkeleton);

            //std::string npcTextureName;
            //if (index == ESM4::Race::UpperBody)
            //    npcTextureName = mTextureUpperBody->getName();
            //else if (index == ESM4::Race::LowerBody)
            //    npcTextureName = mTextureLowerBody->getName();

            //replaceSkinTexture(scene, npcTextureName); // does nothing if none found
            // todo

            mObjectParts[type] = createObject(meshName, mSkeleton);
        }
        else if (mIsFO3 || mIsFONV)
        {
            size_t pos = meshName.find_last_of(".");
            //std::cout << meshName.substr(0, pos+1) << "egm" << std::endl;
            if (pos != std::string::npos && meshName.substr(pos + 1) == "nif" && mResourceSystem->getVFS()->exists(meshName.substr(0, pos + 1) + "egm"))
            {
                mObjectParts[type] = createMorphedObject(meshName, mSkeleton);
            }
            else
            {
                mObjectParts[type] = createObject(meshName, mSkeleton, raceTexture);
            }

            hideDismember(*mObjectParts[type]);
        }
        else // TES4 hands, feet
            mObjectParts[type] = createObject(meshName, mSkeleton, raceTexture);

        return true;
    }
    bool TES4NpcAnimation::equipClothes(const ESM4::Clothing* cloth, bool isFemale)
    {
        std::string meshName;

        if (isFemale && !cloth->mModelFemale.empty())
            meshName = "meshes\\" + cloth->mModelFemale;
        else
            meshName = "meshes\\" + cloth->mModelMale;

        // CLOT only in TES4
        int type = cloth->mClothingFlags & 0xffff; // remove general flags high bits

        // todo
        return false;
    }
    void TES4NpcAnimation::hideDismember(MWRender::PartHolder& scene)
    {
        // todo
    }
    void TES4NpcAnimation::deleteClonedMaterials()
    {
        // todo
    }
    void TES4NpcAnimation::removeParts(ESM::PartReferenceType type)
    {
        // todo
    }
    void TES4NpcAnimation::removeIndividualPart(ESM::PartReferenceType type)
    {
        // todo
    }
}
