# AGENTS.md

Rules for contributors and for AI agents in this repository.

oot-vr is a standalone port of *The Legend of Zelda: Ocarina of Time* to the
Meta Quest 3S. The game runs in first person, natively on the headset. It uses
Ship of Harkinian and `libultraship`.

## Start here

1. [`STATUS.md`](STATUS.md): what works, and where to find the open work.
2. [`docs/architecture.md`](docs/architecture.md): where the VR layer connects
   to the game and to the engine.
3. [`CONTEXT.md`](CONTEXT.md): the terms of this project. Use these terms in
   issues, commits, and code.
4. The open issues. The label `ready-for-agent` shows the issues that an agent
   can do. The label `ready-for-human` shows the issues that need a person. The
   roadmap ([#1](https://github.com/gibranfelix/oot-vr/issues/1)) is closed.

## Documentation language

Write all documentation in **ASD-STE100 Simplified Technical English**. This
rule applies to Markdown files, issues, pull requests, comments, and release
notes.

- Write short sentences: 20 words maximum in procedures, 25 words maximum in
  descriptions.
- Write one instruction in each sentence. Use the imperative for instructions.
- Use the active voice.
- Use one word for one meaning. Do not use synonyms for the same thing.
- Use the terms in `CONTEXT.md`.
- Write paragraphs of 6 sentences maximum.
- Do not use contractions or slang.

Issues, pull requests, and comments must be clear and short:

- Write the main point first.
- Write only facts that you examined.
- In each pull request, write two lists: what you tested, and what the reviewer
  must test. Use checkboxes in the second list. Tell where each test runs: on
  the host, in the build, or on the headset.
- Write each fact one time. Remove introductions, summaries of the text above,
  and praise.
- Use lists and tables for steps and comparisons.
- Use an image or a video when it shows the problem or the result better than
  text. Read [Images and videos](#images-and-videos).

### Images and videos

Add captures when a change is visible: the VR menu, the HUD, the camera, or the
game. Show each view that changed. Record the captures on the headset. The
Quest keeps them in `/sdcard/Oculus/Screenshots/` and
`/sdcard/Oculus/VideoShots/`.

Put the files in the `pr-assets` branch, in a folder with the issue number. Link
them by commit:

```sh
git fetch origin pr-assets
git worktree add --detach /tmp/pr-assets origin/pr-assets && cd /tmp/pr-assets
mkdir <issue> && cp <dir>/*.png <dir>/*.mp4 <issue>/ && git add <issue>
git commit -m "Add captures for #<issue>" && git push origin HEAD:pr-assets
git rev-parse --short HEAD
cd - && git worktree remove /tmp/pr-assets
```

- Show an image with
  `![What it shows](https://raw.githubusercontent.com/gibranfelix/oot-vr/<commit>/<issue>/<file>.png)`.
- GitHub does not play a video from a branch. Link the `.mp4` file, or make a
  short GIF.
- Keep each file smaller than 100 MB.
- Do not delete the `pr-assets` branch, and do not force-push it. The links
  stop working without its commits.

### Release notes

Write the release notes with the template in
[`docs/release-notes.md`](docs/release-notes.md).

## Game assets

**This repository must never contain game assets.**

- The `.gitignore` blocks `*.z64`, `*.n64`, `*.v64`, `*.otr`, and `*.o2r`.
- No build can make an output that contains game assets and that Git can
  commit.
- The APK contains only `soh.o2r`. The player makes `oot.o2r` from a dump of a
  cartridge or disc that the player owns.
- Do not write ROM file names, ROM sources, or ROM paths in issues, commits, or
  documentation.

## Code

All code is in `port/`: the game (`port/soh`), the engine
(`port/libultraship`), the extraction tools (`port/ZAPDTR`,
`port/OTRExporter`), and the Quest wrapper (`port/Android`). There are no
submodules.

- **Build the APK** with `port/Android/build-apk.sh`. Read
  [`port/Android/README.md`](port/Android/README.md).
- **Ship of Harkinian version for players.** Players use Ship of Harkinian for
  PC to make `oot.o2r`. `.github/soh-version` contains that version. Keep the
  version in `README.md` the same. A weekly workflow opens an issue when a new
  release of Ship of Harkinian is available.
- **Build on a Linux file system.** The build needs symlinks and the execute
  bit. NTFS and FAT disks do not have them.
- **Mark changes in upstream files** with a comment. Use `SOH [VR]` for VR
  changes. Use `SOH [Quest]` for Android or Quest changes that are not VR. Then
  `git grep` finds all of them.
- **New files in `port/`.** The `.gitignore` of Ship of Harkinian is a Visual
  Studio template. It ignores directories with the names `debug/` and `log/`,
  and it ignores `*.png` and `Makefile`. `libultraship` has source code in
  `src/fast/debug/` and `src/libultraship/log/`. If `git status` does not show
  a new file, add it with `git add -f`.
- **Do not change the line endings** in `port/libultraship`, `port/ZAPDTR`, or
  `port/OTRExporter`. `port/.gitattributes` marks them `-text`. Some files are
  patches that the build applies with `git apply`.
- **VR controls must also work in Boss Rush.** Use `VrCombat_InPlay()`, not
  `GameInteractor::IsSaveLoaded()`.

## Code Review Rules

Examine each pull request for these problems. Each one is a P1 problem.

- The pull request adds game assets, ROM data, or a ROM path.
- The pull request adds a file in a `debug/` or `log/` directory, but Git does
  not track the file.
- The pull request changes line endings in `port/libultraship`, `port/ZAPDTR`,
  or `port/OTRExporter`.
- The pull request changes an upstream file, but the change does not have the
  `SOH [VR]` or `SOH [Quest]` marker.
- The pull request adds a VR call without a `vr_is_initialized()` or
  `VR_IsInitialized()` check. The game must also run with VR off.
- The pull request adds a VR control that does not work in Boss Rush.
- The pull request adds documentation that does not use ASD-STE100, or that
  does not obey the rules for issues, pull requests, and comments in
  [Documentation language](#documentation-language).
