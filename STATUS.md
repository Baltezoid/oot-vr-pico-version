# Status

This file tells what works. The issues tell what does not work and what is
not tested.

## What works

We tested these on a Quest 3S. We did not test on a Quest 3.

- Many areas of the game, from Kokiri Forest to the fight with Ganon. We did
  not play the full story from the start to the end.
- The first-start setup. The panel asks for the ROM, the headset makes
  `oot.o2r`, and the game starts in VR. We tested it with the European
  GameCube version.
- Stereo render, first-person camera, and head tracking.
- Automatic world scale for child Link and adult Link.
- Quick Setup in VR Settings: view, sword hand, right stick (items or turn),
  item selector or C buttons, and HUD position. By default, the player turns
  with the body and the HUD is on the wrist.
- The physical sword and shield with the Touch controllers. The shield blocks
  with its full size, and the Mirror Shield reflects the Twinrova beams.
- Menus and the pause screen on a floating panel.
- The SoH menu with the Touch controllers and a laser pointer.
- The wrist HUD. Hearts, magic, and rupees show on the off-hand wrist when you
  look at it. The item buttons show above the sword-hand controller.
- The update from the VR menu. The game finds a newer release, downloads the
  APK, and opens the Android installer. We tested it with a local test release.

## Open work

- [Open issues](https://github.com/gibranfelix/oot-vr/issues): known problems
  and improvements.
- [`needs-headset-test`](https://github.com/gibranfelix/oot-vr/issues?q=is%3Aopen+label%3Aneeds-headset-test):
  merged changes that a person must test on a headset.
- [`ready-for-agent`](https://github.com/gibranfelix/oot-vr/issues?q=is%3Aopen+label%3Aready-for-agent):
  an agent can do the issue. A person must test it on a headset.
- [`ready-for-human`](https://github.com/gibranfelix/oot-vr/issues?q=is%3Aopen+label%3Aready-for-human):
  the issue needs a decision or a test on the headset.

Before you start, read [`docs/architecture.md`](docs/architecture.md) and
[`CONTRIBUTING.md`](CONTRIBUTING.md).
