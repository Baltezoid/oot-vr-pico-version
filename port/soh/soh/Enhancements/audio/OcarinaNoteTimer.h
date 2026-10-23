#pragma once

// Rule for the note timer of the ocarina playback. No game types: the host tests in
// port/soh/tests/audio/OcarinaNoteTimerTests.cpp build it alone.
//
// When a note ends, the ticks that are left (the carry) go into the timer of the next note. On the
// N64, one game frame always advances 3 ticks. On the Quest, one frame can advance 6 ticks or
// more. Thus the carry can be more than a short rest. The Serenade of Water and the Nocturne of
// Shadow have a rest of 3 ticks. The vanilla u32 subtraction then goes below zero, and the song
// does not end (issue #128).

#include <stdint.h>

// The timer of the next note: its length minus the carry. It is never less than 1.
static inline uint32_t OcarinaNoteTimer_Next(uint32_t noteLength, uint32_t carry) {
    if (carry >= noteLength) {
        return 1;
    }
    return noteLength - carry;
}
