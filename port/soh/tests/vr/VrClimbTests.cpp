// Host unit tests for VrClimb.h (pure rule, no game or OpenXR). The game build does not compile
// this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrClimbTests.cpp -o /tmp/vr_climb_tests
//   /tmp/vr_climb_tests

#include "VrClimb.h"

#include <cstdio>

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

static const int16_t kHeadYaw = 0x2000;
static const int16_t kStickYaw = 0x6000;

// The player looks to the side and walks toward the wall. Link climbs (#136).
static void WalkingLinkUsesTheStickDirection() {
    EXPECT(VrClimb_WallCheckYaw(true, false, true, kHeadYaw, kStickYaw) == kStickYaw);
}

// Link stands still. The stick direction is old, thus the body direction applies.
static void StandingLinkUsesTheBodyDirection() {
    EXPECT(VrClimb_WallCheckYaw(true, false, false, kHeadYaw, kStickYaw) == kHeadYaw);
}

// Without the VR body that follows the headset, the vanilla rule applies.
static void VanillaUsesTheBodyDirection() {
    EXPECT(VrClimb_WallCheckYaw(false, false, true, kHeadYaw, kStickYaw) == kHeadYaw);
}

// During Lock On, the body faces the target. A strafe into a block does not start a climb.
static void LockOnUsesTheBodyDirection() {
    EXPECT(VrClimb_WallCheckYaw(true, true, true, kHeadYaw, kStickYaw) == kHeadYaw);
}

int main() {
    WalkingLinkUsesTheStickDirection();
    StandingLinkUsesTheBodyDirection();
    VanillaUsesTheBodyDirection();
    LockOnUsesTheBodyDirection();

    if (sFailures != 0) {
        std::printf("%d failure(s)\n", sFailures);
        return 1;
    }
    std::printf("All VrClimb tests passed\n");
    return 0;
}
