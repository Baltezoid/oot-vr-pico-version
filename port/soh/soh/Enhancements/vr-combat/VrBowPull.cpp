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

#include <vr_interface.h>

// String pull (#150). Rules in VrBowPull.h. While the string is pulled, padmgr holds the item
// button, so the vanilla bow path nocks and shoots.

namespace {

constexpr uint32_t kBlockedFlags = PLAYER_STATE1_LOADING | PLAYER_STATE1_INPUT_DISABLED | PLAYER_STATE1_TALKING |
                                   PLAYER_STATE1_DEAD | PLAYER_STATE1_START_CHANGING_HELD_ITEM |
                                   PLAYER_STATE1_IN_ITEM_CS | PLAYER_STATE1_IN_CUTSCENE;

VrBowPull::Pull sPull;
VrBowPull::Result sResult;
// From the last string draw, in world units.
VrBowPull::Vec3 sStringRest;
VrBowPull::Vec3 sArrowRest;
float sMaxPull = 0.0f;
uint32_t sStringRestFrame = 0;
bool sStringRestSet = false;
// Last pull aim. Kept after the release (Result::keepShot).
VrBowPull::Vec3 sShotPos;
VrBowPull::Vec3 sShotDir;
bool sShotSet = false;

bool PullOn() {
    return VrCombat::WeaponAimOn() && VrItemSelect_ModeActive() && VrCombat_InPlay();
}

bool CanPull(Player* player) {
    return PullOn() && (player != NULL) && (Player_HoldsBow(player) || Player_HoldsSlingshot(player)) &&
           !(player->stateFlags1 & kBlockedFlags) && !VrOcarina_InPlay();
}

// The off hand holds the bow (HeldItemVrHand in VrItemSelect.cpp).
int StringHand() {
    return VrBelt::SwordHand();
}

// An untracked hand is far from the bow.
VrBowPull::Vec3 HandPos(int hand) {
    float pos[3];
    float quat[4];
    return VR_GetHandPose(hand, pos, quat) ? VrBowPull::Vec3{ pos[0], pos[1], pos[2] }
                                           : VrBowPull::Vec3{ 1.0e6f, 1.0e6f, 1.0e6f };
}

// Drawn in the last 2 frames.
bool StringRestFresh() {
    return sStringRestSet && (gPlayState != NULL) && (gPlayState->state.frames - sStringRestFrame <= 2);
}

bool Nock(VrBowPull::Vec3* out) {
    float pos[3];
    float quat[4];
    if (!StringRestFresh() || !VR_GetHandPose(StringHand(), pos, quat)) {
        return false;
    }
    *out = VrBowPull::NockPos(sArrowRest, { pos[0], pos[1], pos[2] }, sMaxPull);
    return true;
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
    in.stringRest = StringRestFresh() ? sStringRest : HandPos(stringHand ^ 1);
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
    const VrBowPull::Vec3& pulledVertex = slingshot ? VrBowPull::kSlingshotPulledVertex : VrBowPull::kBowPulledVertex;
    sStringRest = { mf[3][0], mf[3][1], mf[3][2] };
    sArrowRest = VrBowPull::LocalToWorld(mf, VrBowPull::ArrowRest(pulledVertex));
    sMaxPull = VrBowPull::MaxPull(pulledVertex, mf[1]);
    sStringRestFrame = gPlayState->state.frames;
    sStringRestSet = true;
    VrBowPull::Vec3 nock;
    VrBowPull::Vec3 nockLocal;
    if (!sResult.holding || !PullOn() || !Nock(&nock) || !VrBowPull::WorldToLocal(mf, nock, &nockLocal)) {
        return false;
    }
    const VrBowPull::Vec3 axis = VrBowPull::StringPullAxis(pulledVertex, nockLocal);
    outAxisXyz[0] = axis.x;
    outAxisXyz[1] = axis.y;
    outAxisXyz[2] = axis.z;
    return true;
}

extern "C" bool VrCombat_BowPullShot(float* outPosXyz, float* outDirXyz) {
    if (!PullOn()) {
        return false;
    }
    VrBowPull::Vec3 nock;
    VrBowPull::Vec3 dir;
    if (sResult.holding) {
        sShotSet = Nock(&nock) && VrBowPull::AimFromHand(sArrowRest, nock,
                                                         VrBowPull::kMinAimM * VrBelt::UnitsPerMeter(), &dir);
        if (sShotSet) {
            sShotPos = nock;
            sShotDir = dir;
        }
    } else if (!sResult.keepShot) {
        sShotSet = false;
    }
    if (!sShotSet) {
        return false;
    }
    outPosXyz[0] = sShotPos.x;
    outPosXyz[1] = sShotPos.y;
    outPosXyz[2] = sShotPos.z;
    outDirXyz[0] = sShotDir.x;
    outDirXyz[1] = sShotDir.y;
    outDirXyz[2] = sShotDir.z;
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
