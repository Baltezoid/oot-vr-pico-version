// SOH [Quest] The player folder: /sdcard/oot-vr on the Quest. The player puts mods and oot.o2r
// there by hand, and the game keeps the saves there. MainActivity sets OOTVR_PLAYER_DIR only when
// the game can use the folder (read PlayerFolder.java). On other systems there is no player folder,
// and each function gives the usual path of Ship of Harkinian.
#pragma once

#include <string>
#include <vector>

namespace PlayerFolder {

// The player folder, or an empty string when the game cannot use it.
const std::string& Path();

// The game used the player folder before, but cannot use it at this start.
bool Lost();

// A game archive (oot.o2r or oot-mq.o2r): the player folder first, then the app directories. An
// empty file does not count: an interrupted copy leaves one.
std::string LocateArchive(const std::string& name);

// The folders with mods. The player folder comes first: when the same mod name is in the two
// folders, the game loads the one in the player folder.
std::vector<std::string> ModFolders();

// The folder with the saves.
std::string SaveFolder();

// The "All files access" permission is given now. It can be true while Path() is empty: the game
// uses the player folder from the next start.
bool HasAccess();

// Opens the settings screen of the permission.
void RequestAccess();

} // namespace PlayerFolder
