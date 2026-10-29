#pragma once

// The string pull of the bow and the slingshot (VrBowPull.cpp, #150). The off hand holds the
// weapon. The sword-hand grip takes the string near the weapon, the hand pulls it back, and the
// release of the grip shoots. No game types. Tests: port/soh/tests/vr/VrBowPullTests.cpp.

#include "VrBowAim.h"

#include <cmath>

namespace VrBowPull {

using Vec3 = VrBowAim::Vec3;

// The string hand takes the string at this distance from the center of the string at rest.
constexpr float kGrabRadiusM = 0.25f;
// After the grab, the hand must move this distance away from the string at rest. Then the arrow
// is on the string. A smaller pull does not shoot.
constexpr float kMinPullM = 0.10f;
// The longest pull, from the arrow rest. A hand that goes farther does not pull the string more.
constexpr float kMaxPullM = 0.6f;
// After the release, the shot keeps the aim of the pull for this number of ticks (0.5 s). The
// vanilla bow path shoots one or more ticks after the release.
constexpr int kKeepShotTicks = 10;
// A hand nearer to the string at rest than this distance gives no shot direction.
constexpr float kMinAimM = 0.03f;
// A grip that is pushed this number of ticks (0.3 s) before the hand gets to the bow takes the
// string.
constexpr int kGripGraceTicks = 6;

struct Input {
    // The weapon is in the hand and the pull is on.
    bool canPull = false;
    // The grip of the string hand.
    bool gripHeld = false;
    // The center of the string at rest. Without a drawn string, the bow hand.
    Vec3 stringRest = {};
    Vec3 stringHand = {};
    float unitsPerMeter = 1.0f;
};

enum class Event {
    None,
    Grab,  // The hand takes the string.
    Drawn, // The pull is long enough. The arrow is on the string.
    Shoot, // The grip releases a pulled string.
    LetGo, // The string goes without a shot.
};

struct Result {
    Event event = Event::None;
    // The hand is near the bow, or it holds the string. The grip belongs to the string.
    bool inReach = false;
    // The hand holds the string.
    bool holding = false;
    // The button of the held item is held. The vanilla bow path shoots when it is released.
    bool itemButtonHeld = false;
    // The string was released after a pull. The shot keeps the aim of the pull.
    bool keepShot = false;
};

inline float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// The vertex of the string model that the hand pulls, in string space, at a full draw. The
// vanilla draw scales its Y (VrBowAim.h).
constexpr Vec3 kBowPulledVertex = { VrBowAim::kNockAtRest.x, VrBowAim::kNockFullDrawY, VrBowAim::kNockAtRest.z };
constexpr Vec3 kSlingshotPulledVertex = VrBowAim::kPouchFullDraw;

// The point of the weapon where the shot goes through, in string space: on the line of the
// vanilla arrow (the X and Z of the pulled vertex), at the ends of the string (y = 0).
inline Vec3 ArrowRest(const Vec3& pulledVertex) {
    return { pulledVertex.x, 0.0f, pulledVertex.z };
}

// The nock: the hand, but not farther than maxLen from the arrow rest.
inline Vec3 NockPos(const Vec3& arrowRest, const Vec3& hand, float maxLen) {
    const float len = Distance(arrowRest, hand);
    if (len <= maxLen) {
        return hand;
    }
    const float k = maxLen / len;
    return { arrowRest.x + (hand.x - arrowRest.x) * k, arrowRest.y + (hand.y - arrowRest.y) * k,
             arrowRest.z + (hand.z - arrowRest.z) * k };
}

// Returns p (world) in the space of mf (game MtxF layout, an affine matrix). False when mf has no
// inverse.
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
    // p - t = local.x * row0 + local.y * row1 + local.z * row2 (Cramer's rule).
    out->x = (x * (e * i - f * h) - y * (d * i - f * g) + z * (d * h - e * g)) / det;
    out->y = (a * (y * i - z * h) - b * (x * i - z * g) + c * (x * h - y * g)) / det;
    out->z = (a * (e * z - f * y) - b * (d * z - f * x) + c * (d * y - e * x)) / det;
    return true;
}

// The new Y axis of the string matrix (string space). With it, the pulled vertex is on the hand,
// and the ends of the string (y = 0) do not move.
inline Vec3 StringPullAxis(const Vec3& pulledVertex, const Vec3& handLocal) {
    return { (handLocal.x - pulledVertex.x) / pulledVertex.y, handLocal.y / pulledVertex.y,
             (handLocal.z - pulledVertex.z) / pulledVertex.y };
}

// The shot goes from the nock through the arrow rest. False when the nock is nearer than minDist.
inline bool AimFromHand(const Vec3& stringRest, const Vec3& hand, float minDist, Vec3* dir) {
    const float len = Distance(stringRest, hand);
    if (len < minDist) {
        return false;
    }
    *dir = { (stringRest.x - hand.x) / len, (stringRest.y - hand.y) / len, (stringRest.z - hand.z) / len };
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
            // A grip that is held now cannot take the string later.
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
        Idle,    // The grip is released.
        Waiting, // The grip is pushed. The hand has kGripGraceTicks to get to the bow.
        Blocked, // The grip did not take the string. It must be released first.
        Holding, // The hand holds the string.
        Drawn,   // The hand pulled the string.
    };

    State mState = State::Idle;
    bool mGripPrev = false;
    int mGraceTicks = 0;
    int mKeepTicks = 0;
    float mGrabDist = 0.0f;
};

} // namespace VrBowPull
