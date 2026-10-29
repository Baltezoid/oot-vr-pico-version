#pragma once

// The string pull of the bow and the slingshot (VrBowPull.cpp, #150). The off hand holds the
// weapon. The sword-hand grip takes the string near the weapon, the hand pulls it back, and the
// release of the grip shoots. No game types. Tests: port/soh/tests/vr/VrBowPullTests.cpp.

#include <cmath>

namespace VrBowPull {

struct Vec3 {
    float x, y, z;
};

// The string hand takes the string at this distance from the bow hand.
constexpr float kGrabRadiusM = 0.20f;
// After the grab, the hand must move this distance away from the bow hand. Then the arrow is on
// the string. A smaller pull does not shoot.
constexpr float kMinPullM = 0.10f;
// A grip that is pushed this number of ticks (0.3 s) before the hand gets to the bow takes the
// string.
constexpr int kGripGraceTicks = 6;

struct Input {
    // The weapon is in the hand and the pull is on.
    bool canPull = false;
    // The grip of the string hand.
    bool gripHeld = false;
    Vec3 bowHand = {};
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
};

inline float Distance(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
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
            return r;
        }

        const float dist = Distance(in.bowHand, in.stringHand);
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
                r.event = Event::Shoot;
            }
        }

        r.holding = (mState == State::Holding) || (mState == State::Drawn);
        r.itemButtonHeld = (mState == State::Drawn);
        r.inReach = r.holding || near;
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
    float mGrabDist = 0.0f;
};

} // namespace VrBowPull
