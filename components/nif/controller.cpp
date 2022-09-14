#include "controller.hpp"

#include "controlled.hpp"
#include "data.hpp"
#include "node.hpp"
#include "recordptr.hpp"

namespace Nif
{

    void Controller::read(NIFStream *nif)
    {
        next.read(nif);

        flags = nif->getUShort();

        frequency = nif->getFloat();
        phase = nif->getFloat();
        timeStart = nif->getFloat();
        timeStop = nif->getFloat();

        target.read(nif);
    }

    void Controller::post(NIFFile *nif)
    {
        Record::post(nif);
        next.post(nif);
        target.post(nif);
    }

    void NiParticleSystemController::read(NIFStream *nif)
    {
        Controller::read(nif);

        velocity = nif->getFloat();
        velocityRandom = nif->getFloat();
        verticalDir = nif->getFloat();
        verticalAngle = nif->getFloat();
        horizontalDir = nif->getFloat();
        horizontalAngle = nif->getFloat();
        /*normal?*/ nif->getVector3();
        color = nif->getVector4();
        size = nif->getFloat();
        startTime = nif->getFloat();
        stopTime = nif->getFloat();
        nif->getChar();
        emitRate = nif->getFloat();
        lifetime = nif->getFloat();
        lifetimeRandom = nif->getFloat();

        emitFlags = nif->getUShort();
        offsetRandom = nif->getVector3();

        emitter.read(nif);

        /* Unknown Short, 0?
         * Unknown Float, 1.0?
         * Unknown Int, 1?
         * Unknown Int, 0?
         * Unknown Short, 0?
         */
        nif->skip(16);

        numParticles = nif->getUShort();
        activeCount = nif->getUShort();

        particles.resize(numParticles);
        for(size_t i = 0;i < particles.size();i++)
        {
            particles[i].velocity = nif->getVector3();
            nif->getVector3(); /* unknown */
            particles[i].lifetime = nif->getFloat();
            particles[i].lifespan = nif->getFloat();
            particles[i].timestamp = nif->getFloat();
            nif->getUShort(); /* unknown */
            particles[i].vertex = nif->getUShort();
        }

        nif->getUInt(); /* -1? */
        affectors.read(nif);
        colliders.read(nif);
        nif->getChar();
    }

    void NiParticleSystemController::post(NIFFile *nif)
    {
        Controller::post(nif);
        emitter.post(nif);
        affectors.post(nif);
        colliders.post(nif);
    }

    void NiMaterialColorController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() > NIFStream::generateVersion(10,1,0,103))
            interpolator.read(nif);
        // Two bits that correspond to the controlled material color.
        // 00: Ambient
        // 01: Diffuse
        // 10: Specular
        // 11: Emissive
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
            targetColor = nif->getUShort() & 3;
        else
            targetColor = (flags >> 4) & 3;
        if (nif->getVersion() <= NIFStream::generateVersion(10,1,0,103))
            data.read(nif);
    }

    void NiMaterialColorController::post(NIFFile *nif)
    {
        Controller::post(nif);
        interpolator.post(nif);
        data.post(nif);
    }

    void NiLookAtController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
            lookAtFlags = nif->getUShort();
        target.read(nif);
    }

    void NiLookAtController::post(NIFFile *nif)
    {
        Controller::post(nif);
        target.post(nif);
    }

    void NiPathController::read(NIFStream *nif)
    {
        Controller::read(nif);

        bankDir = nif->getInt();
        maxBankAngle = nif->getFloat();
        smoothing = nif->getFloat();
        followAxis = nif->getShort();
        posData.read(nif);
        floatData.read(nif);
    }

    void NiPathController::post(NIFFile *nif)
    {
        Controller::post(nif);

        posData.post(nif);
        floatData.post(nif);
    }

    void NiUVController::read(NIFStream *nif)
    {
        Controller::read(nif);

        uvSet = nif->getUShort();
        data.read(nif);
    }

    void NiUVController::post(NIFFile *nif)
    {
        Controller::post(nif);
        data.post(nif);
    }

    void NiKeyframeController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() <= NIFStream::generateVersion(10,1,0,103))
            data.read(nif);
        else
            interpolator.read(nif);
    }

    void NiKeyframeController::post(NIFFile *nif)
    {
        Controller::post(nif);
        data.post(nif);
        interpolator.post(nif);
    }

    void NiMultiTargetTransformController::read(NIFStream *nif)
    {
        Controller::read(nif);
        size_t numTargets = nif->getUShort();
        std::vector<NodePtr> targets;
        targets.resize(numTargets);
        for (size_t i = 0; i < targets.size(); i++)
            targets[i].read(nif);
        mExtraTargets = targets;
    }

    void NiMultiTargetTransformController::post(NIFFile *nif)
    {
        Controller::post(nif);
        mExtraTargets.post(nif);
    }

    void NiFloatInterpController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() <= NIFStream::generateVersion(10,1,0,103))
            data.read(nif);
        else
            interpolator.read(nif);
    }

    void NiFloatInterpController::post(NIFFile *nif)
    {
        Controller::post(nif);
        data.post(nif);
        interpolator.post(nif);
    }

    void NiGeomMorpherController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() >= NIFFile::NIFVersion::VER_OB_OLD)
            /*bool updateNormals = !!*/nif->getUShort();
        data.read(nif);
        if (nif->getVersion() >= NIFFile::NIFVersion::VER_MW)
        {
            /*bool alwaysActive = */nif->getChar(); // Always 0
            if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,106))
            {
                if (nif->getVersion() <= NIFFile::NIFVersion::VER_OB)
                {
                    interpolators.read(nif);
                    if (nif->getVersion() >= NIFStream::generateVersion(10,2,0,0) && nif->getBethVersion() > 9)
                    {
                        unsigned int numUnknown = nif->getUInt();
                        nif->skip(4 * numUnknown);
                    }
                }
                else
                {
                    // TODO: handle weighted interpolators
                    unsigned int numInterps = nif->getUInt();
                    nif->skip(8 * numInterps);
                }
            }
        }
    }

    void NiGeomMorpherController::post(NIFFile *nif)
    {
        Controller::post(nif);
        data.post(nif);
        interpolators.post(nif);
    }

    void NiVisController::read(NIFStream *nif)
    {
        Controller::read(nif);
        data.read(nif);
    }

    void NiVisController::post(NIFFile *nif)
    {
        Controller::post(nif);
        data.post(nif);
    }

    void NiFlipController::read(NIFStream *nif)
    {
        Controller::read(nif);
        if (nif->getVersion() >= NIFStream::generateVersion(10,2,0,0))
            mInterpolator.read(nif);
        mTexSlot = nif->getUInt();
        if (nif->getVersion() <= NIFStream::generateVersion(10,1,0,103))
        {
            timeStart = nif->getFloat();
            mDelta = nif->getFloat();
        }
        mSources.read(nif);
    }

    void NiFlipController::post(NIFFile *nif)
    {
        Controller::post(nif);
        mInterpolator.post(nif);
        mSources.post(nif);
    }

    void bhkBlendController::read(NIFStream *nif)
    {
        Controller::read(nif);
        nif->getUInt(); // Zero
    }

    void NiControllerManager::read(NIFStream *nif)
    {
        Controller::read(nif);
        mCumulative = nif->getBoolean();
        unsigned int numSequences = nif->getUInt();
        mSequences.resize(numSequences);
        for (auto& seq : mSequences)
            seq.read(nif);
        nif->skip(4); // Object palette
    }

    void NiControllerManager::post(NIFFile* nif)
    {
        Controller::post(nif);
        for (auto& seq : mSequences)
            seq.post(nif);
    }

    void NiPoint3Interpolator::read(NIFStream *nif)
    {
        defaultVal = nif->getVector3();
        data.read(nif);
    }

    void NiPoint3Interpolator::post(NIFFile *nif)
    {
        data.post(nif);
    }

    void NiBoolInterpolator::read(NIFStream *nif)
    {
        defaultVal = nif->getBoolean();
        data.read(nif);
    }

    void NiBoolInterpolator::post(NIFFile *nif)
    {
        data.post(nif);
    }

    void NiFloatInterpolator::read(NIFStream *nif)
    {
        defaultVal = nif->getFloat();
        data.read(nif);
    }

    void NiFloatInterpolator::post(NIFFile *nif)
    {
        data.post(nif);
    }

    void NiTransformInterpolator::read(NIFStream *nif)
    {
        defaultPos = nif->getVector3();
        defaultRot = nif->getQuaternion();
        defaultScale = nif->getFloat();
        if (nif->getVersion() <= NIFStream::generateVersion(10,1,0,109))
        {
            if (!nif->getBoolean())
                defaultPos = osg::Vec3f();
            if (!nif->getBoolean())
                defaultRot = osg::Quat();
            if (!nif->getBoolean())
                defaultScale = 1.f;
        }
        data.read(nif);
    }

    void NiTransformInterpolator::post(NIFFile *nif)
    {
        data.post(nif);
    }

    void NiColorInterpolator::read(NIFStream *nif)
    {
        defaultVal = nif->getVector4();
        data.read(nif);
    }

    void NiColorInterpolator::post(NIFFile *nif)
    {
        data.post(nif);
    }

    void NiBoneLODController::read(NIFStream* nif)
    {
        Controller::read(nif);
        mLOD = nif->getUInt();
        unsigned int numLods = nif->getUInt();
        unsigned int numnodegroups = nif->getUInt(); // num nodegrpups
        mNodeGroups.resize(numLods);
        for (size_t i = 0; i < numLods; i++)
        {
            mNodeGroups[i].mNodes.resize(nif->getUInt());
            for (size_t j = 0; j < mNodeGroups[i].mNodes.size(); j++)
            {
                mNodeGroups[i].mNodes[j].read(nif);
            }
        }
    }

    void NiBoneLODController::post(NIFFile* nif)
    {
        Controller::post(nif);
        for (size_t i = 0; i < mNodeGroups.size(); i++)
        {
            for (size_t j = 0; j < mNodeGroups[i].mNodes.size(); j++)
            {
                mNodeGroups[i].mNodes[j].post(nif);
            }
        }
    }

    void NiSequence::ControlledBlock::read(NIFStream* nif)
    {
        if (nif->getVersion() <= nif->generateVersion(10, 1, 0, 0))
        {
            mTargetName = nif->getSizedString();
            mController.read(nif);
        }

        if (nif->getVersion() >= nif->generateVersion(10, 1, 0, 106))
        {
            mInterpolator.read(nif);
            mController2.read(nif);
            if (nif->getVersion() == nif->generateVersion(10, 1, 0, 106))
            {
                nif->skip(4);
                nif->skip(2);
            }
            mPriority = nif->getChar();
        }

        if (nif->getVersion() == nif->generateVersion(10, 1, 0, 106) || nif->getVersion() >= NIFStream::generateVersion(20, 1, 0, 1))
        {
            mNodeName = nif->getString();
            mPropertyType = nif->getString();
            mControllerType = nif->getString();
            mVariable1 = nif->getString();
            mVariable2 = nif->getString();
        }
        else if (nif->getVersion() == 0x0a020000 && nif->getVersion() <= 0x14000005)
        {
            mStringPalette.read(nif);
            mNodeNameIndex = nif->getUInt();
            mPropertyTypeIndex = nif->getUInt();
            mControllerTypeIndex = nif->getUInt();
            mVariable1Index = nif->getUInt();
            mVariable2Index = nif->getUInt();
        }
    }
    void NiSequence::ControlledBlock::post(NIFFile* nif)
    {
        if (nif->getVersion() <= Nif::NIFStream::generateVersion(10, 1, 0, 0))
        {
            mController.post(nif);
        }

        if (nif->getVersion() >= Nif::NIFStream::generateVersion(10, 1, 0, 106))
        {
            mInterpolator.post(nif);
            mController2.post(nif);
        }
        if (nif->getVersion() == 0x0a020000 && nif->getVersion() <= 0x14000005)
        {
            mStringPalette.post(nif);
            mPropertyType = mStringPalette->palette[mPropertyTypeIndex];
            mControllerType = mStringPalette->palette[mControllerTypeIndex];
            mVariable1 = mStringPalette->palette[mVariable1Index];
            mVariable2 = mStringPalette->palette[mVariable2Index];
        }
    }
    void NiSequence::read(NIFStream* nif)
    {
        mName = nif->getString();

        // probably unused, skip instead?
        if (nif->getVersion() <= 0x0a010000) // up to 10.1.0.0
        {
            mTextKeys.read(nif);
        }

        uint32_t numControlledBlocks = nif->getUInt();
        mControlledBlocks.resize(numControlledBlocks);
        if (nif->getVersion() >= nif->generateVersion(10, 1, 0, 106))
            mArrayGrowBy = nif->getUInt();
        for (size_t i = 0; i < mControlledBlocks.size(); i++)
        {
            mControlledBlocks[i].read(nif);
        }
    }
    void NiSequence::post(NIFFile* nif)
    {

        for (size_t i = 0; i < mControlledBlocks.size(); i++)
        {
            mControlledBlocks[i].post(nif);
        }
    }

    void NiControllerSequence::read(NIFStream* nif)
    {
        NiSequence::read(nif);
        mWeight = nif->getFloat();
        mTextKeys2.read(nif);
        mCycleType = nif->getUInt();

        if (nif->getVersion() == 0x0a01006a)
            mUnknown0 = nif->getChar();

        mFrequency = nif->getFloat();
        mStartTime = nif->getFloat();

        if (nif->getVersion() >= 0x0a020000 && nif->getVersion() <= 0x0a040001)
            mUnknownFloat2 = nif->getFloat();

        mStopTime = nif->getFloat();

        if (nif->getVersion() == 0x0a01006a)
            mUnknownByte = nif->getChar();

        int rIndex = -1;
        mManager.read(nif);

        mTargetName = nif->getString();

        if (nif->getVersion() >= 0x0a020000 && nif->getVersion() <= 0x14000005)
            mStringPalette.read(nif);

        if (nif->getVersion() >= 0x14020007 && nif->getUserVersion() >= 11 && (nif->getBethVersion() >= 24 && nif->getBethVersion() <= 28))
            nif->skip(4); // animnotes ptr

        if (nif->getVersion() >= 0x14020007 && nif->getBethVersion() > 28)
            nif->skip(2); // unknonwn
    }
    void NiControllerSequence::post(NIFFile* nif)
    {
        NiSequence::post(nif);
        mTextKeys2.post(nif);
        mManager.post(nif);

        if (nif->getVersion() >= 0x0a020000 && nif->getVersion() <= 0x14000005)
            mStringPalette.post(nif);
    }

    void NiBSplineData::read(NIFStream* nif)
    {
        mFloats.resize(nif->getUInt());
        for (auto& flt : mFloats)
        {
            flt = nif->getFloat();
        }
        mCompacts.resize(nif->getUInt());
        for (auto& c : mCompacts)
        {
            c = nif->getUShort();
        }
    }

    void NiBSplineBasisData::read(NIFStream* nif)
    {
        mNumControlPoints = nif->getUInt();
    }

    void NiBSplineInterpolator::read(NIFStream* nif)
    {
        mStartTime = nif->getFloat();
        mStopTime = nif->getFloat();
        mSplineData.read(nif);
        mBasisData.read(nif);
    }
    void NiBSplineInterpolator::post(NIFFile* nif)
    {
        mSplineData.post(nif);
        mBasisData.post(nif);
    }

    void NiBSplineTransformInterpolator::read(NIFStream* nif)
    {
        NiBSplineInterpolator::read(nif);
        mTranslation = nif->getVector3();
        mRotation = nif->getQuaternion();
        mScale = nif->getFloat();
        mTranslationHandle = nif->getUInt();
        mRotationHandle = nif->getUInt();
        mScaleHandle = nif->getUInt();
    }
    void NiBSplineCompTransformInterpolator::read(NIFStream* nif)
    {
        NiBSplineTransformInterpolator::read(nif);
        mTranslationOffset = nif->getFloat();
        mTranslationHalfRange = nif->getFloat();
        mRotationOffset = nif->getFloat();
        mRotationHalfRange = nif->getFloat();
        mScaleOffset = nif->getFloat();
        mScaleHalfRange = nif->getFloat();
    }

}
