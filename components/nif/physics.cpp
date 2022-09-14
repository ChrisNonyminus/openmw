#include "physics.hpp"

#include "data.hpp"
#include "node.hpp"

namespace Nif
{

    /// Non-record data types

    void bhkWorldObjCInfoProperty::read(NIFStream *nif)
    {
        mData = nif->getUInt();
        mSize = nif->getUInt();
        mCapacityAndFlags = nif->getUInt();
    }

    void bhkWorldObjectCInfo::read(NIFStream *nif)
    {
        nif->skip(4); // Unused
        mPhaseType = static_cast<BroadPhaseType>(nif->getChar());
        nif->skip(3); // Unused
        mProperty.read(nif);
    }

    void HavokMaterial::read(NIFStream *nif)
    {
        if (nif->getVersion() <= NIFFile::NIFVersion::VER_OB_OLD)
            nif->skip(4); // Unknown
        mMaterial = nif->getUInt();
    }

    void HavokFilter::read(NIFStream *nif)
    {
        mLayer = nif->getChar();
        mFlags = nif->getChar();
        mGroup = nif->getUShort();
    }

    void hkSubPartData::read(NIFStream *nif)
    {
        mHavokFilter.read(nif);
        mNumVertices = nif->getUInt();
        mHavokMaterial.read(nif);
    }

    void hkpMoppCode::read(NIFStream *nif)
    {
        unsigned int size = nif->getUInt();
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
            mOffset = nif->getVector4();
        if (nif->getBethVersion() > NIFFile::BethVersion::BETHVER_FO3)
            nif->getChar(); // MOPP data build type
        if (size)
            nif->getChars(mData, size);
    }

    void bhkEntityCInfo::read(NIFStream *nif)
    {
        mResponseType = static_cast<hkResponseType>(nif->getChar());
        nif->skip(1); // Unused
        mProcessContactDelay = nif->getUShort();
    }

    void TriangleData::read(NIFStream *nif)
    {
        for (int i = 0; i < 3; i++)
            mTriangle[i] = nif->getUShort();
        mWeldingInfo = nif->getUShort();
        if (nif->getVersion() <= NIFFile::NIFVersion::VER_OB)
            mNormal = nif->getVector3();
    }

    void bhkRigidBodyCInfo::read(NIFStream *nif)
    {
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
        {
            nif->skip(4); // Unused
            mHavokFilter.read(nif);
            nif->skip(4); // Unused
            if (nif->getBethVersion() != NIFFile::BethVersion::BETHVER_FO4)
            {
                if (nif->getBethVersion() >= 83)
                    nif->skip(4); // Unused
                mResponseType = static_cast<hkResponseType>(nif->getChar());
                nif->skip(1); // Unused
                mProcessContactDelay = nif->getUShort();
            }
        }
        if (nif->getBethVersion() < 83)
            nif->skip(4); // Unused
        mTranslation = nif->getVector4();
        mRotation = nif->getQuaternion();
        mLinearVelocity = nif->getVector4();
        mAngularVelocity = nif->getVector4();
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                mInertiaTensor[i][j] = nif->getFloat();
        mCenter = nif->getVector4();
        mMass = nif->getFloat();
        mLinearDamping = nif->getFloat();
        mAngularDamping = nif->getFloat();
        if (nif->getBethVersion() >= 83)
        {
            if (nif->getBethVersion() != NIFFile::BethVersion::BETHVER_FO4)
                mTimeFactor = nif->getFloat();
            mGravityFactor = nif->getFloat();
        }
        mFriction = nif->getFloat();
        if (nif->getBethVersion() >= 83)
            mRollingFrictionMult = nif->getFloat();
        mRestitution = nif->getFloat();
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
        {
            mMaxLinearVelocity = nif->getFloat();
            mMaxAngularVelocity = nif->getFloat();
            if (nif->getBethVersion() != NIFFile::BethVersion::BETHVER_FO4)
                mPenetrationDepth = nif->getFloat();
        }
        mMotionType = static_cast<hkMotionType>(nif->getChar());
        if (nif->getBethVersion() < 83)
            mDeactivatorType = static_cast<hkDeactivatorType>(nif->getChar());
        else
            mEnableDeactivation = nif->getBoolean();
        mSolverDeactivation = static_cast<hkSolverDeactivation>(nif->getChar());
        if (nif->getBethVersion() == NIFFile::BethVersion::BETHVER_FO4)
        {
            nif->skip(1);
            mPenetrationDepth = nif->getFloat();
            mTimeFactor = nif->getFloat();
            nif->skip(4);
            mResponseType = static_cast<hkResponseType>(nif->getChar());
            nif->skip(1); // Unused
            mProcessContactDelay = nif->getUShort();
        }
        mQualityType = static_cast<hkQualityType>(nif->getChar());
        if (nif->getBethVersion() >= 83)
        {
            mAutoRemoveLevel = nif->getChar();
            mResponseModifierFlags = nif->getChar();
            mNumContactPointShapeKeys = nif->getChar();
            mForceCollidedOntoPPU = nif->getBoolean();
        }
        if (nif->getBethVersion() == NIFFile::BethVersion::BETHVER_FO4)
            nif->skip(3); // Unused
        else
            nif->skip(12); // Unused
    }

    /// Record types

    void bhkCollisionObject::read(NIFStream *nif)
    {
        NiCollisionObject::read(nif);
        mFlags = nif->getUShort();
        mBody.read(nif);
    }

    void bhkWorldObject::read(NIFStream *nif)
    {
        mShape.read(nif);
        if (nif->getVersion() <= NIFFile::NIFVersion::VER_OB_OLD)
            nif->skip(4); // Unknown
        mHavokFilter.read(nif);
        mWorldObjectInfo.read(nif);
    }

    void bhkWorldObject::post(NIFFile *nif)
    {
        mShape.post(nif);
    }

    void bhkEntity::read(NIFStream *nif)
    {
        bhkWorldObject::read(nif);
        mInfo.read(nif);
    }

    void bhkBvTreeShape::read(NIFStream *nif)
    {
        mShape.read(nif);
    }

    void bhkBvTreeShape::post(NIFFile *nif)
    {
        mShape.post(nif);
    }

    void bhkMoppBvTreeShape::read(NIFStream *nif)
    {
        bhkBvTreeShape::read(nif);
        nif->skip(12); // Unused
        mScale = nif->getFloat();
        mMopp.read(nif);
    }

    void bhkNiTriStripsShape::read(NIFStream *nif)
    {
        mHavokMaterial.read(nif);
        mRadius = nif->getFloat();
        nif->skip(20); // Unused
        mGrowBy = nif->getUInt();
        if (nif->getVersion() >= NIFStream::generateVersion(10,1,0,0))
            mScale = nif->getVector4();
        mData.read(nif);
        unsigned int numFilters = nif->getUInt();
        nif->getUInts(mFilters, numFilters);
    }

    void bhkNiTriStripsShape::post(NIFFile *nif)
    {
        mData.post(nif);
    }

    void bhkPackedNiTriStripsShape::read(NIFStream *nif)
    {
        if (nif->getVersion() <= NIFFile::NIFVersion::VER_OB)
        {
            mSubshapes.resize(nif->getUShort());
            for (hkSubPartData& subshape : mSubshapes)
                subshape.read(nif);
        }
        mUserData = nif->getUInt();
        nif->skip(4); // Unused
        mRadius = nif->getFloat();
        nif->skip(4); // Unused
        mScale = nif->getVector4();
        nif->skip(20); // Duplicates of the two previous fields
        mData.read(nif);
    }

    void bhkPackedNiTriStripsShape::post(NIFFile *nif)
    {
        mData.post(nif);
    }

    void hkPackedNiTriStripsData::read(NIFStream *nif)
    {
        unsigned int numTriangles = nif->getUInt();
        mTriangles.resize(numTriangles);
        for (unsigned int i = 0; i < numTriangles; i++)
            mTriangles[i].read(nif);

        unsigned int numVertices = nif->getUInt();
        bool compressed = false;
        if (nif->getVersion() >= NIFFile::NIFVersion::VER_BGS)
            compressed = nif->getBoolean();
        if (!compressed)
            nif->getVector3s(mVertices, numVertices);
        else
            nif->skip(6 * numVertices); // Half-precision vectors are not currently supported
        if (nif->getVersion() >= NIFFile::NIFVersion::VER_BGS)
        {
            mSubshapes.resize(nif->getUShort());
            for (hkSubPartData& subshape : mSubshapes)
                subshape.read(nif);
        }
    }

    void bhkSphereRepShape::read(NIFStream *nif)
    {
        mHavokMaterial.read(nif);
    }

    void bhkConvexShape::read(NIFStream *nif)
    {
        bhkSphereRepShape::read(nif);
        mRadius = nif->getFloat();
    }

    void bhkConvexVerticesShape::read(NIFStream *nif)
    {
        bhkConvexShape::read(nif);
        mVerticesProperty.read(nif);
        mNormalsProperty.read(nif);
        unsigned int numVertices = nif->getUInt();
        if (numVertices)
            nif->getVector4s(mVertices, numVertices);
        unsigned int numNormals = nif->getUInt();
        if (numNormals)
            nif->getVector4s(mNormals, numNormals);
    }

    void bhkBoxShape::read(NIFStream *nif)
    {
        bhkConvexShape::read(nif);
        nif->skip(8); // Unused
        mExtents = nif->getVector3();
        nif->skip(4); // Unused
    }

    void bhkListShape::read(NIFStream *nif)
    {
        mSubshapes.read(nif);
        mHavokMaterial.read(nif);
        mChildShapeProperty.read(nif);
        mChildFilterProperty.read(nif);
        unsigned int numFilters = nif->getUInt();
        mHavokFilters.resize(numFilters);
        for (HavokFilter& filter : mHavokFilters)
            filter.read(nif);
    }

    void bhkRigidBody::read(NIFStream *nif)
    {
        bhkEntity::read(nif);
        mInfo.read(nif);
        mConstraints.read(nif);
        if (nif->getBethVersion() < 76)
            mBodyFlags = nif->getUInt();
        else
            mBodyFlags = nif->getUShort();
    }

    void bhkCapsuleShape::read(NIFStream* nif)
    {
        bhkConvexShape::read(nif);
        nif->skip(8); // unused
        mFirstPoint = nif->getVector3();
        mRadius1 = nif->getFloat();
        mSecondPoint = nif->getVector3();
        mRadius2 = nif->getFloat();
    }

void bhkBlendCollisionObject::read(NIFStream* nif)
{
    bhkCollisionObject::read(nif);
    mHeirGain = nif->getFloat();
    mVelGain = nif->getFloat();
    if (nif->getBethVersion() < 9)
    {
        mUnknown1 = nif->getFloat();
        mUnknown2 = nif->getFloat();
    }
}

void bhkConstraint::read(NIFStream* nif)
{
    mCInfo.read(nif);
}

void bhkRagdollConstraint::read(NIFStream* nif)
{
    bhkConstraint::read(nif);
    mRagdollCInfo.read(nif);
}

void bhkConstraint::bhkConstraintCInfo::read(NIFStream* nif)
{
    mNumEntities = nif->getUInt();
    assert(mNumEntities == 2);
    mEntityA.read(nif);
    mEntityB.read(nif);
    mPriority = nif->getUInt();
}

void bhkConstraint::bhkConstraintCInfo::post(NIFFile* nif)
{
    mEntityA.post(nif);
    mEntityB.post(nif);
}

void bhkRagdollConstraint::bhkRagdollConstraintCInfo::read(NIFStream* nif)
{
    if (nif->getBethVersion() <= 16)
    {
        mPivotA = nif->getVector4();
        mPlaneA = nif->getVector4();
        mTwistA = nif->getVector4();
        mPivotB = nif->getVector4();
        mPlaneB = nif->getVector4();
        mTwistB = nif->getVector4();
    }
    else
    {
        mTwistA = nif->getVector4();
        mPlaneA = nif->getVector4();
        mMotorA = nif->getVector4();
        mPivotA = nif->getVector4();
        mTwistB = nif->getVector4();
        mPlaneB = nif->getVector4();
        mMotorB = nif->getVector4();
        mPivotB = nif->getVector4();
    }
    mConeMaxAngle = nif->getFloat();
    mPlaneMinAngle = nif->getFloat();
    mPlaneMaxAngle = nif->getFloat();
    mTwistMinAngle = nif->getFloat();
    mTwistMaxAngle = nif->getFloat();
    mMaxFriction = nif->getFloat();

    if (nif->getBethVersion() > 16)
    {
        mMotor.read(nif);
    }
}

void bhkConstraintMotorCInfo::read(NIFStream* nif)
{
    mType = nif->getChar();
    switch (mType)
    {
        case 0:
        {
            break;
        }
        case 1:
        {
            mMotor.mPosition.mMinForce = nif->getFloat();
            mMotor.mPosition.mMaxForce = nif->getFloat();
            mMotor.mPosition.mTau = nif->getFloat();
            mMotor.mPosition.mDamping = nif->getFloat();
            mMotor.mPosition.mProportionalRecoveryVelocity = nif->getFloat();
            mMotor.mPosition.mConstantRecoveryVelocity = nif->getFloat();
            mMotor.mPosition.mMotorEnabled = nif->getBoolean();
            break;
        }
        case 2:
        {
            mMotor.mVelocity.mMinForce = nif->getFloat();
            mMotor.mVelocity.mMaxForce = nif->getFloat();
            mMotor.mVelocity.mTau = nif->getFloat();
            mMotor.mVelocity.mTargetVelocity = nif->getFloat();
            mMotor.mVelocity.mUseVelocityTarget = nif->getBoolean();
            mMotor.mVelocity.mMotorEnabled = nif->getBoolean();
            break;
        }
        case 3:
        {
            mMotor.mSpringDamping.mMinForce = nif->getFloat();
            mMotor.mSpringDamping.mMaxForce = nif->getFloat();
            mMotor.mSpringDamping.mSpringConstant = nif->getFloat();
            mMotor.mSpringDamping.mSpringDamping = nif->getFloat();
            mMotor.mSpringDamping.mMotorEnabled = nif->getBoolean();
            break;
        }
        default:
        {
            throw std::runtime_error("Bad mType in bhkConstraintMotorCInfo");
        }
    }
}

void bhkConstraint::post(NIFFile* nif)
{
    mCInfo.post(nif);
}

void bhkLimitedHingeConstraint::read(NIFStream* nif)
{
    bhkConstraint::read(nif);
    mHingeCInfo.read(nif);
}

void bhkLimitedHingeConstraint::bhkLimitedHingeConstraintCInfo::read(NIFStream* nif)
{
    if (nif->getBethVersion() <= 16)
    {
        mPivotA = nif->getVector4();
        mAxisA = nif->getVector4();
        mPerpAxisInA1 = nif->getVector4();
        mPerpAxisInA2 = nif->getVector4();
        mPivotB = nif->getVector4();
        mAxisB = nif->getVector4();
        mPerpAxisInB2 = nif->getVector4();
    }
    else
    {
        mAxisA = nif->getVector4();
        mPerpAxisInA1 = nif->getVector4();
        mPerpAxisInA2 = nif->getVector4();
        mPivotA = nif->getVector4();
        mAxisB = nif->getVector4();
        mPerpAxisInB1 = nif->getVector4();
        mPerpAxisInB2 = nif->getVector4();
        mPivotB = nif->getVector4();
    }
    mMinAngle = nif->getFloat();
    mMaxAngle = nif->getFloat();
    mMaxFriction = nif->getFloat();

    if (nif->getBethVersion() > 16)
    {
        mMotor.read(nif);
    }
}

void bhkPrismaticConstraint::read(NIFStream* nif)
{
    bhkConstraint::read(nif);
    mPrismaticCInfo.read(nif);
}

void bhkPrismaticConstraint::bhkPrismaticConstraintCInfo::read(NIFStream* nif)
{
    if (nif->getVersion() == NIFFile::VER_OB)
    {
        mPivotA = nif->getVector4();
        mRotationA = nif->getVector4();
        mPlaneA = nif->getVector4();
        mSlidingA = nif->getVector4();
        mSlidingB = nif->getVector4();
        mPivotB = nif->getVector4();
        mRotationB = nif->getVector4();
        mPlaneB = nif->getVector4();
    }
    else if (nif->getVersion() == NIFFile::VER_BGS)
    {
        mSlidingA = nif->getVector4();
        mRotationA = nif->getVector4();
        mPlaneA = nif->getVector4();
        mPivotA = nif->getVector4();
        mSlidingB = nif->getVector4();
        mRotationB = nif->getVector4();
        mPlaneB = nif->getVector4();
        mPivotB = nif->getVector4();
    }
    mMinAngle = nif->getFloat();
    mMaxAngle = nif->getFloat();
    mMaxFriction = nif->getFloat();

    if (nif->getVersion() == NIFFile::VER_BGS)
    {
        mMotor.read(nif);
    }
}

void bhkWrappedConstraintData::read(NIFStream* nif)
{
    mType = nif->getUInt();
    mCInfo.read(nif);
    switch (mType)
    {
        case 2:
        {
            mLimitedHinge.read(nif);
            break;
        }
        case 6:
        {
            mPrismatic.read(nif);
            break;
        }
        case 7:
        {
            mRagdoll.read(nif);
            break;
        }
        case 13:
        {
            mMalleable.read(nif);
            break;
        }
        default:
        {
            std::stringstream errmsg;
            errmsg  << "Unhandled type '" << mType << "' in bhkWrappedConstraintData";
            throw std::runtime_error(errmsg.str());
        }
    }
}

void bhkWrappedConstraintData::post(NIFFile* nif)
{
    mCInfo.post(nif);
    if (mType == 13)
        mMalleable.post(nif);
}

void bhkBreakableConstraint::read(NIFStream* nif)
{
    bhkConstraint::read(nif);
    mConstraintData.read(nif);
    mThreshold = nif->getFloat();
    mRemoveWhenBroken = nif->getBoolean();
}

void bhkBreakableConstraint::post(NIFFile* nif)
{
    bhkConstraint::post(nif);
    mConstraintData.post(nif);
}

void bhkMalleableConstraint::bhkMalleableConstraintCInfo::read(NIFStream* nif)
{
    mType = nif->getUInt();
    mCInfo.read(nif);
    switch (mType)
    {
        case 2:
        {
            mLimitedHinge.read(nif);
            break;
        }
        case 6:
        {
            mPrismatic.read(nif);
            break;
        }
        case 7:
        {
            mRagdoll.read(nif);
            break;
        }
        default:
        {
            std::stringstream errmsg;
            errmsg << "Unhandled type '" << mType << "' in bhkMalleableConstraintCInfo";
            throw std::runtime_error(errmsg.str());
        }
    }
    if (nif->getVersion() != NIFFile::VER_BGS)
    {
        mTau = nif->getFloat();
        mDamping = nif->getFloat();
    }
    else
        mStrength = nif->getFloat();
}

void bhkMalleableConstraint::bhkMalleableConstraintCInfo::post(NIFFile* nif)
{
    mCInfo.post(nif);
}

void bhkMalleableConstraint::read(NIFStream* nif)
{
    bhkConstraint::read(nif);
    mData.read(nif);
}

void bhkMalleableConstraint::post(NIFFile* nif)
{
    bhkConstraint::post(nif);
    mData.post(nif);
}

} // Namespace
