#pragma once

// The ledge climb rule in VR (z_player.c). This file is C and C++, and has no game types. Thus,
// z_player.c can include it, and the host tests in port/soh/tests/vr/VrClimbTests.cpp can build it
// alone.

#include <math.h>
#include <stdint.h>

// The vanilla wall and ledge checks use the direction of Link's body. Link climbs only when his
// body faces the wall. In VR, the body follows the headset. Thus the player must look at the wall
// to climb (#136). While Link walks, the checks use the stick direction instead. During Lock On,
// the body faces the target, as in vanilla. Thus the body direction applies.
static inline int16_t VrClimb_WallCheckYaw(int bodyFollowsHead, int lockedOn, int walking, int16_t bodyYaw,
                                           int16_t stickYaw) {
    return (bodyFollowsHead && !lockedOn && walking) ? stickYaw : bodyYaw;
}

// The vanilla climb reads the stick as it is: up climbs up, sideways moves sideways on a vine wall.
// In VR, the stick moves Link relative to the headset, and the player can look away from the wall.
// Thus the stick is turned into the frame of the wall: toward the wall climbs up, away from the
// wall climbs down, along the wall moves sideways. When the player walks off the top edge and keeps
// the stick forward, Link climbs down. headYaw is the headset yaw, wallYaw is the direction of
// Link's body, which faces the wall. stickX is positive to the right.
static inline void VrClimb_StickToWall(int16_t headYaw, int16_t wallYaw, int stickX, int stickY, int* wallX,
                                       int* wallY) {
    const float kYawToRad = 3.14159265f / 32768.0f;
    float delta = (float)(int16_t)(headYaw - wallYaw) * kYawToRad;
    float s = sinf(delta);
    float c = cosf(delta);

    *wallY = (int)lroundf((float)stickY * c + (float)stickX * s);
    *wallX = (int)lroundf((float)stickX * c - (float)stickY * s);
}
