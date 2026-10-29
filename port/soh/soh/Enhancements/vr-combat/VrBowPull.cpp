extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
extern PlayState* gPlayState;
}

#include "VrBelt.h"
#include "VrBowPull.h"
#include "VrCombat.h"

#include <libultraship/bridge/consolevariablebridge.h>
#include <vr_interface.h>

// The string pull of the bow and the slingshot (#150). The math is in VrBowPull.h. While the hand
// pulls the string, padmgr holds the button of the held item (VrCombat_BowPullItemMask). Thus the
// vanilla bow path nocks the arrow, holds it, and shoots it at the release. The off-hand trigger
// also stays the button of the held item.

namespace {

constexpr uint32_t kBlockedFlags = PLAYER_STATE1_LOADING | PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING |
                                   PLAYER_STATE1_DEAD | PLAYER_STATE1_START_CHANGING_HELD_ITEM |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;

VrBowPull::Pull sPull;
VrBowPull::Result sResult;

bool PullOn() {
    return CVarGetInteger("gVrBowPull", 1) && VrCombat::WeaponAimOn() && VrItemSelect_ModeActive() &&
           VrCombat_InPlay();
}

bool CanPull(Player* player) {
    return PullOn() && (player != NULL) && (Player_HoldsBow(player) || Player_HoldsSlingshot(player)) &&
           !(player->stateFlags1 & kBlockedFlags) && !VrOcarina_InPlay();
}

// An untracked hand is far from the bow.
VrBowPull::Vec3 HandPos(int hand) {
    float pos[3];
    float quat[4];
    return VR_GetHandPose(hand, pos, quat) ? VrBowPull::Vec3{ pos[0], pos[1], pos[2] }
                                           : VrBowPull::Vec3{ 1.0e6f, 1.0e6f, 1.0e6f };
}

} // namespace

void VrCombat::BowPull_OnPlayerUpdate(Player* player) {
    if (!VR_IsInitialized() || (gPlayState == NULL)) {
        sResult = {};
        return;
    }
    // The off hand holds the bow and the slingshot (HeldItemVrHand in VrItemSelect.cpp).
    const int stringHand = VrBelt::SwordHand();
    VrBowPull::Input in;
    in.canPull = CanPull(player);
    in.gripHeld = (VR_GetControllerButton(stringHand) & VR_BTN_GRIP) != 0;
    in.bowHand = HandPos(stringHand ^ 1);
    in.stringHand = HandPos(stringHand);
    in.unitsPerMeter = VrBelt::UnitsPerMeter();
    const bool wasInReach = sResult.inReach && !sResult.holding;
    sResult = sPull.Update(in);

    if (sResult.inReach && !sResult.holding && !wasInReach) {
        VR_TriggerHaptic(stringHand, 0.25f, 0.0f, 15.0f);
    }
    switch (sResult.event) {
        case VrBowPull::Event::Grab:
            VR_TriggerHaptic(stringHand, 0.4f, 0.0f, 25.0f);
            break;
        case VrBowPull::Event::Drawn:
            VR_TriggerHaptic(stringHand, 0.6f, 0.0f, 40.0f);
            VR_TriggerHaptic(stringHand ^ 1, 0.3f, 0.0f, 40.0f);
            break;
        default:
            break;
    }
}

extern "C" uint16_t VrCombat_BowPullItemMask(void) {
    if (!sResult.itemButtonHeld || (gPlayState == NULL) || !PullOn()) {
        return 0;
    }
    return VrItemSelect_HeldItemButton(GET_PLAYER(gPlayState));
}

extern "C" bool VrCombat_BowPullOwnsGrip(void) {
    return sResult.inReach && PullOn();
}

extern "C" bool VrCombat_BowGripConsumed(int32_t vrHand, uint16_t vrBtnMask) {
    return (vrBtnMask & VR_BTN_GRIP) && (vrHand == VrBelt::SwordHand()) && VrCombat_BowPullOwnsGrip();
}
