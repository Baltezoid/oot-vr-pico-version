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
    EXPECT(ShieldYieldsToMeleeWeapon(true, true));
}

// The shield alone touches the ball. The shield hit counts, as in the vanilla game.
static void ShieldAloneCounts() {
    EXPECT(!ShieldYieldsToMeleeWeapon(true, false));
}

// The rule applies only to the physical shield. Other attacks, the melee weapon included, keep
// the vanilla rule: the last hit replaces the earlier hit.
static void OtherAttacksAreNotChanged() {
    EXPECT(!ShieldYieldsToMeleeWeapon(false, true));
    EXPECT(!ShieldYieldsToMeleeWeapon(false, false));
}

int main() {
    ShieldAfterMeleeWeaponIsIgnored();
    ShieldAloneCounts();
    OtherAttacksAreNotChanged();

    if (sFailures == 0) {
        std::printf("VrShieldHit: all tests passed\n");
        return 0;
    }
    std::printf("VrShieldHit: %d failure(s)\n", sFailures);
    return 1;
}
