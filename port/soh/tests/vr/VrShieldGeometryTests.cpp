// Host unit tests for VrShieldGeometry.h (pure math, no game or OpenXR). The game build does
// not compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrShieldGeometryTests.cpp -o /tmp/vr_shield_tests
//   /tmp/vr_shield_tests

#include "VrShieldGeometry.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace VrShieldGeometry;

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

static bool Near(float a, float b, float eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

static const Vec3 kAxW = { 1, 0, 0 };
static const Vec3 kAxH = { 0, 1, 0 };
static const Vec3 kAxN = { 0, 0, 1 };

// Is (w, h) inside the fitted trapezoid (in the collider's own axes)?
static bool Covers(const TrapezoidFit& f, float w, float h) {
    const float t = (h - (f.centerH - f.halfH)) / (2.0f * f.halfH);
    if (t < -1e-4f || t > 1.0f + 1e-4f) {
        return false;
    }
    const float halfW = f.halfWBottom + (f.halfWTop - f.halfWBottom) * t;
    return std::fabs(w - f.centerW) <= halfW + 1e-3f;
}

static void TestFitRectangle() {
    // A flat 40 x 60 rectangle at depth 5, centered at (10, -20).
    std::vector<Vec3> pts = { { -10, -50, 5 }, { 30, -50, 5 }, { -10, 10, 5 }, { 30, 10, 5 } };
    TrapezoidFit f = FitTrapezoid(pts.data(), (int)pts.size(), kAxW, kAxH, kAxN);
    EXPECT(f.valid);
    EXPECT(Near(f.centerW, 10));
    EXPECT(Near(f.centerH, -20));
    EXPECT(Near(f.centerN, 5));
    EXPECT(Near(f.halfH, 30));
    EXPECT(Near(f.halfWTop, 20));
    EXPECT(Near(f.halfWBottom, 20));
}

static void TestFitTaperedKeepsTaper() {
    // Hylian-style outline: wide top, narrow pointed bottom.
    std::vector<Vec3> pts = { { -20, 20, 0 }, { 20, 20, 0 }, { -18, 0, 0 }, { 18, 0, 0 }, { -6, -20, 0 },
                              { 6, -20, 0 } };
    TrapezoidFit f = FitTrapezoid(pts.data(), (int)pts.size(), kAxW, kAxH, kAxN);
    EXPECT(f.valid);
    EXPECT(f.halfWTop > f.halfWBottom);
    for (const Vec3& p : pts) {
        EXPECT(Covers(f, p.x, p.y));
    }
}

static void TestFitCoversRoundOutline() {
    // A round shield (Deku): the widest points are at mid height. A plain "max width per half"
    // trapezoid would cut them off; the fit must still cover every vertex.
    std::vector<Vec3> pts;
    for (int i = 0; i < 32; i++) {
        const float a = (float)i / 32.0f * 6.2831853f;
        pts.push_back({ 15.0f * std::cos(a), 15.0f * std::sin(a), 1.0f });
    }
    TrapezoidFit f = FitTrapezoid(pts.data(), (int)pts.size(), kAxW, kAxH, kAxN);
    EXPECT(f.valid);
    EXPECT(Near(f.halfH, 15.0f, 0.1f));
    for (const Vec3& p : pts) {
        EXPECT(Covers(f, p.x, p.y));
    }
}

static void TestFitUsesColliderAxes() {
    // The shield plane is the hand's YZ plane: width along Z, height along -X, normal along Y.
    const Vec3 axW = { 0, 0, 1 };
    const Vec3 axH = { -1, 0, 0 };
    const Vec3 axN = { 0, 1, 0 };
    std::vector<Vec3> pts = { { 0, 3, -10 }, { 0, 3, 10 }, { -30, 3, -10 }, { -30, 3, 10 } };
    TrapezoidFit f = FitTrapezoid(pts.data(), (int)pts.size(), axW, axH, axN);
    EXPECT(f.valid);
    EXPECT(Near(f.halfWTop, 10));
    EXPECT(Near(f.halfH, 15));
    EXPECT(Near(f.centerH, 15));
    EXPECT(Near(f.centerN, 3));
}

static void TestFitRejectsDegenerateInput() {
    std::vector<Vec3> line = { { 0, 0, 0 }, { 0, 10, 0 }, { 0, 20, 0 } };
    EXPECT(!FitTrapezoid(line.data(), (int)line.size(), kAxW, kAxH, kAxN).valid);
    EXPECT(!FitTrapezoid(line.data(), 0, kAxW, kAxH, kAxN).valid);
}

static void TestQuadFrameNormalPointsAwayFromChest() {
    // Shield quad in the plane z = 20, chest at the origin: the outward normal is +Z.
    const Vec3 quad[4] = { { -5, -5, 20 }, { 5, -5, 20 }, { -5, 5, 20 }, { 5, 5, 20 } };
    Vec3 center, normal;
    EXPECT(QuadFrame(quad, { 0, 0, 0 }, &center, &normal));
    EXPECT(Near(center.z, 20) && Near(center.x, 0) && Near(center.y, 0));
    EXPECT(Near(normal.z, 1));

    // Same quad, chest on the other side: the normal flips.
    EXPECT(QuadFrame(quad, { 0, 0, 40 }, &center, &normal));
    EXPECT(Near(normal.z, -1));
}

static void TestFacesPointCone() {
    const Vec3 center = { 0, 0, 0 };
    const Vec3 normal = { 0, 0, 1 };
    EXPECT(FacesPoint(center, normal, { 0, 0, 100 }, 45.0f));
    EXPECT(FacesPoint(center, normal, { 90, 0, 100 }, 45.0f));  // ~42 degrees
    EXPECT(!FacesPoint(center, normal, { 110, 0, 100 }, 45.0f)); // ~48 degrees
    EXPECT(FacesPoint(center, normal, { 100, 0, 1 }, 90.0f));   // just in front of the plane
    EXPECT(!FacesPoint(center, normal, { 100, 0, -1 }, 90.0f)); // just behind the plane
    EXPECT(FacesPoint(center, normal, { 0, 0, -100 }, 180.0f)); // 180 = any direction
}

// A 20 x 30 shield quad in the plane z = 0, in the vanilla zigzag order.
static const Vec3 kShield[4] = { { -10, 0, 0 }, { 10, 0, 0 }, { -10, 30, 0 }, { 10, 30, 0 } };

static void TestQuadTouchesSphereInFront() {
    EXPECT(QuadTouchesSphere(kShield, { 0, 15, 19 }, 20.0f));  // in front of the face
    EXPECT(!QuadTouchesSphere(kShield, { 0, 15, 21 }, 20.0f)); // just out of reach
    EXPECT(QuadTouchesSphere(kShield, { 0, 15, -19 }, 20.0f)); // behind the face also touches
}

static void TestQuadTouchesSphereAtEdgeAndCorner() {
    EXPECT(QuadTouchesSphere(kShield, { 29, 15, 0 }, 20.0f));   // 19 past the right edge
    EXPECT(!QuadTouchesSphere(kShield, { 31, 15, 0 }, 20.0f));  // 21 past the right edge
    EXPECT(QuadTouchesSphere(kShield, { 20, 40, 10 }, 20.0f));  // corner at ~17.3
    EXPECT(!QuadTouchesSphere(kShield, { 23, 43, 10 }, 20.0f)); // corner at ~20.9
}

// Phantom Ganon's ball has a hit cylinder of radius 40. The shield is 30 from the center: it
// touches the cylinder, but not the visible ball (radius 20).
static void TestShieldInCylinderButOutsideVisibleBall() {
    EXPECT(!QuadTouchesSphere(kShield, { 0, 15, 30 }, 20.0f));
    EXPECT(QuadTouchesSphere(kShield, { 0, 15, 30 }, 40.0f));
}

int main() {
    TestFitRectangle();
    TestFitTaperedKeepsTaper();
    TestFitCoversRoundOutline();
    TestFitUsesColliderAxes();
    TestFitRejectsDegenerateInput();
    TestQuadFrameNormalPointsAwayFromChest();
    TestFacesPointCone();
    TestQuadTouchesSphereInFront();
    TestQuadTouchesSphereAtEdgeAndCorner();
    TestShieldInCylinderButOutsideVisibleBall();
    if (sFailures == 0) {
        std::printf("All VrShieldGeometry tests passed.\n");
    }
    return sFailures == 0 ? 0 : 1;
}
