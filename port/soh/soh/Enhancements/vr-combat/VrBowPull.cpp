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
// also shoots. While the hand holds the string, the string goes to the hand
// (VrCombat_BowPullString), and the shot goes from the hand through the string at rest
// (VrCombat_BowPullShot).

namespace {

constexpr uint32_t kBlockedFlags = PLAYER_STATE1_LOADING | PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING |
                                   PLAYER_STATE1_DEAD | PLAYER_STATE1_START_CHANGING_HELD_ITEM |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;

VrBowPull::Pull sPull;
VrBowPull::Result sResult;
// The center of the string at rest (world), from the last draw of the string.
VrBowPull::Vec3 sStringRest;
uint32_t sStringRestFrame = 0;
bool sStringRestSet = false;

bool PullOn() {
    return CVarGetInteger("gVrBowPull", 1) && VrCombat::WeaponAimOn() && VrItemSelect_ModeActive() &&
           VrCombat_InPlay();
}

bool CanPull(Player* player) {
    return PullOn() && (player != NULL) && (Player_HoldsBow(player) || Player_HoldsSlingshot(player)) &&
           !(player->stateFlags1 & kBlockedFlags) && !VrOcarina_InPlay();
}

int StringHand() {
    // The off hand holds the bow and the slingshot (HeldItemVrHand in VrItemSelect.cpp).
    return VrBelt::SwordHand();
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
    const int stringHand = StringHand();
    VrBowPull::Input in;
    in.canPull = CanPull(player);
    in.gripHeld = (VR_GetControllerButton(stringHand) & VR_BTN_GRIP) != 0;
    // The string of the last two frames. Else the bow hand.
    const bool restFresh = sStringRestSet && (gPlayState->state.frames - sStringRestFrame <= 2);
    in.stringRest = restFresh ? sStringRest : HandPos(stringHand ^ 1);
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

extern "C" bool VrCombat_BowPullString(const float* stringMf16, bool slingshot, float* outAxisXyz) {
    const float(*mf)[4] = reinterpret_cast<const float(*)[4]>(stringMf16);
    if (gPlayState == NULL) {
        return false;
    }
    sStringRest = { mf[3][0], mf[3][1], mf[3][2] };
    sStringRestFrame = gPlayState->state.frames;
    sStringRestSet = true;
    float pos[3];
    float quat[4];
    VrBowPull::Vec3 handLocal;
    if (!sResult.holding || !PullOn() || !VR_GetHandPose(StringHand(), pos, quat) ||
        !VrBowPull::WorldToLocal(mf, { pos[0], pos[1], pos[2] }, &handLocal)) {
        return false;
    }
    const VrBowPull::Vec3 axis = VrBowPull::StringPullAxis(
        slingshot ? VrBowPull::kSlingshotPulledVertex : VrBowPull::kBowPulledVertex, handLocal);
    outAxisXyz[0] = axis.x;
    outAxisXyz[1] = axis.y;
    outAxisXyz[2] = axis.z;
    return true;
}

extern "C" bool VrCombat_BowPullShot(float* outPosXyz, float* outDirXyz) {
    float pos[3];
    float quat[4];
    VrBowPull::Vec3 dir;
    if (!sResult.holding || !sStringRestSet || !PullOn() || !VR_GetHandPose(StringHand(), pos, quat) ||
        !VrBowPull::AimFromHand(sStringRest, { pos[0], pos[1], pos[2] },
                                VrBowPull::kMinAimM * VrBelt::UnitsPerMeter(), &dir)) {
        return false;
    }
    for (int i = 0; i < 3; i++) {
        outPosXyz[i] = pos[i];
    }
    outDirXyz[0] = dir.x;
    outDirXyz[1] = dir.y;
    outDirXyz[2] = dir.z;
    return true;
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

extern "C" bool VrCombat_BowPullGripConsumed(int32_t vrHand, uint16_t vrBtnMask) {
    return (vrBtnMask & VR_BTN_GRIP) && (vrHand == StringHand()) && VrCombat_BowPullOwnsGrip();
}
