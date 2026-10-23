// Host unit tests for OcarinaNoteTimer.h (pure rule, no game types). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/audio port/soh/tests/audio/OcarinaNoteTimerTests.cpp -o /tmp/ocarina_timer_tests
//   /tmp/ocarina_timer_tests

#include "OcarinaNoteTimer.h"

#include <cstdint>
#include <cstdio>

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

// Note lengths in ticks, from sOcarinaSongNotes in code_800EC960.c. A length of 0 ends the song.
// A rest has no pitch.
struct Note {
    uint32_t length;
    bool rest;
};

static const Note kSerenade[] = { { 36, false }, { 36, false }, { 33, false }, { 3, true },
                                  { 36, false }, { 144, false }, { 0, true } };
static const Note kNocturne[] = { { 36, false }, { 33, false }, { 3, true },   { 18, false }, { 18, false },
                                  { 18, false }, { 18, false }, { 144, false }, { 0, true } };

typedef uint32_t (*NextTimerFn)(uint32_t noteLength, uint32_t carry);

static uint32_t VanillaNextTimer(uint32_t noteLength, uint32_t carry) {
    return noteLength - carry;
}

// A copy of the timer flow of AudioOcarina_PlaybackSong. Each call of the loop body is one game
// frame. Returns the number of frames until the song ends, or -1 if it does not end.
static int PlaySong(const Note* song, uint32_t stepPerFrame, NextTimerFn nextTimer) {
    uint32_t notePos = 0;
    uint32_t staffPos = 0;
    uint32_t timer = 0;

    for (int frame = 1; frame <= 1000; frame++) {
        const uint32_t step = (staffPos == 0) ? 3 : stepPerFrame;
        uint32_t carry = 0;

        if (step < timer) {
            timer -= step;
            continue;
        }
        carry = step - timer;

        timer = song[notePos].length;
        if (notePos == 1) {
            timer++;
        }
        if (timer == 0) {
            return frame;
        }
        timer = nextTimer(timer, carry);
        if (!song[notePos].rest) {
            staffPos++;
        }
        notePos++;
    }
    return -1;
}

// The ticks that are left after the last note go into the timer of the next note.
static void TestCarryShortensNextNote() {
    EXPECT(OcarinaNoteTimer_Next(36, 0) == 36);
    EXPECT(OcarinaNoteTimer_Next(36, 2) == 34);
}

// A carry that is equal to or more than the note length gives a timer of 1, not a wrapped value.
static void TestCarryLongerThanNoteDoesNotWrap() {
    EXPECT(OcarinaNoteTimer_Next(3, 3) == 1);
    EXPECT(OcarinaNoteTimer_Next(3, 5) == 1);
}

// The copy of the playback flow shows the bug: with the vanilla subtraction and a step of 6 ticks,
// the 3-tick rest wraps the timer, and the song does not end.
static void TestVanillaStopsWithSixTickSteps() {
    EXPECT(PlaySong(kSerenade, 6, VanillaNextTimer) == -1);
    EXPECT(PlaySong(kNocturne, 6, VanillaNextTimer) == -1);
}

// On the N64, one frame always advances 3 ticks. Both rules end the song.
static void TestThreeTickStepsEnd() {
    EXPECT(PlaySong(kSerenade, 3, VanillaNextTimer) > 0);
    EXPECT(PlaySong(kSerenade, 3, OcarinaNoteTimer_Next) == PlaySong(kSerenade, 3, VanillaNextTimer));
}

// With the fix, a step of 6 ticks at the end of a note before a 3-tick rest does not stop the
// playback.
static void TestFixEndsWithSixTickSteps() {
    EXPECT(PlaySong(kSerenade, 6, OcarinaNoteTimer_Next) > 0);
    EXPECT(PlaySong(kNocturne, 6, OcarinaNoteTimer_Next) > 0);
}

int main() {
    TestCarryShortensNextNote();
    TestCarryLongerThanNoteDoesNotWrap();
    TestVanillaStopsWithSixTickSteps();
    TestThreeTickStepsEnd();
    TestFixEndsWithSixTickSteps();

    if (sFailures == 0) {
        std::printf("All ocarina note timer tests passed.\n");
        return 0;
    }
    std::printf("%d failure(s).\n", sFailures);
    return 1;
}
