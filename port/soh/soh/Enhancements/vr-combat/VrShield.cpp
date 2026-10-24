extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
extern PlayState* gPlayState;
extern SaveContext gSaveContext;
}

#include "VrCombat.h"
#include "VrShieldGeometry.h"
#include "VrShieldHit.h"
#include "overlays/actors/ovl_En_Fhg_Fire/z_en_fhg_fire.h"

#include "soh/cvar_prefixes.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>
#include <ship/resource/ResourceManager.h>
#include <vr_interface.h>
#include <cmath>
#include <vector>

// M4 physical shield: the shield lives in the off hand while a melee weapon is VISIBLY in the
// sword hand (leftHandType — the sword's visibility itself, so the shield leaves the arm the
// same frame the sword leaves the hand, exactly like the base game), and blocking is purely
// geometric — an enemy attack that touches the shield quad bounces (the quad's AC_HARD init
// makes CollisionCheck set AC_BOUNCED) and func_808382DC negates the damage exactly like a
// vanilla stance block, except there is no stance: no button, no crouch, no movement lock, no
// reaction animation, no backwards shove. Hold it up = blocked; hand at your side = hit. Both
// vanilla stance entries (crouch in Player_ActionHandler_11, Z-target upper-body in
// func_80834758) are suppressed at the root while this owns blocking, so their itemAction = -1
// diverged-models marker can never fire from a grip press.
//
// It rides three vanilla gates, all previously keyed on PLAYER_STATE1_SHIELDING (the R-button
// stance, force-cleared every frame — deliberately NOT set here):
//  - Player_SetModelsForHoldingShield (shield model in the off hand)  } widened with
//  - Player_UpdateShieldCollider (shield quad AC/AT registration)     } VrCombat_ShieldHeld()
//  - func_808382DC's block branch (stance reaction skipped via VrCombat_ShieldBlockPhysical()).
// The quad's vertices come from the R_HAND limb matrix, which first-person VR pins to the
// off-hand controller — the visible shield IS the collider, no new collision math needed.

namespace {

bool sWasHeld = false;
int sBlockHapticCooldown = 0; // game ticks; sustained contacts re-bounce every tick

inline int ShieldHand() {
    // Opposite of the sword hand (see VrSwing's SwordHand): Link's R_HAND limb, driven by the
    // player's LEFT controller in right-handed mode.
    return CVarGetInteger("gVrLeftHanded", 0) ? VR_HAND_RIGHT : VR_HAND_LEFT;
}

using VrShieldGeometry::Vec3;

// --- Shield mesh harvest -------------------------------------------------------------------
// The block collider is fitted to the visible shield: the vertices of the hand-with-shield
// display list, in R_HAND limb model space (the same space as the collider). Link's skinned
// DLs load a neighbor limb's matrix (gsSPMatrix 0x0D...) for the wrist vertices and restore
// the hand matrix with gsSPPopMatrix; vertices loaded between the two are in another limb's
// space, so they are skipped.

// By CRC64, as the interpreter resolves the OTR hash commands.
inline void* ResourceByCrc(uint64_t crc) {
    return Ship::Context::GetRawInstance()->GetResourceManager()->GetResourceRawPointer(crc);
}

inline bool IsRealPointer(uintptr_t p) {
    return p > 0x0FFFFFFF; // segmented addresses (0x0D000000...) are not resolvable here
}

void AddVerts(const Vtx* vtx, int count, bool foreign, std::vector<Vec3>& out) {
    if (vtx == nullptr || foreign) {
        return;
    }
    for (int i = 0; i < count; i++) {
        out.push_back({ (float)vtx[i].v.ob[0], (float)vtx[i].v.ob[1], (float)vtx[i].v.ob[2] });
    }
}

void HarvestDListVerts(const Gfx* dl, int depth, bool& foreign, std::vector<Vec3>& out) {
    if (dl == nullptr || depth > 8) {
        return;
    }
    for (int guard = 0; guard < 8192; guard++, dl++) {
        const uint32_t op = (uint32_t)(dl->words.w0 >> 24) & 0xFF;
        const bool branch = ((dl->words.w0 >> 16) & 1) != 0;
        switch (op) {
            case G_ENDDL:
                return;
            case G_VTX:
                if (IsRealPointer(dl->words.w1)) {
                    AddVerts((const Vtx*)dl->words.w1, (int)((dl->words.w0 >> 12) & 0xFF), foreign, out);
                }
                break;
            case G_VTX_OTR_HASH: {
                // Two words. The interpreter caches the resolved pointer in w1 after the first
                // draw (any value above 0xFFFFF); before that, w1 is a byte offset.
                const uintptr_t offset = dl->words.w1;
                const int count = (int)((dl->words.w0 >> 12) & 0xFF);
                dl++;
                const uint64_t hash = ((uint64_t)dl->words.w0 << 32) + (uint64_t)dl->words.w1;
                const Vtx* vtx = nullptr;
                if (offset > 0xFFFFF) {
                    vtx = (const Vtx*)offset;
                } else {
                    const Vtx* base = (const Vtx*)ResourceByCrc(hash);
                    vtx = base != nullptr ? (const Vtx*)((const char*)base + offset) : nullptr;
                }
                AddVerts(vtx, count, foreign, out);
                break;
            }
            case G_VTX_OTR_FILEPATH: {
                char* name = (char*)dl->words.w1;
                dl++;
                const Vtx* base = ResourceMgr_LoadVtxByName(name);
                if (base != nullptr) {
                    AddVerts(base + (dl->words.w1 & 0xFFFF), (int)dl->words.w0, foreign, out);
                }
                break;
            }
            case G_DL:
                if (IsRealPointer(dl->words.w1)) {
                    HarvestDListVerts((const Gfx*)dl->words.w1, depth + 1, foreign, out);
                }
                if (branch) {
                    return;
                }
                break;
            case G_DL_OTR_HASH:
                if (branch) {
                    return; // the interpreter does not support this form either
                }
                dl++;
                HarvestDListVerts((const Gfx*)ResourceByCrc(((uint64_t)dl->words.w0 << 32) + (uint64_t)dl->words.w1),
                                  depth + 1, foreign, out);
                break;
            case G_DL_OTR_FILEPATH:
                HarvestDListVerts(ResourceMgr_LoadGfxByName((const char*)dl->words.w1), depth + 1, foreign, out);
                if (branch) {
                    return;
                }
                break;
            case G_MTX:
            case G_MTX_OTR_FILEPATH:
                foreign = true;
                break;
            case G_MTX_OTR:
                foreign = true;
                dl++;
                break;
            case G_POPMTX:
                foreign = false;
                break;
            case G_SETTIMG_OTR_HASH:
            case G_BRANCH_Z_OTR:
            case G_MARKER:
                dl++; // two-word commands
                break;
            default:
                break;
        }
    }
}

// Vertices of the shield the hand holds now, cached per display list. The draw picks the DL
// the same way (Player_OverrideLimbDrawGameplayDefault): rightHandDLists points at the
// PLAYER_SHIELD_NONE row of sPlayerRightHandShieldDLs, each shield is 4 entries further, and
// entry 0 of a row is the near LOD. Each entry is the resource name of the DL.
const std::vector<Vec3>& ShieldMeshVerts(Player* player) {
    static const void* sCachedDl = nullptr;
    static std::vector<Vec3> sVerts;
    const void* dl = (player->rightHandDLists != nullptr)
                         ? (const void*)player->rightHandDLists[player->currentShield * 4]
                         : nullptr;
    if (dl != sCachedDl) {
        sCachedDl = dl;
        sVerts.clear();
        if (dl != nullptr) {
            bool foreign = false;
            HarvestDListVerts(ResourceMgr_LoadGfxByName((const char*)dl), 0, foreign, sVerts);
        }
    }
    return sVerts;
}

// Center and outward normal of the block collider, from the quad the last draw registered.
bool ShieldFrame(Player* player, Vec3* outCenter, Vec3* outNormal) {
    const Vec3f* q = player->shieldQuad.dim.quad;
    const Vec3 quad[4] = { { q[0].x, q[0].y, q[0].z },
                           { q[1].x, q[1].y, q[1].z },
                           { q[2].x, q[2].y, q[2].z },
                           { q[3].x, q[3].y, q[3].z } };
    const Vec3 chest = { player->actor.world.pos.x, player->actor.world.pos.y + 40.0f, player->actor.world.pos.z };
    return VrShieldGeometry::QuadFrame(quad, chest, outCenter, outNormal);
}

// gVrPhysShieldFacingDeg = the generosity cone: max angle between the shield's outward normal
// and the direction to the attack. 180 disables the gate.
inline float FacingDeg() {
    return CVarGetFloat("gVrPhysShieldFacingDeg", 90.0f);
}

} // namespace

extern "C" bool VrCombat_ShieldHeld(Player* player) {
    if (!VrCombat_Active() || !CVarGetInteger("gVrPhysShield", 1)) {
        return false;
    }
    if (player->actor.category != ACTORCAT_PLAYER) {
        return false; // co-op partner keeps vanilla shielding
    }
    if (player->currentShield == PLAYER_SHIELD_NONE) {
        return false;
    }
    // Vanilla holdability rules: child Link carries the Hylian shield on his back (crouch
    // blocking stays vanilla there), and two-handed weapons occupy both hands. The back is
    // hidden in first person, though, so by default the child holds it like the adult does;
    // gVrChildHylianInHand 0 restores the vanilla rule.
    if (Player_IsChildWithHylianShield(player) && !CVarGetInteger("gVrChildHylianInHand", 1)) {
        return false;
    }
    if (Player_HoldsTwoHandedWeapon(player) &&
        !(Player_CanShieldWithTwoHandedWeapon() && (player->heldItemAction != PLAYER_IA_DEKU_STICK))) {
        return false;
    }
    // The shield rides the off hand exactly while a melee weapon is VISIBLY in the sword hand.
    // leftHandType is the output of Player_SetModels' central model derivation, so it IS the
    // sword's visibility: it goes empty in every scenario that stows the sword — real put-aways
    // AND the visual-only stows (the itemAction = -1 diverged-models family) where
    // heldItemAction still claims the sword. Keying on it restores the vanilla invariant the
    // old logical-item deny-list chased case by case (vanilla's stance could only exist in
    // model-coherent states, so its shield always left with the sword): the shield now leaves
    // the arm the same frame the sword leaves the hand, everywhere. Bare hands, bow/hookshot,
    // bottles, boomerang and the ocarina all fall out of the default case. The base-game rule
    // "the shield unequips with the sword" is deliberate policy here in BOTH control schemes —
    // classic mode loses vanilla's bare-hand shield raise.
    switch (player->leftHandType) {
        case PLAYER_MODELTYPE_LH_SWORD:
        case PLAYER_MODELTYPE_LH_SWORD_2:
        case PLAYER_MODELTYPE_LH_BGS:    // broken Giant's Knife / child Master Sword renders here
        case PLAYER_MODELTYPE_LH_HAMMER: // reachable via Player_CanShieldWithTwoHandedWeapon
            break;
        case PLAYER_MODELTYPE_LH_CLOSED:
            // The Deku stick has no hand model of its own — it renders separately over a closed
            // fist (PLAYER_MODELGROUP_10), so the fist plus the stick logically held is its
            // "visibly in hand". A visual-only stick stow re-derives to an open hand and drops
            // the shield like every other weapon.
            if (player->heldItemAction != PLAYER_IA_DEKU_STICK) {
                return false;
            }
            break;
        default:
            return false;
    }
    // Hands-off states the model derivation doesn't encode (the sword can stay legitimately
    // drawn on Epona, but the off hand holds the reins): the shield goes back on Link's back
    // exactly like his other gear.
    if (player->stateFlags1 &
        (PLAYER_STATE1_CLIMBING_LADDER | PLAYER_STATE1_CLIMBING_LEDGE | PLAYER_STATE1_HANGING_OFF_LEDGE |
         PLAYER_STATE1_IN_WATER | PLAYER_STATE1_ON_HORSE | PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_DEAD)) {
        return false;
    }
    return true;
}

// The collider is the quad of the physical shield in the hand of the player.
static bool IsPhysicalShieldQuad(const void* collider) {
    if (gPlayState == NULL || collider == NULL) {
        return false;
    }
    Player* player = GET_PLAYER(gPlayState);
    return player != NULL && collider == (const void*)&player->shieldQuad.base && VrCombat_ShieldHeld(player);
}

extern "C" void VrCombat_ShieldQuadModelVerts(Player* player, float* outXyz4) {
    // The block collider, built in R_HAND limb MODEL space (game units x100 — the same
    // convention as the blade sliders), replacing the vanilla stance quad wholesale: that one
    // is a 60x60 square hanging at (-15, 0, -6) game units, authored as stance-era forgiveness,
    // and lands nowhere near the visible shield under a controller-driven hand. The shape is a
    // symmetric trapezoid (top width, bottom width, height) so a Hylian-style taper fits. By
    // default (gVrPhysShieldFitMesh) its size and position come from the mesh of the shield in
    // the hand, so an attack that touches the visible shield touches the collider, for every
    // shield and for HD model packs. The size and shift sliders apply when the fit is off.
    float halfWTop = CVarGetFloat("gVrPhysShieldWidthTop", 21.2f) * 50.0f;
    float halfWBot = CVarGetFloat("gVrPhysShieldWidthBottom", 11.9f) * 50.0f;
    float halfH = CVarGetFloat("gVrPhysShieldHeight", 18.0f) * 50.0f;
    const float d2r = 3.14159265f / 180.0f;
    const float pitch = CVarGetFloat("gVrPhysShieldPitch", -4.0f) * d2r;
    const float yaw = CVarGetFloat("gVrPhysShieldYaw", 0.0f) * d2r;
    const float roll = CVarGetFloat("gVrPhysShieldRoll", -90.0f) * d2r;

    // Width along +X, height along +Y (the vanilla quad's plane), tilted by pitch (about X)
    // and yaw (about Y), then rolled in the shield plane: R = Rz(roll) * Ry(yaw) * Rx(pitch).
    const float cp = cosf(pitch), sp = sinf(pitch);
    const float cyw = cosf(yaw), syw = sinf(yaw);
    const float cr = cosf(roll), sr = sinf(roll);
    const float axW[3] = { cr * cyw, sr * cyw, -syw };
    const float axH[3] = { cr * syw * sp - sr * cp, sr * syw * sp + cr * cp, cyw * sp };
    const float axN[3] = { cr * syw * cp + sr * sp, sr * syw * cp - cr * sp, cyw * cp };

    // The shifts live in the COLLIDER'S OWN frame — Across along its width axis, Up/Down
    // along its height axis, Out along its normal — so they keep meaning what their labels
    // say at any tilt (shifting in raw hand axes made "up/down" slide sideways, and no
    // rotation could fix it because the offsets didn't rotate with the quad).
    float cx = CVarGetFloat("gVrPhysShieldShiftX", -1.1f) * 100.0f;
    float cy = CVarGetFloat("gVrPhysShieldShiftY", -0.8f) * 100.0f;
    float cz = CVarGetFloat("gVrPhysShieldShiftZ", -2.5f) * 100.0f;

    if (CVarGetInteger("gVrPhysShieldFitMesh", 1)) {
        const std::vector<Vec3>& verts = ShieldMeshVerts(player);
        const VrShieldGeometry::TrapezoidFit fit = VrShieldGeometry::FitTrapezoid(
            verts.data(), (int)verts.size(), { axW[0], axW[1], axW[2] }, { axH[0], axH[1], axH[2] },
            { axN[0], axN[1], axN[2] });
        // Sanity: 1 to 60 game units (the vanilla stance quad is 60). Outside that, the
        // harvest read something that is not a shield — keep the sliders.
        const float maxHalfW = fmaxf(fit.halfWTop, fit.halfWBottom);
        if (fit.valid && fit.halfH >= 50.0f && fit.halfH <= 3000.0f && maxHalfW >= 50.0f && maxHalfW <= 3000.0f) {
            halfWTop = fit.halfWTop;
            halfWBot = fit.halfWBottom;
            halfH = fit.halfH;
            cx = fit.centerW;
            cy = fit.centerH;
            cz = fit.centerN;
        }
    }

    const float c3[3] = { axW[0] * cx + axH[0] * cy + axN[0] * cz,
                          axW[1] * cx + axH[1] * cy + axN[1] * cz,
                          axW[2] * cx + axH[2] * cy + axN[2] * cz };

    // Vanilla vertex order (the zigzag Collider_SetQuadVertices expects):
    // 0 = bottom-left, 1 = bottom-right, 2 = top-left, 3 = top-right.
    for (int axis = 0; axis < 3; axis++) {
        outXyz4[0 * 3 + axis] = c3[axis] - axW[axis] * halfWBot - axH[axis] * halfH;
        outXyz4[1 * 3 + axis] = c3[axis] + axW[axis] * halfWBot - axH[axis] * halfH;
        outXyz4[2 * 3 + axis] = c3[axis] - axW[axis] * halfWTop + axH[axis] * halfH;
        outXyz4[3 * 3 + axis] = c3[axis] + axW[axis] * halfWTop + axH[axis] * halfH;
    }
}

extern "C" bool VrCombat_ShieldFacingVeto(void* acCollider, void* atCollider) {
    // Called from CollisionCheck_SetATvsAC for EVERY confirmed AT-vs-AC hit: everything that
    // isn't the physical shield quad exits on the first pointer compare.
    if (!IsPhysicalShieldQuad(acCollider)) {
        return false; // not the shield, or vanilla stance blocking (not judged)
    }
    Player* player = GET_PLAYER(gPlayState);
    const float maxDeg = FacingDeg();
    if (maxDeg >= 179.0f) {
        return false;
    }
    Actor* attacker = ((Collider*)atCollider)->actor;
    if (attacker == NULL) {
        return false;
    }
    Vec3 center, normal;
    if (!ShieldFrame(player, &center, &normal)) {
        return false;
    }
    // Attack direction: toward the attacker's focus point (chest/head for tall enemies, the
    // projectile itself for rocks and arrows).
    const Vec3 atk = { attacker->focus.pos.x, attacker->focus.pos.y, attacker->focus.pos.z };
    return !VrShieldGeometry::FacesPoint(center, normal, atk, maxDeg);
}

extern "C" bool VrCombat_ShieldHitIgnored(void* atCollider, void* acCollider, void* acInfo) {
    if (acCollider == NULL || acInfo == NULL || !IsPhysicalShieldQuad(atCollider)) {
        return false;
    }
    const ColliderInfo* info = (const ColliderInfo*)acInfo;
    // SetAC clears BUMP_HIT in each frame, so BUMP_HIT is a hit in this frame.
    const bool hitByMeleeWeapon = (info->bumperFlags & BUMP_HIT) && VrCombat_IsMeleeQuad(info->acHit);

    const Actor* target = ((const Collider*)acCollider)->actor;
    const bool isEnergyBall =
        target != NULL && target->id == ACTOR_EN_FHG_FIRE && target->params == FHGFIRE_ENERGY_BALL;
    bool touchesVisibleBall = false;
    if (isEnergyBall) {
        const ColliderQuad* shield = (const ColliderQuad*)atCollider;
        VrShieldGeometry::Vec3 quad[4];
        for (int i = 0; i < 4; i++) {
            quad[i] = { shield->dim.quad[i].x, shield->dim.quad[i].y, shield->dim.quad[i].z };
        }
        const VrShieldGeometry::Vec3 center = { target->world.pos.x, target->world.pos.y, target->world.pos.z };
        touchesVisibleBall = VrShieldGeometry::QuadTouchesSphere(quad, center, VrShieldHit::kEnergyBallVisibleRadius);
    }
    return VrShieldHit::ShieldHitIgnored(true, hitByMeleeWeapon, isEnergyBall, touchesVisibleBall);
}

extern "C" bool VrCombat_ShieldFacesPoint(Player* player, const float* pointXyz, float* outCenterXyz) {
    if (player == NULL || !VrCombat_ShieldHeld(player)) {
        return false;
    }
    Vec3 center, normal;
    if (!ShieldFrame(player, &center, &normal)) {
        return false;
    }
    if (!VrShieldGeometry::FacesPoint(center, normal, { pointXyz[0], pointXyz[1], pointXyz[2] }, FacingDeg())) {
        return false;
    }
    if (outCenterXyz != NULL) {
        outCenterXyz[0] = center.x;
        outCenterXyz[1] = center.y;
        outCenterXyz[2] = center.z;
    }
    return true;
}

extern "C" int32_t VrCombat_ShieldBlockJudge(Player* player) {
    if (!VrCombat_ShieldHeld(player)) {
        return 0; // vanilla handling
    }
    // Any bounce that reached this point already passed the facing gate inside CollisionCheck
    // (bad-angle hits never bounce at all). The thump re-arms on a short cooldown instead of
    // buzzing while an attack grinds against the shield.
    if (sBlockHapticCooldown <= 0) {
        VR_TriggerHaptic(ShieldHand(), 1.0f, 0.0f, 90.0f);
        sBlockHapticCooldown = 3; // ~150 ms at 20 Hz
    }
    return 1;
}

namespace VrCombat {

void Shield_OnPlayerUpdate(PlayState* play, Player* player) {
    if (sBlockHapticCooldown > 0) {
        sBlockHapticCooldown--;
    }
    const bool held = VrCombat_ShieldHeld(player);
    if (held) {
        // Keep the shield in the off hand. Vanilla re-asserts this every frame while the
        // stance flag is up; we re-assert every tick while physically held (the patched gate
        // inside accepts VrCombat_ShieldHeld). Also converts the sheath model so the shield
        // leaves Link's back while it's in his hand.
        Player_SetModelsForHoldingShield(player);
    } else if (sWasHeld && player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
        // Falling edge (item change, two-hander drawn, mode off): re-derive hand and sheath
        // models from the current model group so the shield returns to Link's back — but ONLY
        // if the shield is actually still mounted. When the falling edge was CAUSED by the game
        // re-deriving models itself (a real put-away, or a visual-only stow that leaves
        // modelGroup pointing at the sword group), the hands are already correct and slamming
        // modelGroup back would re-equip the sword the game just stowed.
        Player_SetModels(player, player->modelGroup);
    }
    sWasHeld = held;
}

void Shield_Deactivate(PlayState* play, Player* player) {
    if (sWasHeld) {
        if (player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD) {
            Player_SetModels(player, player->modelGroup); // see the falling-edge note above
        }
        sWasHeld = false;
    }
    sBlockHapticCooldown = 0;
}

} // namespace VrCombat
