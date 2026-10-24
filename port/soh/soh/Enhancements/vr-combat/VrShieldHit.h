#pragma once

// Rule for VrCombat_ShieldHitIgnored (VrShield.cpp). No game types, for the host tests in
// port/soh/tests/vr/VrShieldHitTests.cpp.
//
// The shield attacks in each frame. The collision check keeps the last hit, and the shield
// registers after the melee weapon. The energy ball breaks on a shield hit and goes back on a
// melee weapon hit. Its hit cylinder (radius 40) is larger than the visible ball.

namespace VrShieldHit {

// Estimate of the visible ball radius: half of the hit cylinder.
constexpr float kEnergyBallVisibleRadius = 20.0f;

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
