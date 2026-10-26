// Host unit tests for VrHammerTarget.h (pure rule, no game or OpenXR). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrHammerTargetTests.cpp -o /tmp/vr_hammer_target_tests
//   /tmp/vr_hammer_target_tests

#include "VrHammerTarget.h"

#include <cmath>
#include <cstdio>

using namespace VrHammerTarget;

static int sFailures = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                   \
        }                                                                  \
    } while (0)

static bool Near(float a, float b) {
    return std::fabs(a - b) < 0.01f;
}

// The collider of Bg_Hidan_Hrock: a flat 80 x 80 square, 3 units above the top of the pillar.
static const Tri kTop[2] = {
    { { -40.0f, 3.0f, -40.0f }, { -40.0f, 3.0f, 40.0f }, { 40.0f, 3.0f, 40.0f } },
    { { 40.0f, 3.0f, 40.0f }, { 40.0f, 3.0f, -40.0f }, { -40.0f, 3.0f, -40.0f } },
};

static void HitOnTheTopGoesToTheColliderAboveIt() {
    const Aim a = NearestOnTris(kTop, 2, { 10.0f, 0.0f, -5.0f });
    EXPECT(a.valid);
    EXPECT(Near(a.point.x, 10.0f));
    EXPECT(Near(a.point.y, 3.0f));
    EXPECT(Near(a.point.z, -5.0f));
}

static void HitOnTheFrontFaceGoesToTheTopEdge() {
    // The front face of the pillar is at z = 40. The hammer hits it 20 units below the top.
    const Aim a = NearestOnTris(kTop, 2, { 5.0f, -20.0f, 41.0f });
    EXPECT(a.valid);
    EXPECT(Near(a.point.y, 3.0f));
    EXPECT(a.point.z < 40.0f - 1.0f);
    EXPECT(a.point.z > 40.0f - 5.0f);
}

static void HitBesideTheSideEdgeGoesInside() {
    // The hit is beside the edge at x = 40. The strike line runs along that edge. On the edge, the
    // collision check of the game can miss it. Thus the point moves a little inside.
    const Aim a = NearestOnTris(kTop, 2, { 50.0f, -20.0f, 0.0f });
    EXPECT(a.valid);
    EXPECT(a.point.x < 40.0f - 1.0f);
    EXPECT(a.point.x > 40.0f - 5.0f);
    EXPECT(Near(a.point.y, 3.0f));
}

static void NormalIsUnitAndFacesTheHit() {
    const Aim above = NearestOnTris(kTop, 2, { 0.0f, 10.0f, 0.0f });
    EXPECT(above.valid);
    EXPECT(Near(above.normal.y, 1.0f));
    const Aim below = NearestOnTris(kTop, 2, { 0.0f, -10.0f, 0.0f });
    EXPECT(below.valid);
    EXPECT(Near(below.normal.y, -1.0f));
}

static void PicksTheNearestSide() {
    // Two vertical sides, as on the totem (Bg_Hidan_Dalm): x = 30 and x = -30.
    const Tri sides[2] = {
        { { 30.0f, 0.0f, -30.0f }, { 30.0f, 60.0f, -30.0f }, { 30.0f, 60.0f, 30.0f } },
        { { -30.0f, 0.0f, -30.0f }, { -30.0f, 60.0f, 30.0f }, { -30.0f, 60.0f, -30.0f } },
    };
    const Aim a = NearestOnTris(sides, 2, { -35.0f, 20.0f, 10.0f });
    EXPECT(a.valid);
    EXPECT(Near(a.point.x, -30.0f));
    EXPECT(Near(a.normal.x, -1.0f));
}

static void NoTrisIsInvalid() {
    EXPECT(!NearestOnTris(kTop, 0, { 0.0f, 0.0f, 0.0f }).valid);
}

static void SkipsADegenerateTri() {
    const Tri tris[2] = {
        { { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
        kTop[0],
    };
    const Aim a = NearestOnTris(tris, 2, { 0.0f, 0.0f, 0.0f });
    EXPECT(a.valid);
    EXPECT(Near(a.point.y, 3.0f));
}

int main() {
    HitOnTheTopGoesToTheColliderAboveIt();
    HitOnTheFrontFaceGoesToTheTopEdge();
    HitBesideTheSideEdgeGoesInside();
    NormalIsUnitAndFacesTheHit();
    PicksTheNearestSide();
    NoTrisIsInvalid();
    SkipsADegenerateTri();

    if (sFailures == 0) {
        std::printf("VrHammerTarget: all tests passed\n");
        return 0;
    }
    std::printf("VrHammerTarget: %d failure(s)\n", sFailures);
    return 1;
}
