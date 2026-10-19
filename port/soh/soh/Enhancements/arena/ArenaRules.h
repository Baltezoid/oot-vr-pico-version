#pragma once

// Pure rules of ARENA (Arena.cpp): the curve of the waves, the wave director, and the spawn points. No game types, so
// the host tests in port/soh/tests/arena/ArenaRulesTests.cpp can build it alone.

#include <cmath>

namespace ArenaRules {

constexpr int kLevels = 4;
constexpr int kMaxAlive = 10;
// The game logic runs 20 times each second (R_UPDATE_RATE 3).
constexpr int kFramesPerSecond = 20;
constexpr int kPauseFrames = 10 * kFramesPerSecond;

// The enemies of a wave: the number of each level, and how many of them are Hyper. Index 0 is level 1.
struct WaveSpec {
    int count[kLevels];
    int hyper[kLevels];

    int Total() const {
        return count[0] + count[1] + count[2] + count[3];
    }
};

// The curve of issue #115, one row for each wave. From wave 22, all waves are the last row.
constexpr WaveSpec kCurve[] = {
    { { 4, 0, 0, 0 }, { 0, 0, 0, 0 } },   // 1
    { { 5, 0, 0, 0 }, { 0, 0, 0, 0 } },   // 2
    { { 6, 0, 0, 0 }, { 0, 0, 0, 0 } },   // 3
    { { 4, 3, 0, 0 }, { 0, 0, 0, 0 } },   // 4
    { { 4, 4, 0, 0 }, { 0, 0, 0, 0 } },   // 5
    { { 3, 6, 0, 0 }, { 0, 0, 0, 0 } },   // 6
    { { 3, 4, 3, 0 }, { 1, 1, 0, 0 } },   // 7
    { { 3, 4, 4, 0 }, { 2, 1, 0, 0 } },   // 8
    { { 3, 4, 5, 0 }, { 2, 1, 1, 0 } },   // 9
    { { 3, 4, 5, 1 }, { 3, 2, 1, 0 } },   // 10
    { { 3, 5, 5, 1 }, { 3, 3, 2, 0 } },   // 11
    { { 3, 5, 6, 1 }, { 3, 3, 2, 0 } },   // 12
    { { 3, 5, 7, 1 }, { 3, 4, 3, 0 } },   // 13
    { { 3, 6, 7, 1 }, { 3, 5, 4, 1 } },   // 14
    { { 3, 6, 8, 1 }, { 3, 5, 6, 1 } },   // 15
    { { 3, 6, 8, 2 }, { 3, 6, 6, 1 } },   // 16
    { { 3, 6, 9, 2 }, { 3, 6, 8, 1 } },   // 17
    { { 3, 7, 9, 2 }, { 3, 7, 9, 1 } },   // 18
    { { 3, 7, 10, 2 }, { 3, 7, 10, 2 } }, // 19
    { { 3, 7, 11, 2 }, { 3, 7, 11, 2 } }, // 20
    { { 3, 8, 11, 2 }, { 3, 8, 11, 2 } }, // 21
    { { 3, 8, 12, 2 }, { 3, 8, 12, 2 } }, // 22 and more
};
constexpr int kCurveWaves = sizeof(kCurve) / sizeof(kCurve[0]);

inline WaveSpec GetWave(int wave) {
    if (wave < 1) {
        wave = 1;
    }
    return kCurve[(wave > kCurveWaves ? kCurveWaves : wave) - 1];
}

struct SpawnRequest {
    int level; // 0 to 3
    bool hyper;
};

// Starts the waves, and tells the game when to spawn an enemy. The game calls Tick one time each game frame.
class Director {
  public:
    void Start() {
        StartAt(1);
    }

    // Starts at a later wave. For the tests.
    void StartAt(int wave) {
        mWave = wave;
        BeginPause();
    }

    // The scene loaded again, and the enemies of the wave are gone. The wave starts again after a pause.
    void RestartWave() {
        BeginPause();
    }

    int Wave() const {
        return mWave;
    }

    bool InPause() const {
        return mPauseFramesLeft > 0;
    }

    // Rounds up, so the countdown shows 10, 9, ..., 1.
    int PauseSecondsLeft() const {
        return (mPauseFramesLeft + kFramesPerSecond - 1) / kFramesPerSecond;
    }

    // alive: the number of wave enemies that are alive now. Returns true and sets request when the game must spawn
    // an enemy in this frame. The game spawns a maximum of one enemy in each frame.
    bool Tick(int alive, SpawnRequest* request) {
        if (mPauseFramesLeft > 0) {
            mPauseFramesLeft--;
            if (mPauseFramesLeft == 0) {
                BeginWave();
            }
            return false;
        }

        if (Remaining() == 0) {
            if (alive == 0) {
                mWave++;
                BeginPause();
            }
            return false;
        }

        if (alive >= kMaxAlive) {
            return false;
        }

        *request = Next();
        return true;
    }

    // The game could not spawn the enemy of the request. The director asks for it again in a later frame.
    void Requeue(const SpawnRequest& request) {
        mPending[request.level]++;
        if (request.hyper) {
            mPendingHyper[request.level]++;
        }
    }

  private:
    void BeginPause() {
        mPauseFramesLeft = kPauseFrames;
        for (int level = 0; level < kLevels; level++) {
            mPending[level] = 0;
            mPendingHyper[level] = 0;
        }
    }

    void BeginWave() {
        WaveSpec spec = GetWave(mWave);
        for (int level = 0; level < kLevels; level++) {
            mPending[level] = spec.count[level];
            mPendingHyper[level] = spec.hyper[level];
        }
        mNextLevel = 0;
    }

    int Remaining() const {
        return mPending[0] + mPending[1] + mPending[2] + mPending[3];
    }

    // Takes the levels in turn, so the enemies of each level come during all the wave. In a level, the normal enemies
    // come first and the Hyper enemies last.
    SpawnRequest Next() {
        while (mPending[mNextLevel] == 0) {
            mNextLevel = (mNextLevel + 1) % kLevels;
        }
        SpawnRequest request;
        request.level = mNextLevel;
        request.hyper = mPendingHyper[mNextLevel] == mPending[mNextLevel];
        mPending[mNextLevel]--;
        if (request.hyper) {
            mPendingHyper[mNextLevel]--;
        }
        mNextLevel = (mNextLevel + 1) % kLevels;
        return request;
    }

    int mWave = 0;
    int mPauseFramesLeft = 0;
    int mPending[kLevels] = {};
    int mPendingHyper[kLevels] = {};
    int mNextLevel = 0;
};

// A point on the floor plane: x and z of the game.
struct Point {
    float x;
    float z;
};

inline float DistanceBetween(Point a, Point b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
}

// Selects a random point in the circle of the arena, minDistance to maxDistance from Link. First it tries points in
// the ring around Link. If no point of the ring is in the arena, it uses the point of the arena that is nearest to the
// ring. random returns values in [0, 1).
template <typename Random>
Point PickSpawnPoint(Point center, float radius, Point link, float minDistance, float maxDistance, Random& random) {
    constexpr int kTries = 16;
    constexpr float kTwoPi = 6.2831853f;

    for (int i = 0; i < kTries; i++) {
        float r = minDistance + (maxDistance - minDistance) * random();
        float angle = kTwoPi * random();
        Point point{ link.x + r * std::cos(angle), link.z + r * std::sin(angle) };
        if (DistanceBetween(point, center) <= radius) {
            return point;
        }
    }

    Point best = center;
    float bestError = -1.0f;
    for (int i = 0; i < kTries; i++) {
        // The square root gives the same density in all the circle.
        float r = radius * std::sqrt(random());
        float angle = kTwoPi * random();
        Point point{ center.x + r * std::cos(angle), center.z + r * std::sin(angle) };
        float distance = DistanceBetween(point, link);
        float error =
            distance < minDistance ? minDistance - distance : (distance > maxDistance ? distance - maxDistance : 0.0f);
        if (bestError < 0.0f || error < bestError) {
            best = point;
            bestError = error;
        }
    }
    return best;
}

// Moves a point that is outside the circle back to the edge of the circle.
inline Point KeepInside(Point center, float radius, Point point) {
    float distance = DistanceBetween(point, center);
    if (distance <= radius) {
        return point;
    }
    float scale = radius / distance;
    return Point{ center.x + (point.x - center.x) * scale, center.z + (point.z - center.z) * scale };
}

} // namespace ArenaRules
