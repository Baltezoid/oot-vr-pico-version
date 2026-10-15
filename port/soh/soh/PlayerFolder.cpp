// SOH [Quest] Read PlayerFolder.h.
#include "PlayerFolder.h"

#include "soh/OTRGlobals.h"

#include <ship/Context.h>

#include <cstdlib>
#include <filesystem>

#include "QuestActivity.h"

namespace PlayerFolder {

const std::string& Path() {
#ifdef __ANDROID__
    static const std::string path = [] {
        const char* env = std::getenv("OOTVR_PLAYER_DIR");
        return std::string(env != nullptr ? env : "");
    }();
#else
    static const std::string path;
#endif
    return path;
}

bool Lost() {
#ifdef __ANDROID__
    static const bool lost = std::getenv("OOTVR_PLAYER_DIR_LOST") != nullptr;
    return lost;
#else
    return false;
#endif
}

std::string LocateArchive(const std::string& name) {
    if (!Path().empty()) {
        std::string inPlayerFolder = Path() + "/" + name;
        std::error_code error;
        if (std::filesystem::is_regular_file(inPlayerFolder, error) &&
            std::filesystem::file_size(inPlayerFolder, error) > 0 && !error) {
            return inPlayerFolder;
        }
    }
    return Ship::Context::LocateFileAcrossAppDirs(name, appShortName);
}

std::vector<std::string> ModFolders() {
    std::vector<std::string> folders;
    if (!Path().empty()) {
        folders.push_back(Path() + "/mods");
    }
    folders.push_back(Ship::Context::LocateFileAcrossAppDirs("mods", appShortName));
    return folders;
}

std::string SaveFolder() {
    if (!Path().empty()) {
        return Path() + "/Save";
    }
    return Ship::Context::GetPathRelativeToAppDirectory("Save");
}

std::string ConfigFile() {
    return Path().empty() ? "shipofharkinian.json" : Path() + "/shipofharkinian.json";
}

bool HasAccess() {
#ifdef __ANDROID__
    return QuestActivity::CallBool("hasAllFilesAccess");
#else
    return false;
#endif
}

void RequestAccess() {
#ifdef __ANDROID__
    QuestActivity::CallVoid("requestAllFilesAccess");
#endif
}

void PickMods() {
#ifdef __ANDROID__
    QuestActivity::CallVoid("pickMods");
#endif
}

bool ModsBusy() {
#ifdef __ANDROID__
    return QuestActivity::CallBool("modsBusy");
#else
    return false;
#endif
}

std::string ModsStatus() {
#ifdef __ANDROID__
    return QuestActivity::CallString("modsStatus");
#else
    return "";
#endif
}

} // namespace PlayerFolder
