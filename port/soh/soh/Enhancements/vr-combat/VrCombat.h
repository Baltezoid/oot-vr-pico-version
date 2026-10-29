#pragma once

// VR Physical Combat (always on in first person): motion-driven melee, shield, carrying and archery,
// replacing the animation-driven systems while active. This header is the module's surface for
// both the C decomp code (master predicate + per-frame shims, added milestone by milestone) and
// the module's own C++ files (soh/soh/Enhancements/vr-combat/).
//
// Game types appear only as pointers to tagged structs (struct Player / struct PlayState), so
// the header stays includable from any TU; consumers that pass real objects naturally already
// include the game headers.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Master predicate: true only when every physical-combat patch should take the VR path THIS
// frame — VR first person with the toggle on, a save loaded, and no state where the game owns
// Link's hands (cutscene, forced action, screen transition). Decomp patches call this and fall
// back to vanilla when false, so scripted sequences behave stock frame-by-frame.
bool VrCombat_Active(void);

// The player plays the game: a save slot, the debug save, or Boss Rush. Gate every VR control
// with this, not GameInteractor::IsSaveLoaded, which is false in Boss Rush (rule and tests in
// VrGameplayGate.h).
bool VrCombat_InPlay(void);

// --- Physical melee (VrSwing.cpp) ---

// Physical combat covers the held melee weapon: a sword, the Deku Stick, or the hammer. False for
// the co-op partner.
bool VrCombat_MeleeCovered(struct Player* player);

// Draw-time feed, called from the Player L_HAND PostLimbDraw seam with the live (controller)
// matrix on the stack and the blade length already configured (D_80126080.x). Tracks swing
// speed, mirrors meleeWeaponState for enemy AI, feeds the sword trail, and registers velocity-
// gated swept sub-quads as AT colliders.
void VrCombat_FeedMelee(struct PlayState* play, struct Player* player);

// True while physical combat drives meleeWeaponState. Vanilla stops Link at a ledge during an
// attack, because the attack moves Link forward. A physical swing does not move Link. Thus the
// ledge stop in func_8083AA10 does not use the state then (#125).
bool VrCombat_SwingIgnoresLedges(struct Player* player);

// True while any physical-melee quad has an AT hit recorded this tick. OR'd into the vanilla
// "landing an attack cancels incoming damage this frame" check, which only knows about the
// player's own two quads.
bool VrCombat_MeleeQuadsHit(void);

// True if the collider is one of the physical-melee quads (VrSwing.cpp).
bool VrCombat_IsMeleeQuad(const void* collider);

// Implemented in z_player.c (// SOH [VR]): forwards to the file-internal melee-weapon-state
// setter func_80833A20, which plays the swing SFX / voice and counts the swing stat on the
// 0 -> nonzero edge. The vr-combat module drives the state from real hand motion.
void VrCombat_SetMeleeWeaponState(struct Player* player, int32_t newState);

// In z_player.c. Calls the vanilla Deku Stick break, func_80842AC4.
void VrCombat_BreakDekuStick(struct PlayState* play, struct Player* player);

// In z_player.c. Starts the ground hit of the original game at posXyz (world units): screen
// shake, enemy stun, rumble, sound, and shock wave.
void VrCombat_HammerGroundHit(struct PlayState* play, struct Player* player, const float* posXyz);

// --- Spin attack gesture (VrSwing.cpp, rules in VrSpinGesture.h, issue #133) ---

// True from the charge pose to the release swing. It replaces the held B button of the vanilla
// spin charge for Link.
bool VrCombat_SpinChargeHeld(void);

// True when the last charge ended with a change of weapon: no spin attack.
bool VrCombat_SpinCancelled(void);

// In z_player.c. True while Link plays the vanilla spin attack.
bool VrCombat_PlayerSpinAttacking(struct Player* player);

// In z_player.c. The part of the hit frames of the vanilla spin attack that Link did: 0 at the
// start, 1 at the end. -1 out of the hit frames.
float VrCombat_PlayerSpinProgress(struct Player* player);

// Visual-mesh harvest exclusion: brackets a display-list section the physical blade must NOT
// collide with (the player's own arms/weapon, the sword trail) by emitting mask marker
// commands into the OPA and XLU buckets. No-ops while physical combat is inactive.
void VrCombat_MeshMaskPush(struct GraphicsContext* gfxCtx);
void VrCombat_MeshMaskPop(struct GraphicsContext* gfxCtx);
// Marks a display-list section as FLESH for the visual-mesh harvest (enemy/NPC bodies):
// contacts stay silent (no stone clank/sparks) and read as bodies to the impact pipeline.
void VrCombat_MeshFleshPush(struct GraphicsContext* gfxCtx);
void VrCombat_MeshFleshPop(struct GraphicsContext* gfxCtx);

// Limb puppetry: the blade manipulates individual limbs of nearby NPC/enemy skeletons —
// rest the blade under an arm and the arm rides it; push a head and it tilts away; hits kick
// the limbs near the impact. The Warp pair brackets each limb's matrix conversion in
// z_skelanime.c: Begin records this limb's world position for the contact solve (the clean
// animated pose) and then offsets the matrix translation by the limb's current puppet offset;
// End restores it so child limbs compute cleanly. Purely visual: animation, hitboxes and AI
// never see the offsets. `actorArg` is compared before any dereference against actors the
// contact gather validated this tick.
void VrCombat_FlinchWarpBegin(struct PlayState* play, void* actorArg, int32_t limbIndex);
void VrCombat_FlinchWarpEnd(void);

// M4 physical shield. ShieldHeld: the shield is physically in the off hand — this widens the
// vanilla PLAYER_STATE1_SHIELDING gates on the shield hand model and the shield quad, so the
// quad registers from the off-hand controller pose every frame with no stance, no button.
// ShieldFacingVeto: the facing gate, called from CollisionCheck_SetATvsAC for every confirmed
// AT-vs-AC hit (one pointer compare for everything that isn't the physical shield quad). True
// when the attack comes from outside the shield's facing cone (gVrPhysShieldFacingDeg): the
// hit is then dropped at the source — no bounce, no enemy recoil, the attack passes the
// shield as if it weren't there. ShieldBlockJudge: called from func_808382DC when the quad
// DID bounce an attack — 0 = not physical (vanilla handling), 1 = physical block (haptic
// fired; caller keeps damage negation + Deku burn, skips the stance reaction anim and shove).
// ShieldFacesPoint: the physical shield is held and its face points at pointXyz (inside the
// same facing cone) — the VR replacement for "PLAYER_STATE1_SHIELDING and Link faces it" in
// actors that read the stance (Twinrova's beams, Dark Link). outCenterXyz (optional) gets the
// center of the block collider.
bool VrCombat_ShieldHeld(struct Player* player);
bool VrCombat_ShieldFacingVeto(void* acCollider, void* atCollider);
// True: CollisionCheck_SetATvsAC ignores this shield hit (VrShieldHit.h, #132).
bool VrCombat_ShieldHitIgnored(void* atCollider, void* acCollider, void* acInfo);
int32_t VrCombat_ShieldBlockJudge(struct Player* player);
bool VrCombat_ShieldFacesPoint(struct Player* player, const float* pointXyz, float* outCenterXyz);

// Alyx-style item selector (VrItemSelect.cpp — VR first person, independent of physical
// combat): hold the configured VR input, flick the hand toward an item, release to take it.
// True when the given VR input on the given hand is DEDICATED to the selector — padmgr skips
// that input's normal button binding so the opening click never leaks its bound action.
bool VrItemSelect_ConsumesInput(int32_t vrHand, uint16_t vrBtnMask);

// SELECTOR MODE (gVrItemSelect on, VR first person): the selector equips and the OFF-HAND TRIGGER
// uses the held item, in either hand, replacing "press the C button you assigned it to".
// ModeActive: the mode owns item activation this frame (third person and flat screen stay stock).
// TriggerItemMask: the N64 button this hand's trigger answers to, or 0 — padmgr ORs it into
// the pad while that hand's trigger is DOWN, as button state rather than a one-frame press, so
// press/hold/release all come out of the vanilla item path (that is what makes draw-and-hold and
// release-to-fire work with nothing re-implemented). The sword-hand trigger is BTN_Z (Z-target)
// in first person and in third person (#79, VrTriggerRole.h). The off-hand trigger is the held
// item's button in first person only. It is 0 for empty hands and for weapons that physical
// combat covers: the swing is the attack. TriggerConsumed: while gVrItemSelect is on, both
// triggers are reserved in every view, so padmgr skips their normal bindings.
bool VrItemSelect_ModeActive(void);
uint16_t VrItemSelect_TriggerItemMask(int32_t vrHand);
bool VrItemSelect_TriggerConsumed(int32_t vrHand, uint16_t vrBtnMask);

// Quick swap to sword & shield (selector mode): squeezing the configured input
// (gVrItemSelSwapInput, default grip) on BOTH controllers at once stows the held item and draws
// the sword. True while the full chord is down for a matching input — padmgr skips its normal
// binding on both hands, so the swap doesn't also emit Z-target / R. Single-hand squeezes keep
// their bindings untouched.
bool VrItemSelect_SwapConsumed(int32_t vrHand, uint16_t vrBtnMask);

// The N64 button of the held item, or 0.
uint16_t VrItemSelect_HeldItemButton(struct Player* player);
// True while the selector press must not use the held item (rule 3 in VrItemSelect.cpp).
bool VrItemSelect_BlocksUse(struct Player* player);
// The two inputs that draw the sword are held (VrItemSelect.cpp QuickSwapTick).
bool VrItemSelect_SwapChordHeld(void);

// True while the ocarina interface is up (free play, song playback, scarecrow recording, the
// frog and Skull Kid minigames — every msgMode from OCARINA_STARTING through FROGS_WAITING).
// While true, padmgr swaps to the dedicated OCARINA binding set (sVrBindOca*) and every
// selector-mode input reservation stands down, so notes can live on any input — including the
// triggers and the selector's own click.
bool VrOcarina_InPlay(void);

// Projectile fire (VR first person, independent of physical combat): the walk-while-aiming
// path only fires on the vanilla item-button RELEASE, so the aim hand's trigger is wired in
// as the natural VR fire. FirePressed: rising edge of that trigger this tick (checked in the
// bow/slingshot/hookshot aim action). AimTriggerConsumed: true while READY_TO_FIRE for the
// aim hand's trigger — padmgr skips its normal binding so a shot can't also toggle Z-target.
bool VrCombat_ProjectileFirePressed(struct Player* player);
bool VrCombat_AimTriggerConsumed(int32_t vrHand, uint16_t vrBtnMask);

// Bow and slingshot aim (VrBowAim.cpp). ArrowOnBow, SeedOnSlingshot: the start and direction of
// the shot from the bow hand matrix (MtxF) and the draw (Player.unk_858). PredictShotHit: the
// first surface on the flight.
// ArrowExtraFrames: the extra flight frames of a bow arrow, 0 when the VR bow aim is off.
void VrCombat_ArrowOnBow(const float* handMf16, float draw, float* outPosXyz, float* outDirXyz);
void VrCombat_SeedOnSlingshot(const float* handMf16, float draw, bool childTilt, float* outPosXyz, float* outDirXyz);
int32_t VrCombat_ArrowExtraFrames(void);
bool VrCombat_PredictShotHit(struct PlayState* play, const float* posXyz, const float* dirXyz, bool seed,
                             float* outHitXyz);
// Boomerang (VrBoomerang.cpp). A throw goes from pos to at. moves: EnBoom.returnTimer. toTarget:
// EnBoom.moveTo is the Z-target.
typedef struct VrCombatBoomerangThrow {
    float pos[3];
    float at[3];
    int32_t moves;
    bool toTarget;
} VrCombatBoomerangThrow;
// The throws return false when there is no VR throw. targetXyz: the Z-target focus, or NULL.
// TriggerThrow: along the aim ray. TakeGripThrow: the grip throw of the last tick, one time.
// AimMark: where the trigger throw hits. GripConsumed: padmgr skips the grip binding.
bool VrCombat_BoomerangTriggerThrow(const float* targetXyz, VrCombatBoomerangThrow* out);
bool VrCombat_BoomerangTakeGripThrow(const float* targetXyz, VrCombatBoomerangThrow* out);
bool VrCombat_BoomerangAimMark(struct PlayState* play, float* outHitXyz);
bool VrCombat_BoomerangGripConsumed(int32_t vrHand, uint16_t vrBtnMask);
// The bomb belt (VrBomb.cpp), also for the bombchu. velocity: units for each tick. The Take
// functions return the event of the last tick one time.
typedef struct VrCombatBombRelease {
    float pos[3];
    float velocity[3];
    bool thrown;
} VrCombatBombRelease;
bool VrCombat_BombUsesBelt(struct Player* player);
bool VrCombat_BombBeltUseNow(struct Player* player);
bool VrCombat_BombBeltPos(struct Player* player, float* outXyz);
bool VrCombat_BombTakeGrab(void);
bool VrCombat_BombHeldPos(struct Player* player, float* outXyz);
bool VrCombat_BombTakeRelease(VrCombatBombRelease* out);
bool VrCombat_BombGripConsumed(int32_t vrHand, uint16_t vrBtnMask);
// HeldYaw: where the bombchu in the hand will run. TakeStart: the start point and the run
// direction of a bombchu that left the hand, one time. False for other bombchus.
bool VrCombat_BombchuHeldYaw(struct Player* player, int16_t* outYaw);
bool VrCombat_BombchuTakeStart(struct Actor* chu, float* outXyz, int16_t* outYaw);
// The scale of the bomb and bombchu models in VR first person: the same real size for child Link
// and adult Link. 1 when VR is off.
float VrCombat_BombDrawScale(void);
// The height of the model center above the actor position, with the draw scale.
float VrCombat_BombCenterHeight(struct Player* player);
// The Deku nut belt (VrNut.cpp). NutTakesOut: the item selector puts the nut on the belt.
bool VrCombat_NutBeltOn(void);
bool VrCombat_NutUsesBelt(struct Player* player);
bool VrCombat_NutTakesOut(struct Player* player);
bool VrCombat_NutBeltUseNow(struct Player* player);
bool VrCombat_NutBeltPos(struct Player* player, float* outXyz);
bool VrCombat_NutHeldPos(struct Player* player, float* outXyz);
float VrCombat_NutDrawScale(void);
// The VR hand that holds the nut, or -1.
int32_t VrCombat_NutHand(void);
// The EnArrow timer of a nut from the hand.
uint8_t VrCombat_NutFlightTicks(void);
bool VrCombat_NutGripConsumed(int32_t vrHand, uint16_t vrBtnMask);
// The string pull of the bow and the slingshot (VrBowPull.cpp, #150). ItemMask: the button of the
// held item while the hand pulls the string, else 0. padmgr ORs it into the pad. OwnsGrip: the
// sword-hand grip takes or holds the string. GripConsumed: padmgr skips the grip binding.
// String: call it with the string matrix (MtxF, after the translation to the string origin). It
// keeps the string position for the grab. While the hand holds the string, it returns true and
// the new Y axis of the string matrix. Shot: while the hand holds the string, the shot start
// (the hand) and the direction (through the string at rest).
bool VrCombat_BowPullString(const float* stringMf16, bool slingshot, float* outAxisXyz);
bool VrCombat_BowPullShot(float* outPosXyz, float* outDirXyz);
uint16_t VrCombat_BowPullItemMask(void);
bool VrCombat_BowPullOwnsGrip(void);
bool VrCombat_BowPullGripConsumed(int32_t vrHand, uint16_t vrBtnMask);
// The physical shield's block collider in R_HAND limb model space (outXyz4 = 4 vertices x xyz,
// vanilla zigzag order) — replaces the vanilla stance quad, whose size/offset never matched a
// controller-held shield. Its size and position come from the mesh of the shield in the hand
// (gVrPhysShieldFitMesh), so it covers the visible shield; the Shield sliders set the tilt,
// and also the size and position when the fit is off.
void VrCombat_ShieldQuadModelVerts(struct Player* player, float* outXyz4);

#ifdef __cplusplus
}

// C++-side module internals (swing tracking, debug overlay, menu readouts).
#include <vr_interface.h>

// Global-scope forward declarations, NOT `struct X` inside the namespace: an elaborated type
// specifier in a namespace-scope parameter would silently declare a fresh VrCombat::X in any TU
// that includes this header before the game headers.
struct PlayState;
struct Player;

namespace VrCombat {

// Hand path drained this game tick: world-space samples at headset rate, oldest first. Drained
// exactly once per tick into this snapshot so every consumer sees the same data. 24 covers a
// 20 Hz tick of 120 Hz XR frames (~6) with generous slack for hitches.
struct TickPath {
    VrHandSample samples[24];
    int count;
};
const TickPath& GetTickPath(int hand);

// VR first person, motion hands, and weapon aim are on (VrBowAim.cpp).
bool WeaponAimOn();
// The boomerang grip throw (VrBoomerang.cpp). Call after the hand paths of the tick are read.
void Boomerang_OnPlayerUpdate(Player* player);
// Call after the hand paths of the tick are read.
void Bomb_OnPlayerUpdate(Player* player);
// Call after the hand paths of the tick are read.
void Nut_OnPlayerUpdate(Player* player);
// The string pull of the bow and the slingshot (VrBowPull.cpp).
void BowPull_OnPlayerUpdate(Player* player);

// Per-tick swing bookkeeping (VrSwing.cpp), driven from VrCombat's OnPlayerUpdate hook:
// reads back last tick's quad results (hit haptics, one-hit-per-swing demotion), then resets
// the quads' AT flags for the coming draw.
void Swing_OnPlayerUpdate(PlayState* play, Player* player);
// Falling edge of VrCombat_Active(): hand melee state back to vanilla (state 0, trail off).
void Swing_Deactivate(PlayState* play, Player* player);

// Per-tick shield bookkeeping (VrShield.cpp): keeps the shield model in the off hand while
// physically held, restores the vanilla models on the falling edge.
void Shield_OnPlayerUpdate(PlayState* play, Player* player);
void Shield_Deactivate(PlayState* play, Player* player);

// Debug-overlay snapshots (this tick): the blade's contact-primitive set, and this draw's
// registered damage quads as 12 floats each (4 verts x xyz). Plain float arrays so this header
// never needs the game's Vec3f type.
int Swing_GetDebugContactPrims(VrContactPrim* out, int maxPrims);
int Swing_GetDebugQuads(float* outVerts12PerQuad, int maxQuads);
// Physical blade rectangle outline (5 world-unit points: rootA, cornerA, tip, cornerB, rootB);
// returns 0 when the sim blade is inactive.
int Swing_GetDebugBladeOutline(float* outPts5x3);
// Puppet-tracked limbs (world positions + current offset magnitude per limb). Overlay proof
// of which bodies the blade can manipulate.
int Swing_GetDebugPuppetLimbs(float* outPos3PerLimb, float* outOffsetMag, int maxLimbs);

} // namespace VrCombat
#endif
