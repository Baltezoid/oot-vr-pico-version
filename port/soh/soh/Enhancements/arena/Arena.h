#pragma once

// ARENA: a quest type of the quest selector. Adult Link fights waves of enemies in one closed scene. The file is
// temporary, as in Boss Rush. The rules of the waves are in ArenaRules.h.

#ifdef __cplusplus
extern "C" {
#endif

struct PlayState;

// Selects the next arena (step 1) or the previous arena (step -1) for the quest menu.
void Arena_SelectNextScene(int step);

// The name of the selected arena, for the quest menu.
const char* Arena_GetSelectedSceneName(void);

// Replaces the new save in gSaveContext with the ARENA save in the selected arena.
void Arena_InitSave(void);

// Draws the wave number on the HUD, and the result when Link dies. Play_Draw calls it after the gameplay timer.
void Arena_Draw(struct PlayState* play);

#ifdef __cplusplus
}
#endif
