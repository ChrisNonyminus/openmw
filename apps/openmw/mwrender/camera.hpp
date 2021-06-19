#ifndef GAME_MWRENDER_CAMERA_H
#define GAME_MWRENDER_CAMERA_H

#include <optional>
#include <string>

#include <osg/ref_ptr>
#include <osg/Vec3>
#include <osg/Vec3d>

#include "../mwworld/ptr.hpp"

namespace osg
{
    class Camera;
    class Callback;
    class Node;
}

namespace MWRender
{
    class NpcAnimation;

    /// \brief Camera control
    class Camera
    {
    public:
        enum class Mode : int {Static = 0, FirstPerson = 1, ThirdPerson = 2, Vanity = 3, Preview = 4, StandingPreview = 5};

    private:
        MWWorld::Ptr mTrackingPtr;
        osg::ref_ptr<const osg::Node> mTrackingNode;
        float mHeightScale;

        osg::ref_ptr<osg::Camera> mCamera;

        NpcAnimation *mAnimation;

        // Always 'true' if mMode == `FirstPerson`. Also it is 'true' in `Vanity` or `Preview` modes if
        // the camera should return to `FirstPerson` view after it.
        bool mFirstPersonView;

        Mode mMode;
        std::optional<Mode> mQueuedMode;
        bool mVanityAllowed;
        bool mStandingPreviewAllowed;
        bool mDeferredRotationAllowed;

        float mNearest;
        float mFurthest;
        bool mIsNearest;

        bool mProcessViewChange;

        float mHeight, mBaseCameraDistance;
        float mPitch, mYaw, mRoll;
        osg::Vec3d mPosition;

        float mCameraDistance;
        float mMaxNextCameraDistance;

        osg::Vec2d mFocalPointCurrentOffset;
        osg::Vec2d mFocalPointTargetOffset;
        float mFocalPointTransitionSpeedCoef;
        bool mSkipFocalPointTransition;

        // This fields are used to make focal point transition smooth if previous transition was not finished.
        float mPreviousTransitionInfluence;
        osg::Vec2d mFocalPointTransitionSpeed;
        osg::Vec2d mPreviousTransitionSpeed;
        osg::Vec2d mPreviousExtraOffset;

        float mSmoothedSpeed;
        float mZoomOutWhenMoveCoef;
        bool mDynamicCameraDistanceEnabled;
        bool mShowCrosshairInThirdPersonMode;

        bool mHeadBobbingEnabled;
        float mHeadBobbingOffset;
        float mHeadBobbingWeight; // Value from 0 to 1 for smooth enabling/disabling.
        float mTotalMovement; // Needed for head bobbing.
        void updateHeadBobbing(float duration);

        osg::Vec3d getTrackingNodePosition() const;
        osg::Vec3d getFocalPointOffset() const;
        void updateFocalPointOffset(float duration);
        void updatePosition();
        float getCameraDistanceCorrection() const;

        osg::ref_ptr<osg::Callback> mUpdateCallback;

        // Used to rotate player to the direction of view after exiting preview or vanity mode.
        osg::Vec3f mDeferredRotation;
        bool mDeferredRotationDisabled;
        void calculateDeferredRotation();
        void updateStandingPreviewMode();

    public:
        Camera(osg::Camera* camera);
        ~Camera();

        /// Attach camera to object
        void attachTo(const MWWorld::Ptr &ptr) { mTrackingPtr = ptr; }
        MWWorld::Ptr getTrackingPtr() const { return mTrackingPtr; }

        void setFocalPointTransitionSpeed(float v) { mFocalPointTransitionSpeedCoef = v; }
        void setFocalPointTargetOffset(const osg::Vec2d& v);
        void instantTransition();
        void enableDynamicCameraDistance(bool v) { mDynamicCameraDistanceEnabled = v; }
        void enableCrosshairInThirdPersonMode(bool v) { mShowCrosshairInThirdPersonMode = v; }

        /// Update the view matrix of \a cam
        void updateCamera(osg::Camera* cam);

        /// Reset to defaults
        void reset() { setMode(Mode::FirstPerson); }

        void rotateCameraToTrackingPtr();

        float getYaw() const { return mYaw; }
        void setYaw(float angle);

        float getPitch() const { return mPitch; }
        void setPitch(float angle);

        /// @param Force view mode switch, even if currently not allowed by the animation.
        void toggleViewMode(bool force=false);

        bool toggleVanityMode(bool enable);
        void allowVanityMode(bool allow);

        /// @note this may be ignored if an important animation is currently playing
        void togglePreviewMode(bool enable);

        void applyDeferredPreviewRotationToPlayer(float dt);
        void disableDeferredPreviewRotation() { mDeferredRotationDisabled = true; }

        /// \brief Lowers the camera for sneak.
        void setSneakOffset(float offset);

        void processViewChange();

        void update(float duration, bool paused=false);

        /// Adds distDelta to the camera distance. Switches 3rd/1st person view if distance is less than limit.
        void adjustCameraDistance(float distDelta);

        float getCameraDistance() const;

        void setAnimation(NpcAnimation *anim);

        osg::Vec3d getThirdPersonBasePosition() const;
        const osg::Vec3d& getPosition() const { return mPosition; }

        bool isVanityOrPreviewModeEnabled() const;
        Mode getMode() const { return mMode; }
        void setMode(Mode mode, bool force = true);
    };
}

#endif
