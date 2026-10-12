// SOH [Quest] The player folder /sdcard/oot-vr: mods, saves, settings, and optionally oot.o2r. Read
// PlayerFolder.java. Without it, and on other systems, each function gives the usual SoH path.
#pragma once

#include <string>
#include <vector>

namespace PlayerFolder {

// Empty when the game cannot use the folder.
const std::string& Path();

// The game used the folder before, but cannot now.
bool Lost();

// oot.o2r or oot-mq.o2r. The player folder first. An empty file does not count.
std::string LocateArchive(const std::string& name);

// The player folder first. A mod name in the two folders loads from the first.
std::vector<std::string> ModFolders();

std::string SaveFolder();

// shipofharkinian.json: an absolute path in the player folder, or the usual relative name.
std::string ConfigFile();

// The permission now. Path() changes only at the next start.
bool HasAccess();

void RequestAccess();

// Opens the system file picker. The selected mods go into the player folder.
void PickMods();

// Empty, "Copying the mods...", or the result of the last PickMods().
std::string ModsStatus();

// The picker is open or the copy runs.
bool ModsBusy();

} // namespace PlayerFolder
