// SOH [Quest] Read Updater.h.
#include "Updater.h"

#include "QuestActivity.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "soh/SohGui/SohGui.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

namespace Updater {

Status Read() {
    Status status;
#ifdef __ANDROID__
    status.state = static_cast<State>(QuestActivity::CallInt("updateState"));
    status.text = QuestActivity::CallString("updateText");
    status.installed = QuestActivity::CallString("updateInstalled");
    status.latest = QuestActivity::CallString("updateLatest");
    // UpdateText.BAR_MAX
    status.progress = static_cast<float>(QuestActivity::CallInt("updateBar")) / 1000.0f;
#endif
    return status;
}

void Start() {
#ifdef __ANDROID__
    QuestActivity::CallVoid("startUpdate");
#endif
}

bool CanStart(State state) {
    return state == Available || state == Installing || state == Failed;
}

static bool sNoticeOpen = false;

bool NoticeOpen() {
    return sNoticeOpen;
}

void CloseNotice() {
    sNoticeOpen = false;
}

} // namespace Updater

#ifdef __ANDROID__
// The notice: on the file select screen, one time in each session, the menu opens at VR Settings >
// Update. The check runs while the game starts, so the file select screen asks again until it ends.
static bool sNoticeDone = false;

static void OnFileChooseMainUpdateNotice(void*) {
    static int sFramesToCheck = 0;
    if (sNoticeDone || --sFramesToCheck > 0) {
        return;
    }
    sFramesToCheck = 20; // JNI: three times a second.
    const Updater::State state = static_cast<Updater::State>(QuestActivity::CallInt("updateState"));
    if (state == Updater::Checking) {
        return;
    }
    sNoticeDone = true;
    if (state == Updater::Available) {
        CVarSetString(CVAR_SETTING("Menu.ActiveHeader"), "VR Settings");
        CVarSetString(CVAR_SETTING("Menu.VRSettingsSidebarSection"), "Update");
        Updater::sNoticeOpen = true;
        SohGui::ShowEscMenu();
    }
}

static void RegisterUpdateNotice() {
    COND_HOOK(OnFileChooseMain, true, OnFileChooseMainUpdateNotice);
}

static RegisterShipInitFunc initFunc(RegisterUpdateNotice, {});
#endif
