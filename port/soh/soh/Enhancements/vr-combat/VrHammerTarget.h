#pragma once

// Rule for a hammer hit on the geometry of an object. No game types: the host tests in
// port/soh/tests/vr/VrHammerTargetTests.cpp build it alone.
//
// Some Fire Temple objects get the hammer only on one flat collider: a pillar has a square on
// its top, and the totem has its two sides. A VR swing can hit another face of the object.
// VrSwing.cpp then puts the strike on the point of the collider that is nearest to the hit.

#include <cmath>

namespace VrHammerTarget {

struct Vec3 {
    float x, y, z;
};

struct Tri {
    Vec3 a, b, c;
};

struct Aim {
    bool valid;
    Vec3 point;  // on the collider
    Vec3 normal; // unit, on the side of the hit
};

inline Vec3 Sub(const Vec3& a, const Vec3& b) {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}
inline Vec3 Add(const Vec3& a, const Vec3& b) {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}
inline Vec3 Scale(const Vec3& a, float s) {
    return { a.x * s, a.y * s, a.z * s };
}
inline float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline Vec3 Cross(const Vec3& a, const Vec3& b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

// The point of triangle abc that is nearest to p (Ericson, Real-Time Collision Detection 5.1.5).
inline Vec3 ClosestOnTri(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c) {
    const Vec3 ab = Sub(b, a);
    const Vec3 ac = Sub(c, a);
    const Vec3 ap = Sub(p, a);
    const float d1 = Dot(ab, ap);
    const float d2 = Dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return a;
    }
    const Vec3 bp = Sub(p, b);
    const float d3 = Dot(ab, bp);
    const float d4 = Dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        return Add(a, Scale(ab, d1 / (d1 - d3)));
    }
    const Vec3 cp = Sub(p, c);
    const float d5 = Dot(ab, cp);
    const float d6 = Dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        return Add(a, Scale(ac, d2 / (d2 - d6)));
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        return Add(b, Scale(Sub(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    }
    const float denom = 1.0f / (va + vb + vc);
    return Add(a, Add(Scale(ab, vb * denom), Scale(ac, vc * denom)));
}

// The point of the collider triangles that is nearest to the hit. A triangle without an area
// does not count. Invalid when no triangle counts.
inline Aim NearestOnTris(const Tri* tris, int count, const Vec3& hit) {
    Aim best = {};
    float bestD2 = 0.0f;
    for (int i = 0; i < count; i++) {
        const Tri& t = tris[i];
        Vec3 n = Cross(Sub(t.b, t.a), Sub(t.c, t.a));
        const float len = std::sqrt(Dot(n, n));
        if (len < 1e-4f) {
            continue;
        }
        n = Scale(n, 1.0f / len);
        const Vec3 q = ClosestOnTri(hit, t.a, t.b, t.c);
        const Vec3 d = Sub(hit, q);
        const float d2 = Dot(d, d);
        if (!best.valid || d2 < bestD2) {
            best.valid = true;
            best.point = q;
            best.normal = Dot(d, n) < 0.0f ? Scale(n, -1.0f) : n;
            bestD2 = d2;
        }
    }
    return best;
}

} // namespace VrHammerTarget
