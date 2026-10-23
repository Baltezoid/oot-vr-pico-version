#pragma once

// Rules for physical melee (VrSwing.cpp). This file has no game types. Thus, the host tests in
// port/soh/tests/vr/VrMeleeWeaponTests.cpp can build it alone.

#include <cstdint>

namespace VrMeleeWeapon {

// Player_GetMeleeWeaponHeld values (z_player_lib.c).
constexpr int kNone = 0;
constexpr int kMaster = 1;
constexpr int kKokiri = 2;
constexpr int kBiggoron = 3; // also the Giant's Knife
constexpr int kDekuStick = 4;
constexpr int kHammer = 5;

// Physical combat covers all melee weapons: the swords, the Deku Stick, and the hammer.
inline bool Covered(int held) {
    return held >= kMaster && held <= kHammer;
}

// [slash, jump slash] dmgFlags. The rows are the same as D_80854488 in z_player.c: Master, Kokiri
// (also the broken Giant's Knife), Biggoron, Deku Stick, hammer. Rusted switches and hammer rocks
// accept only the hammer flags.
constexpr uint32_t kDmgFlags[5][2] = {
    { 0x00000200, 0x08000000 },
    { 0x00000100, 0x02000000 },
    { 0x00000400, 0x04000000 },
    { 0x00000002, 0x08000000 },
    { 0x00000040, 0x40000000 },
};

// A heavy swing applies the jump slash flags. Other swings apply the slash flags.
inline uint32_t DmgFlags(int held, bool brokenKnife, bool heavy) {
    int row = brokenKnife ? 1 : held - 1;
    if (row < 0 || row > 4) {
        row = 1;
    }
    return kDmgFlags[row][heavy ? 1 : 0];
}

// A heavy swing needs a tip speed of heavySpeed or more. heavySpeed 0 turns heavy swings off. Off
// is the default: a fast swing is easy in VR, and the jump slash flags doubled the damage to
// bosses (issue #131). A vanilla slash does the normal damage, and only a jump attack does more.
constexpr float kHeavySpeedOff = 0.0f;
inline bool IsHeavySwing(float tipSpeed, float heavySpeed) {
    return heavySpeed > 0.0f && tipSpeed >= heavySpeed;
}

// The stick length in hand model units is unk_85C * 5000, as in Player_PostLimbDrawGameplay.
// unk_85C is 1 for a full stick. It decreases to 0 while the stick burns. The minimum length
// prevents a weapon with zero length in the last frame.
constexpr float kMinStickModelUnits = 100.0f;
inline float StickLengthModelUnits(float stickLength) {
    const float len = stickLength * 5000.0f;
    return (len > kMinStickModelUnits) ? len : kMinStickModelUnits;
}

// Swing tiers:
// - kIdle: below the arm speed.
// - kArmed: at the arm speed. Enemies see meleeWeaponState -1 and start to guard.
// - kHot: at the hit speed, and the hand also moves. The swing does damage. A wrist movement
//   alone does not do damage.
// The tier goes back to kIdle below the re-arm speed.
enum Tier { kIdle = 0, kArmed, kHot };

struct Speeds {
    float arm;
    float hit;
    float reArm;
    float minHand;
};

inline int NextTier(int tier, float tipSpeed, float handSpeed, const Speeds& s) {
    const bool handCommitted = handSpeed >= s.minHand;
    if (tier == kIdle) {
        if (tipSpeed >= s.hit && handCommitted) {
            return kHot;
        }
        return (tipSpeed >= s.arm) ? kArmed : kIdle;
    }
    if (tier == kArmed && tipSpeed >= s.hit && handCommitted) {
        tier = kHot;
    }
    if (tipSpeed < s.reArm) {
        tier = kIdle;
    }
    return tier;
}

// The player->meleeWeaponState value for a tier, as a vanilla attack sets it.
inline int MeleeWeaponState(int tier) {
    return (tier == kHot) ? 1 : (tier == kArmed) ? -1 : 0;
}

// A swing ends when the player puts the weapon away. An item change and a burned stick also put
// the weapon away.
inline int TierWhenCovered(int tier, bool covered) {
    return covered ? tier : kIdle;
}

// The vanilla game breaks the Deku Stick when an attack hits, bounces, or strikes a wall
// (func_80842DF4). Physical combat uses the same three events.
inline bool StickStrikeBreaks(bool quadHit, bool quadBounced, bool hotWallContact) {
    return quadHit || quadBounced || hotWallContact;
}

// The vanilla wall test does not check floors or ceilings. A contact is on a wall when the
// vertical part of its unit normal is small.
constexpr float kMaxWallNormalY = 0.5f;
inline bool IsWallNormal(float normalY) {
    return normalY < kMaxWallNormalY && normalY > -kMaxWallNormalY;
}

} // namespace VrMeleeWeapon
