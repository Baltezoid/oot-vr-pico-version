# Release notes

These rules apply to the notes of each release on GitHub.

A tag `v*` starts `release.yml`. The workflow publishes the APK with notes that
GitHub makes. Replace these notes with the template below.

- Write in English, in ASD-STE100.
- Write for the player. Tell what changed in the game, not in the code.
- Start each item with one bold sentence. Link the pull request. Add one or two
  short sentences only if the player must know more.
- Use only the sections that have items. Do not add changes to the
  documentation or to CI.
- In "Known problems", link only open issues.
- Set the title to `oot-vr vX.Y.Z-beta`.
- To show a clip of a new feature, put an animated WebP in `docs/media/` on
  `main` first. Link it with the `raw.githubusercontent.com` URL of `main`.
  GitHub does not show a WebP from a release asset as an image. You can also
  attach the MP4 to the release, and link it.

```md
# oot-vr vX.Y.Z-beta

<One or two sentences: the main changes.>

![<Clip description>](https://raw.githubusercontent.com/gibranfelix/oot-vr/main/docs/media/<clip>.webp)

**This is a beta release.** Some problems are known. Save frequently.

> [!IMPORTANT]
> You must own a legal copy of the game. This APK does not contain game assets. At the first start, you select a dump of your own cartridge or disc.

## Added

- **<Feature>** ([#N](https://github.com/gibranfelix/oot-vr/pull/N)). <How to use it.>

## Changed

## Fixed

## Known problems

- <Problem> ([#N](https://github.com/gibranfelix/oot-vr/issues/N)).

## Install

1. Enable developer mode on the headset.
2. Install `oot-vr-vX.Y.Z-beta.apk` with SideQuest or `adb install`.
3. Copy your dump into the `Download` folder of the headset.
4. Start **OoT VR** from **Unknown Sources**, and select your dump.

If you have an older version, select **VR Settings** > **Update** in the game.
If your version does not have **Update**, install this APK over it. Your saves stay.

**Full Changelog**: https://github.com/gibranfelix/oot-vr/compare/<previous tag>...vX.Y.Z-beta
```
