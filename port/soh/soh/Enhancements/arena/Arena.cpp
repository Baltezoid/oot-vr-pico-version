#include "Arena.h"
#include "ArenaRules.h"
#include "soh/ShipInit.hpp"
#include "soh/ShipUtils.h"
#include "soh/frame_interpolation.h"
#include "soh/OTRGlobals.h"
#include "soh/Enhancements/gameplaystats.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <spdlog/spdlog.h>

extern "C" {
#include <z64.h>
#include "functions.h"
#include "macros.h"
#include "variables.h"
#include "src/overlays/actors/ovl_Demo_Gj/z_demo_gj.h"
#include "src/overlays/actors/ovl_Door_Shutter/z_door_shutter.h"
#include "overlays/ovl_Boss_Ganon2/ovl_Boss_Ganon2.h"
#include "textures/icon_item_nes_static/icon_item_nes_static.h"
#include "textures/icon_item_ger_static/icon_item_ger_static.h"
#include "textures/icon_item_fra_static/icon_item_fra_static.h"

extern PlayState* gPlayState;
extern SaveContext gSaveContext;

Gfx* KaleidoScope_QuadTextureIA8(Gfx* gfx, void* texture, s16 width, s16 height, u16 point);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
}

// A closed scene for the waves. All positions are in the coordinates of the scene.
struct ArenaScene {
    s16 sceneNum;
    s16 entrance;
    Vec3f center;
    // The enemies spawn in this circle, minSpawnDistance to maxSpawnDistance from Link.
    f32 spawnRadius;
    f32 minSpawnDistance;
    f32 maxSpawnDistance;
    // Link and the enemies stay in this circle. 0: the walls of the scene close the arena.
    f32 boundaryRadius;
    Vec3f linkStart;
    s16 linkYaw;
    // The name in the quest menu.
    const char* name;
};

// Phantom Ganon: the room is about 300 units from the center to the paintings (z_en_fhg.c). The boss spawns the
// bars that close the room, so ARENA spawns them. Ganon: the fire ring of the boss has a radius of 784 units
// (z_boss_ganon2.c). ARENA draws the ring and keeps Link in it.
static const ArenaScene sArenaScenes[] = {
    { SCENE_FOREST_TEMPLE_BOSS,
      ENTR_FOREST_TEMPLE_BOSS_ENTRANCE,
      { 14.0f, -33.0f, -3315.0f },
      240.0f,
      160.0f,
      480.0f,
      0.0f,
      { 14.0f, -33.0f, -3160.0f },
      static_cast<s16>(0x8000),
      "Phantom Ganon" },
    { SCENE_GANON_BOSS,
      ENTR_GANON_BOSS_0,
      { -200.0f, 1086.0f, -200.0f },
      650.0f,
      300.0f,
      600.0f,
      750.0f,
      { -200.0f, 1086.0f, 300.0f },
      static_cast<s16>(0x8000),
      "Ganon" },
};

struct ArenaEnemy {
    s16 id;
    s16 params;
    // Flying enemies spawn above the floor.
    f32 height;
};

// The levels of issue #115. The entries come from the table of EnemyRandomizer.cpp.
static const ArenaEnemy sLevel1[] = {
    { ACTOR_EN_SKB, 1, 0.0f },      // Stalchild (small)
    { ACTOR_EN_SKB, 20, 0.0f },     // Stalchild (big)
    { ACTOR_EN_FIREFLY, 2, 75.0f }, // Keese
    { ACTOR_EN_TITE, -1, 0.0f },    // Red Tektite
    { ACTOR_EN_DODOJR, 0, 0.0f },   // Baby Dodongo
};
static const ArenaEnemy sLevel2[] = {
    { ACTOR_EN_WF, 0, 0.0f },       // Wolfos
    { ACTOR_EN_ZF, -1, 0.0f },      // Lizalfos
    { ACTOR_EN_TITE, -2, 0.0f },    // Blue Tektite
    { ACTOR_EN_FIREFLY, 1, 75.0f }, // Fire Keese
    { ACTOR_EN_FIREFLY, 4, 75.0f }, // Ice Keese
    { ACTOR_EN_DODONGO, -1, 0.0f }, // Dodongo
    { ACTOR_EN_RR, 0, 0.0f },       // Like-Like
    { ACTOR_EN_MB, 0, 0.0f },       // Club Moblin
    { ACTOR_EN_MB, -1, 0.0f },      // Spear Moblin
};
static const ArenaEnemy sLevel3[] = {
    { ACTOR_EN_TEST, 2, 0.0f },   // Stalfos
    { ACTOR_EN_ZF, -2, 0.0f },    // Dinolfos
    { ACTOR_EN_GELDB, 0, 0.0f },  // Gerudo Fighter
    { ACTOR_EN_WF, 1, 0.0f },     // White Wolfos
    { ACTOR_EN_RD, 1, 0.0f },     // Redead
    { ACTOR_EN_RD, 32766, 0.0f }, // Gibdo
    { ACTOR_EN_BB, -1, 75.0f },   // Bubble
    { ACTOR_EN_TP, -1, 0.0f },    // Tailpasaran
};
static const ArenaEnemy sLevel4[] = {
    { ACTOR_EN_IK, 2, 0.0f },     // Iron Knuckle (black)
    { ACTOR_EN_IK, 3, 0.0f },     // Iron Knuckle (white)
    { ACTOR_EN_TORCH2, 0, 0.0f }, // Dark Link
};

struct EnemyLevel {
    const ArenaEnemy* enemies;
    int count;
};
static const EnemyLevel sLevels[ArenaRules::kLevels] = {
    { sLevel1, ARRAY_COUNT(sLevel1) },
    { sLevel2, ARRAY_COUNT(sLevel2) },
    { sLevel3, ARRAY_COUNT(sLevel3) },
    { sLevel4, ARRAY_COUNT(sLevel4) },
};

static const char* sWaveText[] = { "WAVE", "WELLE", "VAGUE" };
static const char* sEnemiesLeftText[] = { "Enemies", "Gegner", "Ennemis" };

static void* sContinuePromptNoTexs[] = {
    (void*)gPauseNoENGTex,
    (void*)gPauseNoGERTex,
    (void*)gPauseNoFRATex,
};

// The state of one run. The file is temporary, so a run starts with each file.
static const ArenaScene* sArena = nullptr;
static ArenaRules::Director sDirector;
// The wave enemies that are alive or that wait for their deletion. The value is true for a Hyper enemy.
static std::unordered_map<Actor*, bool> sWaveEnemies;
// The ambient light and the directional light that ARENA adds in the Ganon arena.
static const s16 kGanonExtraAmbient = 70;
static const s16 kGanonExtraLight = 50;

static bool sLinkPlaced = false;
static bool sMusicStarted = false;
static bool sDead = false;
static int sDeathWave = 0;
static char sDeathTime[32] = "";

static u8 TextLanguage() {
    return gSaveContext.language < ARRAY_COUNT(sWaveText) ? gSaveContext.language : LANGUAGE_ENG;
}

static ArenaRules::Point ArenaCenter() {
    return { sArena->center.x, sArena->center.z };
}

static bool InArenaScene(PlayState* play) {
    return sArena != nullptr && play != nullptr && play->sceneNum == sArena->sceneNum;
}

static void ResetRun() {
    sDirector.Start();
    sWaveEnemies.clear();
    sLinkPlaced = false;
    sMusicStarted = false;
    sDead = false;
    sDeathWave = 0;
    sDeathTime[0] = '\0';
}

// The scene loaded again, for example after a void-out. The actors of the wave are gone: the wave starts again.
static void RestartWave() {
    sDirector.RestartWave();
    sWaveEnemies.clear();
    sLinkPlaced = false;
}

static int CountAliveEnemies() {
    int alive = 0;
    for (const auto& entry : sWaveEnemies) {
        // A killed actor keeps its place in the actor list for one frame (AlwaysOnFixes.cpp).
        if (entry.first->update != NULL) {
            alive++;
        }
    }
    return alive;
}

static f32 FloorBelow(PlayState* play, f32 x, f32 z) {
    CollisionPoly poly;
    Vec3f pos = { x, sArena->center.y + 300.0f, z };
    return BgCheck_AnyRaycastFloor1(&play->colCtx, &poly, &pos);
}

static bool SpawnEnemy(PlayState* play, const ArenaRules::SpawnRequest& request) {
    const EnemyLevel& level = sLevels[request.level];
    const ArenaEnemy& enemy = level.enemies[static_cast<int>(Rand_ZeroOne() * level.count) % level.count];
    Player* player = GET_PLAYER(play);

    auto random = []() { return Rand_ZeroOne(); };
    ArenaRules::Point point = ArenaRules::PickSpawnPoint(ArenaCenter(), sArena->spawnRadius,
                                                         { player->actor.world.pos.x, player->actor.world.pos.z },
                                                         sArena->minSpawnDistance, sArena->maxSpawnDistance, random);

    // No floor, or a floor far above the arena: the director asks again in the next frame.
    f32 floorY = FloorBelow(play, point.x, point.z);
    if (floorY <= BGCHECK_Y_MIN || floorY > sArena->center.y + 150.0f) {
        return false;
    }

    Vec3f pos = { point.x, floorY + enemy.height, point.z };
    s16 yaw = Math_Vec3f_Yaw(&pos, &player->actor.world.pos);
    Actor* actor = Actor_Spawn(&play->actorCtx, play, enemy.id, pos.x, pos.y, pos.z, 0, yaw, 0, enemy.params);
    if (actor == NULL) {
        return false;
    }
    sWaveEnemies[actor] = request.hyper;

    // Smoke and a sound at the enemy, so the player sees and hears where it comes.
    Vec3f zero = { 0.0f, 0.0f, 0.0f };
    Vec3f smokePos = { pos.x, floorY + 20.0f, pos.z };
    EffectSsDeadDb_Spawn(play, &smokePos, &zero, &zero, 150, 20, 255, 255, 255, 200, 120, 120, 255, 1, 9, false);
    Audio_PlayActorSound2(actor, NA_SE_EN_PO_APPEAR);
    return true;
}

static void PlaceLink(PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 floorY = FloorBelow(play, sArena->linkStart.x, sArena->linkStart.z);

    player->actor.world.pos.x = player->actor.home.pos.x = sArena->linkStart.x;
    player->actor.world.pos.z = player->actor.home.pos.z = sArena->linkStart.z;
    player->actor.world.pos.y = player->actor.home.pos.y = floorY > BGCHECK_Y_MIN ? floorY : sArena->linkStart.y;
    player->actor.world.rot.y = player->actor.shape.rot.y = sArena->linkYaw;
    player->actor.speedXZ = 0.0f;

    if (sArena->sceneNum == SCENE_FOREST_TEMPLE_BOSS) {
        // The same bars as in the intro of Phantom Ganon (z_en_fhg.c), behind Link.
        Actor_Spawn(&play->actorCtx, play, ACTOR_DOOR_SHUTTER, sArena->center.x, sArena->center.y - 97.0f,
                    sArena->center.z + 308.0f, 0, 0, 0, SHUTTER_PG_BARS << 6);
    }
}

static void KeepInBoundary(Actor* actor) {
    ArenaRules::Point inside =
        ArenaRules::KeepInside(ArenaCenter(), sArena->boundaryRadius, { actor->world.pos.x, actor->world.pos.z });
    actor->world.pos.x = inside.x;
    actor->world.pos.z = inside.z;
}

static void OnPlayerUpdate() {
    PlayState* play = gPlayState;
    if (!InArenaScene(play)) {
        return;
    }
    Player* player = GET_PLAYER(play);

    if (!sLinkPlaced) {
        PlaceLink(play);
        sLinkPlaced = true;
        SPDLOG_INFO("[ARENA] Link placed in scene {:#x} at ({}, {}, {})", play->sceneNum, player->actor.world.pos.x,
                    player->actor.world.pos.y, player->actor.world.pos.z);
    }

    // The light of the fight: BossGanon2_Update sets it each frame, and ARENA removes the boss. The fight is too dark
    // to find the enemies, so ARENA adds ambient light.
    if (sArena->sceneNum == SCENE_GANON_BOSS) {
        play->envCtx.unk_BF = 0;
        play->envCtx.unk_DC = 2;
        Math_ApproachZeroF(&play->envCtx.unk_D8, 1.0f, 0.1f);
        for (int i = 0; i < 3; i++) {
            play->envCtx.adjAmbientColor[i] = kGanonExtraAmbient;
            play->envCtx.adjLight1Color[i] = kGanonExtraLight;
        }
    }

    // Stalchildren die in the day (IS_DAY in z_en_skb.c). ARENA keeps the night.
    gSaveContext.dayTime = CLOCK_TIME(0, 0);
    gSaveContext.nightFlag = 1;

    if (sArena->boundaryRadius > 0.0f) {
        KeepInBoundary(&player->actor);
        for (const auto& entry : sWaveEnemies) {
            if (entry.first->update != NULL) {
                KeepInBoundary(entry.first);
            }
        }
    }

    if (sDead || gSaveContext.health <= 0) {
        return;
    }
    if (Player_InBlockingCsMode(play, player)) {
        if (play->gameplayFrames % 40 == 0) {
            SPDLOG_INFO("[ARENA] Waves wait: csAction {} stateFlags1 {:#x} csState {} transition {}", player->csAction,
                        player->stateFlags1, play->csCtx.state, play->transitionTrigger);
        }
        return;
    }

    ArenaRules::SpawnRequest request;
    int wave = sDirector.Wave();
    if (sDirector.Tick(CountAliveEnemies(), &request) && !SpawnEnemy(play, request)) {
        SPDLOG_INFO("[ARENA] Spawn failed, level {}", request.level + 1);
        sDirector.Requeue(request);
    }
    if (sDirector.Wave() != wave) {
        SPDLOG_INFO("[ARENA] Wave {} done", wave);
    }

    if (!sMusicStarted && !sDirector.InPause()) {
        Audio_QueueSeqCmd(SEQ_PLAYER_BGM_MAIN << 24 | NA_BGM_MINI_BOSS);
        sMusicStarted = true;
    }
}

// The rubble of the Ganon fight: the piles where Ganondorf rises, and the rubble that Ganon breaks. It is in the way
// of the enemies. The rubble around the arena stays: it is the wall of the arena.
static bool IsRubbleInTheArena(s16 id, s16 params) {
    return id == ACTOR_DEMO_GJ && (params & 0xFF) != DEMOGJ_TYPE_AROUNDARENA;
}

// A second check after VB_SPAWN_ACTOR_ENTRY: other actors can spawn the boss, Zelda, pots, and grass. The boss of
// Phantom Ganon spawns its horse (EnfHG) in its init.
static void OnActorInit(void* refActor) {
    Actor* actor = static_cast<Actor*>(refActor);
    if (IsRubbleInTheArena(actor->id, actor->params)) {
        Actor_Kill(actor);
        return;
    }
    switch (actor->id) {
        case ACTOR_BOSS_GANONDROF:
        case ACTOR_EN_FHG:
        case ACTOR_BOSS_GANON2:
        case ACTOR_EN_ZL3:
        case ACTOR_EN_KUSA:
        case ACTOR_OBJ_TSUBO:
            SPDLOG_INFO("[ARENA] Kill actor {:#x} after its init", actor->id);
            Actor_Kill(actor);
            break;
        default:
            break;
    }
}

static void OnActorUpdate(void* refActor) {
    Actor* actor = static_cast<Actor*>(refActor);
    auto entry = sWaveEnemies.find(actor);
    if (entry == sWaveEnemies.end() || !entry->second) {
        return;
    }
    // The same second update as Hyper Enemies (HyperEnemies.cpp), only for the Hyper enemies of the wave.
    if (!Player_InBlockingCsMode(gPlayState, GET_PLAYER(gPlayState))) {
        GameInteractor::RawAction::UpdateActor(actor);
    }
}

// Draws the fire ring of Ganon (BossGanon2 draw of the ring, z_boss_ganon2.c), so the player sees the boundary.
static void DrawGanonFireRing() {
    PlayState* play = gPlayState;
    if (!InArenaScene(play) || sArena->sceneNum != SCENE_GANON_BOSS) {
        return;
    }

    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    gDPPipeSync(POLY_XLU_DISP++);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               (uintptr_t)Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, (s32)play->gameplayFrames, 0, 32, 64, 1,
                                             -play->gameplayFrames * 2, -play->gameplayFrames * 8, 32, 32, 1, 0, -2,
                                             -8));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 200, 0, 255);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 128);
    Matrix_Translate(sArena->center.x, sArena->center.y, sArena->center.z, MTXMODE_NEW);
    Matrix_Scale(0.098000005f, 0.1f, 0.098000005f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)gGanonFireRingDL);
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}

static void OnVanillaBehavior(GIVanillaBehavior id, bool* should, va_list originalArgs) {
    va_list args;
    va_copy(args, originalArgs);

    switch (id) {
        // Remove the boss, its blue warp, and the pots, grass, and chests of the scene.
        case VB_SPAWN_ACTOR_ENTRY: {
            va_arg(args, ActorContext*);
            ActorEntry* actorEntry = va_arg(args, ActorEntry*);
            va_arg(args, PlayState*);
            Actor** actor = va_arg(args, Actor**);
            if (IsRubbleInTheArena(actorEntry->id, actorEntry->params)) {
                *actor = NULL;
                *should = false;
                break;
            }
            switch (actorEntry->id) {
                case ACTOR_BOSS_GANONDROF:
                case ACTOR_BOSS_GANON2:
                case ACTOR_EN_ZL3:
                case ACTOR_DOOR_WARP1:
                case ACTOR_ITEM_B_HEART:
                case ACTOR_EN_KUSA:
                case ACTOR_OBJ_TSUBO:
                case ACTOR_EN_BOX:
                    SPDLOG_INFO("[ARENA] Remove scene actor {:#x}", actorEntry->id);
                    *actor = NULL;
                    *should = false;
                    break;
                default:
                    break;
            }
            break;
        }
        // Skip the "Save?" window when the player presses B in the pause menu.
        case VB_CLOSE_PAUSE_MENU: {
            if (CHECK_BTN_ALL(gPlayState->state.input[0].press.button, BTN_B)) {
                *should = true;
            }
            break;
        }
        // When Link dies, keep the result, and go to the "Continue?" screen immediately.
        case VB_TRANSITION_TO_SAVE_SCREEN_ON_DEATH: {
            PauseContext* pauseCtx = va_arg(args, PauseContext*);
            pauseCtx->state = 0xF;
            *should = false;
            if (!sDead) {
                sDead = true;
                sDeathWave = sDirector.Wave();
                char* time = GameplayStats_GetCurrentTime();
                snprintf(sDeathTime, sizeof(sDeathTime), "%s", time);
                free(time);
                // Stops the timer.
                gSaveContext.ship.stats.gameComplete = true;
            }
            break;
        }
        // Show "No" two times, because the player cannot continue.
        case VB_RENDER_YES_ON_CONTINUE_PROMPT: {
            Gfx** disp = va_arg(args, Gfx**);
            *disp = KaleidoScope_QuadTextureIA8(*disp, sContinuePromptNoTexs[TextLanguage()], 48, 16, 12);
            *should = false;
            break;
        }
        // After the death, Arena_Draw shows the time.
        case VB_SHOW_GAMEPLAY_TIMER: {
            *should = !sDead;
            break;
        }
        case VB_BE_ABLE_TO_SAVE:
        case VB_BE_ABLE_TO_OPEN_DOORS:
        case VB_SPAWN_BLUE_WARP:
        case VB_SPAWN_HEART_CONTAINER:
        case VB_RENDER_RUPEE_COUNTER: {
            *should = false;
            break;
        }
        default:
            break;
    }

    va_end(args);
}

// The texture is the alpha of the color of the primitive, as in Interface_DrawTotalGameplayTimer.
static void SetupTextDraw(GraphicsContext* gfxCtx) {
    Gfx_SetupDL_39Overlay(gfxCtx);
    OPEN_DISPS(gfxCtx);
    gDPSetCombineLERP(OVERLAY_DISP++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0,
                      PRIMITIVE, 0);
    CLOSE_DISPS(gfxCtx);
}

static f32 TextWidth(const char* text, f32 scale) {
    f32 width = 0.0f;
    for (const char* c = text; *c != '\0'; c++) {
        width += Ship_GetCharFontWidth(static_cast<u8>(*c)) * 0.8f * scale;
    }
    return width;
}

// Draws a line of the message font in OVERLAY_DISP. Interface_DrawTextLine draws in POLY_OPA_DISP, which is in the 3D
// world in VR. OVERLAY_DISP goes to the HUD, and to the flat panel when Link dies.
static void DrawTextLine(GraphicsContext* gfxCtx, const char* text, f32 x, s32 y, f32 scale, u8 r, u8 g, u8 b) {
    // R_TEXT_CHAR_SCALE of the message box is 80.
    f32 charScale = 0.8f * scale;
    s32 size = static_cast<s32>(FONT_CHAR_TEX_WIDTH * charScale);
    s32 texScale = static_cast<s32>(1024.0f / charScale);

    OPEN_DISPS(gfxCtx);
    for (const char* c = text; *c != '\0'; c++) {
        if (*c != ' ') {
            s32 left = static_cast<s32>(x);
            gDPPipeSync(OVERLAY_DISP++);
            gDPLoadTextureBlock_4b(OVERLAY_DISP++, Ship_GetCharFontTexture(static_cast<u8>(*c)), G_IM_FMT_I,
                                   FONT_CHAR_TEX_WIDTH, FONT_CHAR_TEX_HEIGHT, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                                   G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, 255);
            gSPWideTextureRectangle(OVERLAY_DISP++, (left + 1) << 2, (y + 1) << 2, (left + 1 + size) << 2,
                                    (y + 1 + size) << 2, G_TX_RENDERTILE, 0, 0, texScale, texScale);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, r, g, b, 255);
            gSPWideTextureRectangle(OVERLAY_DISP++, left << 2, y << 2, (left + size) << 2, (y + size) << 2,
                                    G_TX_RENDERTILE, 0, 0, texScale, texScale);
        }
        x += Ship_GetCharFontWidth(static_cast<u8>(*c)) * charScale;
    }
    CLOSE_DISPS(gfxCtx);
}

static void DrawCenteredLine(GraphicsContext* gfxCtx, const char* text, s32 y, f32 scale, u8 r, u8 g, u8 b) {
    DrawTextLine(gfxCtx, text, SCREEN_WIDTH / 2.0f - TextWidth(text, scale) / 2.0f, y, scale, r, g, b);
}

// The hooks stay registered from the boot, and each hook checks IS_ARENA. Do not register them in an OnLoadGame hook:
// other OnLoadGame hooks register OnLoadGame hooks while GameInteractor runs them, and then it can skip a hook.
static void RegisterArena() {
    GameInteractor* gi = GameInteractor::Instance;
    gi->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        // The fixes of EnemyRandomizer.cpp for enemies out of their scene.
        ShipInit::Init("IS_ARENA");
    });
    gi->RegisterGameHook<GameInteractor::OnVanillaBehavior>([](GIVanillaBehavior id, bool* should, va_list args) {
        if (IS_ARENA) {
            OnVanillaBehavior(id, should, args);
        }
    });
    gi->RegisterGameHook<GameInteractor::OnSceneInit>([](s16 sceneNum) {
        if (IS_ARENA) {
            RestartWave();
        }
    });
    gi->RegisterGameHook<GameInteractor::OnPlayerUpdate>([]() {
        if (IS_ARENA) {
            OnPlayerUpdate();
        }
    });
    gi->RegisterGameHook<GameInteractor::OnActorInit>([](void* actor) {
        if (IS_ARENA) {
            OnActorInit(actor);
        }
    });
    gi->RegisterGameHook<GameInteractor::OnActorUpdate>([](void* actor) {
        if (IS_ARENA) {
            OnActorUpdate(actor);
        }
    });
    gi->RegisterGameHook<GameInteractor::OnActorDestroy>([](void* actor) {
        if (IS_ARENA) {
            sWaveEnemies.erase(static_cast<Actor*>(actor));
        }
    });
    gi->RegisterGameHook<GameInteractor::OnPlayDrawEnd>([]() {
        if (IS_ARENA) {
            DrawGanonFireRing();
        }
    });
}

static RegisterShipInitFunc initFunc(RegisterArena);

// The arena that the player selected in the quest menu. It stays selected for the next run.
static int sSelectedScene = 0;

extern "C" void Arena_SelectNextScene(int step) {
    int count = ARRAY_COUNT(sArenaScenes);
    sSelectedScene = ((sSelectedScene + step) % count + count) % count;
}

extern "C" const char* Arena_GetSelectedSceneName(void) {
    return sArenaScenes[sSelectedScene].name;
}

extern "C" void Arena_InitSave(void) {
    // Link, for the few text boxes. The player does not enter a name.
    static const u8 sPlayerName[] = { 0x15, 0x2C, 0x31, 0x2E, 0x3E, 0x3E, 0x3E, 0x3E };
    memcpy(gSaveContext.playerName, sPlayerName, sizeof(sPlayerName));

    sArena = &sArenaScenes[sSelectedScene];

    gSaveContext.ship.quest.id = QUEST_ARENA;
    gSaveContext.entranceIndex = sArena->entrance;
    gSaveContext.cutsceneIndex = 0;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    // Without this flag, the first entrance to the scene of Ganon plays the collapse of the castle.
    Flags_SetEventChkInf(EVENTCHKINF_WATCHED_GANONS_CASTLE_COLLAPSE_CAUGHT_BY_GERUDO);

    // 20 hearts and double magic.
    gSaveContext.healthCapacity = gSaveContext.health = 20 * 16;
    gSaveContext.isMagicAcquired = true;
    gSaveContext.isDoubleMagicAcquired = true;
    gSaveContext.magicLevel = 2;
    gSaveContext.magic = 96;

    // All the weapons of adult Link, Din's Fire, and the Bunny Hood. No bottles. No Farore's Wind: it loads the scene
    // again and restarts the wave. No Nayru's Love: Link cannot take damage.
    std::array<u8, 24> items = {
        ITEM_NONE, ITEM_NUT,  ITEM_BOMB,    ITEM_BOW,      ITEM_ARROW_FIRE,  ITEM_DINS_FIRE,
        ITEM_NONE, ITEM_NONE, ITEM_BOMBCHU, ITEM_LONGSHOT, ITEM_ARROW_ICE,   ITEM_NONE,
        ITEM_NONE, ITEM_NONE, ITEM_NONE,    ITEM_HAMMER,   ITEM_ARROW_LIGHT, ITEM_NONE,
        ITEM_NONE, ITEM_NONE, ITEM_NONE,    ITEM_NONE,     ITEM_NONE,        ITEM_MASK_BUNNY,
    };
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.inventory.items); i++) {
        gSaveContext.inventory.items[i] = items[i];
    }

    // The largest bomb bag, quiver, and nut capacity, and they are full. Boss Rush gives the same ammo with
    // BR_CHOICE_AMMO_MAXED.
    std::array<s8, 16> ammo = { 0, 40, 40, 50, 0, 0, 0, 0, 50, 0, 0, 0, 0, 0, 0, 0 };
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.inventory.ammo); i++) {
        gSaveContext.inventory.ammo[i] = ammo[i];
    }
    Inventory_ChangeUpgrade(UPG_QUIVER, 3);
    Inventory_ChangeUpgrade(UPG_BOMB_BAG, 3);
    Inventory_ChangeUpgrade(UPG_NUTS, 3);

    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_MASTER);
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_BIGGORON);
    gSaveContext.bgsFlag = true;
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_MIRROR);
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_IRON);
    gSaveContext.inventory.equipment |= OWNED_EQUIP_FLAG(EQUIP_TYPE_BOOTS, EQUIP_INV_BOOTS_HOVER);

    // B: the Master Sword, so the shield works. The player can equip the Biggoron Sword in the pause menu.
    // C buttons: the bow, the longshot, and the bombs.
    static const u8 sButtonItems[] = { ITEM_SWORD_MASTER, ITEM_BOW, ITEM_LONGSHOT, ITEM_BOMB };
    static const u8 sCButtonSlots[] = { SLOT_BOW, SLOT_HOOKSHOT, SLOT_BOMB };
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.equips.buttonItems); i++) {
        gSaveContext.equips.buttonItems[i] = i < ARRAY_COUNT(sButtonItems) ? sButtonItems[i] : ITEM_NONE;
    }
    for (int i = 0; i < ARRAY_COUNT(gSaveContext.equips.cButtonSlots); i++) {
        gSaveContext.equips.cButtonSlots[i] = i < ARRAY_COUNT(sCButtonSlots) ? sCButtonSlots[i] : SLOT_NONE;
    }
    Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, EQUIP_VALUE_SWORD_MASTER);
    Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, EQUIP_VALUE_SHIELD_MIRROR);
    Inventory_ChangeEquipment(EQUIP_TYPE_TUNIC, EQUIP_VALUE_TUNIC_KOKIRI);
    Inventory_ChangeEquipment(EQUIP_TYPE_BOOTS, EQUIP_VALUE_BOOTS_KOKIRI);

    ResetRun();
    // The OnLoadGame hook also does this, but GameInteractor can skip it (RegisterArena).
    ShipInit::Init("IS_ARENA");
}

extern "C" void Arena_Draw(PlayState* play) {
    if (!IS_ARENA || !InArenaScene(play)) {
        return;
    }

    char line[64];
    const char* waveText = sWaveText[TextLanguage()];

    if (sDead) {
        SetupTextDraw(play->state.gfxCtx);
        // One line above the GAME OVER title of the game.
        snprintf(line, sizeof(line), "%s %d   %s", waveText, sDeathWave, sDeathTime);
        DrawCenteredLine(play->state.gfxCtx, line, 40, 1.5f, 255, 200, 50);
        return;
    }

    if (play->pauseCtx.state != 0) {
        return;
    }

    SetupTextDraw(play->state.gfxCtx);

    // Below the gameplay timer (Interface_DrawTotalGameplayTimer).
    snprintf(line, sizeof(line), "%s %d", waveText, sDirector.Wave());
    DrawTextLine(play->state.gfxCtx, line, OTRGetRectDimensionFromLeftEdge(24), 92, 1.0f, 255, 255, 255);
    if (!sDirector.InPause()) {
        char left[32];
        snprintf(left, sizeof(left), "%s: %d", sEnemiesLeftText[TextLanguage()],
                 sDirector.EnemiesLeft(CountAliveEnemies()));
        DrawTextLine(play->state.gfxCtx, left, OTRGetRectDimensionFromLeftEdge(24), 108, 1.0f, 255, 255, 255);
    }

    if (sDirector.InPause()) {
        DrawCenteredLine(play->state.gfxCtx, line, 70, 2.5f, 255, 200, 50);
        snprintf(line, sizeof(line), "%d", sDirector.PauseSecondsLeft());
        DrawCenteredLine(play->state.gfxCtx, line, 115, 2.0f, 255, 255, 255);
    }
}
