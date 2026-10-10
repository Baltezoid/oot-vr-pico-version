# Domain context

This file defines the terms that this project uses. It does not tell how the
code works. Read [`docs/architecture.md`](docs/architecture.md) for that.

## Destination

The state in which this project is complete. The roadmap issue
([#1](https://github.com/gibranfelix/oot-vr/issues/1)) contains it. The project
got to the destination on 2026-10-02. The destination is not a task list and not a date.
It describes what success is.

## Standalone

The game runs natively on the headset, without a PC. Other VR ports of Ocarina
of Time need a PC.

## PCVR

The game runs on a PC. The headset is a display that connects with a cable or
with Air Link.

## Practical floor

The quality level of the destination. The player must be able to get to the end
of the game. Crashes are acceptable when the player can save frequently, load
again, and go around the problem. A "no crashes" level is not possible to
measure in a game of 25 to 30 hours.

## Main line

The scope of the destination: the Deku Tree, Dodongo's Cavern, Jabu-Jabu's
Belly, the five adult temples, and Ganon. Optional content is not part of the
destination: Gold Skulltulas, masks, and precision minigames. If a minigame does
not work in VR, the project did not fail.

## VR comfort

The player must not feel sick. VR comfort is part of the destination. The
target is sessions of one hour without discomfort.

## Item kinds

How the player uses an item in VR first person.

| Kind | Items | Use |
|---|---|---|
| Melee weapon | Swords, Deku stick, Megaton Hammer | Swing the hand. |
| Aimed weapon | Bow, slingshot, hookshot, longshot | The shot goes along the weapon model. |
| Throwable item | Boomerang, bombs, bombchus, Deku nuts | Throw with the arm. The off-hand trigger is a second way. |
| Instant item | Bottles with contents, ocarinas, spells, Lens of Truth, masks, magic beans, trade items | The item selector uses the item immediately. |

The empty bottle comes to the hand. The off-hand trigger swings it.

## Item selector

The VR menu that puts an item in the hand, or a bomb, a bombchu, or a Deku nut on
the belt. The player selects an item with the hand or with the right thumbstick.
The item selector does not use a throwable item.

## Belt

The place at the front of Link's waist where a bomb, a bombchu, or a Deku nut
waits. The item selector puts the item on the belt. The player takes it from the
belt with either hand. The fuse of a bomb or a bombchu starts when the item
leaves the belt. A bombchu does not fly: it starts on the floor below the hand.

## Aim mark

A point on the surface where a shot will hit. It shows while the player aims.

## Port assets and game assets

Two different sets of files. They go to the headset in different ways.

- `soh.o2r` contains the **port** assets. The build makes this file, and the
  APK contains it.
- `oot.o2r` contains the **game** assets. The player makes this file from a dump
  of a cartridge or disc that the player owns. The player must own a legal copy
  of the game.

This repository never contains game assets.

## Mod

A file that changes the textures, the models, or the texts of the game. A mod
is an `.o2r` or `.otr` file. The author of the mod gives it. This project does
not give mods.

## Player folder

The folder on the headset where the player puts files by hand: mods and
`oot.o2r`. The player can open it with the SideQuest file manager. The game
also reads the old folder in the app data, so that old installs continue to
work.

## Pre-rendered room

A room that the N64 shows as a fixed 2D image, for example the house of Link
or a shop. In stereo, a 2D image causes discomfort. Without a mod, these rooms
show flat colors.
