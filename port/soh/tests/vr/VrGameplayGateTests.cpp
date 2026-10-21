// Host unit tests for VrGameplayGate.h (pure rule, no game or OpenXR). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrGameplayGateTests.cpp -o /tmp/vr_gate_tests
//   /tmp/vr_gate_tests

#include "VrGameplayGate.h"

#include <cstdio>

using namespace VrGameplayGate;

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

static void SaveSlotsArePlay() {
    for (int slot = 0; slot <= 2; slot++) {
        EXPECT(InPlay(true, slot, false));
    }
}

static void DebugSaveIsPlay() {
    EXPECT(InPlay(true, kDebugSaveFileNum, false));
}

// Boss Rush and ARENA use the temporary file.
static void TemporaryQuestIsPlay() {
    EXPECT(InPlay(true, kTemporaryFileNum, true));
}

// Warping.cpp also uses 0xFE for a temporary file, outside Boss Rush and ARENA.
static void TemporaryFileOutsideTemporaryQuestIsNotPlay() {
    EXPECT(!InPlay(true, kTemporaryFileNum, false));
}

// The title screen runs a demo with the debug save; the file select keeps the quest id of the
// slot under the cursor.
static void OtherGameModesAreNotPlay() {
    EXPECT(!InPlay(false, 0, false));
    EXPECT(!InPlay(false, kDebugSaveFileNum, false));
    EXPECT(!InPlay(false, kTemporaryFileNum, true));
}

static void UnknownFileNumIsNotPlay() {
    EXPECT(!InPlay(true, 3, false));
    EXPECT(!InPlay(true, -1, false));
    EXPECT(!InPlay(true, 3, true));
}

int main() {
    SaveSlotsArePlay();
    DebugSaveIsPlay();
    TemporaryQuestIsPlay();
    TemporaryFileOutsideTemporaryQuestIsNotPlay();
    OtherGameModesAreNotPlay();
    UnknownFileNumIsNotPlay();
    if (sFailures == 0) {
        std::printf("All VrGameplayGate tests passed.\n");
    }
    return sFailures == 0 ? 0 : 1;
}
