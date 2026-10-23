#pragma once

// Rule behind VrCombat_ShieldYieldsToMeleeWeapon (VrShield.cpp): no game types, so the host
// tests in port/soh/tests/vr/VrShieldHitTests.cpp can build it alone.
//
// The physical shield is an attack (AT, DMG_SHIELD) in each frame while it is in the hand. In
// the vanilla game the shield attacks only in the shield stance, and Link cannot swing a melee
// weapon in that stance. Thus a melee weapon and the shield never hit the same target in one
// frame. In VR they can. The collision check keeps only the last hit on a target, and the shield
// registers after the melee weapon. The balls of Phantom Ganon and Ganondorf break on a shield
// hit and go back on a melee weapon hit. Thus the shield hit must not replace the melee weapon
// hit.

namespace VrShieldHit {

// atIsPhysicalShield: the new attack is the quad of the physical shield.
// targetHitByMeleeWeapon: in this frame, a VR melee weapon quad hit the same target before.
// True: ignore the shield hit. The melee weapon hit stays.
inline bool ShieldYieldsToMeleeWeapon(bool atIsPhysicalShield, bool targetHitByMeleeWeapon) {
    return atIsPhysicalShield && targetHitByMeleeWeapon;
}

} // namespace VrShieldHit
