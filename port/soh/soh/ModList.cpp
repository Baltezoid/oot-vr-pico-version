// SOH [Quest] Read ModList.h.
#include "ModList.h"

#include "QuestActivity.h"

#include <nlohmann/json.hpp>

namespace ModList {

void Open() {
#ifdef __ANDROID__
    QuestActivity::CallVoid("openModList");
#endif
}

Status Read() {
    Status status;
#ifdef __ANDROID__
    const nlohmann::json json = nlohmann::json::parse(QuestActivity::CallString("modListStatus"), nullptr, false);
    if (!json.is_object()) {
        return status;
    }
    status.state = static_cast<State>(json.value("state", 0));
    status.text = json.value("text", "");
    // UpdateText.BAR_MAX
    status.progress = static_cast<float>(json.value("bar", 0)) / 1000.0f;
    status.active = json.value("active", -1);
    if (json.contains("mods") && json["mods"].is_array()) {
        for (const auto& m : json["mods"]) {
            Mod mod;
            mod.name = m.value("name", "");
            mod.author = m.value("author", "");
            mod.page = m.value("page", "");
            mod.description = m.value("description", "");
            mod.size = m.value("size", "");
            mod.installedSize = m.value("installed_size", "");
            mod.installed = m.value("installed", false);
            status.mods.push_back(mod);
        }
    }
#endif
    return status;
}

void Download(int index) {
#ifdef __ANDROID__
    QuestActivity::CallVoid("downloadMod", index);
#endif
}

} // namespace ModList
