#pragma once

// ARENA: a quest type of the quest selector. Adult Link fights waves of enemies in one closed scene. The file is
// temporary, as in Boss Rush. The rules of the waves are in ArenaRules.h.

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Replaces the new save in gSaveContext with the ARENA save, and selects the arena at random.
void Arena_InitSave(void);

// Draws the wave number on the HUD, and the result when Link dies. Play_Draw calls it after the gameplay timer.
void Arena_Draw(struct PlayState* play);

#ifdef __cplusplus
}
#endif
