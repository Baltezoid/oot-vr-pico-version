#pragma once

// Rules for the spin attack gesture in VR first person (issue #133). No game types: the host tests
// in port/soh/tests/vr/VrSpinGestureTests.cpp build it alone.
//
// The player holds the charge pose: the sword hand below the chest, at the side of the hip, and
// the blade points back. The vanilla spin charge then runs. A fast swing out of the pose releases
// the spin attack. A slow exit from the pose cancels the charge.

#include "VrMeleeWeapon.h"

namespace VrSpinGesture {

struct Params {
    float maxHandUpM = -0.40f;         // the hand is this far below the eyes, or more
    float minHandSideM = -0.05f;       // toward the sword side; a small value on the other side is good
    float maxHandForwardM = 0.25f;     // the hand is not far in front of the head
    float maxBladeForward = -0.5f;     // the blade points back: 120 degrees or more from the view
    float maxStartHandSpeedMps = 1.0f; // a fast hand that moves through the pose does not charge
    int maxArmedTicks = 6;             // ticks out of the pose at the arm speed before a cancel
};

// One game tick. The positions are relative to the eyes, in the yaw frame of the head, in meters.
struct Sample {
    bool eligible = false;  // VR combat is active and Link holds a weapon for a spin attack
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

// The virtual B button of the vanilla charge. ChargeHeld is true while the player holds the charge
// pose. When the charge ends, Cancelled is true after a slow exit from the charge pose, and false
// after a release swing.
class Detector {
  public:
    void Update(const Params& p, const Sample& s) {
        if (mReleaseSwing && s.tier == VrMeleeWeapon::kIdle) {
            mReleaseSwing = false;
        }
        if (!mCharging) {
            if (s.eligible && InChargePose(p, s) && s.handSpeedMps <= p.maxStartHandSpeedMps) {
                mCharging = true;
                mCancelled = false;
                mReleaseSwing = false;
                mArmedTicks = 0;
                mCharge = s.charge;
            }
            return;
        }
        // The charge of the release swing is the charge of the last tick of the charge.
        if (!s.eligible) {
            End(true);
        } else if (s.tier == VrMeleeWeapon::kHot) {
            End(false);
            mReleaseSwing = true;
        } else if (InChargePose(p, s)) {
            mArmedTicks = 0;
            mCharge = s.charge;
        } else if (s.tier == VrMeleeWeapon::kArmed && ++mArmedTicks <= p.maxArmedTicks) {
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
    int mArmedTicks = 0;
    float mCharge = 0.0f;
};

} // namespace VrSpinGesture
