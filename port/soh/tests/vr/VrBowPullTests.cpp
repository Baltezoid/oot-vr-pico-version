// Host unit tests for VrBowPull.h (pure logic, no game or OpenXR). The game build does not compile
// this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrBowPullTests.cpp -o /tmp/vr_bow_pull_tests
//   /tmp/vr_bow_pull_tests

#include "VrBowPull.h"

#include <cstdio>

using namespace VrBowPull;

static int sFailures = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                   \
        }                                                                  \
    } while (0)

// 1 unit is 1 cm: the distances in the tests are easy to read.
constexpr float kUpm = 100.0f;

// The bow hand is at the origin. stringCm: the distance of the string hand from the bow hand.
static Input At(float stringCm, bool grip, bool canPull = true) {
    Input in;
    in.canPull = canPull;
    in.gripHeld = grip;
    in.bowHand = { 0.0f, 0.0f, 0.0f };
    in.stringHand = { stringCm, 0.0f, 0.0f };
    in.unitsPerMeter = kUpm;
    return in;
}

static void TestTheGripNearTheBowTakesTheString() {
    Pull p;
    EXPECT(p.Update(At(10.0f, false)).inReach);
    const Result r = p.Update(At(10.0f, true));
    EXPECT(r.event == Event::Grab);
    EXPECT(r.holding);
    EXPECT(!r.itemButtonHeld);
}

static void TestTheGripFarFromTheBowDoesNotTakeTheString() {
    Pull p;
    EXPECT(!p.Update(At(60.0f, false)).inReach);
    for (int i = 0; i < 20; i++) {
        const Result r = p.Update(At(60.0f, true));
        EXPECT(r.event == Event::None);
        EXPECT(!r.holding);
    }
    // The hand comes to the bow after the grace time with the grip still held: no grab.
    EXPECT(p.Update(At(10.0f, true)).event == Event::None);
    // A new push of the grip takes the string.
    p.Update(At(10.0f, false));
    EXPECT(p.Update(At(10.0f, true)).event == Event::Grab);
}

static void TestAGripJustBeforeTheHandGetsToTheBowTakesTheString() {
    Pull p;
    p.Update(At(60.0f, false));
    for (int i = 0; i < kGripGraceTicks - 1; i++) {
        EXPECT(p.Update(At(40.0f, true)).event == Event::None);
    }
    EXPECT(p.Update(At(10.0f, true)).event == Event::Grab);
}

static void TestThePullHoldsTheItemButton() {
    Pull p;
    p.Update(At(10.0f, false));
    p.Update(At(10.0f, true));
    // Less than the minimum pull: the arrow is not on the string.
    Result r = p.Update(At(10.0f + kMinPullM * kUpm - 1.0f, true));
    EXPECT(r.event == Event::None);
    EXPECT(!r.itemButtonHeld);
    r = p.Update(At(10.0f + kMinPullM * kUpm + 1.0f, true));
    EXPECT(r.event == Event::Drawn);
    EXPECT(r.itemButtonHeld);
    // The hand goes forward again: the string stays pulled until the release.
    r = p.Update(At(10.0f, true));
    EXPECT(r.event == Event::None);
    EXPECT(r.itemButtonHeld);
}

static void TestTheReleaseOfAPulledStringShoots() {
    Pull p;
    p.Update(At(10.0f, false));
    p.Update(At(10.0f, true));
    p.Update(At(40.0f, true));
    const Result r = p.Update(At(40.0f, false));
    EXPECT(r.event == Event::Shoot);
    EXPECT(!r.itemButtonHeld);
    EXPECT(!r.holding);
}

static void TestTheReleaseWithoutAPullDoesNotShoot() {
    Pull p;
    p.Update(At(10.0f, false));
    p.Update(At(10.0f, true));
    const Result r = p.Update(At(12.0f, false));
    EXPECT(r.event == Event::LetGo);
    EXPECT(!r.itemButtonHeld);
    EXPECT(!r.holding);
}

static void TestThePullUsesTheWorldScale() {
    // Child Link: fewer units for each meter. The same real pull draws the string.
    Pull p;
    Input in = At(5.0f, false);
    in.unitsPerMeter = 50.0f;
    p.Update(in);
    in.gripHeld = true;
    EXPECT(p.Update(in).event == Event::Grab);
    in.stringHand.x = 5.0f + (kMinPullM + 0.01f) * 50.0f;
    EXPECT(p.Update(in).event == Event::Drawn);
}

static void TestNoPullWhenTheBowIsNotReady() {
    Pull p;
    p.Update(At(10.0f, false, false));
    Result r = p.Update(At(10.0f, true, false));
    EXPECT(r.event == Event::None);
    EXPECT(!r.inReach);
    // The bow becomes ready while the grip is held: the player must push the grip again.
    r = p.Update(At(10.0f, true));
    EXPECT(r.event == Event::None);
}

static void TestTheStringGoesWhenTheBowIsPutAway() {
    Pull p;
    p.Update(At(10.0f, false));
    p.Update(At(10.0f, true));
    p.Update(At(40.0f, true));
    const Result r = p.Update(At(40.0f, true, false));
    EXPECT(r.event == Event::LetGo);
    EXPECT(!r.itemButtonHeld);
    EXPECT(!r.holding);
}

static void TestTheHandStaysInReachWhileItHoldsTheString() {
    Pull p;
    p.Update(At(10.0f, false));
    p.Update(At(10.0f, true));
    // inReach tells the input that the grip belongs to the string.
    EXPECT(p.Update(At(70.0f, true)).inReach);
}

int main() {
    TestTheGripNearTheBowTakesTheString();
    TestTheGripFarFromTheBowDoesNotTakeTheString();
    TestAGripJustBeforeTheHandGetsToTheBowTakesTheString();
    TestThePullHoldsTheItemButton();
    TestTheReleaseOfAPulledStringShoots();
    TestTheReleaseWithoutAPullDoesNotShoot();
    TestThePullUsesTheWorldScale();
    TestNoPullWhenTheBowIsNotReady();
    TestTheStringGoesWhenTheBowIsPutAway();
    TestTheHandStaysInReachWhileItHoldsTheString();
    if (sFailures == 0) {
        std::printf("VrBowPull: all tests passed\n");
        return 0;
    }
    std::printf("VrBowPull: %d failures\n", sFailures);
    return 1;
}
