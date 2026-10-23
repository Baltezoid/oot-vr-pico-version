#pragma once

// The ledge climb rule in VR (z_player.c). This file is C and C++, and has no game types. Thus,
// z_player.c can include it, and the host tests in port/soh/tests/vr/VrClimbTests.cpp can build it
// alone.

#include <stdint.h>

// The vanilla wall and ledge checks use the direction of Link's body. Link climbs only when his
// body faces the wall. In VR, the body follows the headset. Thus the player must look at the wall
// to climb (#136). While Link walks, the checks use the stick direction instead. During Lock On,
// the body faces the target, as in vanilla. Thus the body direction applies.
static inline int16_t VrClimb_WallCheckYaw(int bodyFollowsHead, int lockedOn, int walking, int16_t bodyYaw,
                                           int16_t stickYaw) {
    return (bodyFollowsHead && !lockedOn && walking) ? stickYaw : bodyYaw;
}
