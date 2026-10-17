// SOH [Quest] VR Settings > Mods > Mod list. Read ModDownloader.java. On other systems, the state is
// Closed.
#pragma once

#include <string>
#include <vector>

namespace ModList {

// The same numbers are in ModDownloader.java.
enum State {
    Closed = 0,
    Reading = 1,
    NoConnection = 2,
    Damaged = 3,
    Ready = 4,
    Downloading = 5,
};

struct Mod {
    std::string name;
    std::string author;
    std::string page; // The page of the author.
    std::string description;
    std::string size;          // The download, for example "12 MB".
    std::string installedSize; // Empty if the list does not give it.
    bool installed = false;
};

struct Status {
    State state = Closed;
    std::string text;      // For the player. Can be empty.
    float progress = 0.0f; // The download, from 0 to 1.
    int active = -1;       // The position of the mod that downloads, or -1.
    std::vector<Mod> mods; // Empty before Ready.
};

// Reads mod-list.json again. Call it when the player opens the page.
void Open();

Status Read();

// Downloads the mod at this position in Status::mods into oot-vr/mods.
void Download(int index);

} // namespace ModList
