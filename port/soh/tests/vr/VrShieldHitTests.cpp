// Host unit tests for VrShieldHit.h (pure rule, no game or OpenXR). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrShieldHitTests.cpp -o /tmp/vr_shield_hit_tests
//   /tmp/vr_shield_hit_tests

#include "VrShieldHit.h"

#include <cstdio>

using namespace VrShieldHit;

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

// Phantom Ganon's ball: a melee weapon hits it, then the shield hits it in the same frame. The
// melee weapon hit stays, so the ball goes back.
static void ShieldAfterMeleeWeaponIsIgnored() {
    EXPECT(ShieldHitIgnored(true, true, true, true));
    EXPECT(ShieldHitIgnored(true, true, false, false));
}

// The shield alone touches the visible ball. The shield hit counts, as in the vanilla game.
static void ShieldOnVisibleBallCounts() {
    EXPECT(!ShieldHitIgnored(true, false, true, true));
}

// The shield is in the hit cylinder of the ball, but it does not touch the visible ball. The
// shield hit does not count, so the melee weapon can still send the ball back.
static void ShieldOutsideVisibleBallIsIgnored() {
    EXPECT(ShieldHitIgnored(true, false, true, false));
}

// Other targets keep the vanilla shield hit.
static void ShieldOnOtherTargetCounts() {
    EXPECT(!ShieldHitIgnored(true, false, false, false));
}

// The rule applies only to the physical shield. Other attacks, the melee weapon included, keep
// the vanilla rule: the last hit replaces the earlier hit.
static void OtherAttacksAreNotChanged() {
    EXPECT(!ShieldHitIgnored(false, true, true, false));
    EXPECT(!ShieldHitIgnored(false, false, true, false));
    EXPECT(!ShieldHitIgnored(false, false, false, false));
}

int main() {
    ShieldAfterMeleeWeaponIsIgnored();
    ShieldOnVisibleBallCounts();
    ShieldOutsideVisibleBallIsIgnored();
    ShieldOnOtherTargetCounts();
    OtherAttacksAreNotChanged();

    if (sFailures == 0) {
        std::printf("VrShieldHit: all tests passed\n");
        return 0;
    }
    std::printf("VrShieldHit: %d failure(s)\n", sFailures);
    return 1;
}
