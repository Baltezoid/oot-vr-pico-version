#pragma once

// Rule behind VrCombat_ShieldHitIgnored (VrShield.cpp): no game types, so the host tests in
// port/soh/tests/vr/VrShieldHitTests.cpp can build it alone.
//
// The physical shield is an attack (AT, DMG_SHIELD) in each frame while it is in the hand. In
// the vanilla game the shield attacks only in the shield stance, and Link cannot swing a melee
// weapon in that stance. Thus a melee weapon and the shield never hit the same target in one
// frame. In VR they can. The collision check keeps only the last hit on a target, and the shield
// registers after the melee weapon. The balls of Phantom Ganon and Ganondorf break on a shield
// hit and go back on a melee weapon hit. Thus the shield hit must not replace the melee weapon
// hit.
//
// Phantom Ganon's energy ball has a hit cylinder of radius 40, larger than the visible ball. The
// melee weapon keeps this large cylinder, as in the vanilla game. The shield must touch the
// visible ball: else the shield breaks the ball when it is far from the ball, before the melee
// weapon arrives.

namespace VrShieldHit {

// The radius of the visible energy ball, an estimate: half of the hit cylinder. Ganondorf's
// light ball has the same speed and use, and its hit cylinder has this radius.
constexpr float kEnergyBallVisibleRadius = 20.0f;

// atIsPhysicalShield: the new attack is the quad of the physical shield.
// targetHitByMeleeWeapon: in this frame, a VR melee weapon quad hit the same target before.
// targetIsEnergyBall: the target is Phantom Ganon's energy ball.
// shieldTouchesVisibleBall: the shield quad touches the visible ball.
// True: ignore the shield hit.
inline bool ShieldHitIgnored(bool atIsPhysicalShield, bool targetHitByMeleeWeapon, bool targetIsEnergyBall,
                             bool shieldTouchesVisibleBall) {
    if (!atIsPhysicalShield) {
        return false;
    }
    if (targetHitByMeleeWeapon) {
        return true;
    }
    return targetIsEnergyBall && !shieldTouchesVisibleBall;
}

} // namespace VrShieldHit
