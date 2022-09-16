/*
  OpenMW - The completely unofficial reimplementation of Morrowind
  Copyright (C) 2008-2010  Nicolay Korslund
  Email: < korslund@gmail.com >
  WWW: https://openmw.org/

  This file (controller.h) is part of the OpenMW package.

  OpenMW is distributed as free software: you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 3, as published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  version 3 along with this program. If not, see
  https://www.gnu.org/licenses/ .

 */

#ifndef OPENMW_COMPONENTS_NIF_CONTROLLER_HPP
#define OPENMW_COMPONENTS_NIF_CONTROLLER_HPP

#include "base.hpp"
#include "extra.hpp"
#include "data.hpp"

namespace Nif
{

    struct NiBoneLODController : public Controller
    {
        struct NodeSet
        {
            std::vector<NodePtr> mNodes;
        };
        unsigned int mLOD;
        std::vector<NodeSet> mNodeGroups;
        // todo: NISTREAM (non-bethesda)-only members

        void read(NIFStream* nif) override;
        void post(NIFFile* nif) override;
        
    };
    struct NiBSBoneLODController : public NiBoneLODController
    {
    };

struct NiParticleSystemController : public Controller
{
    enum BSPArrayController {
        BSPArrayController_AtNode = 0x8,
        BSPArrayController_AtVertex = 0x10
    };

    struct Particle {
        osg::Vec3f velocity;
        float lifetime;
        float lifespan;
        float timestamp;
        unsigned short vertex;
    };

    float velocity;
    float velocityRandom;

    float verticalDir; // 0=up, pi/2=horizontal, pi=down
    float verticalAngle;
    float horizontalDir;
    float horizontalAngle;

    osg::Vec4f color;
    float size;
    float startTime;
    float stopTime;

    float emitRate;
    float lifetime;
    float lifetimeRandom;

    enum EmitFlags
    {
        EmitFlag_NoAutoAdjust = 0x1 // If this flag is set, we use the emitRate value. Otherwise,
                                    // we calculate an emit rate so that the maximum number of particles
                                    // in the system (numParticles) is never exceeded.
    };
    int emitFlags;

    osg::Vec3f offsetRandom;

    NodePtr emitter;

    int numParticles;
    int activeCount;
    std::vector<Particle> particles;

    NiParticleModifierPtr affectors;
    NiParticleModifierPtr colliders;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;

    bool noAutoAdjust() const { return emitFlags & EmitFlag_NoAutoAdjust; }
    bool emitAtVertex() const { return flags & BSPArrayController_AtVertex; }
};
using NiBSPArrayController = NiParticleSystemController;

struct NiMaterialColorController : public Controller
{
    NiPoint3InterpolatorPtr interpolator;
    NiPosDataPtr data;
    unsigned int targetColor;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiPathController : public Controller
{
    NiPosDataPtr posData;
    NiFloatDataPtr floatData;

    enum Flags
    {
        Flag_OpenCurve      = 0x020,
        Flag_AllowFlip      = 0x040,
        Flag_Bank           = 0x080,
        Flag_ConstVelocity  = 0x100,
        Flag_Follow         = 0x200,
        Flag_FlipFollowAxis = 0x400
    };

    int bankDir;
    float maxBankAngle, smoothing;
    short followAxis;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiLookAtController : public Controller
{
    NodePtr target;
    unsigned short lookAtFlags{0};

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiUVController : public Controller
{
    NiUVDataPtr data;
    unsigned int uvSet;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiKeyframeController : public Controller
{
    NiKeyframeDataPtr data;
    NiTransformInterpolatorPtr interpolator;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiMultiTargetTransformController : public Controller
{
    NodeList mExtraTargets;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiFloatInterpController : public Controller
{
    NiFloatDataPtr data;
    NiFloatInterpolatorPtr interpolator;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiAlphaController : public NiFloatInterpController { };
struct NiRollController : public NiFloatInterpController { };

struct NiExtraDataController : public NiFloatInterpController
{
    std::string mExtraDataName;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct NiFloatExtraDataController : public NiExtraDataController
{
    NiFloatDataPtr mFloatData;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct NiGeomMorpherController : public Controller
{
    NiMorphDataPtr data;
    NiFloatInterpolatorList interpolators;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiVisController : public Controller
{
    NiVisDataPtr data;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiFlipController : public Controller
{
    NiFloatInterpolatorPtr mInterpolator;
    int mTexSlot; // NiTexturingProperty::TextureType
    float mDelta; // Time between two flips. delta = (start_time - stop_time) / num_sources
    NiSourceTextureList mSources;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct bhkBlendController : public Controller
{
    void read(NIFStream *nif) override;
};
struct Interpolator;
struct NiSequence : public Record
{
    struct ControlledBlock
    {
        // up to 10.1.0.0
        std::string mTargetName;
        ControllerPtr mController;

        // from 10.1.0.106
        RecordPtrT<Interpolator> mInterpolator;
        ControllerPtr mController2;
        unsigned char mPriority;

        // 10.1.0.106 and 20.1.0.3 onwards index from header string
        // 10.2.0.0 to 20.0.0.5 offset from string palette (can be -1)
        std::string mNodeName; // target node name
        std::string mPropertyType;
        std::string mControllerType;
        std::string mVariable1;
        std::string mVariable2;
        std::uint32_t mNodeNameIndex; // target node name
        std::uint32_t mPropertyTypeIndex;
        std::uint32_t mControllerTypeIndex;
        std::uint32_t mVariable1Index;
        std::uint32_t mVariable2Index;

        RecordPtrT<NiStringPalette> mStringPalette;
        void read(NIFStream* nif);
        void post(NIFFile* nif);

    };
    std::string mName;
    std::string mAccumRootName;
    RecordPtrT<NiTextKeyExtraData> mTextKeys;
    unsigned int mArrayGrowBy;
    std::vector<ControlledBlock> mControlledBlocks;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};
struct NiControllerManager;
struct NiControllerSequence : public NiSequence
{
    float mWeight;
    RecordPtrT<NiTextKeyExtraData> mTextKeys2;
    uint32_t mCycleType;
    uint32_t mUnknown0;
    float mFrequency;
    float mStartTime;
    float mUnknownFloat2;
    float mStopTime;
    char mUnknownByte;
    RecordPtrT<NiControllerManager> mManager;
    std::string mTargetName;
    uint32_t mTargetNameIndex;
    RecordPtrT<NiStringPalette> mStringPalette;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct NiControllerManager : public Controller
{
    bool mCumulative;
    std::vector<NiControllerSequence> mSequences;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};


struct Interpolator : public Record { };

struct NiPoint3Interpolator : public Interpolator
{
    osg::Vec3f defaultVal;
    NiPosDataPtr data;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiBoolInterpolator : public Interpolator
{
    bool defaultVal;
    NiBoolDataPtr data;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiFloatInterpolator : public Interpolator
{
    float defaultVal;
    NiFloatDataPtr data;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiTransformInterpolator : public Interpolator
{
    osg::Vec3f defaultPos;
    osg::Quat defaultRot;
    float defaultScale;
    NiKeyframeDataPtr data;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

struct NiBSplineData : public Record
{
    std::vector<float> mFloats;
    std::vector<unsigned int> mCompacts;
    void read(NIFStream* nif) override;
};
struct NiBSplineBasisData : public Record
{
    unsigned int mNumControlPoints;
    void read(NIFStream* nif) override;
};

struct NiBSplineInterpolator : public Interpolator
{
    float mStartTime;
    float mStopTime;
    RecordPtrT<NiBSplineData> mSplineData;
    RecordPtrT<NiBSplineBasisData> mBasisData;

    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct NiBSplineTransformInterpolator : public NiBSplineInterpolator
{
    osg::Vec3f mTranslation;
    osg::Quat mRotation;
    float mScale;
    unsigned int mTranslationHandle;
    unsigned int mRotationHandle;
    unsigned int mScaleHandle;

    void read(NIFStream* nif) override;
};


struct NiBSplineCompTransformInterpolator : public NiBSplineTransformInterpolator
{
    float mTranslationOffset;
    float mTranslationHalfRange;
    float mRotationOffset;
    float mRotationHalfRange;
    float mScaleOffset;
    float mScaleHalfRange;

    void read(NIFStream* nif) override;
};

struct NiColorInterpolator : public Interpolator
{
    osg::Vec4f defaultVal;
    NiColorDataPtr data;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

} // Namespace
#endif
