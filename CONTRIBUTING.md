# Contributing

Thank you for your help. This is a small project. We examine each pull request
when we have time.

## Before you start

1. Read [`STATUS.md`](STATUS.md) and [`docs/architecture.md`](docs/architecture.md).
2. Read [`AGENTS.md`](AGENTS.md). Its rules apply to all contributors.
3. Find an issue, or open a new one. For a large change, open an issue first.

## Report a bug

Use the bug report template. Give this data:

- the headset model and the Horizon OS version;
- the release or the commit that you use;
- the location in the game (scene, room, or cutscene);
- the steps to cause the problem;
- a log from `adb logcat -s soh:V`, if possible.

Do not attach ROMs, `oot.o2r`, or other game files. Do not tell where you got
your ROM.

## Send a pull request

1. Make one change in each pull request.
2. Write the commit messages in English.
3. Write the documentation, the pull request, and your comments in ASD-STE100
   Simplified Technical English. Read the rules in
   [`AGENTS.md`](AGENTS.md#documentation-language).
4. Build the APK with `port/Android/build-apk.sh`.
5. Test the change on a headset.
6. In the pull request, tell what you tested and on which headset. Then tell
   what the reviewer must test.

An automatic review can add comments to your pull request. A maintainer does
the final review. Only a maintainer can merge.

## Propose a mod for the Mod list

The Mod list is `mod-list.json`. Read
[`docs/adr/0001-download-mods-from-authors.md`](docs/adr/0001-download-mods-from-authors.md).

1. Make sure that the mod is public on the page of its author.
2. Tell the author, in a comment on that page or in an issue: "oot-vr
   downloads your mod from your page. Tell us if you do not want this, and we
   remove it."
3. Download each file. Get its SHA-256 with `sha256sum` and its size with
   `stat -c %s`.
4. Add an entry to `mod-list.json`:

   ```json
   {
     "name": "Name of the mod",
     "author": "Author",
     "page": "https://<page of the mod>",
     "notice": "https://<link to the notice>",
     "description": "What the player gets, for example: HD textures.",
     "installed_size": 4062463848,
     "files": [
       {
         "name": "<file name>.zip",
         "url": "https://<direct link to the file>",
         "sha256": "<SHA-256>",
         "size": 123456,
         "skip": ["<file or folder in the archive>"]
       }
     ]
   }
   ```

   - The file `name` must end with `.o2r`, `.otr`, `.zip`, or `.7z`. It sets
     the folder in `oot-vr/mods/`.
   - `description` shows in the Mod list. Tell what the mod changes and what
     the game does not install.
   - `installed_size` and `skip` are optional.

5. Install the mod from the Mod list on a headset. In the pull request, link
   the request of the community, and tell the frame rate.

When the author changes a file, the download fails. Send a pull request with
the new SHA-256 and size.

To remove a mod, the author or a rights holder opens an issue. The maintainers
remove the entry without a discussion.

## Test a pull request on a headset

Each pull request makes a debug APK. You do not need a local build to test it.

1. Open the pull request.
2. Open the **Checks** tab.
3. Select the **Build** workflow.
4. In **Artifacts**, download `oot-vr-debug-pr<number>-<commit>.apk`.
5. Connect the headset to the computer with a USB cable.
6. Install the APK with `adb install oot-vr-debug-pr<number>-<commit>.apk`.

The debug APK has a signature that is different from the release APK. If the
release version is on the headset, `adb install` stops with a signature error.
Remove the release version first with `adb uninstall org.oot.vr`.
The `adb uninstall` command also deletes your save data and `oot.o2r`. The app
keeps them in `/sdcard/Android/data/org.oot.vr/files`. Copy this directory to
the computer before you remove the release version:

```
adb pull /sdcard/Android/data/org.oot.vr/files oot-vr-files
```

All debug APKs have the same signature. A debug APK installs over a different
debug APK with `adb install -r`, and your save data stays. A local debug build
from before 2026-10-02 has a different signature. Remove it one time.

GitHub keeps each debug APK for 14 days. A push to `main` also makes a debug
APK, with the name `oot-vr-debug-main-<commit>.apk`.

## License

When you send a pull request, you agree that your change is under the MIT
license of this project. Read [`LICENSE`](LICENSE) and [`NOTICE.md`](NOTICE.md).
