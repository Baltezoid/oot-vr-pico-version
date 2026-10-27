#pragma once

// Rules for the spin attack gesture in VR first person (issue #133). No game types: the host tests
// in port/soh/tests/vr/VrSpinGestureTests.cpp build it alone.
//
// The player holds the charge pose: the sword hand below the chest, at the side of the hip, and
// the blade points back. The vanilla spin charge then runs. A swing out of the pose at the arm
// speed releases the spin attack. A slow exit from the pose cancels the charge.
//
// The pose is in the yaw frame of the body (BodyYaw), not of the head: the player can look to the
// side and start a charge. While the charge runs, the frame does not move, and the hold pose has
// wider limits than the start pose.

#include "VrMeleeWeapon.h"

namespace VrSpinGesture {

struct Params {
    float maxHandUpM = -0.30f;         // the hand is this far below the eyes, or more
    float minHandSideM = -0.10f;       // toward the sword side; a small value on the other side is good
    float maxHandForwardM = 0.30f;     // the hand is not far in front of the head
    float maxBladeForward = -0.25f;    // the blade points back: 105 degrees or more from the front
    float maxStartHandSpeedMps = 1.0f; // a fast hand that moves through the pose does not charge
    // The hold pose: the limits while the charge runs.
    float holdMaxHandUpM = -0.25f;
    float holdMinHandSideM = -0.20f;
    float holdMaxHandForwardM = 0.40f;
    float holdMaxBladeForward = -0.1f; // the blade points back, or to the side
    int maxOutTicks = 8;               // ticks out of the hold pose before a cancel
};

// One game tick. The positions are relative to the eyes, in the yaw frame of the head, in meters.
struct Sample {
    bool eligible = false;  // VR combat is active and Link holds a weapon for a spin attack
    bool tracked = true;    // the pose is known. The headset can lose a hand behind the body.
    float handSideM = 0.0f; // positive toward the side of the sword hand
    float handUpM = 0.0f;
    float handForwardM = 0.0f;
    float bladeForward = 0.0f; // forward part of the unit vector from the hilt to the tip
    float handSpeedMps = 0.0f;
    int tier = VrMeleeWeapon::kIdle;
    float charge = 0.0f; // the vanilla charge (Player unk_858) while Link charges, else 0
};

inline bool InChargePose(const Params& p, const Sample& s) {
    return s.handUpM <= p.maxHandUpM && s.handSideM >= p.minHandSideM && s.handForwardM <= p.maxHandForwardM &&
           s.bladeForward <= p.maxBladeForward;
}

inline bool InHoldPose(const Params& p, const Sample& s) {
    return s.handUpM <= p.holdMaxHandUpM && s.handSideM >= p.holdMinHandSideM &&
           s.handForwardM <= p.holdMaxHandForwardM && s.bladeForward <= p.holdMaxBladeForward;
}

// The yaw of the body, from the yaw of the head. A look to the side does not turn the body. The body
// follows the head when the head turns more than maxOffsetRad, and slowly when the player stays
// turned. Angles in radians.
struct BodyYawParams {
    float maxOffsetRad = 0.79f;     // 45 degrees
    float followPerTick = 0.033f;   // about 1.5 s at 20 ticks per second
};

inline float WrapAngle(float a) {
    constexpr float kPi = 3.14159265f;
    while (a > kPi) {
        a -= 2.0f * kPi;
    }
    while (a < -kPi) {
        a += 2.0f * kPi;
    }
    return a;
}

class BodyYaw {
  public:
    // One tick. A frozen body (for example while charging) does not follow the head.
    void Update(const BodyYawParams& p, float headYawRad, bool frozen) {
        if (!mValid) {
            mYaw = headYawRad;
            mValid = true;
            return;
        }
        if (frozen) {
            return;
        }
        float offset = WrapAngle(headYawRad - mYaw);
        offset -= offset * p.followPerTick;
        if (offset > p.maxOffsetRad) {
            offset = p.maxOffsetRad;
        } else if (offset < -p.maxOffsetRad) {
            offset = -p.maxOffsetRad;
        }
        mYaw = WrapAngle(headYawRad - offset);
    }

    float Yaw() const {
        return mYaw;
    }

    void Reset() {
        mValid = false;
    }

  private:
    float mYaw = 0.0f;
    bool mValid = false;
};

// The vanilla rules (Player_ActionHandler_8, Player_CanSpinAttack): the swords can do a spin
// attack. The Deku Stick, the hammer, and the broken Giant's Knife cannot.
inline bool CanSpin(int held, bool brokenKnife) {
    return (held == VrMeleeWeapon::kKokiri || held == VrMeleeWeapon::kMaster || held == VrMeleeWeapon::kBiggoron) &&
           !brokenKnife;
}

// The vanilla charge levels (Player_Action_80844E68, EnMThunder): a spin attack from 0.1, and a
// full charge from 0.85. A full charge hits a larger area.
constexpr float kSpinCharge = 0.1f;
constexpr float kFullCharge = 0.85f;

// The vibration of the sword hand while the charge grows.
inline float ChargeHapticAmplitude(float charge) {
    if (charge < kSpinCharge) {
        return 0.0f;
    }
    return 0.15f + 0.35f * (charge > 1.0f ? 1.0f : charge);
}

// The tick in which the charge becomes full. The sword hand gets one strong pulse.
inline bool ReachedFullCharge(float prevCharge, float charge) {
    return prevCharge < kFullCharge && charge >= kFullCharge;
}

// The virtual B button of the vanilla charge. ChargeHeld is true while the player holds the hold
// pose. When the charge ends, Cancelled is true after a slow exit from the hold pose, and false
// after a release swing.
class Detector {
  public:
    void Update(const Params& p, const Sample& s) {
        if (mReleaseSwing && s.tier == VrMeleeWeapon::kIdle) {
            mReleaseSwing = false;
        }
        if (!mCharging) {
            if (s.eligible && s.tracked && InChargePose(p, s) && s.handSpeedMps <= p.maxStartHandSpeedMps) {
                mCharging = true;
                mCancelled = false;
                mReleaseSwing = false;
                mOutTicks = 0;
                mCharge = s.charge;
            }
            return;
        }
        // The charge of the release swing is the charge of the last tick of the charge.
        if (!s.eligible) {
            End(true);
        } else if (!s.tracked) {
            // A hand at the hip is often behind the body: keep the charge until the headset sees it.
            mCharge = s.charge;
        } else if (s.tier == VrMeleeWeapon::kHot || (s.tier == VrMeleeWeapon::kArmed && !InHoldPose(p, s))) {
            End(false);
            mReleaseSwing = true;
        } else if (InHoldPose(p, s)) {
            mOutTicks = 0;
            mCharge = s.charge;
        } else if (++mOutTicks <= p.maxOutTicks) {
            // A short time out of the pose: the hand can go back, or swing.
            mCharge = s.charge;
        } else {
            End(true);
        }
    }

    bool ChargeHeld() const {
        return mCharging;
    }

    bool Cancelled() const {
        return mCancelled;
    }

    // The spin attack does the damage of this swing: the physical swing must not hit too. True
    // while the charge can start a spin attack, and for the release swing until it ends.
    bool SpinOwnsSwing() const {
        return (mCharging || mReleaseSwing) && mCharge >= kSpinCharge;
    }

    void Reset() {
        *this = Detector();
    }

  private:
    void End(bool cancelled) {
        mCharging = false;
        mCancelled = cancelled;
    }

    bool mCharging = false;
    bool mCancelled = false;
    bool mReleaseSwing = false;
    int mOutTicks = 0;
    float mCharge = 0.0f;
};

} // namespace VrSpinGesture
