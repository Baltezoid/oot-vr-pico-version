#pragma once

// Pure rule behind VrCombat_InPlay (VrCombat.cpp): no game types, so the host tests in
// port/soh/tests/vr/VrGameplayGateTests.cpp can build it alone.

namespace VrGameplayGate {

constexpr int kDebugSaveFileNum = 0xFF;
// FileChoose_LoadGame starts Boss Rush and ARENA with this fileNum and a temporary save, not a
// slot.
constexpr int kTemporaryFileNum = 0xFE;

// The player plays the game: a save slot, the debug save, Boss Rush, or ARENA. GameInteractor's
// IsSaveLoaded(true) is the same rule without Boss Rush and ARENA. temporaryQuest is true in
// Boss Rush and in ARENA. The quest id is necessary because Warping.cpp also uses 0xFE for a
// temporary file.
inline bool InPlay(bool normalGameMode, int fileNum, bool temporaryQuest) {
    if (!normalGameMode) {
        return false;
    }
    if ((fileNum >= 0 && fileNum <= 2) || fileNum == kDebugSaveFileNum) {
        return true;
    }
    return fileNum == kTemporaryFileNum && temporaryQuest;
}

} // namespace VrGameplayGate
