// Host unit tests for VrBowPull.h (pure logic, no game or OpenXR). The game build does not compile
// this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrBowPullTests.cpp -o /tmp/vr_bow_pull_tests
//   /tmp/vr_bow_pull_tests

#include "VrBowPull.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

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

// The string at rest is at the origin. stringCm: the distance of the string hand from it.
static Input At(float stringCm, bool grip, bool canPull = true) {
    Input in;
    in.canPull = canPull;
    in.gripHeld = grip;
    in.stringRest = { 0.0f, 0.0f, 0.0f };
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
    EXPECT(!p.Update(At(kGrabRadiusM * kUpm + 5.0f, false)).inReach);
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

static bool Near(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

static bool NearVec(const Vec3& a, const Vec3& b, float eps = 0.01f) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

// Game MtxF layout: a point p goes to p.x * mf[0] + p.y * mf[1] + p.z * mf[2] + mf[3].
static Vec3 Apply(const float mf[4][4], const Vec3& p) {
    return { p.x * mf[0][0] + p.y * mf[1][0] + p.z * mf[2][0] + mf[3][0],
             p.x * mf[0][1] + p.y * mf[1][1] + p.z * mf[2][1] + mf[3][1],
             p.x * mf[0][2] + p.y * mf[1][2] + p.z * mf[2][2] + mf[3][2] };
}

static void TestWorldToLocalUndoesTheStringMatrix() {
    // A turn of 90 degrees around Z, the model scale of Link, a mirror on X, and a translation.
    const float s = 0.01f;
    const float mf[4][4] = { { 0.0f, -s, 0.0f, 0.0f },
                             { -s, 0.0f, 0.0f, 0.0f },
                             { 0.0f, 0.0f, s, 0.0f },
                             { 50.0f, -20.0f, 7.0f, 1.0f } };
    const Vec3 local = { 120.0f, -900.0f, 40.0f };
    Vec3 back;
    EXPECT(WorldToLocal(mf, Apply(mf, local), &back));
    EXPECT(NearVec(back, local, 0.1f));
}

static void TestTheStringGoesToTheHand() {
    const float s = 0.01f;
    const float mf[4][4] = { { s, 0.0f, 0.0f, 0.0f },
                             { 0.0f, 0.0f, s, 0.0f },
                             { 0.0f, -s, 0.0f, 0.0f },
                             { 10.0f, 20.0f, 30.0f, 1.0f } };
    const Vec3 handWorld = { 12.0f, 25.0f, 18.0f };
    for (const Vec3& vertex : { kBowPulledVertex, kSlingshotPulledVertex }) {
        Vec3 handLocal;
        EXPECT(WorldToLocal(mf, handWorld, &handLocal));
        const Vec3 axis = StringPullAxis(vertex, handLocal);
        // The string matrix with the new Y axis: the pulled vertex is on the hand.
        float pulled[4][4];
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                pulled[r][c] = mf[r][c];
            }
        }
        for (int c = 0; c < 3; c++) {
            pulled[1][c] = axis.x * mf[0][c] + axis.y * mf[1][c] + axis.z * mf[2][c];
        }
        EXPECT(NearVec(Apply(pulled, vertex), handWorld, 0.01f));
        // The ends of the string (y = 0) do not move.
        const Vec3 end = { 300.0f, 0.0f, -40.0f };
        EXPECT(NearVec(Apply(pulled, end), Apply(mf, end), 0.001f));
    }
}

static void TestTheShotGoesFromTheHandThroughTheString() {
    Vec3 dir;
    EXPECT(AimFromHand({ 0.0f, 10.0f, 0.0f }, { 0.0f, 10.0f, -30.0f }, 1.0f, &dir));
    EXPECT(NearVec(dir, { 0.0f, 0.0f, 1.0f }));
    // A move of the hand down turns the shot up.
    EXPECT(AimFromHand({ 0.0f, 10.0f, 0.0f }, { 0.0f, 0.0f, -10.0f }, 1.0f, &dir));
    EXPECT(dir.y > 0.7f && dir.z > 0.7f);
    // A hand on the string gives no direction.
    EXPECT(!AimFromHand({ 0.0f, 10.0f, 0.0f }, { 0.0f, 10.0f, 0.5f }, 1.0f, &dir));
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
    TestWorldToLocalUndoesTheStringMatrix();
    TestTheStringGoesToTheHand();
    TestTheShotGoesFromTheHandThroughTheString();
    if (sFailures == 0) {
        std::printf("VrBowPull: all tests passed\n");
        return 0;
    }
    std::printf("VrBowPull: %d failures\n", sFailures);
    return 1;
}
