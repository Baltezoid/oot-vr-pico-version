// Host unit tests for VrSpinGesture.h (pure rule, no game or OpenXR). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrSpinGestureTests.cpp -o /tmp/vr_spin_tests
//   /tmp/vr_spin_tests

#include "VrSpinGesture.h"

#include <cstdio>

using namespace VrSpinGesture;

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

static const Params kParams = {};

// The pose of Link: the hand at the hip on the sword side, the blade points back.
static Sample InPose() {
    Sample s;
    s.eligible = true;
    s.handSideM = 0.20f;
    s.handUpM = -0.60f;
    s.handForwardM = 0.0f;
    s.bladeForward = -0.9f;
    s.handSpeedMps = 0.1f;
    s.tier = VrMeleeWeapon::kIdle;
    s.charge = 0.0f;
    return s;
}

static void HipPoseWithBladeBackIsChargePose() {
    EXPECT(InChargePose(kParams, InPose()));
}

static void BladeForwardIsNotChargePose() {
    Sample s = InPose();
    s.bladeForward = 0.9f;
    EXPECT(!InChargePose(kParams, s));
}

static void BladeStraightDownIsNotChargePose() {
    Sample s = InPose();
    s.bladeForward = 0.0f;
    EXPECT(!InChargePose(kParams, s));
}

static void HandAtChestHeightIsNotChargePose() {
    Sample s = InPose();
    s.handUpM = -0.20f;
    EXPECT(!InChargePose(kParams, s));
}

static void HandOnOtherSideIsNotChargePose() {
    Sample s = InPose();
    s.handSideM = -0.30f;
    EXPECT(!InChargePose(kParams, s));
}

static void HandFarInFrontIsNotChargePose() {
    Sample s = InPose();
    s.handForwardM = 0.45f;
    EXPECT(!InChargePose(kParams, s));
}

static Sample OutOfPose(int tier, float handSpeedMps) {
    Sample s = InPose();
    s.handForwardM = 0.50f;
    s.bladeForward = 0.5f;
    s.tier = tier;
    s.handSpeedMps = handSpeedMps;
    return s;
}

static void StillHandInPoseChargesAtOnce() {
    Detector d;
    d.Update(kParams, InPose());
    EXPECT(d.ChargeHeld());
}

static void FastHandThroughPoseDoesNotCharge() {
    Detector d;
    Sample s = InPose();
    s.handSpeedMps = 2.5f;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
}

static void IneligibleWeaponDoesNotCharge() {
    Detector d;
    Sample s = InPose();
    s.eligible = false;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
}

static void HandMayMoveInsidePoseWhileCharging() {
    Detector d;
    d.Update(kParams, InPose());
    Sample s = InPose();
    s.handSpeedMps = 1.5f;
    s.tier = VrMeleeWeapon::kArmed;
    d.Update(kParams, s);
    EXPECT(d.ChargeHeld());
}

static void FastSwingOutOfPoseReleases() {
    Detector d;
    d.Update(kParams, InPose());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(!d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

static void SlowExitFromPoseCancels() {
    Detector d;
    d.Update(kParams, InPose());
    for (int i = 0; i < kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    }
    EXPECT(d.ChargeHeld());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    EXPECT(!d.ChargeHeld());
    EXPECT(d.Cancelled());
}

// The hold pose has wider limits than the start pose. A small change of the angle of the blade or
// of the height of the hand does not cancel the charge.
static void HoldPoseIsWiderThanChargePose() {
    Sample s = InPose();
    s.bladeForward = -0.3f;
    s.handUpM = -0.30f;
    s.handSideM = -0.10f;
    EXPECT(!InChargePose(kParams, s));
    EXPECT(InHoldPose(kParams, s));
}

static void ChargeStaysInHoldPose() {
    Detector d;
    d.Update(kParams, InPose());
    Sample s = InPose();
    s.bladeForward = -0.3f;
    for (int i = 0; i < 3 * kParams.maxOutTicks; i++) {
        d.Update(kParams, s);
    }
    EXPECT(d.ChargeHeld());
}

static void HoldPoseDoesNotStartACharge() {
    Detector d;
    Sample s = InPose();
    s.bladeForward = -0.3f;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
}

// The hand stops for a short time out of the pose, then swings fast: the spin attack.
static void ShortStopOutOfPoseThenSwingReleases() {
    Detector d;
    d.Update(kParams, InPose());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    EXPECT(d.ChargeHeld());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(!d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

// After a short time out of the pose, a return to the pose keeps the charge.
static void ReturnToPoseKeepsTheCharge() {
    Detector d;
    d.Update(kParams, InPose());
    for (int i = 0; i < kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    }
    d.Update(kParams, InPose());
    for (int i = 0; i < kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    }
    EXPECT(d.ChargeHeld());
}

// The swing speeds up after the hand leaves the pose. A few armed ticks wait for the hit speed.
static void ArmedExitThatGetsHotReleases() {
    Detector d;
    d.Update(kParams, InPose());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kArmed, 2.0f));
    EXPECT(d.ChargeHeld());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(!d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

static void ArmedExitThatStaysSlowCancels() {
    Detector d;
    d.Update(kParams, InPose());
    for (int i = 0; i <= kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kArmed, 2.0f));
    }
    EXPECT(!d.ChargeHeld());
    EXPECT(d.Cancelled());
}

static void WeaponChangeWhileChargingCancels() {
    Detector d;
    d.Update(kParams, InPose());
    Sample s = InPose();
    s.eligible = false;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
    EXPECT(d.Cancelled());
}

static void NewChargeClearsCancel() {
    Detector d;
    d.Update(kParams, InPose());
    for (int i = 0; i <= kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    }
    EXPECT(d.Cancelled());
    d.Update(kParams, InPose());
    EXPECT(d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

static Sample Charged(float charge) {
    Sample s = InPose();
    s.charge = charge;
    return s;
}

// From a charge of 0.1, the vanilla spin attack does the damage. It replaces the damage of the
// physical swing, with magic and without magic.
static void SpinChargeOwnsTheSwing() {
    Detector d;
    d.Update(kParams, Charged(0.0f));
    d.Update(kParams, Charged(0.12f));
    EXPECT(d.SpinOwnsSwing());
}

static void ShortChargeLeavesTheSwingItsDamage() {
    Detector d;
    d.Update(kParams, Charged(0.0f));
    d.Update(kParams, Charged(0.05f));
    EXPECT(!d.SpinOwnsSwing());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(!d.SpinOwnsSwing());
}

static void ReleaseSwingStaysOwnedUntilItEnds() {
    Detector d;
    d.Update(kParams, Charged(0.0f));
    d.Update(kParams, Charged(0.9f));
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(d.SpinOwnsSwing());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 5.0f));
    EXPECT(d.SpinOwnsSwing());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.3f));
    EXPECT(!d.SpinOwnsSwing());
}

static void CancelledChargeDoesNotOwnTheNextSwing() {
    Detector d;
    d.Update(kParams, Charged(0.0f));
    d.Update(kParams, Charged(0.9f));
    for (int i = 0; i <= kParams.maxOutTicks; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kIdle, 0.5f));
    }
    EXPECT(!d.SpinOwnsSwing());
}

// The vanilla rules: the swords can do a spin attack. The Deku Stick, the hammer, and the broken
// knife cannot.
static void SwordsCanSpin() {
    EXPECT(CanSpin(VrMeleeWeapon::kKokiri, false));
    EXPECT(CanSpin(VrMeleeWeapon::kMaster, false));
    EXPECT(CanSpin(VrMeleeWeapon::kBiggoron, false));
}

static void OtherWeaponsCannotSpin() {
    EXPECT(!CanSpin(VrMeleeWeapon::kNone, false));
    EXPECT(!CanSpin(VrMeleeWeapon::kDekuStick, false));
    EXPECT(!CanSpin(VrMeleeWeapon::kHammer, false));
    EXPECT(!CanSpin(VrMeleeWeapon::kBiggoron, true));
}

// The vanilla spin attack starts at a charge of 0.1. The vibration starts there and gets stronger.
static void NoVibrationBeforeTheSpinCharge() {
    EXPECT(ChargeHapticAmplitude(0.05f) == 0.0f);
}

static void VibrationGrowsWithTheCharge() {
    EXPECT(ChargeHapticAmplitude(0.2f) > 0.0f);
    EXPECT(ChargeHapticAmplitude(0.8f) > ChargeHapticAmplitude(0.2f));
}

// The vanilla full charge starts at 0.85. One pulse tells the player.
static void PulseOnceWhenTheChargeIsFull() {
    EXPECT(!ReachedFullCharge(0.80f, 0.84f));
    EXPECT(ReachedFullCharge(0.84f, 0.86f));
    EXPECT(!ReachedFullCharge(0.86f, 0.88f));
}

int main() {
    HipPoseWithBladeBackIsChargePose();
    BladeForwardIsNotChargePose();
    BladeStraightDownIsNotChargePose();
    HandAtChestHeightIsNotChargePose();
    HandOnOtherSideIsNotChargePose();
    HandFarInFrontIsNotChargePose();
    StillHandInPoseChargesAtOnce();
    FastHandThroughPoseDoesNotCharge();
    IneligibleWeaponDoesNotCharge();
    HandMayMoveInsidePoseWhileCharging();
    FastSwingOutOfPoseReleases();
    SlowExitFromPoseCancels();
    HoldPoseIsWiderThanChargePose();
    ChargeStaysInHoldPose();
    HoldPoseDoesNotStartACharge();
    ShortStopOutOfPoseThenSwingReleases();
    ReturnToPoseKeepsTheCharge();
    ArmedExitThatGetsHotReleases();
    ArmedExitThatStaysSlowCancels();
    WeaponChangeWhileChargingCancels();
    NewChargeClearsCancel();
    SpinChargeOwnsTheSwing();
    ShortChargeLeavesTheSwingItsDamage();
    ReleaseSwingStaysOwnedUntilItEnds();
    CancelledChargeDoesNotOwnTheNextSwing();
    SwordsCanSpin();
    OtherWeaponsCannotSpin();
    NoVibrationBeforeTheSpinCharge();
    VibrationGrowsWithTheCharge();
    PulseOnceWhenTheChargeIsFull();

    if (sFailures == 0) {
        std::printf("VrSpinGesture: all tests passed\n");
        return 0;
    }
    std::printf("VrSpinGesture: %d failure(s)\n", sFailures);
    return 1;
}
