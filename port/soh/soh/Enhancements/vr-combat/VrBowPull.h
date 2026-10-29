#pragma once

// String pull of the bow and the slingshot (#150). No game types. Game side: VrBowPull.cpp.
// Tests: port/soh/tests/vr/VrBowPullTests.cpp.

#include "VrBowAim.h"

#include <cmath>

namespace VrBowPull {

using Vec3 = VrBowAim::Vec3;

// Grab distance from the string at rest.
constexpr float kGrabRadiusM = 0.25f;
// Pull after the grab that nocks the arrow. A release before it does not shoot.
constexpr float kMinPullM = 0.10f;
// Ticks (0.5 s) that the shot keeps the pull aim after the release. The game shoots 1 or more
// ticks late.
constexpr int kKeepShotTicks = 10;
// A nock nearer to the arrow rest gives no direction.
constexpr float kMinAimM = 0.03f;
// Ticks (0.3 s) that a grip pushed before the hand gets to the string still grabs.
constexpr int kGripGraceTicks = 6;

struct Input {
    bool canPull = false;  // The bow or the slingshot is in the hand.
    bool gripHeld = false; // Grip of the string hand.
    Vec3 stringRest = {};  // Center of the string at rest.
    Vec3 stringHand = {};
    float unitsPerMeter = 1.0f;
};

enum class Event {
    None,
    Grab,
    Drawn, // The arrow is on the string.
    Shoot,
    LetGo, // Release without a shot.
};

struct Result {
    Event event = Event::None;
    bool inReach = false;        // Near the string, or holding it. The grip belongs to the string.
    bool holding = false;
    bool itemButtonHeld = false; // The game shoots when it is released.
    bool keepShot = false;       // After a shot: keep the pull aim (kKeepShotTicks).
};

inline float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Pulled vertex of the string model at a full draw, in string space (VrBowAim.h).
constexpr Vec3 kBowPulledVertex = { VrBowAim::kNockAtRest.x, VrBowAim::kNockFullDrawY, VrBowAim::kNockAtRest.z };
constexpr Vec3 kSlingshotPulledVertex = VrBowAim::kPouchFullDraw;

// Where the shot crosses the weapon, in string space: on the vanilla arrow line, at y = 0.
inline Vec3 ArrowRest(const Vec3& pulledVertex) {
    return { pulledVertex.x, 0.0f, pulledVertex.z };
}

// Longest pull in world units: the vanilla full draw. Longer, the arrow is behind the bow.
// stringYAxis: mf[1] of the string matrix.
inline float MaxPull(const Vec3& pulledVertex, const float stringYAxis[3]) {
    return std::fabs(pulledVertex.y) * std::sqrt(stringYAxis[0] * stringYAxis[0] + stringYAxis[1] * stringYAxis[1] +
                                                 stringYAxis[2] * stringYAxis[2]);
}

// The hand, at most maxLen from the arrow rest.
inline Vec3 NockPos(const Vec3& arrowRest, const Vec3& hand, float maxLen) {
    const float len = Distance(arrowRest, hand);
    if (len <= maxLen) {
        return hand;
    }
    const float k = maxLen / len;
    return { arrowRest.x + (hand.x - arrowRest.x) * k, arrowRest.y + (hand.y - arrowRest.y) * k,
             arrowRest.z + (hand.z - arrowRest.z) * k };
}

// mf: game MtxF layout (mf[3] is the translation).
inline Vec3 LocalToWorld(const float mf[4][4], const Vec3& p) {
    return { p.x * mf[0][0] + p.y * mf[1][0] + p.z * mf[2][0] + mf[3][0],
             p.x * mf[0][1] + p.y * mf[1][1] + p.z * mf[2][1] + mf[3][1],
             p.x * mf[0][2] + p.y * mf[1][2] + p.z * mf[2][2] + mf[3][2] };
}

// False when mf has no inverse.
inline bool WorldToLocal(const float mf[4][4], const Vec3& p, Vec3* out) {
    const float a = mf[0][0], b = mf[0][1], c = mf[0][2];
    const float d = mf[1][0], e = mf[1][1], f = mf[1][2];
    const float g = mf[2][0], h = mf[2][1], i = mf[2][2];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1.0e-12f) {
        return false;
    }
    const float x = p.x - mf[3][0];
    const float y = p.y - mf[3][1];
    const float z = p.z - mf[3][2];
    // Cramer's rule.
    out->x = (x * (e * i - f * h) - y * (d * i - f * g) + z * (d * h - e * g)) / det;
    out->y = (a * (y * i - z * h) - b * (x * i - z * g) + c * (x * h - y * g)) / det;
    out->z = (a * (e * z - f * y) - b * (d * z - f * x) + c * (d * y - e * x)) / det;
    return true;
}

// New Y axis of the string matrix: the pulled vertex goes to the hand, the ends (y = 0) stay.
inline Vec3 StringPullAxis(const Vec3& pulledVertex, const Vec3& handLocal) {
    return { (handLocal.x - pulledVertex.x) / pulledVertex.y, handLocal.y / pulledVertex.y,
             (handLocal.z - pulledVertex.z) / pulledVertex.y };
}

// Direction from the nock through the arrow rest.
inline bool AimFromHand(const Vec3& arrowRest, const Vec3& nock, float minDist, Vec3* dir) {
    const float len = Distance(arrowRest, nock);
    if (len < minDist) {
        return false;
    }
    *dir = { (arrowRest.x - nock.x) / len, (arrowRest.y - nock.y) / len, (arrowRest.z - nock.z) / len };
    return true;
}

class Pull {
  public:
    Result Update(const Input& in) {
        Result r;
        const bool pushed = in.gripHeld && !mGripPrev;
        mGripPrev = in.gripHeld;

        if (!in.canPull) {
            if (mState == State::Holding || mState == State::Drawn) {
                r.event = Event::LetGo;
            }
            // A grip held now cannot grab later.
            mState = in.gripHeld ? State::Blocked : State::Idle;
            mKeepTicks = 0;
            return r;
        }

        const float dist = Distance(in.stringRest, in.stringHand);
        const bool near = dist <= kGrabRadiusM * in.unitsPerMeter;

        switch (mState) {
            case State::Idle:
                if (pushed) {
                    mState = State::Waiting;
                    mGraceTicks = kGripGraceTicks;
                }
                break;
            case State::Blocked:
                if (!in.gripHeld) {
                    mState = State::Idle;
                }
                break;
            case State::Waiting:
            case State::Holding:
            case State::Drawn:
                break;
        }

        if (mState == State::Waiting) {
            if (!in.gripHeld) {
                mState = State::Idle;
            } else if (near) {
                mState = State::Holding;
                mGrabDist = dist;
                mKeepTicks = 0;
                r.event = Event::Grab;
            } else if (--mGraceTicks <= 0) {
                mState = State::Blocked;
            }
        } else if (mState == State::Holding) {
            if (!in.gripHeld) {
                mState = State::Idle;
                r.event = Event::LetGo;
            } else if (dist - mGrabDist >= kMinPullM * in.unitsPerMeter) {
                mState = State::Drawn;
                r.event = Event::Drawn;
            }
        } else if (mState == State::Drawn) {
            if (!in.gripHeld) {
                mState = State::Idle;
                mKeepTicks = kKeepShotTicks;
                r.event = Event::Shoot;
            }
        }

        r.holding = (mState == State::Holding) || (mState == State::Drawn);
        r.itemButtonHeld = (mState == State::Drawn);
        r.inReach = r.holding || near;
        r.keepShot = mKeepTicks > 0;
        if (mKeepTicks > 0) {
            mKeepTicks--;
        }
        return r;
    }

  private:
    enum class State {
        Idle,
        Waiting, // Grip pushed, kGripGraceTicks to reach the string.
        Blocked, // Grip missed. Release it first.
        Holding,
        Drawn,
    };

    State mState = State::Idle;
    bool mGripPrev = false;
    int mGraceTicks = 0;
    int mKeepTicks = 0;
    float mGrabDist = 0.0f;
};

} // namespace VrBowPull
