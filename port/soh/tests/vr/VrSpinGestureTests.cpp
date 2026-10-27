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

// On the headset, the charges started with the blade at 105 to 120 degrees from the front. A blade
// at 100 degrees also starts a charge.
static void BladeLessFarBackIsChargePose() {
    Sample s = InPose();
    s.bladeForward = -0.17f;
    EXPECT(InChargePose(kParams, s));
}

static void HandAtChestHeightIsNotChargePose() {
    Sample s = InPose();
    s.handUpM = -0.15f;
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
    s.handForwardM = 0.70f;
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

// On the headset, the end of a release swing was in the charge pose, and a new charge started.
static void SwingThroughPoseDoesNotCharge() {
    Detector d;
    Sample s = InPose();
    s.handSpeedMps = 0.4f;
    s.tier = VrMeleeWeapon::kHot;
    d.Update(kParams, s);
    s.tier = VrMeleeWeapon::kArmed;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
    s.tier = VrMeleeWeapon::kIdle;
    d.Update(kParams, s);
    EXPECT(d.ChargeHeld());
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

// The player can move the charged sword freely: the hand at the head, the blade to the front.
// As in the vanilla game, the charge does not end without a spin attack.
static void ChargeStaysAnywhere() {
    Detector d;
    d.Update(kParams, InPose());
    Sample s = InPose();
    s.handUpM = 0.10f;
    s.handSideM = -0.40f;
    s.handForwardM = 0.70f;
    s.bladeForward = 0.9f;
    for (int i = 0; i < 100; i++) {
        d.Update(kParams, s);
    }
    EXPECT(d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

// A move of the charged sword at the arm speed does not release the spin attack.
static void ArmedMoveKeepsTheCharge() {
    Detector d;
    d.Update(kParams, InPose());
    for (int i = 0; i < 20; i++) {
        d.Update(kParams, OutOfPose(VrMeleeWeapon::kArmed, 2.0f));
    }
    EXPECT(d.ChargeHeld());
    d.Update(kParams, OutOfPose(VrMeleeWeapon::kHot, 6.0f));
    EXPECT(!d.ChargeHeld());
    EXPECT(!d.Cancelled());
}

// The headset can lose the hand at the hip, behind the body. This does not cancel the charge.
static void UntrackedHandKeepsTheCharge() {
    Detector d;
    d.Update(kParams, InPose());
    Sample s = OutOfPose(VrMeleeWeapon::kIdle, 0.0f);
    s.tracked = false;
    for (int i = 0; i < 20; i++) {
        d.Update(kParams, s);
    }
    EXPECT(d.ChargeHeld());
}

static void UntrackedHandDoesNotStartACharge() {
    Detector d;
    Sample s = InPose();
    s.tracked = false;
    d.Update(kParams, s);
    EXPECT(!d.ChargeHeld());
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

static Sample Ineligible() {
    Sample s = InPose();
    s.eligible = false;
    return s;
}

static void NewChargeClearsCancel() {
    Detector d;
    d.Update(kParams, InPose());
    d.Update(kParams, Ineligible());
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
    d.Update(kParams, Ineligible());
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

static const BodyYawParams kBody = {};

static void BodyStartsAtTheHead() {
    BodyYaw b;
    b.Update(kBody, 1.0f);
    EXPECT(b.Yaw() == 1.0f);
}

// A look to the side does not turn the body at once.
static void ShortLookToTheSideKeepsTheBody() {
    BodyYaw b;
    b.Update(kBody, 0.0f);
    for (int i = 0; i < 5; i++) {
        b.Update(kBody, 0.6f);
    }
    EXPECT(b.Yaw() < 0.15f);
}

// A large turn of the head pulls the body: the head is never more than 45 degrees from it.
static void LargeTurnPullsTheBody() {
    BodyYaw b;
    b.Update(kBody, 0.0f);
    b.Update(kBody, 1.5f);
    EXPECT(b.Yaw() >= 1.5f - kBody.maxOffsetRad - 0.001f);
}

// The player stays turned: the body follows slowly.
static void BodyFollowsAHeadThatStaysTurned() {
    BodyYaw b;
    b.Update(kBody, 0.0f);
    for (int i = 0; i < 200; i++) {
        b.Update(kBody, 0.6f);
    }
    EXPECT(b.Yaw() > 0.55f);
}

// From -170 to 170 degrees the head turns 20 degrees, not 340 degrees.
static void BodyFollowsAcrossTheWrap() {
    BodyYaw b;
    b.Update(kBody, -2.97f);
    for (int i = 0; i < 200; i++) {
        b.Update(kBody, 2.97f);
    }
    EXPECT(b.Yaw() > 2.9f || b.Yaw() < -3.1f);
}

int main() {
    HipPoseWithBladeBackIsChargePose();
    BladeForwardIsNotChargePose();
    BladeStraightDownIsNotChargePose();
    BladeLessFarBackIsChargePose();
    HandAtChestHeightIsNotChargePose();
    HandOnOtherSideIsNotChargePose();
    HandFarInFrontIsNotChargePose();
    StillHandInPoseChargesAtOnce();
    FastHandThroughPoseDoesNotCharge();
    SwingThroughPoseDoesNotCharge();
    IneligibleWeaponDoesNotCharge();
    HandMayMoveInsidePoseWhileCharging();
    FastSwingOutOfPoseReleases();
    ChargeStaysAnywhere();
    ArmedMoveKeepsTheCharge();
    UntrackedHandKeepsTheCharge();
    UntrackedHandDoesNotStartACharge();
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
    BodyStartsAtTheHead();
    ShortLookToTheSideKeepsTheBody();
    LargeTurnPullsTheBody();
    BodyFollowsAHeadThatStaysTurned();
    BodyFollowsAcrossTheWrap();

    if (sFailures == 0) {
        std::printf("VrSpinGesture: all tests passed\n");
        return 0;
    }
    std::printf("VrSpinGesture: %d failure(s)\n", sFailures);
    return 1;
}
