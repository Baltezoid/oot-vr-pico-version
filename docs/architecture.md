# Architecture

This document tells you where the VR layer connects to the game and to the
engine. Read it before you change VR code.

## The three parts

| Part | Directory | Contents |
|---|---|---|
| Game | `port/soh` | Ship of Harkinian 9.2.3 "Ackbar Delta" with the VR changes from `Shipwright-VR` |
| Engine | `port/libultraship` | The Fast3D renderer, the GLES backend, and the OpenXR session |
| Android wrapper | `port/Android` | The Gradle project, `MainActivity`, `SetupActivity`, and the manifest for Horizon OS |

The game and the Android wrapper use SDL2. Upstream `libultraship` changed to
SDL3. Thus, we do not merge upstream changes. We copy each fix by hand.

## How to find VR code

The code of the game comes from the decompilation of the original game. When
Ship of Harkinian changes that code, it adds a comment that starts with
`SOH [Category]`. The comment tells what the change does and why. This port
adds two categories:

- `SOH [VR]`: a change for VR.
- `SOH [Quest]`: a change for Android or for the Quest that is not VR.

For example, this comment in `port/libultraship/src/fast/interpreter.cpp` marks
the start of the stereo render:

```cpp
// SOH [VR] Stereo rendering: replace the game's perspective projection with the current
// eye's VR view * projection. 2D targets (HUD quad, flat-screen panel) keep the game's own
// flat projection.
```

To list all changes of this port, run:

```
git grep -n -e 'SOH \[VR\]' -e 'SOH \[Quest\]' port
```

The VR code that is new has its own files:

| File | Contents |
|---|---|
| `port/libultraship/src/fast/vr_openxr.cpp` | The OpenXR session, the swapchains, and the eye poses |
| `port/libultraship/src/fast/vr_interface.cpp` | The C interface that the game calls (`VR_*` functions) |
| `port/libultraship/include/vr_interface.h` | The declarations of the `VR_*` functions, with comments |
| `port/libultraship/src/fast/vr_physics.cpp` | The collision mesh for the physical hands |
| `port/libultraship/src/fast/vr_menu_input.cpp` | The SoH menu with the Touch controllers: the Y toggle, the menu navigation, and the input filter for the game |
| `port/soh/soh/Enhancements/vr-combat/` | Sword and hammer swing, shield, bow and boomerang aim, belt, and item selection |
| `port/soh/soh/SohGui/SohMenuVRSettings.cpp` | The VR settings menu. The Developer tab shows only with Dev Tools > Debug Mode on |

## Render path

`libultraship` does not know about stereo. The renderer gets triangles that are
already in clip space. Thus, the VR layer must change the projection before the
renderer gets the triangles.

The connection point is `Interpreter::GfxSpMatrix` in
`port/libultraship/src/fast/interpreter.cpp`. When the game loads a perspective
projection, the VR layer replaces it with the view and projection of the
current eye. 2D targets keep the projection of the game. The 2D targets are the
HUD quad and the floating menu panel.

Each VR pass renders into an OpenXR swapchain. The frame buffers of the
`libultraship` backend make each eye pass a normal off-screen render.

`vr_openxr.cpp` has two graphics paths:

- `XR_USE_GRAPHICS_API_OPENGL_ES`: the Quest path. This path works.
- `XR_USE_GRAPHICS_API_D3D11`, under `ENABLE_DX11`: the Windows path. We
  changed this path but we did not compile it again. Think that it is broken.

## SoH menu with the Touch controllers

The SoH menu is an ImGui window. Without VR, the companion window on the PC
shows it. In VR, the window layer draws the menu onto the floating panel.

1. `Fast3dGui::UpdateVrMenu` runs at the start of each frame. It reads the
   controllers and opens or closes the menu on a push of the left Y button.
2. While the menu is open, it sends the controls to ImGui as gamepad keys.
   ImGui then moves the selection and activates the items. `Menu.cpp` changes
   the tab on the L1 and R1 keys (the grips).
3. `Fast3dWindow` runs the GUI frame on each frame. After the GUI frame, it
   draws the result onto the panel swapchain. While the menu is open, the GUI
   frame has the size of the panel swapchain.
4. `VR_GetControllerButton` and the other `VR_Get*` input functions send the
   controls through `VrMenuInput`. While the menu is open, the game sees
   released controls. A button or a thumbstick that is held when the menu
   closes stays released for the game until the player releases it.

The left Y button is not available to the bindings, also in the ocarina set.
The binding
listener in VR Settings reads the raw controls (`VR_GetControllerButtonRaw`).
It stops the menu navigation with `VR_HoldMenuNavigation`.

The host unit tests for `VrMenuInput` are in
`port/libultraship/tests/vr_menu_input_tests.cpp`.

## Game modes

Boss Rush does not use a save slot. Its `fileNum` is `0xFE`.
`GameInteractor::IsSaveLoaded()` is false for it. `VrCombat_InPlay()` accepts
the slots, the debug save, and Boss Rush. Tests:
`port/soh/tests/vr/VrGameplayGateTests.cpp`.

## Physical melee

The swords, the Deku Stick, and the Megaton Hammer hit when the player swings
them.

- `VrSwing.cpp` measures the speed of the weapon. A slow movement does not hit.
- Each weapon applies its vanilla damage flags.
- The Deku Stick breaks on a hit, a bounce, or a wall, as in the vanilla game.
  The torches light the stick at its tip.
- A hammer hit on a surface also makes a collider across the surface. The
  rusted switch has a flat collider on its top.
- A hammer ground hit occurs when the head moves down fast and touches a floor
  near the feet of Link. `VrCombat_HammerGroundHit` in `z_player.c` then starts
  the effects of the vanilla game.

The rules are in `VrMeleeWeapon.h` and `VrHammerSlam.h`. The host unit tests
are in `port/soh/tests/vr/`.

## Physical shield

The shield is in the off hand while the sword is in the sword hand. No button
is necessary. `VrShield.cpp` sets the block collider, and the collision check
does the block.

- **Block collider.** The collider has the size of the shield mesh in the hand.
  `VrShield.cpp` reads the vertices of the display list of the hand. The tilt
  sliders in VR Settings > Developer set the plane of the collider.
- **Facing cone.** The shield blocks an attack only when the attack comes from
  the front of the shield. The default cone is 90 degrees.
- **Actors that read the shield stance.** Twinrova and Dark Link read
  `PLAYER_STATE1_SHIELDING`. The physical shield does not set this flag.
  These actors also call `VrCombat_ShieldFacesPoint`. Thus, a shield that
  points at the beam reflects it.

The pure geometry is in `VrShieldGeometry.h`. The host unit tests are in
`port/soh/tests/vr/VrShieldGeometryTests.cpp`. The first lines of the file
show how to run them.

## Item selector

`VrItemSelect.cpp` is the item selector. It presses the C button of the item
(`EmulateButtonPress`), thus each item uses the vanilla path. Three rules
change the vanilla path:

1. No button changes the held item (`VB_CHANGE_HELD_ITEM_AND_USE_ITEM`).
2. The off-hand trigger is the button of the held item. The sword-hand trigger
   is Z-target (`VrItemSelect_TriggerItemMask`).
3. The item selector takes a throwable item out and does not use it
   (`SelectorOnlyTakesOut`, `VrItemSelect_BlocksUse`).

`Player_UseItem` in `z_player.c` uses an instant item immediately.
`HeldItemVrHand` tells which hand holds the item.

## Bomb belt

In selector mode, the bomb waits on the belt (`VrBomb.cpp`). Link keeps
`PLAYER_IA_BOMB` with empty hands, and there is no `EnBom` actor. Thus the belt
has no fuse and no ammo cost.

| Step | Code in `z_player.c` |
|---|---|
| The grip takes the bomb | `Player_VrTakeBeltBomb` spawns the `EnBom`. |
| The grip is released | `Player_VrReleaseBomb` throws the bomb, or drops it. |
| The off-hand trigger | `Player_InitExplosiveIA` spawns the bomb, and `Player_ActionHandler_9` throws it. |
| A throw is done | `Player_DetachHeldActor` keeps the bombs selected. |

In VR first person, `EnBom_Draw` draws the bomb 15 cm wide in the real world,
for child Link and adult Link. The fuse and the shadow follow this size. The
explosion size does not change. The throw distance does not use the world
scale.

The belt also holds the bombchu. The bombchu does not fly. At the release,
`VrCombat_BombchuTakeStart` gives `EnBomChu_WaitForRelease` two values:

| Value | Result |
|---|---|
| Start point | The floor below the hand. With a wall or a different floor, the position of Link. |
| Run direction | Along a fast hand. Else, where the controller points. |

`EnBomChu_Move` runs along `world.rot.y` on a flat floor. Thus
`EnBomChu_WaitForRelease` sets `world.rot.y` and `shape.rot.y`. The bombchu
updates after the player in the same tick.

In VR first person, `EnBomChu_Draw` uses the draw scale of the bomb.

`VrGripHand.cpp` closes an empty hand of Link while the player holds the grip.

The math is in `VrBombThrow.h`. The host unit tests are in
`port/soh/tests/vr/VrBombThrowTests.cpp`.

## Deku nut belt

The Deku nut uses the same belt as the bomb. `VrBelt.cpp` and `VrBeltThrow.h`
contain the shared code. The nut has no actor on the belt or in the hand.

| Step | Code |
|---|---|
| The item selector takes the nut out | `Player_UseItem` sets `PLAYER_IA_DEKU_NUT`. It does not throw the nut. |
| The grip is released | `VrNut.cpp` spawns an `EnArrow` nut with the hand velocity. |
| The nut flies | `EnArrow` keeps the gravity of the hand (`vrFromHand`). |
| The off-hand trigger | `Player_UseItem` does the vanilla nut throw. |

The nut has no collider, as in the vanilla game. It flashes when it touches a
wall or the floor. The nut is 10 cm above the bomb on the belt. The hand that
holds the nut stays open, and the nut is on the palm (`VrGripHand.cpp`).

In VR stereo, `z_play.c` does not draw the screen flash of the nut. The flash is
a 2D fill, and it shows as a small gray rectangle in VR.

## Ranged weapons

`Player_PostLimbDrawGameplay` in `port/soh/src/code/z_player_lib.c` sets the
start point and the direction of the held projectile. The flight code reads
them when the player shoots.

| Weapon | Direction |
|---|---|
| Hookshot, longshot | The barrel. With motion hands, the hand limb has the controller pose. |
| Bow | The arrow on the bow model (`Player_VrAimHeldShot`). The math is in `VrBowAim.h`. |
| Slingshot | The seed in the pouch of the slingshot model (`Player_VrAimHeldShot`). The math is in `VrBowAim.h`. |
| Boomerang, left trigger | The controller aim ray (`Player_VrThrowBoomerang` in `z_player.c`). |
| Boomerang, right grip | The hand velocity at the release. The math is in `VrBoomerangThrow.h`. |

The aim mark is the hookshot reticle. `Player_DrawReticleAt` draws it in the
world. For the hookshot, it is green when the hook can hold, and red when the
hook cannot hold. The rule is in `VrAimMark.h`. For the bow and the slingshot,
the aim mark shows where the shot will hit. It follows the `EnArrow_Fly` flight
to the first surface. The Bow Reticle enhancement does not show in VR first
person.

In VR, the arrow flies 24 frames more than in the vanilla game, thus it hits
what the player sees. The arc does not change. With VR off, the flight is the
vanilla flight. The seed flight does not change.

The VR boomerang throws spawn `EnBoom` with `EN_BOOM_PARAMS_VR`. This flight
is two times faster, has the same range, and has a catch distance of 60 units
(`VrBoomerangFlight.h`). With Z-targeting, the boomerang turns to the target
only if the throw is in a cone of 30 degrees. Only the trigger throw has an aim
mark.

The host unit tests are in `port/soh/tests/vr/VrAimMarkTests.cpp`,
`port/soh/tests/vr/VrBowAimTests.cpp`, and
`port/soh/tests/vr/VrBoomerangThrowTests.cpp`.

## Camera and movement

- **First-person camera.** Vanilla Ocarina of Time has a first-person camera:
  `Camera_Subj3` in `port/soh/src/code/z_camera.c`. The game sends the position
  of the head of Link to the VR layer with `VR_SetCameraAnchor`. The VR layer
  adds the offset and the rotation of the headset.
- **Movement.** `Player_GetMovementSpeedAndYaw` in
  `port/soh/src/overlays/actors/ovl_player_actor/z_player.c` gives the speed and
  the direction for all locomotion actions. The VR layer changes the direction
  at this one point.
- **Cutscenes.** Cutscenes use a different sub-camera (`z_demo.c`). Thus, the
  code can find a cutscene with one check.
- **World scale.** Link has two heights: 68.0 for adult and 44.0 for child. The
  game sends the eye height of Link with `VR_SetLinkEyeHeight`. The VR layer
  compares it with the real eye height of the player and calculates the scale.

## Android wrapper

The wrapper has two activities. Horizon OS runs them as a hybrid app.

| Activity | Type | Contents |
|---|---|---|
| `MainActivity` | Immersive, launcher | The game. It extends `SDLActivity`. |
| `SetupActivity` | 2D panel | The first-start setup: ROM selection and extraction. |

### First start

1. `MainActivity` starts. If the external files folder has no `oot.o2r` and no
   `oot-mq.o2r`, it asks Home to open `SetupActivity`. Then it ends. The SDL
   thread does not start, so VR does not start.
2. `SetupActivity` opens the system file picker. It copies the selected file to
   the app cache.
3. `SetupActivity` calls the extractor in `libsoh.so` through JNI
   (`RomExtractor.java` and `port/soh/soh/Extractor/AndroidRomExtractor.cpp`).
   The extractor checks the ROM without dialogs, and then runs ZAPD. ZAPD uses
   the XML files of the ROM version. The APK contains these files in
   `extractor-xml/`.
4. The archive goes first to a staging folder. Only a complete archive moves to
   the external files folder.
5. `SetupActivity` deletes the ROM copy and the work files. Then it starts
   `MainActivity`, and VR starts.

With this order, the OpenXR session never has to come back from a 2D window.
The logic of `SetupActivity` is in small Java classes without Android
dependencies. Unit tests examine them on the host:
`port/Android/app/src/test/`.

ZAPD keeps global state. After a failed extraction, the setup asks the player
to close the app, and then it ends the process.

### Extractor configuration files

The wrapper keeps the extractor configuration files (300 KB). `RunExtract()`
needs this directory at each start, also when `oot.o2r` is present. Without the
directory, the game waits for a click on a dialog. In VR, you cannot click that
dialog.

## Debug on the headset

The headset has no console and no debugger. Use these tools:

| Tool | Command or location |
|---|---|
| Game log | `adb logcat -s soh:V`. VR messages start with `[VR]`. |
| Frame rate and frame times | `adb logcat -s soh:V \| grep Perf`. The game writes one `[VR] Perf:` line each 5 seconds. |
| Headset CPU and GPU levels | `adb logcat -s VrApi`. |
| Log files | `/sdcard/Android/data/org.oot.vr/files/logs/` |
| Player folder permission | `adb shell appops set org.oot.vr MANAGE_EXTERNAL_STORAGE allow`. Use `default` to remove it. The game reads it at start. |
| Screenshot of the left eye | `adb exec-out screencap -p > s.png`. Not tested on the device. |
| Headset recordings | `/sdcard/Oculus/VideoShots/`. Get them with `adb pull`. |
| Save file with all items | In `shipofharkinian.json`, set `gDeveloperTools.DebugEnabled=1` and `DebugSaveFileMode=2`. A new save in slot 1 then has all items. Developer mode also gives a scene selector. |

Horizon OS does not start the app when nobody wears the headset. You cannot
change this with `adb`. Put on the headset for each test.
