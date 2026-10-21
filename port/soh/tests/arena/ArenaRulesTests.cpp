// Host unit tests for ArenaRules.h (pure rules of ARENA, no game). The game build does not compile this file. Run it
// from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/arena port/soh/tests/arena/ArenaRulesTests.cpp -o /tmp/arena_tests
//   /tmp/arena_tests

#include "ArenaRules.h"

#include <cmath>
#include <cstdio>

using namespace ArenaRules;

static int sFailures = 0;

#define EXPECT(cond)                                                       \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                   \
        }                                                                  \
    } while (0)

// The curve of issue #115: { count, Hyper } for levels 1 to 4, wave 1 to 22.
static const int kIssueCurve[22][4][2] = {
    { { 4, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },   { { 5, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 6, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },   { { 4, 0 }, { 3, 0 }, { 0, 0 }, { 0, 0 } },
    { { 4, 0 }, { 4, 0 }, { 0, 0 }, { 0, 0 } },   { { 3, 0 }, { 6, 0 }, { 0, 0 }, { 0, 0 } },
    { { 3, 1 }, { 4, 1 }, { 3, 0 }, { 0, 0 } },   { { 3, 2 }, { 4, 1 }, { 4, 0 }, { 0, 0 } },
    { { 3, 2 }, { 4, 1 }, { 5, 1 }, { 0, 0 } },   { { 3, 3 }, { 4, 2 }, { 5, 1 }, { 1, 0 } },
    { { 3, 3 }, { 5, 3 }, { 5, 2 }, { 1, 0 } },   { { 3, 3 }, { 5, 3 }, { 6, 2 }, { 1, 0 } },
    { { 3, 3 }, { 5, 4 }, { 7, 3 }, { 1, 0 } },   { { 3, 3 }, { 6, 5 }, { 7, 4 }, { 1, 1 } },
    { { 3, 3 }, { 6, 5 }, { 8, 6 }, { 1, 1 } },   { { 3, 3 }, { 6, 6 }, { 8, 6 }, { 2, 1 } },
    { { 3, 3 }, { 6, 6 }, { 9, 8 }, { 2, 1 } },   { { 3, 3 }, { 7, 7 }, { 9, 9 }, { 2, 1 } },
    { { 3, 3 }, { 7, 7 }, { 10, 10 }, { 2, 2 } }, { { 3, 3 }, { 7, 7 }, { 11, 11 }, { 2, 2 } },
    { { 3, 3 }, { 8, 8 }, { 11, 11 }, { 2, 2 } }, { { 3, 3 }, { 8, 8 }, { 12, 12 }, { 2, 2 } },
};

static void CurveIsTheCurveOfTheIssue() {
    for (int wave = 1; wave <= 22; wave++) {
        WaveSpec spec = GetWave(wave);
        for (int level = 0; level < kLevels; level++) {
            EXPECT(spec.count[level] == kIssueCurve[wave - 1][level][0]);
            EXPECT(spec.hyper[level] == kIssueCurve[wave - 1][level][1]);
        }
    }
}

static void WaveHasThreePlusNEnemiesUpTo25() {
    for (int wave = 1; wave <= 40; wave++) {
        int expected = wave + 3 > 25 ? 25 : wave + 3;
        EXPECT(GetWave(wave).Total() == expected);
    }
}

static void WavesAfter22AreWave22() {
    WaveSpec last = GetWave(22);
    WaveSpec later = GetWave(500);
    for (int level = 0; level < kLevels; level++) {
        EXPECT(later.count[level] == last.count[level]);
        EXPECT(later.hyper[level] == last.hyper[level]);
    }
}

// Runs the director until it asks for no enemy. Counts the requests.
static int DrainSpawns(Director& director, int alive, int counts[kLevels], int hyper[kLevels]) {
    int total = 0;
    SpawnRequest request;
    while (director.Tick(alive, &request)) {
        counts[request.level]++;
        if (request.hyper) {
            hyper[request.level]++;
        }
        alive++;
        total++;
    }
    return total;
}

static void SkipPause(Director& director) {
    SpawnRequest request;
    int frames = 0;
    while (director.InPause() && frames <= kStartPauseFrames) {
        EXPECT(!director.Tick(0, &request));
        frames++;
    }
    EXPECT(!director.InPause());
}

static void FirstWaveStartsAfterThePause() {
    Director director;
    director.Start();
    EXPECT(director.Wave() == 1);
    SkipPause(director);
    EXPECT(!director.InPause());
}

static void PauseShowsTheSecondsLeft() {
    Director director;
    director.Start();
    // The player needs 10 seconds to get ready (headset test of issue #115).
    EXPECT(director.PauseSecondsLeft() == 10);
    SpawnRequest request;
    director.Tick(0, &request);
    EXPECT(director.PauseSecondsLeft() == 10);
    for (int i = 1; i < kStartPauseFrames - 1; i++) {
        director.Tick(0, &request);
    }
    EXPECT(director.PauseSecondsLeft() == 1);
}

// Between waves the player is ready, so the pause is shorter (headset test of issue #115).
static void PauseBetweenWavesIsFiveSeconds() {
    Director director;
    director.Start();
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    DrainSpawns(director, 0, counts, hyper);
    SpawnRequest request;
    EXPECT(!director.Tick(0, &request));
    EXPECT(director.Wave() == 2);
    EXPECT(director.PauseSecondsLeft() == 5);
}

static void DirectorSpawnsTheWaveOfTheCurve() {
    Director director;
    director.Start();
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    // Wave 1 has 4 enemies, fewer than the maximum alive.
    EXPECT(DrainSpawns(director, 0, counts, hyper) == 4);
    EXPECT(counts[0] == 4);
    EXPECT(hyper[0] == 0);
}

static void DirectorKeepsTheMaximumAlive() {
    Director director;
    director.StartAt(20);
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    EXPECT(DrainSpawns(director, 0, counts, hyper) == kMaxAlive);
    // One enemy died: one more spawns.
    EXPECT(DrainSpawns(director, kMaxAlive - 1, counts, hyper) == 1);
}

static void DirectorSpawnsAllLevelsAndHyperOfTheWave() {
    Director director;
    director.StartAt(16);
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    // The enemies die as fast as they spawn.
    SpawnRequest request;
    while (director.Tick(0, &request)) {
        counts[request.level]++;
        hyper[request.level] += request.hyper ? 1 : 0;
    }
    WaveSpec spec = GetWave(16);
    for (int level = 0; level < kLevels; level++) {
        EXPECT(counts[level] == spec.count[level]);
        EXPECT(hyper[level] == spec.hyper[level]);
    }
}

static void NextWaveStartsWhenAllEnemiesAreDead() {
    Director director;
    director.Start();
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    DrainSpawns(director, 0, counts, hyper);
    SpawnRequest request;
    // An enemy is alive: the wave continues.
    EXPECT(!director.Tick(1, &request));
    EXPECT(director.Wave() == 1);
    EXPECT(!director.InPause());
    // All enemies are dead: the pause before wave 2 starts.
    EXPECT(!director.Tick(0, &request));
    EXPECT(director.Wave() == 2);
    EXPECT(director.InPause());
    SkipPause(director);
    EXPECT(DrainSpawns(director, 0, counts, hyper) == 5);
}

static void RequeueKeepsTheCountsOfTheWave() {
    Director director;
    director.StartAt(10);
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    SpawnRequest request;
    bool failOnce = true;
    while (director.Tick(0, &request)) {
        if (failOnce) {
            director.Requeue(request);
            failOnce = false;
            continue;
        }
        counts[request.level]++;
        hyper[request.level] += request.hyper ? 1 : 0;
    }
    WaveSpec spec = GetWave(10);
    for (int level = 0; level < kLevels; level++) {
        EXPECT(counts[level] == spec.count[level]);
        EXPECT(hyper[level] == spec.hyper[level]);
    }
}

static float Distance(Point a, Point b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.z - b.z) * (a.z - b.z));
}

// A fixed sequence of values in [0, 1), so the tests do not depend on a seed.
struct SequenceRandom {
    const float* values;
    int length;
    int next = 0;
    float operator()() {
        float value = values[next % length];
        next++;
        return value;
    }
};

static void SpawnPointIsInTheArenaAndFarFromLink() {
    static const float kValues[] = { 0.1f, 0.9f, 0.5f, 0.3f, 0.7f, 0.2f, 0.95f, 0.05f, 0.6f, 0.4f };
    SequenceRandom random{ kValues, 10 };
    Point center{ 100.0f, -200.0f };
    Point link{ 100.0f, -200.0f };
    for (int i = 0; i < 50; i++) {
        Point point = PickSpawnPoint(center, 600.0f, link, 300.0f, 600.0f, random);
        EXPECT(Distance(point, center) <= 600.0f + 0.01f);
        EXPECT(Distance(point, link) >= 300.0f);
    }
}

static void SpawnPointIsNearestToTheRingWhenTheRingIsOutsideTheArena() {
    static const float kValues[] = { 0.5f, 0.0f, 0.5f, 0.5f };
    SequenceRandom random{ kValues, 4 };
    Point center{ 0.0f, 0.0f };
    Point link{ 0.0f, 0.0f };
    // The arena is smaller than the minimum distance: no point is far enough.
    Point point = PickSpawnPoint(center, 100.0f, link, 500.0f, 600.0f, random);
    EXPECT(Distance(point, center) <= 100.0f + 0.01f);
    EXPECT(Distance(point, link) > 50.0f);
}

// Link is at the edge of a large arena: the far side of the arena is more than the maximum distance from Link.
static void SpawnPointIsNotFartherThanTheMaximum() {
    static const float kValues[] = { 0.1f, 0.9f, 0.5f, 0.3f, 0.7f, 0.2f, 0.95f, 0.05f, 0.6f, 0.4f, 0.8f };
    SequenceRandom random{ kValues, 11 };
    Point center{ -200.0f, -200.0f };
    Point link{ 450.0f, -200.0f };
    for (int i = 0; i < 50; i++) {
        Point point = PickSpawnPoint(center, 650.0f, link, 300.0f, 600.0f, random);
        EXPECT(Distance(point, center) <= 650.0f + 0.01f);
        EXPECT(Distance(point, link) >= 300.0f - 0.01f);
        EXPECT(Distance(point, link) <= 600.0f + 0.01f);
    }
}

// The scene loads again (for example after a void-out): the actors of the wave are gone.
static void RestartWaveSpawnsTheWholeWaveAgain() {
    Director director;
    director.StartAt(3);
    SkipPause(director);
    SpawnRequest request;
    EXPECT(director.Tick(0, &request));
    EXPECT(director.Tick(1, &request));
    director.RestartWave();
    EXPECT(director.Wave() == 3);
    // The scene loaded again, so the player needs the time to get ready.
    EXPECT(director.PauseSecondsLeft() == 10);
    SkipPause(director);
    int counts[kLevels] = {};
    int hyper[kLevels] = {};
    EXPECT(DrainSpawns(director, 0, counts, hyper) == GetWave(3).Total());
}

static void PointOutsideTheBoundaryComesBack() {
    Point center{ -200.0f, -200.0f };
    Point inside = KeepInside(center, 700.0f, Point{ 0.0f, 0.0f });
    EXPECT(inside.x == 0.0f && inside.z == 0.0f);
    Point outside = KeepInside(center, 700.0f, Point{ 1000.0f, -200.0f });
    EXPECT(std::fabs(Distance(outside, center) - 700.0f) < 0.01f);
    EXPECT(std::fabs(outside.z - -200.0f) < 0.01f);
}

static void EnemiesLeftCountsTheAliveAndTheEnemiesToSpawn() {
    Director director;
    director.Start();
    SkipPause(director);
    // Wave 1 has 4 enemies.
    EXPECT(director.EnemiesLeft(0) == 4);
    SpawnRequest request;
    EXPECT(director.Tick(0, &request));
    EXPECT(director.Tick(1, &request));
    EXPECT(director.EnemiesLeft(2) == 4);
    // One of the two enemies is dead.
    EXPECT(director.EnemiesLeft(1) == 3);
}

int main() {
    CurveIsTheCurveOfTheIssue();
    WaveHasThreePlusNEnemiesUpTo25();
    WavesAfter22AreWave22();
    FirstWaveStartsAfterThePause();
    PauseShowsTheSecondsLeft();
    PauseBetweenWavesIsFiveSeconds();
    DirectorSpawnsTheWaveOfTheCurve();
    DirectorKeepsTheMaximumAlive();
    DirectorSpawnsAllLevelsAndHyperOfTheWave();
    NextWaveStartsWhenAllEnemiesAreDead();
    RequeueKeepsTheCountsOfTheWave();
    SpawnPointIsInTheArenaAndFarFromLink();
    SpawnPointIsNearestToTheRingWhenTheRingIsOutsideTheArena();
    SpawnPointIsNotFartherThanTheMaximum();
    RestartWaveSpawnsTheWholeWaveAgain();
    PointOutsideTheBoundaryComesBack();
    EnemiesLeftCountsTheAliveAndTheEnemiesToSpawn();
    if (sFailures == 0) {
        std::printf("All ArenaRules tests passed.\n");
    }
    return sFailures == 0 ? 0 : 1;
}
