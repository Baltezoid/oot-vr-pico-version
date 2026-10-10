// SOH [Quest] The player folder /sdcard/oot-vr: mods, saves, and optionally oot.o2r. Read
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

// The permission now. Path() changes only at the next start.
bool HasAccess();

void RequestAccess();

} // namespace PlayerFolder
