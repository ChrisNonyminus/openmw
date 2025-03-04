#include "nifloader.hpp"

#include <mutex>
#include <string_view>

#include <osg/Array>
#include <osg/Geometry>
#include <osg/LOD>
#include <osg/Matrixf>
#include <osg/Sequence>
#include <osg/Switch>
#include <osg/TexGen>
#include <osg/TexMat>
#include <osg/ValueObject>

// resource
#include <components/debug/debuglog.hpp>
#include <components/misc/constants.hpp>
#include <components/misc/osguservalues.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/misc/strings/lower.hpp>
#include <components/nif/parent.hpp>
#include <components/resource/imagemanager.hpp>

// particle
#include <osgParticle/BoxPlacer>
#include <osgParticle/ConstantRateCounter>
#include <osgParticle/ModularProgram>
#include <osgParticle/ParticleSystem>
#include <osgParticle/ParticleSystemUpdater>

#include <osg/AlphaFunc>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/FrontFace>
#include <osg/Material>
#include <osg/PolygonMode>
#include <osg/Stencil>
#include <osg/TexEnv>
#include <osg/TexEnvCombine>
#include <osg/Texture2D>

#include <components/nif/controlled.hpp>
#include <components/nif/effect.hpp>
#include <components/nif/exception.hpp>
#include <components/nif/extra.hpp>
#include <components/nif/node.hpp>
#include <components/nif/property.hpp>
#include <components/sceneutil/morphgeometry.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/skeleton.hpp>

#include "matrixtransform.hpp"
#include "particle.hpp"

namespace
{
    struct DisableOptimizer : osg::NodeVisitor
    {
        DisableOptimizer(osg::NodeVisitor::TraversalMode mode = TRAVERSE_ALL_CHILDREN)
            : osg::NodeVisitor(mode)
        {
        }

        void apply(osg::Node& node) override
        {
            node.setDataVariance(osg::Object::DYNAMIC);
            traverse(node);
        }

        void apply(osg::Drawable& node) override { traverse(node); }
    };

    void getAllNiNodes(const Nif::Node* node, std::vector<int>& outIndices)
    {
        if (const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(node))
        {
            outIndices.push_back(ninode->recIndex);
            for (const auto& child : ninode->children)
                if (!child.empty())
                    getAllNiNodes(child.getPtr(), outIndices);
        }
    }

    bool isTypeGeometry(int type)
    {
        switch (type)
        {
            case Nif::RC_NiTriShape:
            case Nif::RC_NiTriStrips:
            case Nif::RC_NiLines:
            case Nif::RC_BSLODTriShape:
                return true;
        }
        return false;
    }

    // Collect all properties affecting the given drawable that should be handled on drawable basis rather than on the
    // node hierarchy above it.
    void collectDrawableProperties(
        const Nif::Node* nifNode, const Nif::Parent* parent, std::vector<const Nif::Property*>& out)
    {
        if (parent != nullptr)
            collectDrawableProperties(&parent->mNiNode, parent->mParent, out);
        for (const auto& property : nifNode->props)
        {
            if (!property.empty())
            {
                switch (property->recType)
                {
                    case Nif::RC_NiMaterialProperty:
                    case Nif::RC_NiVertexColorProperty:
                    case Nif::RC_NiSpecularProperty:
                    case Nif::RC_NiAlphaProperty:
                        out.push_back(property.getPtr());
                        break;
                    default:
                        break;
                }
            }
        }

        auto geometry = dynamic_cast<const Nif::NiGeometry*>(nifNode);
        if (geometry)
        {
            if (!geometry->shaderprop.empty())
                out.emplace_back(geometry->shaderprop.getPtr());
            if (!geometry->alphaprop.empty())
                out.emplace_back(geometry->alphaprop.getPtr());
        }
    }

    // NodeCallback used to have a node always oriented towards the camera. The node can have translation and scale
    // set just like a regular MatrixTransform, but the rotation set will be overridden in order to face the camera.
    class BillboardCallback : public SceneUtil::NodeCallback<BillboardCallback, osg::Node*, osgUtil::CullVisitor*>
    {
    public:
        BillboardCallback() {}
        BillboardCallback(const BillboardCallback& copy, const osg::CopyOp& copyop)
            : SceneUtil::NodeCallback<BillboardCallback, osg::Node*, osgUtil::CullVisitor*>(copy, copyop)
        {
        }

        META_Object(NifOsg, BillboardCallback)

        void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
        {
            osg::Matrix modelView = *cv->getModelViewMatrix();

            // attempt to preserve scale
            float mag[3];
            for (int i = 0; i < 3; ++i)
            {
                mag[i] = std::sqrt(modelView(0, i) * modelView(0, i) + modelView(1, i) * modelView(1, i)
                    + modelView(2, i) * modelView(2, i));
            }

            modelView.setRotate(osg::Quat());
            modelView(0, 0) = mag[0];
            modelView(1, 1) = mag[1];
            modelView(2, 2) = mag[2];

            cv->pushModelViewMatrix(new osg::RefMatrix(modelView), osg::Transform::RELATIVE_RF);

            traverse(node, cv);

            cv->popModelViewMatrix();
        }
    };

    void extractTextKeys(const Nif::NiTextKeyExtraData* tk, SceneUtil::TextKeyMap& textkeys)
    {
        for (size_t i = 0; i < tk->list.size(); i++)
        {
            std::vector<std::string> results;
            Misc::StringUtils::split(tk->list[i].text, results, "\r\n");
            for (std::string& result : results)
            {
                Misc::StringUtils::trim(result);
                Misc::StringUtils::lowerCaseInPlace(result);
                if (!result.empty())
                    textkeys.emplace(tk->list[i].time, std::move(result));
            }
        }
    }
}

namespace NifOsg
{
    bool Loader::sShowMarkers = false;

    void Loader::setShowMarkers(bool show)
    {
        sShowMarkers = show;
    }

    bool Loader::getShowMarkers()
    {
        return sShowMarkers;
    }

    unsigned int Loader::sHiddenNodeMask = 0;

    void Loader::setHiddenNodeMask(unsigned int mask)
    {
        sHiddenNodeMask = mask;
    }
    unsigned int Loader::getHiddenNodeMask()
    {
        return sHiddenNodeMask;
    }

    unsigned int Loader::sIntersectionDisabledNodeMask = ~0u;

    void Loader::setIntersectionDisabledNodeMask(unsigned int mask)
    {
        sIntersectionDisabledNodeMask = mask;
    }

    unsigned int Loader::getIntersectionDisabledNodeMask()
    {
        return sIntersectionDisabledNodeMask;
    }

    class LoaderImpl
    {
    public:
        /// @param filename used for warning messages.
        LoaderImpl(const std::filesystem::path& filename, unsigned int ver, unsigned int userver, unsigned int bethver)
            : mFilename(filename)
            , mVersion(ver)
            , mUserVersion(userver)
            , mBethVersion(bethver)
        {
        }
        std::filesystem::path mFilename;
        unsigned int mVersion, mUserVersion, mBethVersion;

        size_t mFirstRootTextureIndex{ ~0u };
        bool mFoundFirstRootTexturingProperty = false;

        bool mHasNightDayLabel = false;
        bool mHasHerbalismLabel = false;
        bool mHasStencilProperty = false;

        const Nif::NiSortAdjustNode* mPushedSorter = nullptr;
        const Nif::NiSortAdjustNode* mLastAppliedNoInheritSorter = nullptr;

        // This is used to queue emitters that weren't attached to their node yet.
        std::vector<std::pair<size_t, osg::ref_ptr<Emitter>>> mEmitterQueue;

        void loadKf(Nif::FileView nif, SceneUtil::KeyframeHolder& target) const
        {
            const Nif::NiSequenceStreamHelper* seq = nullptr;
            const size_t numRoots = nif.numRoots();
            for (size_t i = 0; i < numRoots; ++i)
            {
                const Nif::Record* r = nif.getRoot(i);
                if (r && r->recType == Nif::RC_NiSequenceStreamHelper)
                {
                    seq = static_cast<const Nif::NiSequenceStreamHelper*>(r);
                    break;
                }
            }

            if (!seq)
            {
                Log(Debug::Warning) << "NIFFile Warning: Found no NiSequenceStreamHelper root record. File: "
                                    << nif.getFilename();
                return;
            }

            Nif::ExtraPtr extra = seq->extra;
            if (extra.empty() || extra->recType != Nif::RC_NiTextKeyExtraData)
            {
                Log(Debug::Warning) << "NIFFile Warning: First extra data was not a NiTextKeyExtraData, but a "
                                    << (extra.empty() ? std::string_view("nil") : std::string_view(extra->recName))
                                    << ". File: " << nif.getFilename();
                return;
            }

            extractTextKeys(static_cast<const Nif::NiTextKeyExtraData*>(extra.getPtr()), target.mTextKeys);

            extra = extra->next;
            Nif::ControllerPtr ctrl = seq->controller;
            for (; !extra.empty() && !ctrl.empty(); (extra = extra->next), (ctrl = ctrl->next))
            {
                if (extra->recType != Nif::RC_NiStringExtraData || ctrl->recType != Nif::RC_NiKeyframeController)
                {
                    Log(Debug::Warning) << "NIFFile Warning: Unexpected extra data " << extra->recName
                                        << " with controller " << ctrl->recName << ". File: " << nif.getFilename();
                    continue;
                }

                // Vanilla seems to ignore the "active" flag for NiKeyframeController,
                // so we don't want to skip inactive controllers here.

                const Nif::NiStringExtraData* strdata = static_cast<const Nif::NiStringExtraData*>(extra.getPtr());
                const Nif::NiKeyframeController* key = static_cast<const Nif::NiKeyframeController*>(ctrl.getPtr());

                if (key->mData.empty() && key->mInterpolator.empty())
                    continue;

                if (!key->mInterpolator.empty() && key->mInterpolator->recType != Nif::RC_NiTransformInterpolator)
                {
                    Log(Debug::Error) << "Unsupported interpolator type for NiKeyframeController " << key->recIndex
                                      << " in " << mFilename;
                    continue;
                }

                osg::ref_ptr<SceneUtil::KeyframeController> callback = new NifOsg::KeyframeController(key);
                setupController(key, callback, /*animflags*/ 0);

                if (!target.mKeyframeControllers.emplace(strdata->string, callback).second)
                    Log(Debug::Verbose) << "Controller " << strdata->string << " present more than once in "
                                        << nif.getFilename() << ", ignoring later version";
            }
        }

        osg::ref_ptr<osg::Node> load(Nif::FileView nif, Resource::ImageManager* imageManager)
        {
            const size_t numRoots = nif.numRoots();
            std::vector<const Nif::Node*> roots;
            for (size_t i = 0; i < numRoots; ++i)
            {
                const Nif::Record* r = nif.getRoot(i);
                if (!r)
                    continue;
                const Nif::Node* nifNode = dynamic_cast<const Nif::Node*>(r);
                if (nifNode)
                    roots.emplace_back(nifNode);
            }
            if (roots.empty())
                throw Nif::Exception("Found no root nodes", nif.getFilename());

            osg::ref_ptr<SceneUtil::TextKeyMapHolder> textkeys(new SceneUtil::TextKeyMapHolder);

            osg::ref_ptr<osg::Group> created(new osg::Group);
            created->setDataVariance(osg::Object::STATIC);
            for (const Nif::Node* root : roots)
            {
                auto node = handleNode(root, nullptr, nullptr, imageManager, std::vector<unsigned int>(), 0, false,
                    false, false, &textkeys->mTextKeys);
                created->addChild(node);
            }
            if (mHasNightDayLabel)
                created->getOrCreateUserDataContainer()->addDescription(Constants::NightDayLabel);
            if (mHasHerbalismLabel)
                created->getOrCreateUserDataContainer()->addDescription(Constants::HerbalismLabel);

            // Attach particle emitters to their nodes which should all be loaded by now.
            handleQueuedParticleEmitters(created, nif);

            if (nif.getUseSkinning())
            {
                osg::ref_ptr<SceneUtil::Skeleton> skel = new SceneUtil::Skeleton;
                skel->setStateSet(created->getStateSet());
                skel->setName(created->getName());
                for (unsigned int i = 0; i < created->getNumChildren(); ++i)
                    skel->addChild(created->getChild(i));
                created->removeChildren(0, created->getNumChildren());
                created = skel;
            }

            if (!textkeys->mTextKeys.empty())
                created->getOrCreateUserDataContainer()->addUserObject(textkeys);

            created->setUserValue(Misc::OsgUserValues::sFileHash, nif.getHash());

            return created;
        }

        void applyNodeProperties(const Nif::Node* nifNode, osg::Node* applyTo,
            SceneUtil::CompositeStateSetUpdater* composite, Resource::ImageManager* imageManager,
            std::vector<unsigned int>& boundTextures, int animflags)
        {
            bool hasStencilProperty = false;

            for (const auto& property : nifNode->props)
            {
                if (property.empty())
                    continue;

                if (property.getPtr()->recType == Nif::RC_NiStencilProperty)
                {
                    const Nif::NiStencilProperty* stencilprop
                        = static_cast<const Nif::NiStencilProperty*>(property.getPtr());
                    if (stencilprop->data.enabled != 0)
                    {
                        hasStencilProperty = true;
                        break;
                    }
                }
            }

            for (const auto& property : nifNode->props)
            {
                if (!property.empty())
                {
                    // Get the lowest numbered recIndex of the NiTexturingProperty root node.
                    // This is what is overridden when a spell effect "particle texture" is used.
                    if (nifNode->parents.empty() && !mFoundFirstRootTexturingProperty
                        && property.getPtr()->recType == Nif::RC_NiTexturingProperty)
                    {
                        mFirstRootTextureIndex = property.getPtr()->recIndex;
                        mFoundFirstRootTexturingProperty = true;
                    }
                    else if (property.getPtr()->recType == Nif::RC_NiTexturingProperty)
                    {
                        if (property.getPtr()->recIndex == mFirstRootTextureIndex)
                            applyTo->setUserValue("overrideFx", 1);
                    }
                    handleProperty(property.getPtr(), applyTo, composite, imageManager, boundTextures, animflags,
                        hasStencilProperty);
                }
            }

            auto geometry = dynamic_cast<const Nif::NiGeometry*>(nifNode);
            // NiGeometry's NiAlphaProperty doesn't get handled here because it's a drawable property
            if (geometry && !geometry->shaderprop.empty())
                handleProperty(geometry->shaderprop.getPtr(), applyTo, composite, imageManager, boundTextures,
                    animflags, hasStencilProperty);
        }

        static void setupController(const Nif::Controller* ctrl, SceneUtil::Controller* toSetup, int animflags)
        {
            bool autoPlay = animflags & Nif::NiNode::AnimFlag_AutoPlay;
            if (autoPlay)
                toSetup->setSource(std::make_shared<SceneUtil::FrameTimeSource>());

            toSetup->setFunction(std::make_shared<ControllerFunction>(ctrl));
        }

        static osg::ref_ptr<osg::LOD> handleLodNode(const Nif::NiLODNode* niLodNode)
        {
            osg::ref_ptr<osg::LOD> lod(new osg::LOD);
            lod->setName(niLodNode->name);
            lod->setCenterMode(osg::LOD::USER_DEFINED_CENTER);
            lod->setCenter(niLodNode->lodCenter);
            for (unsigned int i = 0; i < niLodNode->lodLevels.size(); ++i)
            {
                const Nif::NiLODNode::LODRange& range = niLodNode->lodLevels[i];
                lod->setRange(i, range.minRange, range.maxRange);
            }
            lod->setRangeMode(osg::LOD::DISTANCE_FROM_EYE_POINT);
            return lod;
        }

        static osg::ref_ptr<osg::Switch> handleSwitchNode(const Nif::NiSwitchNode* niSwitchNode)
        {
            osg::ref_ptr<osg::Switch> switchNode(new osg::Switch);
            switchNode->setName(niSwitchNode->name);
            switchNode->setNewChildDefaultValue(false);
            switchNode->setSingleChildOn(niSwitchNode->initialIndex);
            return switchNode;
        }

        static osg::ref_ptr<osg::Sequence> prepareSequenceNode(const Nif::Node* nifNode)
        {
            const Nif::NiFltAnimationNode* niFltAnimationNode = static_cast<const Nif::NiFltAnimationNode*>(nifNode);
            osg::ref_ptr<osg::Sequence> sequenceNode(new osg::Sequence);
            sequenceNode->setName(niFltAnimationNode->name);
            if (!niFltAnimationNode->children.empty())
            {
                if (niFltAnimationNode->swing())
                    sequenceNode->setDefaultTime(
                        niFltAnimationNode->mDuration / (niFltAnimationNode->children.size() * 2));
                else
                    sequenceNode->setDefaultTime(niFltAnimationNode->mDuration / niFltAnimationNode->children.size());
            }
            return sequenceNode;
        }

        static void activateSequenceNode(osg::Group* osgNode, const Nif::Node* nifNode)
        {
            const Nif::NiFltAnimationNode* niFltAnimationNode = static_cast<const Nif::NiFltAnimationNode*>(nifNode);
            osg::Sequence* sequenceNode = static_cast<osg::Sequence*>(osgNode);
            if (niFltAnimationNode->swing())
                sequenceNode->setInterval(osg::Sequence::SWING, 0, -1);
            else
                sequenceNode->setInterval(osg::Sequence::LOOP, 0, -1);
            sequenceNode->setDuration(1.0f, -1);
            sequenceNode->setMode(osg::Sequence::START);
        }

        osg::ref_ptr<osg::Image> handleSourceTexture(
            const Nif::NiSourceTexture* st, Resource::ImageManager* imageManager)
        {
            if (!st)
                return nullptr;

            osg::ref_ptr<osg::Image> image;
            if (!st->external && !st->data.empty())
            {
                image = handleInternalTexture(st->data.getPtr());
            }
            else
            {
                std::string filename = Misc::ResourceHelpers::correctTexturePath(st->filename, imageManager->getVFS());
                image = imageManager->getImage(filename);
            }
            return image;
        }

        void handleTextureWrapping(osg::Texture2D* texture, bool wrapS, bool wrapT)
        {
            texture->setWrap(osg::Texture::WRAP_S, wrapS ? osg::Texture::REPEAT : osg::Texture::CLAMP_TO_EDGE);
            texture->setWrap(osg::Texture::WRAP_T, wrapT ? osg::Texture::REPEAT : osg::Texture::CLAMP_TO_EDGE);
        }

        bool handleEffect(const Nif::Node* nifNode, osg::StateSet* stateset, Resource::ImageManager* imageManager)
        {
            if (nifNode->recType != Nif::RC_NiTextureEffect)
            {
                Log(Debug::Info) << "Unhandled effect " << nifNode->recName << " in " << mFilename;
                return false;
            }

            const Nif::NiTextureEffect* textureEffect = static_cast<const Nif::NiTextureEffect*>(nifNode);
            if (textureEffect->textureType != Nif::NiTextureEffect::Environment_Map)
            {
                Log(Debug::Info) << "Unhandled NiTextureEffect type " << textureEffect->textureType << " in "
                                 << mFilename;
                return false;
            }

            if (textureEffect->texture.empty())
            {
                Log(Debug::Info) << "NiTextureEffect missing source texture in " << mFilename;
                return false;
            }

            osg::ref_ptr<osg::TexGen> texGen(new osg::TexGen);
            switch (textureEffect->coordGenType)
            {
                case Nif::NiTextureEffect::World_Parallel:
                    texGen->setMode(osg::TexGen::OBJECT_LINEAR);
                    break;
                case Nif::NiTextureEffect::World_Perspective:
                    texGen->setMode(osg::TexGen::EYE_LINEAR);
                    break;
                case Nif::NiTextureEffect::Sphere_Map:
                    texGen->setMode(osg::TexGen::SPHERE_MAP);
                    break;
                default:
                    Log(Debug::Info) << "Unhandled NiTextureEffect coordGenType " << textureEffect->coordGenType
                                     << " in " << mFilename;
                    return false;
            }

            osg::ref_ptr<osg::Image> image(handleSourceTexture(textureEffect->texture.getPtr(), imageManager));
            osg::ref_ptr<osg::Texture2D> texture2d(new osg::Texture2D(image));
            if (image)
                texture2d->setTextureSize(image->s(), image->t());
            texture2d->setName("envMap");
            handleTextureWrapping(texture2d, textureEffect->wrapS(), textureEffect->wrapT());

            int texUnit = 3; // FIXME

            stateset->setTextureAttributeAndModes(texUnit, texture2d, osg::StateAttribute::ON);
            stateset->setTextureAttributeAndModes(texUnit, texGen, osg::StateAttribute::ON);
            stateset->setTextureAttributeAndModes(texUnit, createEmissiveTexEnv(), osg::StateAttribute::ON);

            stateset->addUniform(new osg::Uniform("envMapColor", osg::Vec4f(1, 1, 1, 1)));
            return true;
        }

        // Get a default dataVariance for this node to be used as a hint by optimization (post)routines
        osg::ref_ptr<osg::Group> createNode(const Nif::Node* nifNode)
        {
            osg::ref_ptr<osg::Group> node;
            osg::Object::DataVariance dataVariance = osg::Object::UNSPECIFIED;

            switch (nifNode->recType)
            {
                case Nif::RC_NiBillboardNode:
                    dataVariance = osg::Object::DYNAMIC;
                    break;
                default:
                    // The Root node can be created as a Group if no transformation is required.
                    // This takes advantage of the fact root nodes can't have additional controllers
                    // loaded from an external .kf file (original engine just throws "can't find node" errors if you
                    // try).
                    if (nifNode->parents.empty() && nifNode->controller.empty() && nifNode->trafo.isIdentity())
                        node = new osg::Group;

                    dataVariance = nifNode->isBone ? osg::Object::DYNAMIC : osg::Object::STATIC;

                    break;
            }
            if (!node)
                node = new NifOsg::MatrixTransform(nifNode->trafo);

            node->setDataVariance(dataVariance);

            return node;
        }

        osg::ref_ptr<osg::Node> handleNode(const Nif::Node* nifNode, const Nif::Parent* parent, osg::Group* parentNode,
            Resource::ImageManager* imageManager, std::vector<unsigned int> boundTextures, int animflags,
            bool skipMeshes, bool hasMarkers, bool hasAnimatedParents, SceneUtil::TextKeyMap* textKeys,
            osg::Node* rootNode = nullptr)
        {
            if (rootNode != nullptr && Misc::StringUtils::ciEqual(nifNode->name, "Bounding Box"))
                return nullptr;

            osg::ref_ptr<osg::Group> node = createNode(nifNode);

            if (nifNode->recType == Nif::RC_NiBillboardNode)
            {
                node->addCullCallback(new BillboardCallback);
            }

            node->setName(nifNode->name);

            if (parentNode)
                parentNode->addChild(node);

            if (!rootNode)
                rootNode = node;

            // The original NIF record index is used for a variety of features:
            // - finding the correct emitter node for a particle system
            // - establishing connections to the animated collision shapes, which are handled in a separate loader
            // - finding a random child NiNode in NiBspArrayController
            node->setUserValue("recIndex", nifNode->recIndex);

            std::vector<Nif::ExtraPtr> extraCollection;

            for (Nif::ExtraPtr e = nifNode->extra; !e.empty(); e = e->next)
                extraCollection.emplace_back(e);

            for (const auto& extraNode : nifNode->extralist)
                if (!extraNode.empty())
                    extraCollection.emplace_back(extraNode);

            for (const auto& e : extraCollection)
            {
                if (e->recType == Nif::RC_NiTextKeyExtraData && textKeys)
                {
                    const Nif::NiTextKeyExtraData* tk = static_cast<const Nif::NiTextKeyExtraData*>(e.getPtr());
                    extractTextKeys(tk, *textKeys);
                }
                else if (e->recType == Nif::RC_NiStringExtraData)
                {
                    const Nif::NiStringExtraData* sd = static_cast<const Nif::NiStringExtraData*>(e.getPtr());

                    constexpr std::string_view extraDataIdentifer = "omw:data";

                    // String markers may contain important information
                    // affecting the entire subtree of this obj
                    if (sd->string == "MRK" && !Loader::getShowMarkers())
                    {
                        // Marker objects. These meshes are only visible in the editor.
                        hasMarkers = true;
                    }
                    else if (sd->string == "BONE")
                    {
                        node->getOrCreateUserDataContainer()->addDescription("CustomBone");
                    }
                    else if (sd->string.rfind(extraDataIdentifer, 0) == 0)
                    {
                        node->setUserValue(
                            Misc::OsgUserValues::sExtraData, sd->string.substr(extraDataIdentifer.length()));
                    }
                }
            }

            if (nifNode->recType == Nif::RC_NiBSAnimationNode || nifNode->recType == Nif::RC_NiBSParticleNode)
                animflags = nifNode->flags;

            if (nifNode->recType == Nif::RC_NiSortAdjustNode)
            {
                auto sortNode = static_cast<const Nif::NiSortAdjustNode*>(nifNode);

                if (sortNode->mSubSorter.empty())
                {
                    Log(Debug::Warning) << "Empty accumulator found in '" << nifNode->recName << "' node "
                                        << nifNode->recIndex;
                }
                else
                {
                    if (mPushedSorter && !mPushedSorter->mSubSorter.empty()
                        && mPushedSorter->mMode != Nif::NiSortAdjustNode::SortingMode_Inherit)
                        mLastAppliedNoInheritSorter = mPushedSorter;
                    mPushedSorter = sortNode;
                }
            }

            // Hide collision shapes, but don't skip the subgraph
            // We still need to animate the hidden bones so the physics system can access them
            if (nifNode->recType == Nif::RC_RootCollisionNode)
            {
                skipMeshes = true;
                node->setNodeMask(Loader::getHiddenNodeMask());
            }

            // We can skip creating meshes for hidden nodes if they don't have a VisController that
            // might make them visible later
            if (nifNode->isHidden())
            {
                bool hasVisController = false;
                for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
                {
                    hasVisController |= (ctrl->recType == Nif::RC_NiVisController);
                    if (hasVisController)
                        break;
                }

                if (!hasVisController)
                    skipMeshes = true; // skip child meshes, but still create the child node hierarchy for animating
                                       // collision shapes

                node->setNodeMask(Loader::getHiddenNodeMask());
            }

            if (nifNode->recType == Nif::RC_NiCollisionSwitch && !nifNode->collisionActive())
                node->setNodeMask(Loader::getIntersectionDisabledNodeMask());

            osg::ref_ptr<SceneUtil::CompositeStateSetUpdater> composite = new SceneUtil::CompositeStateSetUpdater;

            applyNodeProperties(nifNode, node, composite, imageManager, boundTextures, animflags);

            const bool isGeometry = isTypeGeometry(nifNode->recType);

            if (isGeometry && !skipMeshes)
            {
                const std::string nodeName = Misc::StringUtils::lowerCase(nifNode->name);
                static const std::string markerName = "tri editormarker";
                static const std::string shadowName = "shadow";
                static const std::string shadowName2 = "tri shadow";
                const bool isMarker = hasMarkers && !nodeName.compare(0, markerName.size(), markerName);
                if (!isMarker && nodeName.compare(0, shadowName.size(), shadowName)
                    && nodeName.compare(0, shadowName2.size(), shadowName2))
                {
                    Nif::NiSkinInstancePtr skin = static_cast<const Nif::NiGeometry*>(nifNode)->skin;

                    if (skin.empty())
                        handleGeometry(nifNode, parent, node, composite, boundTextures, animflags);
                    else
                        handleSkinnedGeometry(nifNode, parent, node, composite, boundTextures, animflags);

                    if (!nifNode->controller.empty())
                        handleMeshControllers(nifNode, node, composite, boundTextures, animflags);
                }
            }

            if (nifNode->recType == Nif::RC_NiParticles)
                handleParticleSystem(nifNode, parent, node, composite, animflags);

            if (composite->getNumControllers() > 0)
            {
                osg::Callback* cb = composite;
                if (composite->getNumControllers() == 1)
                    cb = composite->getController(0);
                if (animflags & Nif::NiNode::AnimFlag_AutoPlay)
                    node->addCullCallback(cb);
                else
                    node->addUpdateCallback(
                        cb); // have to remain as UpdateCallback so AssignControllerSourcesVisitor can find it.
            }

            bool isAnimated = false;
            handleNodeControllers(nifNode, node, animflags, isAnimated);
            hasAnimatedParents |= isAnimated;
            // Make sure empty nodes and animated shapes are not optimized away so the physics system can find them.
            if (isAnimated || (hasAnimatedParents && ((skipMeshes || hasMarkers) || isGeometry)))
                node->setDataVariance(osg::Object::DYNAMIC);

            // LOD and Switch nodes must be wrapped by a transform (the current node) to support transformations
            // properly and we need to attach their children to the osg::LOD/osg::Switch nodes but we must return that
            // transform to the caller of handleNode instead of the actual LOD/Switch nodes.
            osg::ref_ptr<osg::Group> currentNode = node;

            if (nifNode->recType == Nif::RC_NiSwitchNode)
            {
                const Nif::NiSwitchNode* niSwitchNode = static_cast<const Nif::NiSwitchNode*>(nifNode);
                osg::ref_ptr<osg::Switch> switchNode = handleSwitchNode(niSwitchNode);
                node->addChild(switchNode);
                if (niSwitchNode->name == Constants::NightDayLabel)
                    mHasNightDayLabel = true;
                else if (niSwitchNode->name == Constants::HerbalismLabel)
                    mHasHerbalismLabel = true;

                currentNode = switchNode;
            }
            else if (nifNode->recType == Nif::RC_NiLODNode)
            {
                const Nif::NiLODNode* niLodNode = static_cast<const Nif::NiLODNode*>(nifNode);
                osg::ref_ptr<osg::LOD> lodNode = handleLodNode(niLodNode);
                node->addChild(lodNode);
                currentNode = lodNode;
            }
            else if (nifNode->recType == Nif::RC_NiFltAnimationNode)
            {
                osg::ref_ptr<osg::Sequence> sequenceNode = prepareSequenceNode(nifNode);
                node->addChild(sequenceNode);
                currentNode = sequenceNode;
            }

            const Nif::NiNode* ninode = dynamic_cast<const Nif::NiNode*>(nifNode);
            if (ninode)
            {
                const Nif::NodeList& children = ninode->children;
                const Nif::Parent currentParent{ *ninode, parent };
                for (const auto& child : children)
                    if (!child.empty())
                        handleNode(child.getPtr(), &currentParent, currentNode, imageManager, boundTextures, animflags,
                            skipMeshes, hasMarkers, hasAnimatedParents, textKeys, rootNode);

                // Propagate effects to the the direct subgraph instead of the node itself
                // This simulates their "affected node list" which Morrowind appears to replace with the subgraph (?)
                // Note that the serialized affected node list is actually unused
                for (const auto& effect : ninode->effects)
                    if (!effect.empty())
                    {
                        osg::ref_ptr<osg::StateSet> effectStateSet = new osg::StateSet;
                        if (handleEffect(effect.getPtr(), effectStateSet, imageManager))
                            for (unsigned int i = 0; i < currentNode->getNumChildren(); ++i)
                                currentNode->getChild(i)->getOrCreateStateSet()->merge(*effectStateSet);
                    }
            }

            if (nifNode->recType == Nif::RC_NiFltAnimationNode)
                activateSequenceNode(currentNode, nifNode);

            return node;
        }

        void handleMeshControllers(const Nif::Node* nifNode, osg::Node* node,
            SceneUtil::CompositeStateSetUpdater* composite, const std::vector<unsigned int>& boundTextures,
            int animflags)
        {
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiUVController)
                {
                    const Nif::NiUVController* niuvctrl = static_cast<const Nif::NiUVController*>(ctrl.getPtr());
                    if (niuvctrl->data.empty())
                        continue;
                    const unsigned int uvSet = niuvctrl->uvSet;
                    std::set<int> texUnits;
                    // UVController should work only for textures which use a given UV Set, usually 0.
                    for (unsigned int i = 0; i < boundTextures.size(); ++i)
                    {
                        if (boundTextures[i] == uvSet)
                            texUnits.insert(i);
                    }

                    osg::ref_ptr<UVController> uvctrl = new UVController(niuvctrl->data.getPtr(), texUnits);
                    setupController(niuvctrl, uvctrl, animflags);
                    composite->addController(uvctrl);
                }
            }
        }

        void handleNodeControllers(const Nif::Node* nifNode, osg::Node* node, int animflags, bool& isAnimated)
        {
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiKeyframeController)
                {
                    const Nif::NiKeyframeController* key = static_cast<const Nif::NiKeyframeController*>(ctrl.getPtr());
                    if (key->mData.empty() && key->mInterpolator.empty())
                        continue;
                    if (!key->mInterpolator.empty() && key->mInterpolator->recType != Nif::RC_NiTransformInterpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiKeyframeController " << key->recIndex
                                          << " in " << mFilename;
                        continue;
                    }
                    osg::ref_ptr<KeyframeController> callback = new KeyframeController(key);
                    setupController(key, callback, animflags);
                    node->addUpdateCallback(callback);
                    isAnimated = true;
                }
                else if (ctrl->recType == Nif::RC_NiPathController)
                {
                    const Nif::NiPathController* path = static_cast<const Nif::NiPathController*>(ctrl.getPtr());
                    if (path->posData.empty() || path->floatData.empty())
                        continue;
                    osg::ref_ptr<PathController> callback(new PathController(path));
                    setupController(path, callback, animflags);
                    node->addUpdateCallback(callback);
                    isAnimated = true;
                }
                else if (ctrl->recType == Nif::RC_NiVisController)
                {
                    const Nif::NiVisController* visctrl = static_cast<const Nif::NiVisController*>(ctrl.getPtr());
                    if (visctrl->mData.empty() && visctrl->mInterpolator.empty())
                        continue;
                    if (!visctrl->mInterpolator.empty()
                        && visctrl->mInterpolator->recType != Nif::RC_NiBoolInterpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiVisController " << visctrl->recIndex
                                          << " in " << mFilename;
                        continue;
                    }
                    osg::ref_ptr<VisController> callback(new VisController(visctrl, Loader::getHiddenNodeMask()));
                    setupController(visctrl, callback, animflags);
                    node->addUpdateCallback(callback);
                }
                else if (ctrl->recType == Nif::RC_NiRollController)
                {
                    const Nif::NiRollController* rollctrl = static_cast<const Nif::NiRollController*>(ctrl.getPtr());
                    if (rollctrl->mData.empty() && rollctrl->mInterpolator.empty())
                        continue;
                    if (!rollctrl->mInterpolator.empty()
                        && rollctrl->mInterpolator->recType != Nif::RC_NiFloatInterpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiRollController " << rollctrl->recIndex
                                          << " in " << mFilename;
                        continue;
                    }
                    osg::ref_ptr<RollController> callback = new RollController(rollctrl);
                    setupController(rollctrl, callback, animflags);
                    node->addUpdateCallback(callback);
                    isAnimated = true;
                }
                else if (ctrl->recType == Nif::RC_NiGeomMorpherController
                    || ctrl->recType == Nif::RC_NiParticleSystemController
                    || ctrl->recType == Nif::RC_NiBSPArrayController || ctrl->recType == Nif::RC_NiUVController)
                {
                    // These controllers are handled elsewhere
                }
                else
                    Log(Debug::Info) << "Unhandled controller " << ctrl->recName << " on node " << nifNode->recIndex
                                     << " in " << mFilename;
            }
        }

        void handleMaterialControllers(const Nif::Property* materialProperty,
            SceneUtil::CompositeStateSetUpdater* composite, int animflags, const osg::Material* baseMaterial)
        {
            for (Nif::ControllerPtr ctrl = materialProperty->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiAlphaController)
                {
                    const Nif::NiAlphaController* alphactrl = static_cast<const Nif::NiAlphaController*>(ctrl.getPtr());
                    if (alphactrl->mData.empty() && alphactrl->mInterpolator.empty())
                        continue;
                    if (!alphactrl->mInterpolator.empty()
                        && alphactrl->mInterpolator->recType != Nif::RC_NiFloatInterpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiAlphaController "
                                          << alphactrl->recIndex << " in " << mFilename;
                        continue;
                    }
                    osg::ref_ptr<AlphaController> osgctrl = new AlphaController(alphactrl, baseMaterial);
                    setupController(alphactrl, osgctrl, animflags);
                    composite->addController(osgctrl);
                }
                else if (ctrl->recType == Nif::RC_NiMaterialColorController)
                {
                    const Nif::NiMaterialColorController* matctrl
                        = static_cast<const Nif::NiMaterialColorController*>(ctrl.getPtr());
                    if (matctrl->mData.empty() && matctrl->mInterpolator.empty())
                        continue;
                    auto targetColor = static_cast<MaterialColorController::TargetColor>(matctrl->mTargetColor);
                    if (mVersion <= Nif::NIFFile::VER_MW
                        && targetColor == MaterialColorController::TargetColor::Specular)
                        continue;
                    if (!matctrl->mInterpolator.empty()
                        && matctrl->mInterpolator->recType != Nif::RC_NiPoint3Interpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiMaterialColorController "
                                          << matctrl->recIndex << " in " << mFilename;
                        continue;
                    }
                    osg::ref_ptr<MaterialColorController> osgctrl = new MaterialColorController(matctrl, baseMaterial);
                    setupController(matctrl, osgctrl, animflags);
                    composite->addController(osgctrl);
                }
                else
                    Log(Debug::Info) << "Unexpected material controller " << ctrl->recType << " in " << mFilename;
            }
        }

        void handleTextureControllers(const Nif::Property* texProperty, SceneUtil::CompositeStateSetUpdater* composite,
            Resource::ImageManager* imageManager, osg::StateSet* stateset, int animflags)
        {
            for (Nif::ControllerPtr ctrl = texProperty->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiFlipController)
                {
                    const Nif::NiFlipController* flipctrl = static_cast<const Nif::NiFlipController*>(ctrl.getPtr());
                    if (!flipctrl->mInterpolator.empty()
                        && flipctrl->mInterpolator->recType != Nif::RC_NiFloatInterpolator)
                    {
                        Log(Debug::Error) << "Unsupported interpolator type for NiFlipController " << flipctrl->recIndex
                                          << " in " << mFilename;
                        continue;
                    }
                    std::vector<osg::ref_ptr<osg::Texture2D>> textures;

                    // inherit wrap settings from the target slot
                    osg::Texture2D* inherit
                        = dynamic_cast<osg::Texture2D*>(stateset->getTextureAttribute(0, osg::StateAttribute::TEXTURE));
                    osg::Texture2D::WrapMode wrapS = osg::Texture2D::REPEAT;
                    osg::Texture2D::WrapMode wrapT = osg::Texture2D::REPEAT;
                    if (inherit)
                    {
                        wrapS = inherit->getWrap(osg::Texture2D::WRAP_S);
                        wrapT = inherit->getWrap(osg::Texture2D::WRAP_T);
                    }

                    for (const auto& source : flipctrl->mSources)
                    {
                        if (source.empty())
                            continue;

                        osg::ref_ptr<osg::Image> image(handleSourceTexture(source.getPtr(), imageManager));
                        osg::ref_ptr<osg::Texture2D> texture(new osg::Texture2D(image));
                        if (image)
                            texture->setTextureSize(image->s(), image->t());
                        texture->setWrap(osg::Texture::WRAP_S, wrapS);
                        texture->setWrap(osg::Texture::WRAP_T, wrapT);
                        textures.push_back(texture);
                    }
                    osg::ref_ptr<FlipController> callback(new FlipController(flipctrl, textures));
                    setupController(ctrl.getPtr(), callback, animflags);
                    composite->addController(callback);
                }
                else
                    Log(Debug::Info) << "Unexpected texture controller " << ctrl->recName << " in " << mFilename;
            }
        }

        void handleParticlePrograms(Nif::NiParticleModifierPtr affectors, Nif::NiParticleModifierPtr colliders,
            osg::Group* attachTo, osgParticle::ParticleSystem* partsys,
            osgParticle::ParticleProcessor::ReferenceFrame rf)
        {
            osgParticle::ModularProgram* program = new osgParticle::ModularProgram;
            attachTo->addChild(program);
            program->setParticleSystem(partsys);
            program->setReferenceFrame(rf);
            for (; !affectors.empty(); affectors = affectors->next)
            {
                if (affectors->recType == Nif::RC_NiParticleGrowFade)
                {
                    const Nif::NiParticleGrowFade* gf = static_cast<const Nif::NiParticleGrowFade*>(affectors.getPtr());
                    program->addOperator(new GrowFadeAffector(gf->growTime, gf->fadeTime));
                }
                else if (affectors->recType == Nif::RC_NiGravity)
                {
                    const Nif::NiGravity* gr = static_cast<const Nif::NiGravity*>(affectors.getPtr());
                    program->addOperator(new GravityAffector(gr));
                }
                else if (affectors->recType == Nif::RC_NiParticleColorModifier)
                {
                    const Nif::NiParticleColorModifier* cl
                        = static_cast<const Nif::NiParticleColorModifier*>(affectors.getPtr());
                    if (cl->data.empty())
                        continue;
                    const Nif::NiColorData* clrdata = cl->data.getPtr();
                    program->addOperator(new ParticleColorAffector(clrdata));
                }
                else if (affectors->recType == Nif::RC_NiParticleRotation)
                {
                    // unused
                }
                else
                    Log(Debug::Info) << "Unhandled particle modifier " << affectors->recName << " in " << mFilename;
            }
            for (; !colliders.empty(); colliders = colliders->next)
            {
                if (colliders->recType == Nif::RC_NiPlanarCollider)
                {
                    const Nif::NiPlanarCollider* planarcollider
                        = static_cast<const Nif::NiPlanarCollider*>(colliders.getPtr());
                    program->addOperator(new PlanarCollider(planarcollider));
                }
                else if (colliders->recType == Nif::RC_NiSphericalCollider)
                {
                    const Nif::NiSphericalCollider* sphericalcollider
                        = static_cast<const Nif::NiSphericalCollider*>(colliders.getPtr());
                    program->addOperator(new SphericalCollider(sphericalcollider));
                }
                else
                    Log(Debug::Info) << "Unhandled particle collider " << colliders->recName << " in " << mFilename;
            }
        }

        // Load the initial state of the particle system, i.e. the initial particles and their positions, velocity and
        // colors.
        void handleParticleInitialState(
            const Nif::Node* nifNode, ParticleSystem* partsys, const Nif::NiParticleSystemController* partctrl)
        {
            auto particleNode = static_cast<const Nif::NiParticles*>(nifNode);
            if (particleNode->data.empty() || particleNode->data->recType != Nif::RC_NiParticlesData)
            {
                partsys->setQuota(partctrl->numParticles);
                return;
            }

            auto particledata = static_cast<const Nif::NiParticlesData*>(particleNode->data.getPtr());
            partsys->setQuota(particledata->numParticles);

            osg::BoundingBox box;

            int i = 0;
            for (const auto& particle : partctrl->particles)
            {
                if (i++ >= particledata->activeCount)
                    break;

                if (particle.lifespan <= 0)
                    continue;

                if (particle.vertex >= particledata->vertices.size())
                    continue;

                ParticleAgeSetter particletemplate(std::max(0.f, particle.lifetime));

                osgParticle::Particle* created = partsys->createParticle(&particletemplate);
                created->setLifeTime(particle.lifespan);

                // Note this position and velocity is not correct for a particle system with absolute reference frame,
                // which can not be done in this loader since we are not attached to the scene yet. Will be fixed up
                // post-load in the SceneManager.
                created->setVelocity(particle.velocity);
                const osg::Vec3f& position = particledata->vertices[particle.vertex];
                created->setPosition(position);

                created->setColorRange(osgParticle::rangev4(partctrl->color, partctrl->color));
                created->setAlphaRange(osgParticle::rangef(1.f, 1.f));

                float size = partctrl->size;
                if (particle.vertex < particledata->sizes.size())
                    size *= particledata->sizes[particle.vertex];

                created->setSizeRange(osgParticle::rangef(size, size));
                box.expandBy(osg::BoundingSphere(position, size));
            }

            // radius may be used to force a larger bounding box
            box.expandBy(osg::BoundingSphere(osg::Vec3(0, 0, 0), particledata->radius));

            partsys->setInitialBound(box);
        }

        osg::ref_ptr<Emitter> handleParticleEmitter(const Nif::NiParticleSystemController* partctrl)
        {
            std::vector<int> targets;
            if (partctrl->recType == Nif::RC_NiBSPArrayController && !partctrl->emitAtVertex())
            {
                getAllNiNodes(partctrl->emitter.getPtr(), targets);
            }

            osg::ref_ptr<Emitter> emitter = new Emitter(targets);

            osgParticle::ConstantRateCounter* counter = new osgParticle::ConstantRateCounter;
            if (partctrl->noAutoAdjust())
                counter->setNumberOfParticlesPerSecondToCreate(partctrl->emitRate);
            else if (partctrl->lifetime == 0 && partctrl->lifetimeRandom == 0)
                counter->setNumberOfParticlesPerSecondToCreate(0);
            else
                counter->setNumberOfParticlesPerSecondToCreate(
                    partctrl->numParticles / (partctrl->lifetime + partctrl->lifetimeRandom / 2));

            emitter->setCounter(counter);

            ParticleShooter* shooter = new ParticleShooter(partctrl->velocity - partctrl->velocityRandom * 0.5f,
                partctrl->velocity + partctrl->velocityRandom * 0.5f, partctrl->horizontalDir,
                partctrl->horizontalAngle, partctrl->verticalDir, partctrl->verticalAngle, partctrl->lifetime,
                partctrl->lifetimeRandom);
            emitter->setShooter(shooter);
            emitter->setFlags(partctrl->flags);

            if (partctrl->recType == Nif::RC_NiBSPArrayController && partctrl->emitAtVertex())
            {
                emitter->setGeometryEmitterTarget(partctrl->emitter->recIndex);
            }
            else
            {
                osgParticle::BoxPlacer* placer = new osgParticle::BoxPlacer;
                placer->setXRange(-partctrl->offsetRandom.x() / 2.f, partctrl->offsetRandom.x() / 2.f);
                placer->setYRange(-partctrl->offsetRandom.y() / 2.f, partctrl->offsetRandom.y() / 2.f);
                placer->setZRange(-partctrl->offsetRandom.z() / 2.f, partctrl->offsetRandom.z() / 2.f);
                emitter->setPlacer(placer);
            }

            return emitter;
        }

        void handleQueuedParticleEmitters(osg::Group* rootNode, Nif::FileView nif)
        {
            for (const auto& emitterPair : mEmitterQueue)
            {
                size_t recIndex = emitterPair.first;
                FindGroupByRecIndex findEmitterNode(recIndex);
                rootNode->accept(findEmitterNode);
                osg::Group* emitterNode = findEmitterNode.mFound;
                if (!emitterNode)
                {
                    Log(Debug::Warning)
                        << "NIFFile Warning: Failed to find particle emitter emitter node (node record index "
                        << recIndex << "). File: " << nif.getFilename();
                    continue;
                }

                // Emitter attached to the emitter node. Note one side effect of the emitter using the CullVisitor is
                // that hiding its node actually causes the emitter to stop firing. Convenient, because MW behaves this
                // way too!
                emitterNode->addChild(emitterPair.second);

                DisableOptimizer disableOptimizer;
                emitterNode->accept(disableOptimizer);
            }
            mEmitterQueue.clear();
        }

        void handleParticleSystem(const Nif::Node* nifNode, const Nif::Parent* parent, osg::Group* parentNode,
            SceneUtil::CompositeStateSetUpdater* composite, int animflags)
        {
            osg::ref_ptr<ParticleSystem> partsys(new ParticleSystem);
            partsys->setSortMode(osgParticle::ParticleSystem::SORT_BACK_TO_FRONT);

            const Nif::NiParticleSystemController* partctrl = nullptr;
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiParticleSystemController
                    || ctrl->recType == Nif::RC_NiBSPArrayController)
                    partctrl = static_cast<Nif::NiParticleSystemController*>(ctrl.getPtr());
            }
            if (!partctrl)
            {
                Log(Debug::Info) << "No particle controller found in " << mFilename;
                return;
            }

            osgParticle::ParticleProcessor::ReferenceFrame rf = (animflags & Nif::NiNode::ParticleFlag_LocalSpace)
                ? osgParticle::ParticleProcessor::RELATIVE_RF
                : osgParticle::ParticleProcessor::ABSOLUTE_RF;

            // HACK: ParticleSystem has no setReferenceFrame method
            if (rf == osgParticle::ParticleProcessor::ABSOLUTE_RF)
            {
                partsys->getOrCreateUserDataContainer()->addDescription("worldspace");
            }

            partsys->setParticleScaleReferenceFrame(osgParticle::ParticleSystem::LOCAL_COORDINATES);

            handleParticleInitialState(nifNode, partsys, partctrl);

            partsys->getDefaultParticleTemplate().setSizeRange(osgParticle::rangef(partctrl->size, partctrl->size));
            partsys->getDefaultParticleTemplate().setColorRange(osgParticle::rangev4(partctrl->color, partctrl->color));
            partsys->getDefaultParticleTemplate().setAlphaRange(osgParticle::rangef(1.f, 1.f));

            if (!partctrl->emitter.empty())
            {
                osg::ref_ptr<Emitter> emitter = handleParticleEmitter(partctrl);
                emitter->setParticleSystem(partsys);
                emitter->setReferenceFrame(osgParticle::ParticleProcessor::RELATIVE_RF);

                // The emitter node may not actually be handled yet, so let's delay attaching the emitter to a later
                // moment. If the emitter node is placed later than the particle node, it'll have a single frame delay
                // in particle processing. But that shouldn't be a game-breaking issue.
                mEmitterQueue.emplace_back(partctrl->emitter->recIndex, emitter);

                osg::ref_ptr<ParticleSystemController> callback(new ParticleSystemController(partctrl));
                setupController(partctrl, callback, animflags);
                emitter->setUpdateCallback(callback);

                if (!(animflags & Nif::NiNode::ParticleFlag_AutoPlay))
                {
                    partsys->setFrozen(true);
                }

                // Due to odd code in the ParticleSystemUpdater, particle systems will not be updated in the first frame
                // So do that update manually
                osg::NodeVisitor nv;
                partsys->update(0.0, nv);
            }

            // affectors should be attached *after* the emitter in the scene graph for correct update order
            // attach to same node as the ParticleSystem, we need osgParticle Operators to get the correct
            // localToWorldMatrix for transforming to particle space
            handleParticlePrograms(partctrl->affectors, partctrl->colliders, parentNode, partsys.get(), rf);

            std::vector<const Nif::Property*> drawableProps;
            collectDrawableProperties(nifNode, parent, drawableProps);
            applyDrawableProperties(parentNode, drawableProps, composite, true, animflags);

            // particle system updater (after the emitters and affectors in the scene graph)
            // I think for correct culling needs to be *before* the ParticleSystem, though osg examples do it the other
            // way
            osg::ref_ptr<osgParticle::ParticleSystemUpdater> updater = new osgParticle::ParticleSystemUpdater;
            updater->addParticleSystem(partsys);
            parentNode->addChild(updater);

            osg::Node* toAttach = partsys.get();

            if (rf == osgParticle::ParticleProcessor::RELATIVE_RF)
                parentNode->addChild(toAttach);
            else
            {
                osg::MatrixTransform* trans = new osg::MatrixTransform;
                trans->setUpdateCallback(new InverseWorldMatrix);
                trans->addChild(toAttach);
                parentNode->addChild(trans);
            }
        }

        void handleNiGeometryData(osg::Geometry* geometry, const Nif::NiGeometryData* data,
            const std::vector<unsigned int>& boundTextures, const std::string& name)
        {
            const auto& vertices = data->vertices;
            const auto& normals = data->normals;
            const auto& colors = data->colors;
            if (!vertices.empty())
                geometry->setVertexArray(new osg::Vec3Array(vertices.size(), vertices.data()));
            if (!normals.empty())
                geometry->setNormalArray(
                    new osg::Vec3Array(normals.size(), normals.data()), osg::Array::BIND_PER_VERTEX);
            if (!colors.empty())
                geometry->setColorArray(new osg::Vec4Array(colors.size(), colors.data()), osg::Array::BIND_PER_VERTEX);

            const auto& uvlist = data->uvlist;
            int textureStage = 0;
            for (std::vector<unsigned int>::const_iterator it = boundTextures.begin(); it != boundTextures.end();
                 ++it, ++textureStage)
            {
                unsigned int uvSet = *it;
                if (uvSet >= uvlist.size())
                {
                    Log(Debug::Verbose) << "Out of bounds UV set " << uvSet << " on shape \"" << name << "\" in "
                                        << mFilename;
                    if (uvlist.empty())
                        continue;
                    uvSet = 0;
                }

                geometry->setTexCoordArray(textureStage, new osg::Vec2Array(uvlist[uvSet].size(), uvlist[uvSet].data()),
                    osg::Array::BIND_PER_VERTEX);
            }
        }

        void handleNiGeometry(const Nif::Node* nifNode, const Nif::Parent* parent, osg::Geometry* geometry,
            osg::Node* parentNode, SceneUtil::CompositeStateSetUpdater* composite,
            const std::vector<unsigned int>& boundTextures, int animflags)
        {
            const Nif::NiGeometry* niGeometry = static_cast<const Nif::NiGeometry*>(nifNode);
            if (niGeometry->data.empty())
                return;
            const Nif::NiGeometryData* niGeometryData = niGeometry->data.getPtr();

            if (niGeometry->recType == Nif::RC_NiTriShape || nifNode->recType == Nif::RC_BSLODTriShape)
            {
                if (niGeometryData->recType != Nif::RC_NiTriShapeData)
                    return;
                auto triangles = static_cast<const Nif::NiTriShapeData*>(niGeometryData)->triangles;
                if (triangles.empty())
                    return;
                geometry->addPrimitiveSet(new osg::DrawElementsUShort(
                    osg::PrimitiveSet::TRIANGLES, triangles.size(), (unsigned short*)triangles.data()));
            }
            else if (niGeometry->recType == Nif::RC_NiTriStrips)
            {
                if (niGeometryData->recType != Nif::RC_NiTriStripsData)
                    return;
                auto data = static_cast<const Nif::NiTriStripsData*>(niGeometryData);
                bool hasGeometry = false;
                for (const auto& strip : data->strips)
                {
                    if (strip.size() < 3)
                        continue;
                    geometry->addPrimitiveSet(new osg::DrawElementsUShort(osg::PrimitiveSet::TRIANGLE_STRIP,
                        strip.size(), reinterpret_cast<const unsigned short*>(strip.data())));
                    hasGeometry = true;
                }
                if (!hasGeometry)
                    return;
            }
            else if (niGeometry->recType == Nif::RC_NiLines)
            {
                if (niGeometryData->recType != Nif::RC_NiLinesData)
                    return;
                auto data = static_cast<const Nif::NiLinesData*>(niGeometryData);
                const auto& line = data->lines;
                if (line.empty())
                    return;
                geometry->addPrimitiveSet(new osg::DrawElementsUShort(
                    osg::PrimitiveSet::LINES, line.size(), reinterpret_cast<const unsigned short*>(line.data())));
            }
            handleNiGeometryData(geometry, niGeometryData, boundTextures, nifNode->name);

            // osg::Material properties are handled here for two reasons:
            // - if there are no vertex colors, we need to disable colorMode.
            // - there are 3 "overlapping" nif properties that all affect the osg::Material, handling them
            //   above the actual renderable would be tedious.
            std::vector<const Nif::Property*> drawableProps;
            collectDrawableProperties(nifNode, parent, drawableProps);
            applyDrawableProperties(parentNode, drawableProps, composite, !niGeometryData->colors.empty(), animflags);
        }

        void handleGeometry(const Nif::Node* nifNode, const Nif::Parent* parent, osg::Group* parentNode,
            SceneUtil::CompositeStateSetUpdater* composite, const std::vector<unsigned int>& boundTextures,
            int animflags)
        {
            assert(isTypeGeometry(nifNode->recType));
            osg::ref_ptr<osg::Geometry> geom(new osg::Geometry);
            handleNiGeometry(nifNode, parent, geom, parentNode, composite, boundTextures, animflags);
            // If the record had no valid geometry data in it, early-out
            if (geom->empty())
                return;
            osg::ref_ptr<osg::Drawable> drawable;
            for (Nif::ControllerPtr ctrl = nifNode->controller; !ctrl.empty(); ctrl = ctrl->next)
            {
                if (!ctrl->isActive())
                    continue;
                if (ctrl->recType == Nif::RC_NiGeomMorpherController)
                {
                    const Nif::NiGeomMorpherController* nimorphctrl
                        = static_cast<const Nif::NiGeomMorpherController*>(ctrl.getPtr());
                    if (nimorphctrl->mData.empty())
                        continue;
                    drawable = handleMorphGeometry(nimorphctrl, geom, parentNode, composite, boundTextures, animflags);

                    osg::ref_ptr<GeomMorpherController> morphctrl = new GeomMorpherController(nimorphctrl);
                    setupController(ctrl.getPtr(), morphctrl, animflags);
                    drawable->setUpdateCallback(morphctrl);
                    break;
                }
            }
            if (!drawable.get())
                drawable = geom;
            drawable->setName(nifNode->name);
            parentNode->addChild(drawable);
        }

        osg::ref_ptr<osg::Drawable> handleMorphGeometry(const Nif::NiGeomMorpherController* morpher,
            osg::ref_ptr<osg::Geometry> sourceGeometry, osg::Node* parentNode,
            SceneUtil::CompositeStateSetUpdater* composite, const std::vector<unsigned int>& boundTextures,
            int animflags)
        {
            osg::ref_ptr<SceneUtil::MorphGeometry> morphGeom = new SceneUtil::MorphGeometry;
            morphGeom->setSourceGeometry(sourceGeometry);

            const std::vector<Nif::NiMorphData::MorphData>& morphs = morpher->mData.getPtr()->mMorphs;
            if (morphs.empty())
                return morphGeom;
            if (morphs[0].mVertices.size()
                != static_cast<const osg::Vec3Array*>(sourceGeometry->getVertexArray())->size())
                return morphGeom;
            for (unsigned int i = 0; i < morphs.size(); ++i)
                morphGeom->addMorphTarget(
                    new osg::Vec3Array(morphs[i].mVertices.size(), morphs[i].mVertices.data()), 0.f);

            return morphGeom;
        }

        void handleSkinnedGeometry(const Nif::Node* nifNode, const Nif::Parent* parent, osg::Group* parentNode,
            SceneUtil::CompositeStateSetUpdater* composite, const std::vector<unsigned int>& boundTextures,
            int animflags)
        {
            assert(isTypeGeometry(nifNode->recType));
            osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry);
            handleNiGeometry(nifNode, parent, geometry, parentNode, composite, boundTextures, animflags);
            if (geometry->empty())
                return;
            osg::ref_ptr<SceneUtil::RigGeometry> rig(new SceneUtil::RigGeometry);
            rig->setSourceGeometry(geometry);
            rig->setName(nifNode->name);

            // Assign bone weights
            osg::ref_ptr<SceneUtil::RigGeometry::InfluenceMap> map(new SceneUtil::RigGeometry::InfluenceMap);

            const Nif::NiSkinInstance* skin = static_cast<const Nif::NiGeometry*>(nifNode)->skin.getPtr();
            const Nif::NiSkinData* data = skin->data.getPtr();
            const Nif::NodeList& bones = skin->bones;
            for (std::size_t i = 0, n = bones.size(); i < n; ++i)
            {
                std::string boneName = Misc::StringUtils::lowerCase(bones[i].getPtr()->name);

                SceneUtil::RigGeometry::BoneInfluence influence;
                const std::vector<Nif::NiSkinData::VertWeight>& weights = data->bones[i].weights;
                for (size_t j = 0; j < weights.size(); j++)
                {
                    influence.mWeights.emplace_back(weights[j].vertex, weights[j].weight);
                }
                influence.mInvBindMatrix = data->bones[i].trafo.toMatrix();
                influence.mBoundSphere
                    = osg::BoundingSpheref(data->bones[i].boundSphereCenter, data->bones[i].boundSphereRadius);

                map->mData.emplace_back(boneName, influence);
            }
            rig->setInfluenceMap(map);

            parentNode->addChild(rig);
        }

        osg::BlendFunc::BlendFuncMode getBlendMode(int mode)
        {
            switch (mode)
            {
                case 0:
                    return osg::BlendFunc::ONE;
                case 1:
                    return osg::BlendFunc::ZERO;
                case 2:
                    return osg::BlendFunc::SRC_COLOR;
                case 3:
                    return osg::BlendFunc::ONE_MINUS_SRC_COLOR;
                case 4:
                    return osg::BlendFunc::DST_COLOR;
                case 5:
                    return osg::BlendFunc::ONE_MINUS_DST_COLOR;
                case 6:
                    return osg::BlendFunc::SRC_ALPHA;
                case 7:
                    return osg::BlendFunc::ONE_MINUS_SRC_ALPHA;
                case 8:
                    return osg::BlendFunc::DST_ALPHA;
                case 9:
                    return osg::BlendFunc::ONE_MINUS_DST_ALPHA;
                case 10:
                    return osg::BlendFunc::SRC_ALPHA_SATURATE;
                default:
                    Log(Debug::Info) << "Unexpected blend mode: " << mode << " in " << mFilename;
                    return osg::BlendFunc::SRC_ALPHA;
            }
        }

        osg::AlphaFunc::ComparisonFunction getTestMode(int mode)
        {
            switch (mode)
            {
                case 0:
                    return osg::AlphaFunc::ALWAYS;
                case 1:
                    return osg::AlphaFunc::LESS;
                case 2:
                    return osg::AlphaFunc::EQUAL;
                case 3:
                    return osg::AlphaFunc::LEQUAL;
                case 4:
                    return osg::AlphaFunc::GREATER;
                case 5:
                    return osg::AlphaFunc::NOTEQUAL;
                case 6:
                    return osg::AlphaFunc::GEQUAL;
                case 7:
                    return osg::AlphaFunc::NEVER;
                default:
                    Log(Debug::Info) << "Unexpected blend mode: " << mode << " in " << mFilename;
                    return osg::AlphaFunc::LEQUAL;
            }
        }

        osg::Stencil::Function getStencilFunction(int func)
        {
            switch (func)
            {
                case 0:
                    return osg::Stencil::NEVER;
                case 1:
                    return osg::Stencil::LESS;
                case 2:
                    return osg::Stencil::EQUAL;
                case 3:
                    return osg::Stencil::LEQUAL;
                case 4:
                    return osg::Stencil::GREATER;
                case 5:
                    return osg::Stencil::NOTEQUAL;
                case 6:
                    return osg::Stencil::GEQUAL;
                case 7:
                    return osg::Stencil::ALWAYS;
                default:
                    Log(Debug::Info) << "Unexpected stencil function: " << func << " in " << mFilename;
                    return osg::Stencil::NEVER;
            }
        }

        osg::Stencil::Operation getStencilOperation(int op)
        {
            switch (op)
            {
                case 0:
                    return osg::Stencil::KEEP;
                case 1:
                    return osg::Stencil::ZERO;
                case 2:
                    return osg::Stencil::REPLACE;
                case 3:
                    return osg::Stencil::INCR;
                case 4:
                    return osg::Stencil::DECR;
                case 5:
                    return osg::Stencil::INVERT;
                default:
                    Log(Debug::Info) << "Unexpected stencil operation: " << op << " in " << mFilename;
                    return osg::Stencil::KEEP;
            }
        }

        osg::ref_ptr<osg::Image> handleInternalTexture(const Nif::NiPixelData* pixelData)
        {
            osg::ref_ptr<osg::Image> image(new osg::Image);

            // Pixel row alignment, defining it to be consistent with OSG DDS plugin
            int packing = 1;
            GLenum pixelformat = 0;
            switch (pixelData->fmt)
            {
                case Nif::NiPixelData::NIPXFMT_RGB8:
                    pixelformat = GL_RGB;
                    break;
                case Nif::NiPixelData::NIPXFMT_RGBA8:
                    pixelformat = GL_RGBA;
                    break;
                case Nif::NiPixelData::NIPXFMT_PAL8:
                case Nif::NiPixelData::NIPXFMT_PALA8:
                    pixelformat = GL_RED; // Each color is defined by a byte.
                    break;
                case Nif::NiPixelData::NIPXFMT_BGR8:
                    pixelformat = GL_BGR;
                    break;
                case Nif::NiPixelData::NIPXFMT_BGRA8:
                    pixelformat = GL_BGRA;
                    break;
                case Nif::NiPixelData::NIPXFMT_DXT1:
                    pixelformat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
                    packing = 2;
                    break;
                case Nif::NiPixelData::NIPXFMT_DXT3:
                    pixelformat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                    packing = 4;
                    break;
                case Nif::NiPixelData::NIPXFMT_DXT5:
                    pixelformat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                    packing = 4;
                    break;
                default:
                    Log(Debug::Info) << "Unhandled internal pixel format " << pixelData->fmt << " in " << mFilename;
                    return nullptr;
            }

            if (pixelData->mipmaps.empty())
                return nullptr;

            int width = 0;
            int height = 0;

            std::vector<unsigned int> mipmapVector;
            for (unsigned int i = 0; i < pixelData->mipmaps.size(); ++i)
            {
                const Nif::NiPixelData::Mipmap& mip = pixelData->mipmaps[i];

                size_t mipSize = osg::Image::computeImageSizeInBytes(
                    mip.width, mip.height, 1, pixelformat, GL_UNSIGNED_BYTE, packing);
                if (mipSize + mip.dataOffset > pixelData->data.size())
                {
                    Log(Debug::Info) << "Internal texture's mipmap data out of bounds, ignoring texture";
                    return nullptr;
                }

                if (i != 0)
                    mipmapVector.push_back(mip.dataOffset);
                else
                {
                    width = mip.width;
                    height = mip.height;
                }
            }

            if (width <= 0 || height <= 0)
            {
                Log(Debug::Info) << "Internal Texture Width and height must be non zero, ignoring texture";
                return nullptr;
            }

            const std::vector<unsigned char>& pixels = pixelData->data;
            switch (pixelData->fmt)
            {
                case Nif::NiPixelData::NIPXFMT_RGB8:
                case Nif::NiPixelData::NIPXFMT_RGBA8:
                case Nif::NiPixelData::NIPXFMT_BGR8:
                case Nif::NiPixelData::NIPXFMT_BGRA8:
                case Nif::NiPixelData::NIPXFMT_DXT1:
                case Nif::NiPixelData::NIPXFMT_DXT3:
                case Nif::NiPixelData::NIPXFMT_DXT5:
                {
                    unsigned char* data = new unsigned char[pixels.size()];
                    memcpy(data, pixels.data(), pixels.size());
                    image->setImage(width, height, 1, pixelformat, pixelformat, GL_UNSIGNED_BYTE, data,
                        osg::Image::USE_NEW_DELETE, packing);
                    break;
                }
                case Nif::NiPixelData::NIPXFMT_PAL8:
                case Nif::NiPixelData::NIPXFMT_PALA8:
                {
                    if (pixelData->palette.empty() || pixelData->bpp != 8)
                    {
                        Log(Debug::Info) << "Palettized texture in " << mFilename << " is invalid, ignoring";
                        return nullptr;
                    }
                    pixelformat = pixelData->fmt == Nif::NiPixelData::NIPXFMT_PAL8 ? GL_RGB : GL_RGBA;
                    // We're going to convert the indices that pixel data contains
                    // into real colors using the palette.
                    const auto& palette = pixelData->palette->colors;
                    const int numChannels = pixelformat == GL_RGBA ? 4 : 3;
                    unsigned char* data = new unsigned char[pixels.size() * numChannels];
                    unsigned char* pixel = data;
                    for (unsigned char index : pixels)
                    {
                        memcpy(pixel, &palette[index], sizeof(unsigned char) * numChannels);
                        pixel += numChannels;
                    }
                    for (unsigned int& offset : mipmapVector)
                        offset *= numChannels;
                    image->setImage(width, height, 1, pixelformat, pixelformat, GL_UNSIGNED_BYTE, data,
                        osg::Image::USE_NEW_DELETE, packing);
                    break;
                }
                default:
                    return nullptr;
            }

            image->setMipmapLevels(mipmapVector);
            image->flipVertical();

            return image;
        }

        osg::ref_ptr<osg::TexEnvCombine> createEmissiveTexEnv()
        {
            osg::ref_ptr<osg::TexEnvCombine> texEnv(new osg::TexEnvCombine);
            // Sum the previous colour and the emissive colour.
            texEnv->setCombine_RGB(osg::TexEnvCombine::ADD);
            texEnv->setSource0_RGB(osg::TexEnvCombine::PREVIOUS);
            texEnv->setSource1_RGB(osg::TexEnvCombine::TEXTURE);
            // Keep the previous alpha.
            texEnv->setCombine_Alpha(osg::TexEnvCombine::REPLACE);
            texEnv->setSource0_Alpha(osg::TexEnvCombine::PREVIOUS);
            texEnv->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
            return texEnv;
        }

        void handleTextureProperty(const Nif::NiTexturingProperty* texprop, const std::string& nodeName,
            osg::StateSet* stateset, SceneUtil::CompositeStateSetUpdater* composite,
            Resource::ImageManager* imageManager, std::vector<unsigned int>& boundTextures, int animflags)
        {
            if (!boundTextures.empty())
            {
                // overriding a parent NiTexturingProperty, so remove what was previously bound
                for (unsigned int i = 0; i < boundTextures.size(); ++i)
                    stateset->setTextureMode(i, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                boundTextures.clear();
            }

            // If this loop is changed such that the base texture isn't guaranteed to end up in texture unit 0, the
            // shadow casting shader will need to be updated accordingly.
            for (size_t i = 0; i < texprop->textures.size(); ++i)
            {
                if (texprop->textures[i].inUse
                    || (i == Nif::NiTexturingProperty::BaseTexture && !texprop->controller.empty()))
                {
                    switch (i)
                    {
                        // These are handled later on
                        case Nif::NiTexturingProperty::BaseTexture:
                        case Nif::NiTexturingProperty::GlowTexture:
                        case Nif::NiTexturingProperty::DarkTexture:
                        case Nif::NiTexturingProperty::BumpTexture:
                        case Nif::NiTexturingProperty::DetailTexture:
                        case Nif::NiTexturingProperty::DecalTexture:
                        case Nif::NiTexturingProperty::GlossTexture:
                            break;
                        default:
                        {
                            Log(Debug::Info) << "Unhandled texture stage " << i << " on shape \"" << nodeName
                                             << "\" in " << mFilename;
                            continue;
                        }
                    }

                    unsigned int uvSet = 0;
                    // create a new texture, will later attempt to share using the SharedStateManager
                    osg::ref_ptr<osg::Texture2D> texture2d;
                    if (texprop->textures[i].inUse)
                    {
                        const Nif::NiTexturingProperty::Texture& tex = texprop->textures[i];
                        if (tex.texture.empty() && texprop->controller.empty())
                        {
                            if (i == 0)
                                Log(Debug::Warning) << "Base texture is in use but empty on shape \"" << nodeName
                                                    << "\" in " << mFilename;
                            continue;
                        }

                        if (!tex.texture.empty())
                        {
                            const Nif::NiSourceTexture* st = tex.texture.getPtr();
                            osg::ref_ptr<osg::Image> image = handleSourceTexture(st, imageManager);
                            texture2d = new osg::Texture2D(image);
                            if (image)
                                texture2d->setTextureSize(image->s(), image->t());
                        }
                        else
                            texture2d = new osg::Texture2D;

                        handleTextureWrapping(texture2d, tex.wrapS(), tex.wrapT());

                        uvSet = tex.uvSet;
                    }
                    else
                    {
                        // Texture only comes from NiFlipController, so tex is ignored, set defaults
                        texture2d = new osg::Texture2D;
                        handleTextureWrapping(texture2d, true, true);
                        uvSet = 0;
                    }

                    unsigned int texUnit = boundTextures.size();

                    stateset->setTextureAttributeAndModes(texUnit, texture2d, osg::StateAttribute::ON);

                    if (i == Nif::NiTexturingProperty::GlowTexture)
                    {
                        stateset->setTextureAttributeAndModes(texUnit, createEmissiveTexEnv(), osg::StateAttribute::ON);
                    }
                    else if (i == Nif::NiTexturingProperty::DarkTexture)
                    {
                        osg::TexEnv* texEnv = new osg::TexEnv;
                        // Modulate both the colour and the alpha with the dark map.
                        texEnv->setMode(osg::TexEnv::MODULATE);
                        stateset->setTextureAttributeAndModes(texUnit, texEnv, osg::StateAttribute::ON);
                    }
                    else if (i == Nif::NiTexturingProperty::DetailTexture)
                    {
                        osg::TexEnvCombine* texEnv = new osg::TexEnvCombine;
                        // Modulate previous colour...
                        texEnv->setCombine_RGB(osg::TexEnvCombine::MODULATE);
                        texEnv->setSource0_RGB(osg::TexEnvCombine::PREVIOUS);
                        texEnv->setOperand0_RGB(osg::TexEnvCombine::SRC_COLOR);
                        // with the detail map's colour,
                        texEnv->setSource1_RGB(osg::TexEnvCombine::TEXTURE);
                        texEnv->setOperand1_RGB(osg::TexEnvCombine::SRC_COLOR);
                        // and a twist:
                        texEnv->setScale_RGB(2.f);
                        // Keep the previous alpha.
                        texEnv->setCombine_Alpha(osg::TexEnvCombine::REPLACE);
                        texEnv->setSource0_Alpha(osg::TexEnvCombine::PREVIOUS);
                        texEnv->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                        stateset->setTextureAttributeAndModes(texUnit, texEnv, osg::StateAttribute::ON);
                    }
                    else if (i == Nif::NiTexturingProperty::BumpTexture)
                    {
                        // Bump maps offset the environment map.
                        // Set this texture to Off by default since we can't render it with the fixed-function pipeline
                        stateset->setTextureMode(texUnit, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                        osg::Matrix2 bumpMapMatrix(texprop->bumpMapMatrix.x(), texprop->bumpMapMatrix.y(),
                            texprop->bumpMapMatrix.z(), texprop->bumpMapMatrix.w());
                        stateset->addUniform(new osg::Uniform("bumpMapMatrix", bumpMapMatrix));
                        stateset->addUniform(new osg::Uniform("envMapLumaBias", texprop->envMapLumaBias));
                    }
                    else if (i == Nif::NiTexturingProperty::GlossTexture)
                    {
                        // A gloss map is an environment map mask.
                        // Gloss maps are only implemented in the object shaders as well.
                        stateset->setTextureMode(texUnit, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                    }
                    else if (i == Nif::NiTexturingProperty::DecalTexture)
                    {
                        // This is only an inaccurate imitation of the original implementation,
                        // see https://github.com/niftools/nifskope/issues/184

                        osg::TexEnvCombine* texEnv = new osg::TexEnvCombine;
                        // Interpolate to the decal texture's colour...
                        texEnv->setCombine_RGB(osg::TexEnvCombine::INTERPOLATE);
                        texEnv->setSource0_RGB(osg::TexEnvCombine::TEXTURE);
                        texEnv->setOperand0_RGB(osg::TexEnvCombine::SRC_COLOR);
                        // ...from the previous colour...
                        texEnv->setSource1_RGB(osg::TexEnvCombine::PREVIOUS);
                        texEnv->setOperand1_RGB(osg::TexEnvCombine::SRC_COLOR);
                        // using the decal texture's alpha as the factor.
                        texEnv->setSource2_RGB(osg::TexEnvCombine::TEXTURE);
                        texEnv->setOperand2_RGB(osg::TexEnvCombine::SRC_ALPHA);
                        // Keep the previous alpha.
                        texEnv->setCombine_Alpha(osg::TexEnvCombine::REPLACE);
                        texEnv->setSource0_Alpha(osg::TexEnvCombine::PREVIOUS);
                        texEnv->setOperand0_Alpha(osg::TexEnvCombine::SRC_ALPHA);
                        stateset->setTextureAttributeAndModes(texUnit, texEnv, osg::StateAttribute::ON);
                    }

                    switch (i)
                    {
                        case Nif::NiTexturingProperty::BaseTexture:
                            texture2d->setName("diffuseMap");
                            break;
                        case Nif::NiTexturingProperty::BumpTexture:
                            texture2d->setName("bumpMap");
                            break;
                        case Nif::NiTexturingProperty::GlowTexture:
                            texture2d->setName("emissiveMap");
                            break;
                        case Nif::NiTexturingProperty::DarkTexture:
                            texture2d->setName("darkMap");
                            break;
                        case Nif::NiTexturingProperty::DetailTexture:
                            texture2d->setName("detailMap");
                            break;
                        case Nif::NiTexturingProperty::DecalTexture:
                            texture2d->setName("decalMap");
                            break;
                        case Nif::NiTexturingProperty::GlossTexture:
                            texture2d->setName("glossMap");
                            break;
                        default:
                            break;
                    }

                    boundTextures.push_back(uvSet);
                }
            }
            handleTextureControllers(texprop, composite, imageManager, stateset, animflags);
        }

        void handleTextureSet(const Nif::BSShaderTextureSet* textureSet, unsigned int clamp,
            const std::string& nodeName, osg::StateSet* stateset, Resource::ImageManager* imageManager,
            std::vector<unsigned int>& boundTextures)
        {
            if (!boundTextures.empty())
            {
                for (unsigned int i = 0; i < boundTextures.size(); ++i)
                    stateset->setTextureMode(i, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                boundTextures.clear();
            }

            const unsigned int uvSet = 0;

            for (size_t i = 0; i < textureSet->textures.size(); ++i)
            {
                if (textureSet->textures[i].empty())
                    continue;
                switch (i)
                {
                    case Nif::BSShaderTextureSet::TextureType_Base:
                    case Nif::BSShaderTextureSet::TextureType_Normal:
                    case Nif::BSShaderTextureSet::TextureType_Glow:
                        break;
                    default:
                    {
                        Log(Debug::Info) << "Unhandled texture stage " << i << " on shape \"" << nodeName << "\" in "
                                         << mFilename;
                        continue;
                    }
                }
                std::string filename
                    = Misc::ResourceHelpers::correctTexturePath(textureSet->textures[i], imageManager->getVFS());
                osg::ref_ptr<osg::Image> image = imageManager->getImage(filename);
                osg::ref_ptr<osg::Texture2D> texture2d = new osg::Texture2D(image);
                if (image)
                    texture2d->setTextureSize(image->s(), image->t());
                handleTextureWrapping(texture2d, (clamp >> 1) & 0x1, clamp & 0x1);
                unsigned int texUnit = boundTextures.size();
                stateset->setTextureAttributeAndModes(texUnit, texture2d, osg::StateAttribute::ON);
                // BSShaderTextureSet presence means there's no need for FFP support for the affected node
                switch (i)
                {
                    case Nif::BSShaderTextureSet::TextureType_Base:
                        texture2d->setName("diffuseMap");
                        break;
                    case Nif::BSShaderTextureSet::TextureType_Normal:
                        texture2d->setName("normalMap");
                        break;
                    case Nif::BSShaderTextureSet::TextureType_Glow:
                        texture2d->setName("emissiveMap");
                        break;
                }
                boundTextures.emplace_back(uvSet);
            }
        }

        std::string_view getBSShaderPrefix(unsigned int type) const
        {
            switch (static_cast<Nif::BSShaderType>(type))
            {
                case Nif::BSShaderType::ShaderType_Default:
                    return "nv_default";
                case Nif::BSShaderType::ShaderType_NoLighting:
                    return "nv_nolighting";
                case Nif::BSShaderType::ShaderType_TallGrass:
                case Nif::BSShaderType::ShaderType_Sky:
                case Nif::BSShaderType::ShaderType_Skin:
                case Nif::BSShaderType::ShaderType_Water:
                case Nif::BSShaderType::ShaderType_Lighting30:
                case Nif::BSShaderType::ShaderType_Tile:
                    Log(Debug::Warning) << "Unhandled BSShaderType " << type << " in " << mFilename;
                    return "nv_default";
            }
            Log(Debug::Warning) << "Unknown BSShaderType " << type << " in " << mFilename;
            return "nv_default";
        }

        std::string_view getBSLightingShaderPrefix(unsigned int type) const
        {
            switch (static_cast<Nif::BSLightingShaderType>(type))
            {
                case Nif::BSLightingShaderType::ShaderType_Default:
                    return "nv_default";
                case Nif::BSLightingShaderType::ShaderType_EnvMap:
                case Nif::BSLightingShaderType::ShaderType_Glow:
                case Nif::BSLightingShaderType::ShaderType_Parallax:
                case Nif::BSLightingShaderType::ShaderType_FaceTint:
                case Nif::BSLightingShaderType::ShaderType_SkinTint:
                case Nif::BSLightingShaderType::ShaderType_HairTint:
                case Nif::BSLightingShaderType::ShaderType_ParallaxOcc:
                case Nif::BSLightingShaderType::ShaderType_MultitexLand:
                case Nif::BSLightingShaderType::ShaderType_LODLand:
                case Nif::BSLightingShaderType::ShaderType_Snow:
                case Nif::BSLightingShaderType::ShaderType_MultiLayerParallax:
                case Nif::BSLightingShaderType::ShaderType_TreeAnim:
                case Nif::BSLightingShaderType::ShaderType_LODObjects:
                case Nif::BSLightingShaderType::ShaderType_SparkleSnow:
                case Nif::BSLightingShaderType::ShaderType_LODObjectsHD:
                case Nif::BSLightingShaderType::ShaderType_EyeEnvmap:
                case Nif::BSLightingShaderType::ShaderType_Cloud:
                case Nif::BSLightingShaderType::ShaderType_LODNoise:
                case Nif::BSLightingShaderType::ShaderType_MultitexLandLODBlend:
                case Nif::BSLightingShaderType::ShaderType_Dismemberment:
                    Log(Debug::Warning) << "Unhandled BSLightingShaderType " << type << " in " << mFilename;
                    return "nv_default";
            }
            Log(Debug::Warning) << "Unknown BSLightingShaderType " << type << " in " << mFilename;
            return "nv_default";
        }

        void handleProperty(const Nif::Property* property, osg::Node* node,
            SceneUtil::CompositeStateSetUpdater* composite, Resource::ImageManager* imageManager,
            std::vector<unsigned int>& boundTextures, int animflags, bool hasStencilProperty)
        {
            switch (property->recType)
            {
                case Nif::RC_NiStencilProperty:
                {
                    const Nif::NiStencilProperty* stencilprop = static_cast<const Nif::NiStencilProperty*>(property);
                    osg::ref_ptr<osg::FrontFace> frontFace = new osg::FrontFace;
                    switch (stencilprop->data.drawMode)
                    {
                        case 2:
                            frontFace->setMode(osg::FrontFace::CLOCKWISE);
                            break;
                        case 0:
                        case 1:
                        default:
                            frontFace->setMode(osg::FrontFace::COUNTER_CLOCKWISE);
                            break;
                    }
                    frontFace = shareAttribute(frontFace);

                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    stateset->setAttribute(frontFace, osg::StateAttribute::ON);
                    stateset->setMode(GL_CULL_FACE,
                        stencilprop->data.drawMode == 3 ? osg::StateAttribute::OFF : osg::StateAttribute::ON);

                    if (stencilprop->data.enabled != 0)
                    {
                        mHasStencilProperty = true;
                        osg::ref_ptr<osg::Stencil> stencil = new osg::Stencil;
                        stencil->setFunction(getStencilFunction(stencilprop->data.compareFunc),
                            stencilprop->data.stencilRef, stencilprop->data.stencilMask);
                        stencil->setStencilFailOperation(getStencilOperation(stencilprop->data.failAction));
                        stencil->setStencilPassAndDepthFailOperation(
                            getStencilOperation(stencilprop->data.zFailAction));
                        stencil->setStencilPassAndDepthPassOperation(
                            getStencilOperation(stencilprop->data.zPassAction));
                        stencil = shareAttribute(stencil);

                        stateset->setAttributeAndModes(stencil, osg::StateAttribute::ON);
                    }
                    break;
                }
                case Nif::RC_NiWireframeProperty:
                {
                    const Nif::NiWireframeProperty* wireprop = static_cast<const Nif::NiWireframeProperty*>(property);
                    osg::ref_ptr<osg::PolygonMode> mode = new osg::PolygonMode;
                    mode->setMode(osg::PolygonMode::FRONT_AND_BACK,
                        wireprop->isEnabled() ? osg::PolygonMode::LINE : osg::PolygonMode::FILL);
                    mode = shareAttribute(mode);
                    node->getOrCreateStateSet()->setAttributeAndModes(mode, osg::StateAttribute::ON);
                    break;
                }
                case Nif::RC_NiZBufferProperty:
                {
                    const Nif::NiZBufferProperty* zprop = static_cast<const Nif::NiZBufferProperty*>(property);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    stateset->setMode(
                        GL_DEPTH_TEST, zprop->depthTest() ? osg::StateAttribute::ON : osg::StateAttribute::OFF);
                    osg::ref_ptr<osg::Depth> depth = new osg::Depth;
                    depth->setWriteMask(zprop->depthWrite());
                    // Morrowind ignores depth test function, unless a NiStencilProperty is present, in which case it
                    // uses a fixed depth function of GL_ALWAYS.
                    if (hasStencilProperty)
                        depth->setFunction(osg::Depth::ALWAYS);
                    depth = shareAttribute(depth);
                    stateset->setAttributeAndModes(depth, osg::StateAttribute::ON);
                    break;
                }
                // OSG groups the material properties that NIFs have separate, so we have to parse them all again when
                // one changed
                case Nif::RC_NiMaterialProperty:
                case Nif::RC_NiVertexColorProperty:
                case Nif::RC_NiSpecularProperty:
                {
                    // Handled on drawable level so we know whether vertex colors are available
                    break;
                }
                case Nif::RC_NiAlphaProperty:
                {
                    // Handled on drawable level to prevent RenderBin nesting issues
                    break;
                }
                case Nif::RC_NiTexturingProperty:
                {
                    const Nif::NiTexturingProperty* texprop = static_cast<const Nif::NiTexturingProperty*>(property);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    handleTextureProperty(
                        texprop, node->getName(), stateset, composite, imageManager, boundTextures, animflags);
                    break;
                }
                case Nif::RC_BSShaderPPLightingProperty:
                {
                    auto texprop = static_cast<const Nif::BSShaderPPLightingProperty*>(property);
                    bool shaderRequired = true;
                    node->setUserValue("shaderPrefix", std::string(getBSShaderPrefix(texprop->type)));
                    node->setUserValue("shaderRequired", shaderRequired);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    if (!texprop->textureSet.empty())
                    {
                        auto textureSet = texprop->textureSet.getPtr();
                        handleTextureSet(
                            textureSet, texprop->clamp, node->getName(), stateset, imageManager, boundTextures);
                    }
                    handleTextureControllers(texprop, composite, imageManager, stateset, animflags);
                    break;
                }
                case Nif::RC_BSShaderNoLightingProperty:
                {
                    auto texprop = static_cast<const Nif::BSShaderNoLightingProperty*>(property);
                    bool shaderRequired = true;
                    node->setUserValue("shaderPrefix", std::string(getBSShaderPrefix(texprop->type)));
                    node->setUserValue("shaderRequired", shaderRequired);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    if (!texprop->filename.empty())
                    {
                        if (!boundTextures.empty())
                        {
                            for (unsigned int i = 0; i < boundTextures.size(); ++i)
                                stateset->setTextureMode(i, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                            boundTextures.clear();
                        }
                        std::string filename
                            = Misc::ResourceHelpers::correctTexturePath(texprop->filename, imageManager->getVFS());
                        osg::ref_ptr<osg::Image> image = imageManager->getImage(filename);
                        osg::ref_ptr<osg::Texture2D> texture2d = new osg::Texture2D(image);
                        texture2d->setName("diffuseMap");
                        if (image)
                            texture2d->setTextureSize(image->s(), image->t());
                        handleTextureWrapping(texture2d, texprop->wrapS(), texprop->wrapT());
                        const unsigned int texUnit = 0;
                        const unsigned int uvSet = 0;
                        stateset->setTextureAttributeAndModes(texUnit, texture2d, osg::StateAttribute::ON);
                        boundTextures.push_back(uvSet);
                    }
                    if (mBethVersion >= 27)
                    {
                        stateset->addUniform(new osg::Uniform("useFalloff", true));
                        stateset->addUniform(new osg::Uniform("falloffParams", texprop->falloffParams));
                    }
                    else
                    {
                        stateset->addUniform(new osg::Uniform("useFalloff", false));
                    }
                    handleTextureControllers(texprop, composite, imageManager, stateset, animflags);
                    break;
                }
                case Nif::RC_BSLightingShaderProperty:
                {
                    auto texprop = static_cast<const Nif::BSLightingShaderProperty*>(property);
                    bool shaderRequired = true;
                    node->setUserValue("shaderPrefix", std::string(getBSLightingShaderPrefix(texprop->type)));
                    node->setUserValue("shaderRequired", shaderRequired);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    if (!texprop->mTextureSet.empty())
                        handleTextureSet(texprop->mTextureSet.getPtr(), texprop->mClamp, node->getName(), stateset,
                            imageManager, boundTextures);
                    handleTextureControllers(texprop, composite, imageManager, stateset, animflags);
                    break;
                }
                case Nif::RC_BSEffectShaderProperty:
                {
                    auto texprop = static_cast<const Nif::BSEffectShaderProperty*>(property);
                    bool shaderRequired = true;
                    node->setUserValue("shaderPrefix", std::string("nv_nolighting"));
                    node->setUserValue("shaderRequired", shaderRequired);
                    osg::StateSet* stateset = node->getOrCreateStateSet();
                    if (!texprop->mSourceTexture.empty())
                    {
                        if (!boundTextures.empty())
                        {
                            for (unsigned int i = 0; i < boundTextures.size(); ++i)
                                stateset->setTextureMode(i, GL_TEXTURE_2D, osg::StateAttribute::OFF);
                            boundTextures.clear();
                        }
                        std::string filename = Misc::ResourceHelpers::correctTexturePath(
                            texprop->mSourceTexture, imageManager->getVFS());
                        osg::ref_ptr<osg::Image> image = imageManager->getImage(filename);
                        osg::ref_ptr<osg::Texture2D> texture2d = new osg::Texture2D(image);
                        texture2d->setName("diffuseMap");
                        if (image)
                            texture2d->setTextureSize(image->s(), image->t());
                        handleTextureWrapping(texture2d, (texprop->mClamp >> 1) & 0x1, texprop->mClamp & 0x1);
                        const unsigned int texUnit = 0;
                        const unsigned int uvSet = 0;
                        stateset->setTextureAttributeAndModes(texUnit, texture2d, osg::StateAttribute::ON);
                        boundTextures.push_back(uvSet);

                        {
                            osg::ref_ptr<osg::TexMat> texMat(new osg::TexMat);
                            // This handles 20.2.0.7 UV settings like 4.0.0.2 UV settings (see NifOsg::UVController)
                            // TODO: verify
                            osg::Vec3f uvOrigin(0.5f, 0.5f, 0.f);
                            osg::Vec3f uvScale(texprop->mUVScale.x(), texprop->mUVScale.y(), 1.f);
                            osg::Vec3f uvTrans(-texprop->mUVOffset.x(), -texprop->mUVOffset.y(), 0.f);

                            osg::Matrixf mat = osg::Matrixf::translate(uvOrigin);
                            mat.preMultScale(uvScale);
                            mat.preMultTranslate(-uvOrigin);
                            mat.setTrans(mat.getTrans() + uvTrans);

                            texMat->setMatrix(mat);
                            stateset->setTextureAttributeAndModes(texUnit, texMat, osg::StateAttribute::ON);
                        }
                    }
                    stateset->addUniform(new osg::Uniform("useFalloff", true)); // Should use the shader flag
                    stateset->addUniform(new osg::Uniform("falloffParams", texprop->mFalloffParams));
                    handleTextureControllers(texprop, composite, imageManager, stateset, animflags);
                    break;
                }
                // unused by mw
                case Nif::RC_NiShadeProperty:
                case Nif::RC_NiDitherProperty:
                case Nif::RC_NiFogProperty:
                {
                    break;
                }
                default:
                    Log(Debug::Info) << "Unhandled " << property->recName << " in " << mFilename;
                    break;
            }
        }

        struct CompareStateAttribute
        {
            bool operator()(
                const osg::ref_ptr<osg::StateAttribute>& left, const osg::ref_ptr<osg::StateAttribute>& right) const
            {
                return left->compare(*right) < 0;
            }
        };

        // global sharing of State Attributes will reduce the number of GL calls as the osg::State will check by pointer
        // to see if state is the same
        template <class Attribute>
        Attribute* shareAttribute(const osg::ref_ptr<Attribute>& attr)
        {
            typedef std::set<osg::ref_ptr<Attribute>, CompareStateAttribute> Cache;
            static Cache sCache;
            static std::mutex sMutex;
            std::lock_guard<std::mutex> lock(sMutex);
            typename Cache::iterator found = sCache.find(attr);
            if (found == sCache.end())
                found = sCache.insert(attr).first;
            return *found;
        }

        void applyDrawableProperties(osg::Node* node, const std::vector<const Nif::Property*>& properties,
            SceneUtil::CompositeStateSetUpdater* composite, bool hasVertexColors, int animflags)
        {
            // Specular lighting is enabled by default, but there's a quirk...
            bool specEnabled = true;
            osg::ref_ptr<osg::Material> mat(new osg::Material);
            mat->setColorMode(hasVertexColors ? osg::Material::AMBIENT_AND_DIFFUSE : osg::Material::OFF);

            // NIF material defaults don't match OpenGL defaults
            mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
            mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));

            bool hasMatCtrl = false;
            bool hasSortAlpha = false;
            osg::StateSet* blendFuncStateSet = nullptr;

            auto setBin_Transparent = [](osg::StateSet* ss) { ss->setRenderingHint(osg::StateSet::TRANSPARENT_BIN); };
            auto setBin_BackToFront = [](osg::StateSet* ss) { ss->setRenderBinDetails(0, "SORT_BACK_TO_FRONT"); };
            auto setBin_Traversal = [](osg::StateSet* ss) { ss->setRenderBinDetails(2, "TraversalOrderBin"); };
            auto setBin_Inherit = [](osg::StateSet* ss) { ss->setRenderBinToInherit(); };

            int lightmode = 1;
            float emissiveMult = 1.f;
            float specStrength = 1.f;

            for (const Nif::Property* property : properties)
            {
                switch (property->recType)
                {
                    case Nif::RC_NiSpecularProperty:
                    {
                        // Specular property can turn specular lighting off.
                        // FIXME: NiMaterialColorController doesn't care about this.
                        auto specprop = static_cast<const Nif::NiSpecularProperty*>(property);
                        specEnabled = specprop->isEnabled();
                        break;
                    }
                    case Nif::RC_NiMaterialProperty:
                    {
                        const Nif::NiMaterialProperty* matprop = static_cast<const Nif::NiMaterialProperty*>(property);

                        mat->setDiffuse(
                            osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.diffuse, matprop->data.alpha));
                        mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.ambient, 1.f));
                        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.emissive, 1.f));
                        emissiveMult = matprop->data.emissiveMult;

                        mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(matprop->data.specular, 1.f));
                        mat->setShininess(osg::Material::FRONT_AND_BACK, matprop->data.glossiness);

                        if (!matprop->controller.empty())
                        {
                            hasMatCtrl = true;
                            handleMaterialControllers(matprop, composite, animflags, mat);
                        }

                        break;
                    }
                    case Nif::RC_NiVertexColorProperty:
                    {
                        const Nif::NiVertexColorProperty* vertprop
                            = static_cast<const Nif::NiVertexColorProperty*>(property);

                        switch (vertprop->mVertexMode)
                        {
                            case Nif::NiVertexColorProperty::VertexMode::VertMode_SrcIgnore:
                            {
                                mat->setColorMode(osg::Material::OFF);
                                break;
                            }
                            case Nif::NiVertexColorProperty::VertexMode::VertMode_SrcEmissive:
                            {
                                mat->setColorMode(osg::Material::EMISSION);
                                break;
                            }
                            case Nif::NiVertexColorProperty::VertexMode::VertMode_SrcAmbDif:
                            {
                                switch (vertprop->mLightingMode)
                                {
                                    case Nif::NiVertexColorProperty::LightMode::LightMode_Emissive:
                                    {
                                        mat->setColorMode(osg::Material::OFF);
                                        break;
                                    }
                                    case Nif::NiVertexColorProperty::LightMode::LightMode_EmiAmbDif:
                                    default:
                                    {
                                        mat->setColorMode(osg::Material::AMBIENT_AND_DIFFUSE);
                                        break;
                                    }
                                }
                                break;
                            }
                        }

                        break;
                    }
                    case Nif::RC_NiAlphaProperty:
                    {
                        const Nif::NiAlphaProperty* alphaprop = static_cast<const Nif::NiAlphaProperty*>(property);
                        if (alphaprop->useAlphaBlending())
                        {
                            osg::ref_ptr<osg::BlendFunc> blendFunc(
                                new osg::BlendFunc(getBlendMode(alphaprop->sourceBlendMode()),
                                    getBlendMode(alphaprop->destinationBlendMode())));
                            // on AMD hardware, alpha still seems to be stored with an RGBA framebuffer with OpenGL.
                            // This might be mandated by the OpenGL 2.1 specification section 2.14.9, or might be a bug.
                            // Either way, D3D8.1 doesn't do that, so adapt the destination factor.
                            if (blendFunc->getDestination() == GL_DST_ALPHA)
                                blendFunc->setDestination(GL_ONE);
                            blendFunc = shareAttribute(blendFunc);
                            node->getOrCreateStateSet()->setAttributeAndModes(blendFunc, osg::StateAttribute::ON);

                            if (!alphaprop->noSorter())
                            {
                                hasSortAlpha = true;
                                if (!mPushedSorter)
                                    setBin_Transparent(node->getStateSet());
                            }
                            else
                            {
                                if (!mPushedSorter)
                                    setBin_Inherit(node->getStateSet());
                            }
                        }
                        else if (osg::StateSet* stateset = node->getStateSet())
                        {
                            stateset->removeAttribute(osg::StateAttribute::BLENDFUNC);
                            stateset->removeMode(GL_BLEND);
                            blendFuncStateSet = stateset;
                            if (!mPushedSorter)
                                blendFuncStateSet->setRenderBinToInherit();
                        }

                        if (alphaprop->useAlphaTesting())
                        {
                            osg::ref_ptr<osg::AlphaFunc> alphaFunc(new osg::AlphaFunc(
                                getTestMode(alphaprop->alphaTestMode()), alphaprop->data.threshold / 255.f));
                            alphaFunc = shareAttribute(alphaFunc);
                            node->getOrCreateStateSet()->setAttributeAndModes(alphaFunc, osg::StateAttribute::ON);
                        }
                        else if (osg::StateSet* stateset = node->getStateSet())
                        {
                            stateset->removeAttribute(osg::StateAttribute::ALPHAFUNC);
                            stateset->removeMode(GL_ALPHA_TEST);
                        }
                        break;
                    }
                    case Nif::RC_BSShaderNoLightingProperty:
                    {
                        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(1.f, 1.f, 1.f, 1.f));
                        break;
                    }
                    case Nif::RC_BSLightingShaderProperty:
                    {
                        auto shaderprop = static_cast<const Nif::BSLightingShaderProperty*>(property);
                        mat->setAlpha(osg::Material::FRONT_AND_BACK, shaderprop->mAlpha);
                        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(shaderprop->mEmissive, 1.f));
                        mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(shaderprop->mSpecular, 1.f));
                        mat->setShininess(osg::Material::FRONT_AND_BACK, shaderprop->mGlossiness);
                        emissiveMult = shaderprop->mEmissiveMult;
                        specStrength = shaderprop->mSpecStrength;
                        break;
                    }
                    case Nif::RC_BSEffectShaderProperty:
                    {
                        auto shaderprop = static_cast<const Nif::BSEffectShaderProperty*>(property);
                        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(shaderprop->mBaseColor));
                        emissiveMult = shaderprop->mBaseColorScale;
                        break;
                    }
                    default:
                        break;
                }
            }

            // While NetImmerse and Gamebryo support specular lighting, Morrowind has its support disabled.
            if (mVersion <= Nif::NIFFile::VER_MW || !specEnabled)
                mat->setSpecular(osg::Material::FRONT_AND_BACK, osg::Vec4f(0.f, 0.f, 0.f, 0.f));

            if (lightmode == 0)
            {
                osg::Vec4f diffuse = mat->getDiffuse(osg::Material::FRONT_AND_BACK);
                diffuse = osg::Vec4f(0, 0, 0, diffuse.a());
                mat->setDiffuse(osg::Material::FRONT_AND_BACK, diffuse);
                mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f());
            }

            // If we're told to use vertex colors but there are none to use, use a default color instead.
            if (!hasVertexColors)
            {
                switch (mat->getColorMode())
                {
                    case osg::Material::AMBIENT:
                        mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
                        break;
                    case osg::Material::AMBIENT_AND_DIFFUSE:
                        mat->setAmbient(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
                        mat->setDiffuse(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
                        break;
                    case osg::Material::EMISSION:
                        mat->setEmission(osg::Material::FRONT_AND_BACK, osg::Vec4f(1, 1, 1, 1));
                        break;
                    default:
                        break;
                }
                mat->setColorMode(osg::Material::OFF);
            }

            if (!mPushedSorter && !hasSortAlpha && mHasStencilProperty)
                setBin_Traversal(node->getOrCreateStateSet());

            if (!mPushedSorter && !hasMatCtrl && mat->getColorMode() == osg::Material::OFF
                && mat->getEmission(osg::Material::FRONT_AND_BACK) == osg::Vec4f(0, 0, 0, 1)
                && mat->getDiffuse(osg::Material::FRONT_AND_BACK) == osg::Vec4f(1, 1, 1, 1)
                && mat->getAmbient(osg::Material::FRONT_AND_BACK) == osg::Vec4f(1, 1, 1, 1)
                && mat->getShininess(osg::Material::FRONT_AND_BACK) == 0
                && mat->getSpecular(osg::Material::FRONT_AND_BACK) == osg::Vec4f(0.f, 0.f, 0.f, 0.f))
            {
                // default state, skip
                return;
            }

            mat = shareAttribute(mat);

            osg::StateSet* stateset = node->getOrCreateStateSet();
            stateset->setAttributeAndModes(mat, osg::StateAttribute::ON);
            if (emissiveMult != 1.f)
                stateset->addUniform(new osg::Uniform("emissiveMult", emissiveMult));
            if (specStrength != 1.f)
                stateset->addUniform(new osg::Uniform("specStrength", specStrength));

            if (!mPushedSorter)
                return;

            auto assignBin = [&](int mode, int type) {
                if (mode == Nif::NiSortAdjustNode::SortingMode_Off)
                {
                    setBin_Traversal(stateset);
                    return;
                }

                if (type == Nif::RC_NiAlphaAccumulator)
                {
                    if (hasSortAlpha)
                        setBin_BackToFront(stateset);
                    else
                        setBin_Traversal(stateset);
                }
                else if (type == Nif::RC_NiClusterAccumulator)
                    setBin_BackToFront(stateset);
                else
                    Log(Debug::Error) << "Unrecognized NiAccumulator in " << mFilename;
            };

            switch (mPushedSorter->mMode)
            {
                case Nif::NiSortAdjustNode::SortingMode_Inherit:
                {
                    if (mLastAppliedNoInheritSorter)
                        assignBin(mLastAppliedNoInheritSorter->mMode, mLastAppliedNoInheritSorter->mSubSorter->recType);
                    else
                        assignBin(mPushedSorter->mMode, Nif::RC_NiAlphaAccumulator);
                    break;
                }
                case Nif::NiSortAdjustNode::SortingMode_Off:
                {
                    setBin_Traversal(stateset);
                    break;
                }
                case Nif::NiSortAdjustNode::SortingMode_Subsort:
                {
                    assignBin(mPushedSorter->mMode, mPushedSorter->mSubSorter->recType);
                    break;
                }
            }
        }
    };

    osg::ref_ptr<osg::Node> Loader::load(Nif::FileView file, Resource::ImageManager* imageManager)
    {
        LoaderImpl impl(file.getFilename(), file.getVersion(), file.getUserVersion(), file.getBethVersion());
        return impl.load(file, imageManager);
    }

    void Loader::loadKf(Nif::FileView kf, SceneUtil::KeyframeHolder& target)
    {
        LoaderImpl impl(kf.getFilename(), kf.getVersion(), kf.getUserVersion(), kf.getBethVersion());
        impl.loadKf(kf, target);
    }

}
