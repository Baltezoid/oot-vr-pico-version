// SOH [Quest] VR Settings > Update: the game finds a newer release on GitHub, downloads its APK, and
// gives it to the Android installer. Read Updater.java. On other systems, the state is Off.
#pragma once

#include <string>

namespace Updater {

// The same numbers are in Updater.java.
enum State {
    Off = 0, // A development build. It does not look for updates.
    Checking = 1,
    Latest = 2,
    CheckFailed = 3,
    Available = 4,
    Downloading = 5,
    Installing = 6, // The installer is open, or the player closed it.
    Failed = 7,
    OldSignature = 8,
};

struct Status {
    State state = Off;
    std::string text;      // For the player.
    std::string installed; // The version name of the APK.
    std::string latest;    // The tag of the newer release, or empty.
    float progress = 0.0f; // The download, from 0 to 1.
};

Status Read();

// Only the state: one JNI call.
State ReadState();

// Downloads the APK of the newer release and opens the installer.
void Start();

// The player can select Update in this state.
bool CanStart(State state);

// The menu is open at VR Settings > Update because of the notice of the file select screen.
bool NoticeOpen();
void OpenNotice();
void CloseNotice();

} // namespace Updater
