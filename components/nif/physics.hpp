#ifndef OPENMW_COMPONENTS_NIF_PHYSICS_HPP
#define OPENMW_COMPONENTS_NIF_PHYSICS_HPP

#include "record.hpp"
#include "recordptr.hpp"

#include <osg/Vec3f>
#include <osg/Vec4f>
#include <osg/Quat>

#include <cstdint>
#include <vector>

// This header contains certain record definitions
// specific to Bethesda implementation of Havok physics
namespace Nif
{

class NIFStream;
class NIFFile;

/// Non-record data types

struct bhkWorldObjCInfoProperty
{
    unsigned int mData;
    unsigned int mSize;
    unsigned int mCapacityAndFlags;
    void read(NIFStream *nif);
};

enum class BroadPhaseType : uint8_t
{
    BroadPhase_Invalid = 0,
    BroadPhase_Entity = 1,
    BroadPhase_Phantom = 2,
    BroadPhase_Border = 3
};

struct bhkWorldObjectCInfo
{
    BroadPhaseType mPhaseType;
    bhkWorldObjCInfoProperty mProperty;
    void read(NIFStream *nif);
};

struct HavokMaterial
{
    unsigned int mMaterial;
    void read(NIFStream *nif);
};

struct HavokFilter
{
    unsigned char mLayer;
    unsigned char mFlags;
    unsigned short mGroup;
    void read(NIFStream *nif);
};

struct hkSubPartData
{
    HavokMaterial mHavokMaterial;
    unsigned int mNumVertices;
    HavokFilter mHavokFilter;
    void read(NIFStream *nif);
};

enum class hkResponseType : uint8_t
{
    Response_Invalid = 0,
    Response_SimpleContact = 1,
    Response_Reporting = 2,
    Response_None = 3
};

struct bhkEntityCInfo
{
    hkResponseType mResponseType;
    unsigned short mProcessContactDelay;
    void read(NIFStream *nif);
};

struct hkpMoppCode
{
    osg::Vec4f mOffset;
    std::vector<char> mData;
    void read(NIFStream *nif);
};

struct TriangleData
{
    unsigned short mTriangle[3];
    unsigned short mWeldingInfo;
    osg::Vec3f mNormal;
    void read(NIFStream *nif);
};

enum class hkMotionType : uint8_t
{
    Motion_Invalid = 0,
    Motion_Dynamic = 1,
    Motion_SphereInertia = 2,
    Motion_SphereStabilized = 3,
    Motion_BoxInertia = 4,
    Motion_BoxStabilized = 5,
    Motion_Keyframed = 6,
    Motion_Fixed = 7,
    Motion_ThinBox = 8,
    Motion_Character = 9
};

enum class hkDeactivatorType : uint8_t
{
    Deactivator_Invalid = 0,
    Deactivator_Never = 1,
    Deactivator_Spatial = 2
};

enum class hkSolverDeactivation : uint8_t
{
    SolverDeactivation_Invalid = 0,
    SolverDeactivation_Off = 1,
    SolverDeactivation_Low = 2,
    SolverDeactivation_Medium = 3,
    SolverDeactivation_High = 4,
    SolverDeactivation_Max = 5
};

enum class hkQualityType : uint8_t
{
    Quality_Invalid = 0,
    Quality_Fixed = 1,
    Quality_Keyframed = 2,
    Quality_Debris = 3,
    Quality_Moving = 4,
    Quality_Critical = 5,
    Quality_Bullet = 6,
    Quality_User = 7,
    Quality_Character = 8,
    Quality_KeyframedReport = 9
};

struct bhkRigidBodyCInfo
{
    HavokFilter mHavokFilter;
    hkResponseType mResponseType;
    unsigned short mProcessContactDelay;
    osg::Vec4f mTranslation;
    osg::Quat mRotation;
    osg::Vec4f mLinearVelocity;
    osg::Vec4f mAngularVelocity;
    float mInertiaTensor[3][4];
    osg::Vec4f mCenter;
    float mMass;
    float mLinearDamping;
    float mAngularDamping;
    float mTimeFactor{1.f};
    float mGravityFactor{1.f};
    float mFriction;
    float mRollingFrictionMult;
    float mRestitution;
    float mMaxLinearVelocity;
    float mMaxAngularVelocity;
    float mPenetrationDepth;
    hkMotionType mMotionType;
    hkDeactivatorType mDeactivatorType;
    bool mEnableDeactivation{true};
    hkSolverDeactivation mSolverDeactivation;
    hkQualityType mQualityType;
    unsigned char mAutoRemoveLevel;
    unsigned char mResponseModifierFlags;
    unsigned char mNumContactPointShapeKeys;
    bool mForceCollidedOntoPPU;
    void read(NIFStream *nif);
};

/// Record types

// Abstract Bethesda Havok object
struct bhkRefObject : public Record {};

// Abstract serializable Bethesda Havok object
struct bhkSerializable : public bhkRefObject {};

// Abstract narrowphase collision detection object
struct bhkShape : public bhkSerializable {};


// Abstract bhkShape collection
struct bhkShapeCollection : public bhkShape {};

// Generic collision object
struct NiCollisionObject : public Record
{
    // The node that references this object
    NodePtr mTarget;

    void read(NIFStream *nif) override
    {
        mTarget.read(nif);
    }
    void post(NIFFile *nif) override
    {
        mTarget.post(nif);
    }
};

// Bethesda Havok-specific collision object
struct bhkCollisionObject : public NiCollisionObject
{
    unsigned short mFlags;
    bhkWorldObjectPtr mBody;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override
    {
        NiCollisionObject::post(nif);
        mBody.post(nif);
    }
};

struct bhkBlendCollisionObject : public bhkCollisionObject
{
    float mHeirGain;
    float mVelGain;
    float mUnknown1;
    float mUnknown2;

    void read(NIFStream* nif) override;
};

// Abstract Havok shape info record
struct bhkWorldObject : public bhkSerializable
{
    bhkShapePtr mShape;
    HavokFilter mHavokFilter;
    bhkWorldObjectCInfo mWorldObjectInfo;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

// Abstract
struct bhkEntity : public bhkWorldObject
{
    bhkEntityCInfo mInfo;
    void read(NIFStream *nif) override;
};


struct bhkConstraint : public bhkSerializable
{
    struct bhkConstraintCInfo
    {
        unsigned int mNumEntities; // always 2
        RecordPtrT<bhkEntity> mEntityA;
        RecordPtrT<bhkEntity> mEntityB;
        unsigned int mPriority;

        void read(NIFStream* nif);
        void post(NIFFile* nif);
    };
    bhkConstraintCInfo mCInfo;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct bhkConstraintMotorCInfo
{
    unsigned char mType;

    struct bhkPositionConstraintMotor
    {
        float mMinForce;
        float mMaxForce;
        float mTau;
        float mDamping;
        float mProportionalRecoveryVelocity;
        float mConstantRecoveryVelocity;
        bool mMotorEnabled;
    }; // if type == 1
    struct bhkVelocityConstraintMotor
    {
        float mMinForce;
        float mMaxForce;
        float mTau;
        float mTargetVelocity;
        bool mUseVelocityTarget;
        bool mMotorEnabled;
    }; // if type == 2
    struct bhkSpringDamperConstraintMotor
    {
        float mMinForce;
        float mMaxForce;
        float mSpringConstant;
        float mSpringDamping;
        bool mMotorEnabled;
    }; // if type == 3;

    union
    {
        bhkPositionConstraintMotor mPosition;
        bhkVelocityConstraintMotor mVelocity;
        bhkSpringDamperConstraintMotor mSpringDamping;
    } mMotor;
    void read(NIFStream* nif);
};

struct bhkLimitedHingeConstraint : public bhkConstraint
{
    struct bhkLimitedHingeConstraintCInfo
    {
        osg::Vec4f mPivotA;
        osg::Vec4f mAxisA;
        osg::Vec4f mPerpAxisInA1;
        osg::Vec4f mPerpAxisInA2;
        osg::Vec4f mPivotB;
        osg::Vec4f mAxisB;
        osg::Vec4f mPerpAxisInB1; // only in fallout 3 and later
        osg::Vec4f mPerpAxisInB2;

        float mMinAngle;
        float mMaxAngle;
        float mMaxFriction;

        bhkConstraintMotorCInfo mMotor; // fallout 3 and later only

        void read(NIFStream* nif);
    };
    bhkLimitedHingeConstraintCInfo mHingeCInfo;
    void read(NIFStream* nif) override;
};

struct bhkRagdollConstraint : public bhkConstraint
{
    struct bhkRagdollConstraintCInfo
    {
        osg::Vec4f mPivotA;
        osg::Vec4f mPlaneA;
        osg::Vec4f mTwistA;
        osg::Vec4f mPivotB;
        osg::Vec4f mPlaneB;
        osg::Vec4f mTwistB;
        osg::Vec4f mMotorA; // fallout 3 and later
        osg::Vec4f mMotorB; // fallout 3 and later

        float mConeMaxAngle;
        float mPlaneMinAngle;
        float mPlaneMaxAngle;
        float mTwistMinAngle;
        float mTwistMaxAngle;
        float mMaxFriction;

        bhkConstraintMotorCInfo mMotor; // fallout 3 and later
        void read(NIFStream* nif);
    };
    bhkRagdollConstraintCInfo mRagdollCInfo;
    void read(NIFStream* nif) override;
};

struct bhkPrismaticConstraint : public bhkConstraint
{
    struct bhkPrismaticConstraintCInfo
    {
        osg::Vec4f mPivotA;
        osg::Vec4f mRotationA;
        osg::Vec4f mPlaneA;
        osg::Vec4f mSlidingA;
        osg::Vec4f mSlidingB;
        osg::Vec4f mPivotB;
        osg::Vec4f mRotationB;
        osg::Vec4f mPlaneB;

        float mMinAngle;
        float mMaxAngle;
        float mMaxFriction;

        bhkConstraintMotorCInfo mMotor; // fallout 3 and later only
        void read(NIFStream* nif);
    };
    bhkPrismaticConstraintCInfo mPrismaticCInfo;
    void read(NIFStream* nif) override;
};

struct bhkMalleableConstraint : public bhkConstraint
{
    struct bhkMalleableConstraintCInfo
    {
        unsigned int mType; // hkConstraintType
        bhkConstraint::bhkConstraintCInfo mCInfo;
        // todo: implement commented out constraint info
        // bhkBallAndSocketConstraintCInfo mBallAndSocket; // if type == 0
        // bhkHingeConstraintCInfo mHinge; // if type == 1
        bhkLimitedHingeConstraint::bhkLimitedHingeConstraintCInfo mLimitedHinge; // if type == 2
        bhkPrismaticConstraint::bhkPrismaticConstraintCInfo mPrismatic; // if type == 6
        bhkRagdollConstraint::bhkRagdollConstraintCInfo mRagdoll; // if type == 7
        // bhkStiffSpringConstraintCInfo if type == 8
        float mTau;
        float mDamping;
        float mStrength;
        void read(NIFStream* nif);
        void post(NIFFile* nif);
    };
    bhkMalleableConstraintCInfo mData;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

struct bhkWrappedConstraintData
{
    unsigned int mType; // hkConstraintType
    bhkConstraint::bhkConstraintCInfo mCInfo;
    // todo: implement commented out constraint info
    // bhkBallAndSocketConstraintCInfo mBallAndSocket; // if type == 0
    // bhkHingeConstraintCInfo mHinge; // if type == 1
    bhkLimitedHingeConstraint::bhkLimitedHingeConstraintCInfo mLimitedHinge; // if type == 2
    bhkPrismaticConstraint::bhkPrismaticConstraintCInfo mPrismatic; // if type == 6
    bhkRagdollConstraint::bhkRagdollConstraintCInfo mRagdoll; // if type == 7
    // bhkStiffSpringConstraintCInfo if type == 8
    bhkMalleableConstraint::bhkMalleableConstraintCInfo mMalleable; //if type == 13
    void read(NIFStream* nif);
    void post(NIFFile* nif);
};

struct bhkBreakableConstraint : public bhkConstraint
{
    bhkWrappedConstraintData mConstraintData;
    float mThreshold;
    bool mRemoveWhenBroken;
    void read(NIFStream* nif) override;
    void post(NIFFile* nif) override;
};

// Bethesda extension of hkpBvTreeShape
// hkpBvTreeShape adds a bounding volume tree to an hkpShapeCollection
struct bhkBvTreeShape : public bhkShape
{
    bhkShapePtr mShape;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

// bhkBvTreeShape with Havok MOPP code
struct bhkMoppBvTreeShape : public bhkBvTreeShape
{
    float mScale;
    hkpMoppCode mMopp;
    void read(NIFStream *nif) override;
};

// Bethesda triangle strip-based Havok shape collection
struct bhkNiTriStripsShape : public bhkShape
{
    HavokMaterial mHavokMaterial;
    float mRadius;
    unsigned int mGrowBy;
    osg::Vec4f mScale{1.f, 1.f, 1.f, 0.f};
    NiTriStripsDataList mData;
    std::vector<unsigned int> mFilters;
    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

// Bethesda packed triangle strip-based Havok shape collection
struct bhkPackedNiTriStripsShape : public bhkShapeCollection
{
    std::vector<hkSubPartData> mSubshapes;
    unsigned int mUserData;
    float mRadius;
    osg::Vec4f mScale;
    hkPackedNiTriStripsDataPtr mData;

    void read(NIFStream *nif) override;
    void post(NIFFile *nif) override;
};

// bhkPackedNiTriStripsShape data block
struct hkPackedNiTriStripsData : public bhkShapeCollection
{
    std::vector<TriangleData> mTriangles;
    std::vector<osg::Vec3f> mVertices;
    std::vector<hkSubPartData> mSubshapes;
    void read(NIFStream *nif) override;
};

// Abstract
struct bhkSphereRepShape : public bhkShape
{
    HavokMaterial mHavokMaterial;
    void read(NIFStream *nif) override;
};

// Abstract
struct bhkConvexShape : public bhkSphereRepShape
{
    float mRadius;
    void read(NIFStream *nif) override;
};

// A convex shape built from vertices
struct bhkConvexVerticesShape : public bhkConvexShape
{
    bhkWorldObjCInfoProperty mVerticesProperty;
    bhkWorldObjCInfoProperty mNormalsProperty;
    std::vector<osg::Vec4f> mVertices;
    std::vector<osg::Vec4f> mNormals;
    void read(NIFStream *nif) override;
};

// A box
struct bhkBoxShape : public bhkConvexShape
{
    osg::Vec3f mExtents;
    void read(NIFStream *nif) override;
};

// a capsule
struct bhkCapsuleShape : public bhkConvexShape
{
    osg::Vec3f mFirstPoint;
    float mRadius1;
    osg::Vec3f mSecondPoint;
    float mRadius2;
    void read(NIFStream* nif);
};

struct bhkSphereShape : public bhkConvexShape
{
};

// A list of shapes
struct bhkListShape : public bhkShapeCollection
{
    bhkShapeList mSubshapes;
    HavokMaterial mHavokMaterial;
    bhkWorldObjCInfoProperty mChildShapeProperty;
    bhkWorldObjCInfoProperty mChildFilterProperty;
    std::vector<HavokFilter> mHavokFilters;
    void read(NIFStream *nif) override;
};

struct bhkRigidBody : public bhkEntity
{
    bhkRigidBodyCInfo mInfo;
    bhkSerializableList mConstraints;
    unsigned int mBodyFlags;

    void read(NIFStream *nif) override;
};

} // Namespace
#endif
