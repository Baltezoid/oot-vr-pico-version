#include "SohMenu.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "SohGui.hpp"
#include "soh/PlayerFolder.h" // SOH [Quest]
#include <libultraship/bridge/consolevariablebridge.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vr_interface.h>
#include <fast/vr_openxr.h>

namespace SohGui {

extern std::shared_ptr<SohMenu> mSohMenu;
using namespace UIWidgets;

static const std::map<int32_t, const char*> vrMirrorAxisOptions = {
    { 0, "X (finger axis)" },
    { 1, "Y" },
    { 2, "Z (thumb axis)" },
};

static const std::map<int32_t, const char*> vrViewModeOptions = {
    { 0, "Third Person" },
    { 1, "First Person" },
};

static const std::map<int32_t, const char*> vrTurnStyleOptions = {
    { 0, "Snap" },
    { 1, "Smooth" },
};

static const std::map<int32_t, const char*> vrRefreshRateOptions = {
    { 0, "Automatic" }, // SOH [VR] 90 Hz, or 72 Hz when frames are late (issue #83)
    { 72, "72 Hz" },
    { 80, "80 Hz" },
    { 90, "90 Hz" },
    { 120, "120 Hz" },
};

static const std::map<int32_t, const char*> vrHudAttachOptions = {
    { 0, "Head (Floating)" },
    { 1, "Left Hand" },
    { 2, "Right Hand" },
    { 3, "Wrist" },
};

// HUD Attachment modes: 0 = head, 1/2 = the whole HUD on one hand, 3 = wrist (split HUD). The
// wrist mode keeps its text boxes on the head panel, so it shares the head sliders.
static bool VrHudHasHeadPanel() {
    const int32_t attach = CVarGetInteger("gVrHudAttach", 0);
    return attach == 0 || attach == 3;
}

static bool VrHudIsOnHand() {
    const int32_t attach = CVarGetInteger("gVrHudAttach", 0);
    return attach == 1 || attach == 2;
}

static bool VrHudIsWrist() {
    return CVarGetInteger("gVrHudAttach", 0) == 3;
}

static const std::map<int32_t, const char*> vrItemSelHandOptions = {
    { 0, "Sword Hand" },
    { 1, "Off Hand" },
};

static const std::map<int32_t, const char*> vrItemSelInputOptions = {
    { VR_BTN_TRIGGER, "Trigger" },     { VR_BTN_GRIP, "Grip" },
    { VR_BTN_PRIMARY, "A / X" },       { VR_BTN_SECONDARY, "B / Y" },
    { VR_BTN_THUMBCLICK, "Stick Click" }, { VR_BTN_MENU, "Menu Button" },
};

// Sword-swap chord choices: no Trigger entry (both triggers are reserved: Z-target and the held
// item in selector mode), and 0 turns the chord off.
static const std::map<int32_t, const char*> vrItemSelSwapOptions = {
    { 0, "Disabled" },
    { VR_BTN_GRIP, "Both Grips" },
    { VR_BTN_PRIMARY, "A + X" },
    { VR_BTN_SECONDARY, "B + Y" },
    { VR_BTN_THUMBCLICK, "Both Stick Clicks" },
    { VR_BTN_MENU, "Both Menu Buttons" },
};

// --- Controls: N64-button-first binding editor (styled after the base game's bindings window:
// colored N64 chip per row, removable chips for each bound VR input, "+" to add). All state lives
// in the gVrBind* mask CVars that padmgr.c reads.
//
// There are THREE independent binding sets. The two GAMEPLAY sets are picked by gVrItemSelect,
// so swapping control schemes in the menu never costs you your bindings; selector mode reserves
// both TRIGGERS for using the held item, so their rows won't bind there. The OCARINA set takes
// over the controllers whenever the ocarina interface is up (in either scheme) — it maps notes,
// sharps/flats and put-away, and because every selector reservation stands down while playing,
// notes may live on any input including the triggers. Defaults must match
// sVrBindDefaults / sVrBindSelDefaults / sVrBindOcaDefaults in padmgr.c.
struct VrInputDef {
    const char* label;
    const char* cvar;
    int32_t defaultMask;
};
static const VrInputDef sVrInputDefsClassic[] = {
    { "L Trigger", "gVrBindLTrigger", BTN_Z },      { "L Grip", "gVrBindLGrip", BTN_R },
    { "X", "gVrBindLPrimary", BTN_CLEFT },          { "Y", "gVrBindLSecondary", 0 },
    { "L Stick", "gVrBindLStickClick", BTN_START }, { "L Menu", "gVrBindLMenu", BTN_CRIGHT },
    { "R Trigger", "gVrBindRTrigger", BTN_B },      { "R Grip", "gVrBindRGrip", 0 },
    { "A", "gVrBindRPrimary", BTN_A },              { "B", "gVrBindRSecondary", BTN_CDOWN },
    { "R Stick", "gVrBindRStickClick", 0 },         { "R Menu", "gVrBindRMenu", 0 },
};
static const VrInputDef sVrInputDefsSelector[] = {
    { "L Trigger", "gVrBindSelLTrigger", 0 },              { "L Grip", "gVrBindSelLGrip", BTN_R },
    { "X", "gVrBindSelLPrimary", 0 },                      { "Y", "gVrBindSelLSecondary", 0 },
    { "L Stick", "gVrBindSelLStickClick", 0 },             { "L Menu", "gVrBindSelLMenu", BTN_START },
    { "R Trigger", "gVrBindSelRTrigger", 0 },              { "R Grip", "gVrBindSelRGrip", 0 },
    { "A", "gVrBindSelRPrimary", BTN_A },                  { "B", "gVrBindSelRSecondary", BTN_B },
    { "R Stick", "gVrBindSelRStickClick", 0 },             { "R Menu", "gVrBindSelRMenu", 0 },
};
static const VrInputDef sVrInputDefsOcarina[] = {
    { "L Trigger", "gVrBindOcaLTrigger", 0 },         { "L Grip", "gVrBindOcaLGrip", BTN_Z },
    { "X", "gVrBindOcaLPrimary", 0 },                 { "Y", "gVrBindOcaLSecondary", 0 },
    { "L Stick", "gVrBindOcaLStickClick", 0 },        { "L Menu", "gVrBindOcaLMenu", 0 },
    { "R Trigger", "gVrBindOcaRTrigger", 0 },         { "R Grip", "gVrBindOcaRGrip", BTN_R },
    { "A", "gVrBindOcaRPrimary", BTN_A },             { "B", "gVrBindOcaRSecondary", BTN_B },
    { "R Stick", "gVrBindOcaRStickClick", 0 },        { "R Menu", "gVrBindOcaRMenu", 0 },
    // Stick DIRECTIONS, bindable in the ocarina set only (indices 12+, order up/down/left/right
    // per hand — the listener below computes 12 + hand * 4 + dir). A hand with any direction
    // bound claims that whole stick while playing: its stock job (left: pitch bend; right:
    // third-person C-stick) stands down. Matches sVrBindOcaStickCvars in padmgr.c.
    { "L Stick " ICON_FA_ARROW_UP, "gVrBindOcaLStickUp", 0 },
    { "L Stick " ICON_FA_ARROW_DOWN, "gVrBindOcaLStickDown", 0 },
    { "L Stick " ICON_FA_ARROW_LEFT, "gVrBindOcaLStickLeft", 0 },
    { "L Stick " ICON_FA_ARROW_RIGHT, "gVrBindOcaLStickRight", 0 },
    { "R Stick " ICON_FA_ARROW_UP, "gVrBindOcaRStickUp", BTN_CUP },
    { "R Stick " ICON_FA_ARROW_DOWN, "gVrBindOcaRStickDown", BTN_CDOWN },
    { "R Stick " ICON_FA_ARROW_LEFT, "gVrBindOcaRStickLeft", BTN_CLEFT },
    { "R Stick " ICON_FA_ARROW_RIGHT, "gVrBindOcaRStickRight", BTN_CRIGHT },
};
static const int kVrButtonInputCount = 12;
static const int kVrOcarinaInputCount = 20; // buttons + the 8 stick directions

// Which set the editor is showing: the active gameplay scheme, or the ocarina set.
static bool sVrEditOcarina = false;

static bool VrSelectorProfile() {
    return CVarGetInteger("gVrItemSelect", 1) != 0;
}
static const VrInputDef* VrInputDefs() {
    if (sVrEditOcarina) {
        return sVrInputDefsOcarina;
    }
    return VrSelectorProfile() ? sVrInputDefsSelector : sVrInputDefsClassic;
}
static int VrInputCount() {
    return sVrEditOcarina ? kVrOcarinaInputCount : kVrButtonInputCount;
}
// Dominant-axis stick direction: 0 up, 1 down, 2 left, 3 right, -1 centered. Must match the
// firing logic in padmgr.c so what binds here is what plays there.
static int VrStickDir(float x, float y) {
    if ((y * y) >= (x * x)) {
        return (y > 0.5f) ? 0 : (y < -0.5f) ? 1 : -1;
    }
    return (x < -0.5f) ? 2 : (x > 0.5f) ? 3 : -1;
}
// Indices 0 and 6 are the two triggers: reserved by selector mode, so they are not bindable —
// except in the ocarina set, where the reservations don't apply and triggers are prime note real
// estate. Index 3 is the left Y button: it opens the SoH menu at all times (vr_menu_input.cpp
// hides it from the game), so no set can bind it.
static bool VrInputReserved(int idx) {
    if (idx == 3) {
        return true;
    }
    if (sVrEditOcarina) {
        return false;
    }
    return VrSelectorProfile() && ((idx == 0) || (idx == 6));
}

struct VrN64RowDef {
    const char* label;
    uint16_t mask;
    ImVec4 color;
};

// Which N64 button row is currently listening for a physical VR press (0 = none), and the
// previous frame's controller state per hand for rising-edge detection (so a button already held
// when listening starts doesn't instantly bind).
static uint16_t sVrListenRowMask = 0;
static uint16_t sVrListenPrevBtn[2] = { 0, 0 };
// Previous frame's stick direction per hand (VrStickDir result), for the same rising-edge rule:
// a stick already deflected when listening starts must not instantly bind.
static int sVrListenPrevStickDir[2] = { -1, -1 };
// When listening started (ImGui time). With the Touch controllers, the menu navigation stands down
// while listening (every input can be the one to bind), so listening also ends after a time.
static double sVrListenStartTime = 0.0;
static const double kVrListenTimeout = 5.0;

static void VrInputBindingRow(const VrN64RowDef& row) {
    ImGui::PushID(row.label);

    // The N64 button chip (colored, inert — it's a label).
    ImGui::PushStyleColor(ImGuiCol_Button, row.color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, row.color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, row.color);
    ImGui::Button(row.label, ImVec2(96.0f, 0.0f)); // wide enough for the ocarina note labels
    ImGui::PopStyleColor(3);

    // One removable chip per VR input currently bound to this button.
    for (int i = 0; i < VrInputCount(); i++) {
        const VrInputDef& input = VrInputDefs()[i];
        int32_t cur = VrInputReserved(i) ? 0 : CVarGetInteger(input.cvar, input.defaultMask);
        if (cur & row.mask) {
            ImGui::SameLine();
            ImGui::PushID(input.cvar);
            char chip[48];
            snprintf(chip, sizeof(chip), "%s %s x", ICON_FA_GAMEPAD, input.label);
            if (ImGui::SmallButton(chip)) {
                CVarSetInteger(input.cvar, cur & ~row.mask);
                CVarSave();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Remove this binding");
            }
            ImGui::PopID();
        }
    }

    ImGui::SameLine();
    const bool listening = (sVrListenRowMask == row.mask);
    if (listening) {
        // Listening: press any input on either VR controller to bind it to this row.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.55f, 0.88f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.30f, 0.55f, 1.0f));
        char listenLabel[64];
        snprintf(listenLabel, sizeof(listenLabel), "Push a controller button (%.0f s). Select to cancel.",
                 kVrListenTimeout);
        if (ImGui::SmallButton(listenLabel)) {
            sVrListenRowMask = 0;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
            (ImGui::GetTime() - sVrListenStartTime) > kVrListenTimeout) {
            sVrListenRowMask = 0;
        }
        if (sVrListenRowMask != 0) {
            // The listener reads the raw controllers: hold the menu navigation and the Y toggle.
            VR_HoldMenuNavigation();
        }
        static const uint16_t sVrBtnBits[6] = { VR_BTN_TRIGGER,   VR_BTN_GRIP,       VR_BTN_PRIMARY,
                                                VR_BTN_SECONDARY, VR_BTN_THUMBCLICK, VR_BTN_MENU };
        for (int hand = 0; hand < 2 && sVrListenRowMask != 0; hand++) {
            uint16_t curBtn = VR_GetControllerButtonRaw(hand);
            uint16_t pressed = curBtn & ~sVrListenPrevBtn[hand];
            sVrListenPrevBtn[hand] = curBtn;
            for (int b = 0; b < 6; b++) {
                if (pressed & sVrBtnBits[b]) {
                    const int idx = hand * 6 + b;
                    if (VrInputReserved(idx)) {
                        continue; // reserved (the held item or the SoH menu) — keep listening
                    }
                    const VrInputDef& input = VrInputDefs()[idx];
                    CVarSetInteger(input.cvar, CVarGetInteger(input.cvar, input.defaultMask) | row.mask);
                    CVarSave();
                    sVrListenRowMask = 0;
                    break;
                }
            }
        }
        // Stick directions are bindable in the ocarina set only: a fresh deflection past the
        // threshold binds this row to that direction.
        for (int hand = 0; sVrEditOcarina && hand < 2 && sVrListenRowMask != 0; hand++) {
            float sx = 0.0f, sy = 0.0f;
            VR_GetThumbstickRaw(hand, &sx, &sy);
            const int dir = VrStickDir(sx, sy);
            const bool fresh = (dir >= 0) && (dir != sVrListenPrevStickDir[hand]);
            sVrListenPrevStickDir[hand] = dir;
            if (fresh) {
                const VrInputDef& input = VrInputDefs()[12 + hand * 4 + dir];
                CVarSetInteger(input.cvar, CVarGetInteger(input.cvar, input.defaultMask) | row.mask);
                CVarSave();
                sVrListenRowMask = 0;
            }
        }
    } else {
        // Blue "+": in VR, listen for a physical press; outside VR, fall back to a picker list.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.38f, 0.65f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.48f, 0.78f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.30f, 0.55f, 1.0f));
        if (ImGui::SmallButton("+")) {
            if (VR_IsInitialized()) {
                sVrListenRowMask = row.mask;
                sVrListenStartTime = ImGui::GetTime();
                for (int hand = 0; hand < 2; hand++) {
                    sVrListenPrevBtn[hand] = VR_GetControllerButtonRaw(hand);
                    float sx = 0.0f, sy = 0.0f;
                    VR_GetThumbstickRaw(hand, &sx, &sy);
                    sVrListenPrevStickDir[hand] = VrStickDir(sx, sy);
                }
            } else {
                ImGui::OpenPopup("VrAddBinding");
            }
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(VR_IsInitialized() ? "Then push the controller button to add"
                                                 : "Select the controller button to add");
        }
        if (ImGui::BeginPopup("VrAddBinding")) {
            for (int i = 0; i < VrInputCount(); i++) {
                if (VrInputReserved(i)) {
                    continue;
                }
                const VrInputDef& input = VrInputDefs()[i];
                int32_t cur = CVarGetInteger(input.cvar, input.defaultMask);
                if (!(cur & row.mask)) {
                    if (ImGui::MenuItem(input.label)) {
                        CVarSetInteger(input.cvar, cur | row.mask);
                        CVarSave();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    ImGui::PopID();
}

// Resets the binding set that the editor shows.
static void VrResetBindingsButton() {
    ImGui::Spacing();
    if (ImGui::Button("Reset to Default##VrInputs")) {
        for (int i = 0; i < VrInputCount(); i++) {
            CVarClear(VrInputDefs()[i].cvar);
        }
        CVarSave();
        sVrListenRowMask = 0;
    }
}

static void VrInputBindings(WidgetInfo& info) {
    static const VrN64RowDef sButtonRows[] = {
        { "A", BTN_A, ImVec4(0.22f, 0.24f, 0.50f, 1.0f) },
        { "B", BTN_B, ImVec4(0.12f, 0.35f, 0.14f, 1.0f) },
        { "Start", BTN_START, ImVec4(0.48f, 0.14f, 0.14f, 1.0f) },
        { "L", BTN_L, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "R", BTN_R, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "Z", BTN_Z, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "C " ICON_FA_ARROW_UP, BTN_CUP, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "C " ICON_FA_ARROW_DOWN, BTN_CDOWN, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "C " ICON_FA_ARROW_LEFT, BTN_CLEFT, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "C " ICON_FA_ARROW_RIGHT, BTN_CRIGHT, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
    };
    static const VrN64RowDef sDpadRows[] = {
        { "D " ICON_FA_ARROW_UP, BTN_DUP, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "D " ICON_FA_ARROW_DOWN, BTN_DDOWN, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "D " ICON_FA_ARROW_LEFT, BTN_DLEFT, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "D " ICON_FA_ARROW_RIGHT, BTN_DRIGHT, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
    };
    // The ocarina set is edited in NOTES, not N64 buttons: each row is the note (with the N64
    // button it stands for), plus the sharp/flat modifiers, put-away, and the free-play guard
    // (hold L: songs are not recognized, so nothing interrupts noodling).
    static const VrN64RowDef sOcarinaRows[] = {
        { "D4 (A)", BTN_A, ImVec4(0.22f, 0.24f, 0.50f, 1.0f) },
        { "F4 (C" ICON_FA_ARROW_DOWN ")", BTN_CDOWN, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "A4 (C" ICON_FA_ARROW_RIGHT ")", BTN_CRIGHT, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "B4 (C" ICON_FA_ARROW_LEFT ")", BTN_CLEFT, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "D5 (C" ICON_FA_ARROW_UP ")", BTN_CUP, ImVec4(0.60f, 0.44f, 0.06f, 1.0f) },
        { "Sharp (R)", BTN_R, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "Flat (Z)", BTN_Z, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
        { "Put Away", BTN_B, ImVec4(0.12f, 0.35f, 0.14f, 1.0f) },
        { "No Song (L)", BTN_L, ImVec4(0.32f, 0.32f, 0.32f, 1.0f) },
    };

    // Which binding set is being edited. The gameplay tab follows the active scheme
    // (gVrItemSelect), so flipping the selector toggle swaps schemes with bindings intact; the
    // ocarina set is its own thing, in force whenever the ocarina interface is up.
    if (ImGui::RadioButton(VrSelectorProfile() ? "Game (Item Selector)" : "Game (C Buttons)", !sVrEditOcarina)) {
        sVrEditOcarina = false;
        sVrListenRowMask = 0;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Ocarina", sVrEditOcarina)) {
        sVrEditOcarina = true;
        sVrListenRowMask = 0;
    }
    ImGui::Separator();

    if (sVrEditOcarina) {
        ImGui::TextWrapped("These buttons apply while you play the ocarina. The notes go from low to "
                           "high: D4, F4, A4, B4, D5. The left stick bends the pitch. You can also add a "
                           "stick direction. Then that stick plays notes only.");
        ImGui::Separator();
        for (const VrN64RowDef& row : sOcarinaRows) {
            VrInputBindingRow(row);
        }
        VrResetBindingsButton();
        return;
    }
    if (VrSelectorProfile()) {
        ImGui::TextWrapped("The sword-hand trigger is Z-target. The other trigger uses the item in the "
                           "hand. Y opens this menu.");
    } else {
        ImGui::TextWrapped("Push the C button of an item to use it. Y opens this menu.");
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Buttons##VrInputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const VrN64RowDef& row : sButtonRows) {
            VrInputBindingRow(row);
        }
    }
    if (ImGui::CollapsingHeader("D-Pad##VrInputs", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const VrN64RowDef& row : sDpadRows) {
            VrInputBindingRow(row);
        }
    }
    VrResetBindingsButton();
}

// --- Control schemes: the turning CVars, the item selector on, and the default bindings. The first
// start in VR asks for one. gVrControlSchemeAsked records the answer.
struct VrControlScheme {
    const char* name;
    const char* summary;
    int32_t turnOn;
    int32_t turnStyle;
};
static const VrControlScheme sVrControlSchemes[] = {
    { "Default", "Turn with your body. Flick the right stick to take a C item into your hand.", 0, 0 },
    { "Shipwright-VR", "Turn smoothly with the right stick. Use the item selector to take items.", 1, 1 },
};

static void VrApplyControlScheme(int idx) {
    const VrControlScheme& scheme = sVrControlSchemes[idx];
    CVarSetInteger("gVrControlScheme", idx);
    CVarSetInteger("gVrItemSelect", 1);
    CVarSetInteger("gVrSnapTurnOn", scheme.turnOn);
    CVarSetInteger("gVrTurnStyle", scheme.turnStyle);
    for (int i = 0; i < kVrButtonInputCount; i++) {
        CVarClear(sVrInputDefsSelector[i].cvar);
    }
    for (int i = 0; i < kVrOcarinaInputCount; i++) {
        CVarClear(sVrInputDefsOcarina[i].cvar);
    }
    CVarSave();
    // The selector hooks register on this CVar; the widgets do the same after a change.
    ShipInit::Init("gVrItemSelect");
    sVrListenRowMask = 0;
}

// Shows in place of Quick Setup until the player answers.
static void VrControlSchemeQuestion() {
    ImGui::PushFont(OTRGlobals::Instance->fontStandardLargest);
    ImGui::TextUnformatted("How do you want to turn?");
    ImGui::PopFont();
    ImGui::Spacing();
    const ImVec2 buttonSize(260.0f, 70.0f);
    static const char* sAnswers[2] = { "With my body", "With the stick" };
    for (int i = 0; i < 2; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        if (ImGui::Button(sAnswers[i], buttonSize)) {
            VrApplyControlScheme(i);
            CVarSetInteger("gVrControlSchemeAsked", 1);
            CVarSave();
            mSohMenu->Hide();
        }
        // Focus the first answer, so that A on the Touch controller answers at once.
        if (i == 0 && ImGui::IsWindowAppearing()) {
            ImGui::SetItemDefaultFocus();
            ImGui::SetKeyboardFocusHere(-1);
        }
        ImGui::TextDisabled("%s", sVrControlSchemes[i].name);
        ImGui::TextWrapped("%s", sVrControlSchemes[i].summary);
        ImGui::EndGroup();
    }
    ImGui::Spacing();
    ImGui::TextWrapped("You can change this later in VR Settings > Quick Setup.");
    ImGui::Separator();
}

// Opens Quick Setup at each start in VR until the player answers the question. Waits 60 game
// frames, so that the headset shows the game first.
static void VrControlSchemeFirstStartTick() {
    static bool sAskedThisSession = false;
    static int sFramesInVr = 0;
    if (sAskedThisSession || CVarGetInteger("gVrControlSchemeAsked", 0) || !VR_IsInitialized() ||
        !VR_GetFirstPerson()) {
        return;
    }
    if (++sFramesInVr < 60) {
        return;
    }
    sAskedThisSession = true;
    CVarSetString(CVAR_SETTING("Menu.ActiveHeader"), "VR Settings");
    CVarSetString(CVAR_SETTING("Menu.VRSettingsSidebarSection"), "Quick Setup");
    mSohMenu->Show();
}

static void RegisterVrControlSchemeFirstStart() {
    COND_HOOK(OnGameFrameUpdate, !CVarGetInteger("gVrControlSchemeAsked", 0), VrControlSchemeFirstStartTick);
}

static RegisterShipInitFunc initVrControlSchemeFirstStart(RegisterVrControlSchemeFirstStart,
                                                          { "gVrControlSchemeAsked" });

// --- Quick Setup (#104): rows of large buttons for the main choices. They write the same CVars as
// the old widgets.
struct VrChoice {
    const char* label;
    const char* detail; // second line, or nullptr
};

static void VrSetCVar(const char* cvar, int32_t value) {
    CVarSetInteger(cvar, value);
    CVarSave();
    ShipInit::Init(cvar);
}

// Returns the index of the pushed button, or -1.
static int VrChoiceRow(const char* id, const VrChoice* choices, int count, int selected, float width) {
    const ImGuiStyle& style = ImGui::GetStyle();
    bool hasDetail = false;
    for (int i = 0; i < count; i++) {
        hasDetail |= (choices[i].detail != nullptr);
    }
    const float lineHeight = ImGui::GetTextLineHeight();
    const float height = lineHeight * (hasDetail ? 2.0f : 1.0f) + style.FramePadding.y * 4.0f;
    const float buttonWidth = (width - style.ItemSpacing.x * (float)(count - 1)) / (float)count;
    const ImU32 selectedFill = ImGui::GetColorU32(UIWidgets::ColorValues.at(THEME_COLOR));
    const ImU32 fill = ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
    const ImU32 hoveredFill = ImGui::GetColorU32(ImVec4(0.28f, 0.28f, 0.31f, 1.0f));
    const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 detailText = ImGui::GetColorU32(ImVec4(0.75f, 0.75f, 0.78f, 1.0f));
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    int pressed = -1;
    ImGui::PushID(id);
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            ImGui::SameLine();
        }
        ImGui::PushID(i);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + buttonWidth, min.y + height);
        if (ImGui::InvisibleButton("##choice", ImVec2(buttonWidth, height))) {
            pressed = i;
        }
        const bool on = (i == selected);
        drawList->AddRectFilled(min, max, on ? selectedFill : ImGui::IsItemHovered() ? hoveredFill : fill, 6.0f);
        if (on) {
            drawList->AddRect(min, max, IM_COL32(255, 255, 255, 220), 6.0f, 0, 2.0f);
        }
        ImGui::RenderNavCursor(ImRect(min, max), ImGui::GetItemID());

        drawList->PushClipRect(min, max, true);
        const char* detail = choices[i].detail;
        float y = min.y + (height - lineHeight * (detail != nullptr ? 2.0f : 1.0f)) * 0.5f;
        const ImVec2 labelSize = ImGui::CalcTextSize(choices[i].label);
        drawList->AddText(ImVec2(min.x + (buttonWidth - labelSize.x) * 0.5f, y), text, choices[i].label);
        if (detail != nullptr) {
            const ImVec2 detailSize = ImGui::CalcTextSize(detail);
            drawList->AddText(ImVec2(min.x + (buttonWidth - detailSize.x) * 0.5f, y + lineHeight),
                              on ? text : detailText, detail);
        }
        drawList->PopClipRect();
        ImGui::PopID();
    }
    ImGui::PopID();
    return pressed;
}

// A title and a row of large buttons.
static int VrChoiceCard(const char* title, const VrChoice* choices, int count, int selected, float width) {
    ImGui::BeginGroup();
    ImGui::TextUnformatted(title);
    const int pressed = VrChoiceRow(title, choices, count, selected, width);
    ImGui::EndGroup();
    return pressed;
}

static void VrViewCard(float width) {
    static const VrChoice sChoices[] = { { "First Person", "You are Link" }, { "Third Person", "Camera behind Link" } };
    const int pressed = VrChoiceCard("View", sChoices, 2, CVarGetInteger("gVrFirstPerson", 1) ? 0 : 1, width);
    if (pressed >= 0) {
        VrSetCVar("gVrFirstPerson", pressed == 0 ? 1 : 0);
    }
}

static void VrSwordHandCard(float width) {
    static const VrChoice sChoices[] = { { "Right", nullptr }, { "Left", nullptr } };
    const int pressed = VrChoiceCard("Sword Hand", sChoices, 2, CVarGetInteger("gVrLeftHanded", 0) ? 1 : 0, width);
    if (pressed >= 0) {
        VrSetCVar("gVrLeftHanded", pressed);
    }
}

// In first person, the right stick takes C items or turns. It does not do the two.
static void VrRightStickCard(float width) {
    static const VrChoice sChoices[] = { { "Items", "Turn with your body" },
                                         { "Snap Turn", "Turn in steps" },
                                         { "Smooth Turn", "Turn without steps" } };
    const int current = !CVarGetInteger("gVrSnapTurnOn", 0) ? 0 : (CVarGetInteger("gVrTurnStyle", 0) == 0 ? 1 : 2);
    const int pressed = VrChoiceCard("Right Stick", sChoices, 3, current, width);
    if (pressed >= 0) {
        VrSetCVar("gVrSnapTurnOn", pressed != 0);
        if (pressed != 0) {
            VrSetCVar("gVrTurnStyle", pressed - 1);
        }
    }
}

// The text under each button follows the right stick choice.
static void VrUseItemsCard(float width) {
    const bool stickTakesItems = !CVarGetInteger("gVrSnapTurnOn", 0);
    const int32_t selectorInput = CVarGetInteger("gVrItemSelInput", VR_BTN_THUMBCLICK);
    const auto inputName = vrItemSelInputOptions.find(selectorInput);
    char selectorDetail[64];
    snprintf(selectorDetail, sizeof(selectorDetail), stickTakesItems ? "Flick the stick, or hold %s" : "Hold %s",
             inputName != vrItemSelInputOptions.end() ? inputName->second : "the selector input");
    const VrChoice choices[] = {
        { "Item Selector", selectorDetail },
        { "C Buttons", stickTakesItems ? "Flick the stick, or push a C button" : "Push a C button" },
    };
    const int pressed = VrChoiceCard("Use Items", choices, 2, CVarGetInteger("gVrItemSelect", 1) ? 0 : 1, width);
    if (pressed >= 0) {
        VrSetCVar("gVrItemSelect", pressed == 0 ? 1 : 0);
        sVrListenRowMask = 0;
    }
}

static void VrHudCard(float width) {
    // gVrHudAttach values in the order of the buttons: head, wrist, left hand, right hand.
    static const int32_t sValues[] = { 0, 3, 1, 2 };
    static const VrChoice sChoices[] = {
        { "In Front", nullptr }, { "On the Wrist", nullptr }, { "Left Hand", nullptr }, { "Right Hand", nullptr }
    };
    const int32_t attach = CVarGetInteger("gVrHudAttach", 0);
    int current = 0;
    for (int i = 0; i < 4; i++) {
        if (sValues[i] == attach) {
            current = i;
        }
    }
    const int pressed = VrChoiceCard("Hearts and Items (HUD)", sChoices, 4, current, width);
    if (pressed >= 0) {
        VrSetCVar("gVrHudAttach", sValues[pressed]);
    }
}

static void VrQuickSetup(WidgetInfo& info) {
    if (!CVarGetInteger("gVrControlSchemeAsked", 0)) {
        VrControlSchemeQuestion();
        return;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width = ImGui::GetContentRegionAvail().x;
    const float half = (width - style.ItemSpacing.x) * 0.5f;
    VrViewCard(half);
    ImGui::SameLine();
    VrSwordHandCard(half);
    ImGui::Dummy(ImVec2(0.0f, style.ItemSpacing.y * 2.0f));
    VrRightStickCard(width);
    ImGui::Dummy(ImVec2(0.0f, style.ItemSpacing.y * 2.0f));
    VrUseItemsCard(width);
    ImGui::Dummy(ImVec2(0.0f, style.ItemSpacing.y * 2.0f));
    VrHudCard(width);
}

#ifdef __ANDROID__
// SOH [Quest] The player folder and its permission. Read soh/PlayerFolder.h.
static void VrModsFolder(WidgetInfo& info) {
    static int sFramesToCheck = 0;
    static bool sGranted = false;
    if (--sFramesToCheck <= 0) {
        sGranted = PlayerFolder::HasAccess(); // JNI: once a second.
        sFramesToCheck = 72;
    }
    if (!PlayerFolder::Path().empty()) {
        ImGui::TextWrapped("Copy mods into oot-vr/mods, then start the game again.");
        return;
    }
    if (sGranted) {
        ImGui::TextWrapped("Start the game again to use the folder oot-vr.");
        return;
    }
    ImGui::TextWrapped("Allow access to make the folder oot-vr for mods and saves.");
    ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
    static const VrChoice sChoices[] = { { "Allow Access", nullptr } };
    if (VrChoiceRow("VrModsAccess", sChoices, 1, -1, ImGui::GetContentRegionAvail().x) == 0) {
        PlayerFolder::RequestAccess();
        sFramesToCheck = 0;
    }
}
#endif

static void VrRightStickCardWidget(WidgetInfo& info) {
    VrRightStickCard(ImGui::GetContentRegionAvail().x);
}

static void VrUseItemsCardWidget(WidgetInfo& info) {
    VrUseItemsCard(ImGui::GetContentRegionAvail().x);
}

static void VrHudCardWidget(WidgetInfo& info) {
    VrHudCard(ImGui::GetContentRegionAvail().x);
}

// Live frame-cost breakdown. The interesting number is "XR wait": that is time spent blocked in
// xrWaitFrame, i.e. spare headroom. When it trends toward zero the frame no longer fits and the
// compositor starts reprojecting.
static void VrPerformanceReadout(WidgetInfo& info) {
    if (!VR_IsInitialized()) {
        ImGui::TextUnformatted("Not in VR.");
        return;
    }

    VrFrameStats s = {};
    vr_get_frame_stats(&s);

    ImGui::Text("XR frames submitted   %6.1f Hz", s.frame_hz);
    ImGui::Text("Stereo pairs rendered %6.1f Hz", s.eye_hz);
    ImGui::Separator();
    ImGui::Text("XR wait (headroom)  %6.2f ms", s.wait_ms);
    ImGui::Text("Both eye passes     %6.2f ms", s.eyes_ms);
    ImGui::Text("HUD quad pass       %6.2f ms", s.hud_ms);
    ImGui::Text("Companion window    %6.2f ms", s.desktop_ms);
    ImGui::Text("Whole frame         %6.2f ms", s.frame_ms);
    ImGui::Separator();
    ImGui::Text("Game logic tick     %6.2f ms", s.tick_ms);
    ImGui::TextUnformatted("(game logic runs once per 20 Hz tick, on this\n"
                           "same thread, so it comes out of the render budget)");
}

// Live hand-speed readout for tuning physical combat: current speed plus a slowly-bleeding peak
// per hand, in physical meters/second — the unit every swing threshold is tuned in, independent
// of world scale and Link's age. Runs at menu (render) rate, so it shows every XR frame's sample.
static void VrPhysCombatReadout(WidgetInfo& info) {
    if (!VR_IsInitialized()) {
        ImGui::TextUnformatted("Not in VR.");
        return;
    }
    static float sPeak[2] = { 0.0f, 0.0f };
    const float dt = ImGui::GetIO().DeltaTime;
    static const char* sNames[2] = { "Left ", "Right" };
    for (int hand = 0; hand < 2; hand++) {
        float lin[3];
        float ang[3];
        float speed = 0.0f;
        if (VR_GetHandVelocity(hand, lin, ang)) {
            speed = sqrtf(lin[0] * lin[0] + lin[1] * lin[1] + lin[2] * lin[2]);
        }
        sPeak[hand] = fmaxf(speed, sPeak[hand] - 2.0f * dt); // bleed 2 m/s per second
        ImGui::Text("%s hand  %5.2f m/s   peak %5.2f m/s", sNames[hand], speed, sPeak[hand]);
    }
    ImGui::TextUnformatted("Swing a controller and watch the numbers move.");
}

// Physics flight recorder. The checkbox arms a ring buffer holding the most recent ~20 s of sim
// steps; switching it off writes the capture to CSV next to the executable. Recording keeps the
// LAST window rather than the first, so the workflow is: enable, go reproduce the problem, then
// disable — whatever just happened is in the file.
static void VrPhysLogControl(WidgetInfo& info) {
    static bool sWasLogging = false;
    static char sStatus[512] = "";

    const bool wantLog = CVarGetInteger("gVrPhysLog", 0) != 0;
    if (wantLog != sWasLogging) {
        sWasLogging = wantLog;
        if (wantLog) {
            VR_PhysLogSetEnabled(true);
            snprintf(sStatus, sizeof(sStatus), "Recording...");
        } else {
            VR_PhysLogSetEnabled(false);
            const char* path = "vr_phys_log.csv";
            const int32_t n = VR_PhysLogWrite(path);
            if (n > 0) {
                // SOH [Quest] MAX_PATH and _fullpath are MSVC-only; realpath is the POSIX
                // equivalent, and it needs the file to exist - which it does, we just wrote it.
                char abs[1024] = "";
#ifdef _WIN32
                if (_fullpath(abs, path, sizeof(abs)) == nullptr) {
                    snprintf(abs, sizeof(abs), "%s", path);
                }
#else
                if (realpath(path, abs) == nullptr) {
                    snprintf(abs, sizeof(abs), "%s", path);
                }
#endif
                snprintf(sStatus, sizeof(sStatus), "Wrote %d samples to:\n%s", n, abs);
            } else if (n == 0) {
                snprintf(sStatus, sizeof(sStatus), "Nothing captured (was the sword in hand, "
                                                   "with Physical Combat + blade inertia on?)");
            } else {
                snprintf(sStatus, sizeof(sStatus), "Could not open the log file for writing.");
            }
        }
    }

    if (wantLog) {
        ImGui::Text("Recording: %d samples buffered", VR_PhysLogCount());
        ImGui::TextUnformatted("Reproduce the problem, then turn this off to write the file.");
    } else if (sStatus[0] != '\0') {
        ImGui::TextUnformatted(sStatus);
    }
}

void SohMenu::AddMenuVRSettings() {
    AddMenuEntry("VR Settings", CVAR_SETTING("Menu.VRSettingsSidebarSection"));

    // --------------------------------------------------------------- Quick Setup
    AddSidebarEntry("VR Settings", "Quick Setup", 1);
    WidgetPath quickPath = { "VR Settings", "Quick Setup", SECTION_COLUMN_1 };
    AddWidget(quickPath, "VrQuickSetup", WIDGET_CUSTOM).CustomFunction(VrQuickSetup).HideInSearch(true);

    // ------------------------------------------------------------------ Movement
    AddSidebarEntry("VR Settings", "Movement", 1);
    WidgetPath movePath = { "VR Settings", "Movement", SECTION_COLUMN_1 };

    AddWidget(movePath, "VrRightStickCard", WIDGET_CUSTOM).CustomFunction(VrRightStickCardWidget).HideInSearch(true);
    AddWidget(movePath, "Turning", WIDGET_SEPARATOR_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger("gVrSnapTurnOn", 0);
    });
    AddWidget(movePath, "Snap Turn Angle: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrSnapTurnDegrees")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrSnapTurnOn", 0) || CVarGetInteger("gVrTurnStyle", 0) != 0;
        })
        .Options(FloatSliderOptions().Min(10.0f).Max(180.0f).DefaultValue(45.0f).Step(5.0f).Format("%.0f").Tooltip(
            "The angle of one turn step."));
    AddWidget(movePath, "Smooth Turn Speed: %.0f deg/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrSmoothTurnSpeed")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrSnapTurnOn", 0) || CVarGetInteger("gVrTurnStyle", 0) != 1;
        })
        .Options(FloatSliderOptions().Min(30.0f).Max(360.0f).DefaultValue(120.0f).Step(5.0f).Format("%.0f").Tooltip(
            "The turn speed when you push the stick fully."));
    AddWidget(movePath, "Stick Dead Zone: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrSmoothTurnDeadzone")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrSnapTurnOn", 0) || CVarGetInteger("gVrTurnStyle", 0) != 1;
        })
        .Options(FloatSliderOptions().Min(0.05f).Max(0.80f).DefaultValue(0.25f).Step(0.05f).Format("%.2f").Tooltip(
            "How far you push the stick before the turn starts. Increase it if the view turns when "
            "your thumb is on the stick."));
    AddWidget(movePath, "Turn Speed Follows the Stick", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrSmoothTurnAnalog")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrSnapTurnOn", 0) || CVarGetInteger("gVrTurnStyle", 0) != 1;
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "On: a small push turns slowly. Off: each push turns at full speed."));

    AddWidget(movePath, "Body", WIDGET_SEPARATOR_TEXT);
    AddWidget(movePath, "Link Walks Where You Look", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrBodyFollowsHead")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrEnabled", 1) || !CVarGetInteger("gVrFirstPerson", 1);
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "On: Link faces where you look and moves as the stick moves, with no delay. This is "
            "better for VR comfort. Off: Link moves as in the original game."));
    AddWidget(movePath, "Make Me the Size of Link", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrAutoWorldScale")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The game measures your eye height and makes the world match it. Stand normally when you "
            "recenter the view. Off: World Scale sets the size of the world."));
    AddWidget(movePath, "World Scale: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrWorldScale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = CVarGetInteger("gVrAutoWorldScale", 1); })
        .Options(FloatSliderOptions().Min(10.0f).Max(100.0f).DefaultValue(35.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Game units for each meter. A higher value makes the world smaller."));
    AddWidget(movePath, "Eye Height: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHeadHeightOffset")
        .Options(FloatSliderOptions().Min(-100.0f).Max(100.0f).DefaultValue(-9.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Moves your eyes up or down from the eyes of Link, in game units."));

    AddWidget(movePath, "Z-Targeting", WIDGET_SEPARATOR_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger("gVrEnabled", 1) || !CVarGetInteger("gVrFirstPerson", 1);
    });
    // The lock-on mode of Legaiaflame.
    AddWidget(movePath, "Turn the View to the Target", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrLegaiaLockOn")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrEnabled", 1) || !CVarGetInteger("gVrFirstPerson", 1);
        })
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "On: while you Z-target, the view turns to keep the target in front of you. This can make "
            "some players feel sick. Off: the view never turns for you."));
    AddWidget(movePath, "Free Look Angle: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrLockOnDeadzone")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrLegaiaLockOn", 0); })
        .Options(FloatSliderOptions().Min(0.0f).Max(80.0f).DefaultValue(0.0f).Step(5.0f).Format("%.0f").Tooltip(
            "The target can move this far from the center before the view turns. 0 keeps the target "
            "in the center."));
    AddWidget(movePath, "View Turn Speed: %.0f deg/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrLockOnTurnSpeed")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrLegaiaLockOn", 0); })
        .Options(FloatSliderOptions().Min(30.0f).Max(360.0f).DefaultValue(120.0f).Step(10.0f).Format("%.0f").Tooltip(
            "The maximum speed of the view turn. A lower value is more comfortable."));

    AddWidget(movePath, "Camera", WIDGET_SEPARATOR_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger("gVrEnabled", 1) || !CVarGetInteger("gVrFirstPerson", 1);
    });
    AddWidget(movePath, "Use the Game Camera Far From Link", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrFpAutoDirectorCam")
        .PreFunc([](WidgetInfo& info) {
            info.isHidden = !CVarGetInteger("gVrEnabled", 1) || !CVarGetInteger("gVrFirstPerson", 1);
        })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "On: when a cutscene shows a place far from Link, you see it from the game camera. Off: "
            "you always see from the eyes of Link."));

    // ------------------------------------------------------------------ Controls
    AddSidebarEntry("VR Settings", "Controls", 1);
    WidgetPath controlsPath = { "VR Settings", "Controls", SECTION_COLUMN_1 };

    AddWidget(controlsPath, "VrUseItemsCard", WIDGET_CUSTOM).CustomFunction(VrUseItemsCardWidget).HideInSearch(true);
    AddWidget(controlsPath, "Item Selector", WIDGET_SEPARATOR_TEXT).PreFunc([](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger("gVrItemSelect", 1);
    });
    AddWidget(controlsPath, "Selector Hand", WIDGET_CVAR_COMBOBOX)
        .CVar("gVrItemSelHand")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrItemSelect", 1); })
        .Options(ComboboxOptions()
                     .DefaultIndex(0)
                     .ComboMap(vrItemSelHandOptions)
                     .Tooltip("The hand that opens the item selector."));
    AddWidget(controlsPath, "Hold to Open", WIDGET_CVAR_COMBOBOX)
        .CVar("gVrItemSelInput")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrItemSelect", 1); })
        .Options(ComboboxOptions()
                     .DefaultIndex(VR_BTN_THUMBCLICK)
                     .ComboMap(vrItemSelInputOptions)
                     .Tooltip("Hold this button to open the item selector. This button does not do its usual job."));
    AddWidget(controlsPath, "Sword and Shield (Both Hands)", WIDGET_CVAR_COMBOBOX)
        .CVar("gVrItemSelSwapInput")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrItemSelect", 1); })
        .Options(ComboboxOptions()
                     .DefaultIndex(VR_BTN_GRIP)
                     .ComboMap(vrItemSelSwapOptions)
                     .Tooltip("Push this button on the two controllers at the same time. Link puts away the item and "
                              "takes the sword and the shield."));
    AddWidget(controlsPath, "Selector Hand Distance: %.0f cm", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrItemSelDistance")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrItemSelect", 1); })
        .Options(FloatSliderOptions().Min(3.0f).Max(30.0f).DefaultValue(5.0f).Step(1.0f).Format("%.0f").Tooltip(
            "How far your hand must move to select an item."));

    AddWidget(controlsPath, "Buttons", WIDGET_SEPARATOR_TEXT);
    AddWidget(controlsPath, "VrInputBindings", WIDGET_CUSTOM).CustomFunction(VrInputBindings).HideInSearch(true);
    AddWidget(controlsPath, "Select a button to remove it. Select + to add one.", WIDGET_TEXT);

    // ----------------------------------------------------------------------- HUD
    AddSidebarEntry("VR Settings", "HUD", 1);
    WidgetPath hudPath = { "VR Settings", "HUD", SECTION_COLUMN_1 };

    AddWidget(hudPath, "VrHudCard", WIDGET_CUSTOM).CustomFunction(VrHudCardWidget).HideInSearch(true);
    AddWidget(hudPath, "Size and Position", WIDGET_SEPARATOR_TEXT);
    AddWidget(hudPath, "HUD Distance: %.1f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudDistance")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudHasHeadPanel(); })
        .Options(FloatSliderOptions().Min(0.5f).Max(5.0f).DefaultValue(2.0f).Step(0.1f).Format("%.1f"));
    AddWidget(hudPath, "HUD Size: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudSize")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudHasHeadPanel(); })
        .Options(FloatSliderOptions().Min(0.2f).Max(3.0f).DefaultValue(1.5f).Step(0.05f).Format("%.2f"));
    AddWidget(hudPath, "HUD Horizontal: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudOffX")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudHasHeadPanel(); })
        .Options(FloatSliderOptions().Min(-1.5f).Max(1.5f).DefaultValue(0.0f).Step(0.02f).Format("%.2f"));
    AddWidget(hudPath, "HUD Vertical: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudOffY")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudHasHeadPanel(); })
        .Options(FloatSliderOptions().Min(-1.5f).Max(1.5f).DefaultValue(0.0f).Step(0.02f).Format("%.2f"));
    AddWidget(hudPath, "Hand HUD Size: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudHandSize")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsOnHand(); })
        .Options(FloatSliderOptions().Min(0.1f).Max(1.0f).DefaultValue(0.35f).Step(0.01f).Format("%.2f"));
    AddWidget(hudPath, "Hand HUD Sideways: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudHandOffX")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsOnHand(); })
        .Options(FloatSliderOptions().Min(-0.5f).Max(0.5f).DefaultValue(0.0f).Step(0.01f).Format("%.2f").Tooltip(
            "Moves the HUD to the side. The other hand uses the mirror value."));
    AddWidget(hudPath, "Hand HUD Up: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudHandOffY")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsOnHand(); })
        .Options(FloatSliderOptions().Min(-0.5f).Max(0.5f).DefaultValue(0.10f).Step(0.01f).Format("%.2f"));
    AddWidget(hudPath, "Hand HUD Forward: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudHandOffZ")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsOnHand(); })
        .Options(FloatSliderOptions().Min(-0.5f).Max(0.5f).DefaultValue(-0.08f).Step(0.01f).Format("%.2f"));
    AddWidget(hudPath, "Hand HUD Tilt: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHudHandPitch")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsOnHand(); })
        .Options(FloatSliderOptions().Min(-90.0f).Max(90.0f).DefaultValue(-40.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Tilts the HUD toward your eyes."));
    AddWidget(hudPath, "Show Wrist HUD Only When Looking", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrWristHudGlance")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsWrist(); })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "On: hearts, rupees, and the map show only when you look at the back of your wrist. Off: "
            "they always show."));
    AddWidget(hudPath, "Wrist HUD Size: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrWristHudSize")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsWrist(); })
        .Options(FloatSliderOptions().Min(0.08f).Max(0.5f).DefaultValue(0.20f).Step(0.01f).Format("%.2f").Tooltip(
            "The width of the wrist HUD. The item buttons above the controller use the same scale."));
    AddWidget(hudPath, "Wrist HUD Sideways: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrWristHudOffX")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsWrist(); })
        .Options(FloatSliderOptions().Min(-0.3f).Max(0.3f).DefaultValue(-0.04f).Step(0.01f).Format("%.2f").Tooltip(
            "A negative value moves the HUD to the back of the hand. The other hand uses the mirror "
            "value."));
    AddWidget(hudPath, "Wrist HUD Up: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrWristHudOffY")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsWrist(); })
        .Options(FloatSliderOptions().Min(-0.3f).Max(0.3f).DefaultValue(0.02f).Step(0.01f).Format("%.2f"));
    AddWidget(hudPath, "Wrist HUD Along Forearm: %.2f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrWristHudOffZ")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !VrHudIsWrist(); })
        .Options(FloatSliderOptions().Min(-0.3f).Max(0.4f).DefaultValue(0.10f).Step(0.01f).Format("%.2f").Tooltip(
            "A positive value moves the HUD toward the elbow."));
    AddWidget(hudPath, "Z-Target Mark Size: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrReticleScale")
        .Options(FloatSliderOptions().Min(0.3f).Max(3.0f).DefaultValue(1.0f).Step(0.05f).Format("%.2f").Tooltip(
            "The size of the triangles around the target."));

    AddWidget(hudPath, "Menu Panel", WIDGET_SEPARATOR_TEXT);
    AddWidget(hudPath, "Menu Panel Distance: %.1f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrScreenDistance")
        .Options(FloatSliderOptions().Min(0.5f).Max(5.0f).DefaultValue(2.2f).Step(0.1f).Format("%.1f").Tooltip(
            "The distance to this menu, the file select, and the pause screen. It changes the next "
            "time a menu opens."));
    AddWidget(hudPath, "Menu Panel Size: %.1f m", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrScreenSize")
        .Options(FloatSliderOptions().Min(0.5f).Max(5.0f).DefaultValue(2.4f).Step(0.1f).Format("%.1f").Tooltip(
            "The width of the menu panel."));

    // ------------------------------------------------------------------- Display
    AddSidebarEntry("VR Settings", "Display", 1);
    WidgetPath displayPath = { "VR Settings", "Display", SECTION_COLUMN_1 };

    AddWidget(displayPath, "VR Mode", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrEnabled")
#ifdef __ANDROID__
        // On the Quest there is no flat-screen play: with VR off the headset shows nothing, the
        // Touch controllers cannot open this menu again, and the next start stays out of VR.
        .PreFunc([](WidgetInfo& info) { info.isHidden = true; })
#endif
        .Options(
            CheckboxOptions().DefaultValue(true).Tooltip("Changes between VR and the flat screen. F9 does the same."));
    AddWidget(displayPath, "Stay in VR When the Headset Is Off", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrStayOnDoff")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrEnabled", 1); })
        .Options(CheckboxOptions().Tooltip(
            "On: the game stays in VR when you take off the headset. Off: the game changes to the "
            "flat screen."));
    AddWidget(displayPath, "Headset Refresh Rate", WIDGET_CVAR_COMBOBOX)
        .CVar("gVrRefreshRate")
        .Options(ComboboxOptions()
                     .DefaultIndex(0)
                     .ComboMap(vrRefreshRateOptions)
                     .Tooltip("How many images per second the headset shows. Automatic (default) "
                              "uses 90 Hz, and 72 Hz when frames are late, for example in Hyrule "
                              "Field. A fixed rate does not change."));
    AddWidget(displayPath, "Resolution: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrResolutionScale")
        .Options(FloatSliderOptions().Min(0.5f).Max(1.5f).DefaultValue(1.0f).Step(0.05f).Format("%.2f").Tooltip(
            "The image resolution in the headset. A higher value is sharper but slower. The change "
            "applies after you start the game again."));
    AddWidget(displayPath, "Hide Link's Body", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrHideBody")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "First person only. On: you see only the hands of Link. Off: you also see his body."));
    AddWidget(displayPath, "Black Bars in Cutscenes", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrLetterbox")
        .Options(
            CheckboxOptions().Tooltip("Shows the black bars of the original game during Z-targeting and cutscenes."));

#ifdef __ANDROID__
    // ---------------------------------------------------------------------- Mods
    // SOH [Quest] The player folder. Settings > Mod Menu sets the order.
    AddSidebarEntry("VR Settings", "Mods", 1);
    WidgetPath modsPath = { "VR Settings", "Mods", SECTION_COLUMN_1 };
    AddWidget(modsPath, "VrModsFolder", WIDGET_CUSTOM).CustomFunction(VrModsFolder).HideInSearch(true);
#endif

    // ----------------------------------------------------------------- Developer
    // Tuning and tests. Shows only with Dev Tools > Debug Mode on.
    AddSidebarEntry("VR Settings", "Developer", 2);
    SetSidebarHidden("VR Settings", "Developer",
                     []() { return !CVarGetInteger(CVAR_DEVELOPER_TOOLS("DebugEnabled"), 0); });
    WidgetPath devPath = { "VR Settings", "Developer", SECTION_COLUMN_1 };

    AddWidget(devPath, "Hands and Aim", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Motion-Control Hands", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrMotionHands")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Detach Link's hands from his body and pin them to the VR controllers."));
    AddWidget(devPath, "Motion Weapon Aim", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrWeaponAim")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrMotionHands", 1); })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Seeds and arrows fly where the hand with the bow or the slingshot points. The hookshot "
            "shoots along its barrel. When you lock on to a target, your hand still aims the shot. "
            "Off: the thumbstick aims, as in the original game."));

    AddWidget(devPath, "Render Cost", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Stereo Render Divisor: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar("gVrStereoDivisor")
        .Options(IntSliderOptions()
                     .Min(1)
                     .Max(4)
                     .DefaultValue(2) // SOH [VR] issue #83
                     .Format("%d")
                     .Tooltip("Redraw the stereo pair every Nth frame; in between, the previous "
                              "images are resubmitted with the pose they were drawn from and the "
                              "compositor reprojects them onto your live head pose. 2 roughly "
                              "halves render cost. The source animation is 20 fps, so the drop "
                              "from 120 to 60 world updates is hard to see; head tracking is "
                              "unaffected."));
    AddWidget(devPath, "Draw HUD Once Per Game Tick", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrHudPerTick")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The overlay display list is rebuilt once per 20 Hz game tick, so "
            "redrawing it on every interpolated sub-frame renders identical "
            "content up to six times. Off = redraw every frame (only useful if "
            "something in the HUD looks like it is updating too slowly)."));
    AddWidget(devPath, "Companion Window Divisor: %d", WIDGET_CVAR_SLIDER_INT)
        .CVar("gVrDesktopViewDivisor")
        .Options(IntSliderOptions().Min(1).Max(16).DefaultValue(4).Format("%d").Tooltip(
            "How often the desktop window is updated, in frames. Each update "
            "costs an ImGui frame, a full-eye-resolution mirror copy and a "
            "Present, all on the critical path. 4 gives roughly 30 fps on the "
            "monitor at a 120 Hz headset. This menu updates at that rate too."));

    AddWidget(devPath, "Live Frame Cost", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "VRPerformanceReadout", WIDGET_CUSTOM).CustomFunction(VrPerformanceReadout).HideInSearch(true);

    AddWidget(devPath, "Diagnostics", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Debug Overlay", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysCombatDebug")
        .Options(CheckboxOptions().Tooltip(
            "Draw hand velocity arrows and the per-tick motion path in the world, plus the live "
            "speed readout below. Arrow color previews the swing tiers: green = too slow to "
            "count, yellow = normal hit, red = strong hit."));
    AddWidget(devPath, "Pacify Enemies (Testing)", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysPacifist")
        .Options(
            CheckboxOptions().Tooltip("Freezes all enemies solid: no AI, no detection, no attacks, animation paused - "
                                      "living statues for testing blade physics and limb manipulation. Damage still "
                                      "lands. Uncheck to thaw."));
    AddWidget(devPath, "Record Physics Log", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysLog")
        .Options(CheckboxOptions().Tooltip(
            "Capture every physics step of the held weapon (hand target, the pose the spring "
            "produced, the pose after collision, every contact point/normal/depth, and a "
            "fingerprint of the collision geometry in play). Keeps the most recent ~20 seconds. "
            "Turn it ON, go reproduce the problem, then turn it OFF - the capture is written to "
            "vr_phys_log.csv next to the game executable."));
    AddWidget(devPath, "VrPhysLogControl", WIDGET_CUSTOM).CustomFunction(VrPhysLogControl).HideInSearch(true);

    AddWidget(devPath, "Haptics", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Test Left Haptic", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Buzz the left controller for 0.1 s."))
        .Callback([](WidgetInfo& info) { VR_TriggerHaptic(0, 0.8f, 0.0f, 100.0f); });
    AddWidget(devPath, "Test Right Haptic", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Buzz the right controller for 0.1 s."))
        .Callback([](WidgetInfo& info) { VR_TriggerHaptic(1, 0.8f, 0.0f, 100.0f); });

    AddWidget(devPath, "Live Hand Speed", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "VrPhysCombatReadout", WIDGET_CUSTOM).CustomFunction(VrPhysCombatReadout).HideInSearch(true);

    AddWidget(devPath, "Hand Calibration: Mirroring", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Mirror Sword Hand", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrHandMirrorSword")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Reflect the sword hand's mesh so it reads as a right hand on the right "
            "controller. Only applies in right-handed mode."));
    AddWidget(devPath, "Mirror Shield Hand", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrHandMirrorShield")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "Reflect the shield hand's mesh so it reads as a left hand on the left "
            "controller. Note the reflection also mirrors the shield's face design; "
            "pair with the Left Hand Override values to orient it correctly."));
    AddWidget(devPath, "Mirror Axis", WIDGET_CVAR_COMBOBOX)
        .CVar("gVrHandMirrorAxis")
        .Options(ComboboxOptions()
                     .DefaultIndex(2)
                     .ComboMap(vrMirrorAxisOptions)
                     .Tooltip("Which model-local axis the mirror reflection negates. Should be the thumb "
                              "axis: it must keep the finger direction and flip the thumb so the mesh reads "
                              "as the opposite hand. Try each if the hands look inside-out."));

    AddWidget(devPath, "Hand Calibration: Rotation", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Tune while looking at the SWORD hand - the other hand mirrors automatically.", WIDGET_TEXT);
    AddWidget(devPath, "Pitch: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandCalPitch")
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(88.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Rotation about the grip X axis (wrist tilt up/down)."));
    AddWidget(devPath, "Yaw: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandCalYaw")
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(-100.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Rotation about the grip Y axis. If the sword points backward or sideways "
            "out of your fist, adjust this first."));
    AddWidget(devPath, "Roll: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandCalRoll")
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(80.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Rotation about the grip Z axis (twist around the handle - use to line up "
            "the blade edge and palm)."));

    AddWidget(devPath, "Hand Calibration: Position", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Offset X: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandOffX")
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Slide the hand along the grip X axis (game units, tuned for the left "
            "controller; the right controller mirrors)."));
    AddWidget(devPath, "Offset Y: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandOffY")
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Slide the hand along the grip Y axis."));
    AddWidget(devPath, "Offset Z: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandOffZ")
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Slide the hand along the grip Z axis (roughly along the handle)."));

    AddWidget(devPath, "Left Hand Override", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Tune Left Hand Separately", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrHandLOverride")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "By default the left controller's hand is derived from the values above by mirror symmetry. "
            "If it doesn't look right, enable this and dial it in with its own values below."));
    AddWidget(devPath, "L Pitch: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLCalPitch")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(-149.0f).Step(1.0f).Format("%.1f"));
    AddWidget(devPath, "L Yaw: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLCalYaw")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(76.0f).Step(1.0f).Format("%.1f"));
    AddWidget(devPath, "L Roll: %.1f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLCalRoll")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(30.0f).Step(1.0f).Format("%.1f"));
    AddWidget(devPath, "L Offset X: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLOffX")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f"));
    AddWidget(devPath, "L Offset Y: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLOffY")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f"));
    AddWidget(devPath, "L Offset Z: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHandLOffZ")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrHandLOverride", 1); })
        .Options(FloatSliderOptions().Min(-30.0f).Max(30.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f"));

    AddWidget(devPath, "Head Position (relative to Link's body)", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Forward Offset: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHeadOffsetForward")
        .Options(FloatSliderOptions().Min(-60.0f).Max(60.0f).DefaultValue(6.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Move the eye anchor along Link's facing (game units). Positive pushes "
            "the camera forward out of his head; negative pulls it back."));
    AddWidget(devPath, "Side Offset: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrHeadOffsetSide")
        .Options(FloatSliderOptions().Min(-60.0f).Max(60.0f).DefaultValue(0.0f).Step(1.0f).Format("%.1f").Tooltip(
            "Move the eye anchor sideways relative to Link's facing (game units)."));

    AddWidget(devPath, "Calibration Export", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Copy All Calibration Values", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Copy every VR tuning value (hands, weapon aim, camera, "
                                         "world scale, HUD placement) so they can be handed to a "
                                         "developer to become the defaults."))
        .Callback([](WidgetInfo& info) {
            char buf[2048];
            snprintf(buf, sizeof(buf),
                     "gVrMotionHands=%d\n"
                     "gVrLeftHanded=%d\n"
                     "gVrHandMirrorSword=%d\n"
                     "gVrHandMirrorShield=%d\n"
                     "gVrHandMirrorAxis=%d\n"
                     "gVrHandCalPitch=%.1f\n"
                     "gVrHandCalYaw=%.1f\n"
                     "gVrHandCalRoll=%.1f\n"
                     "gVrHandOffX=%.1f\n"
                     "gVrHandOffY=%.1f\n"
                     "gVrHandOffZ=%.1f\n"
                     "gVrHandLOverride=%d\n"
                     "gVrHandLCalPitch=%.1f\n"
                     "gVrHandLCalYaw=%.1f\n"
                     "gVrHandLCalRoll=%.1f\n"
                     "gVrHandLOffX=%.1f\n"
                     "gVrHandLOffY=%.1f\n"
                     "gVrHandLOffZ=%.1f\n"
                     "gVrHeadHeightOffset=%.1f\n"
                     "gVrHeadOffsetForward=%.1f\n"
                     "gVrHeadOffsetSide=%.1f\n"
                     "gVrWorldScale=%.1f\n"
                     "gVrScreenDistance=%.1f\n"
                     "gVrScreenSize=%.1f\n"
                     "gVrHudAttach=%d\n"
                     "gVrHudDistance=%.1f\n"
                     "gVrHudSize=%.2f\n"
                     "gVrHudOffX=%.2f\n"
                     "gVrHudOffY=%.2f\n"
                     "gVrHudHandSize=%.2f\n"
                     "gVrHudHandOffX=%.2f\n"
                     "gVrHudHandOffY=%.2f\n"
                     "gVrHudHandOffZ=%.2f\n"
                     "gVrHudHandPitch=%.0f\n"
                     "gVrWristHudGlance=%d\n"
                     "gVrWristHudSize=%.2f\n"
                     "gVrWristHudOffX=%.2f\n"
                     "gVrWristHudOffY=%.2f\n"
                     "gVrWristHudOffZ=%.2f\n",
                     CVarGetInteger("gVrMotionHands", 1), CVarGetInteger("gVrLeftHanded", 0),
                     CVarGetInteger("gVrHandMirrorSword", 1), CVarGetInteger("gVrHandMirrorShield", 1),
                     CVarGetInteger("gVrHandMirrorAxis", 2), CVarGetFloat("gVrHandCalPitch", 88.0f),
                     CVarGetFloat("gVrHandCalYaw", -100.0f), CVarGetFloat("gVrHandCalRoll", 80.0f),
                     CVarGetFloat("gVrHandOffX", 0.0f), CVarGetFloat("gVrHandOffY", 0.0f),
                     CVarGetFloat("gVrHandOffZ", 0.0f), CVarGetInteger("gVrHandLOverride", 1),
                     CVarGetFloat("gVrHandLCalPitch", -149.0f), CVarGetFloat("gVrHandLCalYaw", 76.0f),
                     CVarGetFloat("gVrHandLCalRoll", 30.0f), CVarGetFloat("gVrHandLOffX", 0.0f),
                     CVarGetFloat("gVrHandLOffY", 0.0f), CVarGetFloat("gVrHandLOffZ", 0.0f),
                     CVarGetFloat("gVrHeadHeightOffset", -9.0f), CVarGetFloat("gVrHeadOffsetForward", 6.0f),
                     CVarGetFloat("gVrHeadOffsetSide", 0.0f), CVarGetFloat("gVrWorldScale", 35.0f),
                     CVarGetFloat("gVrScreenDistance", 2.2f), CVarGetFloat("gVrScreenSize", 2.4f),
                     CVarGetInteger("gVrHudAttach", 0), CVarGetFloat("gVrHudDistance", 2.0f),
                     CVarGetFloat("gVrHudSize", 1.5f), CVarGetFloat("gVrHudOffX", 0.0f),
                     CVarGetFloat("gVrHudOffY", 0.0f), CVarGetFloat("gVrHudHandSize", 0.35f),
                     CVarGetFloat("gVrHudHandOffX", 0.0f), CVarGetFloat("gVrHudHandOffY", 0.10f),
                     CVarGetFloat("gVrHudHandOffZ", -0.08f), CVarGetFloat("gVrHudHandPitch", -40.0f),
                     CVarGetInteger("gVrWristHudGlance", 1), CVarGetFloat("gVrWristHudSize", 0.20f),
                     CVarGetFloat("gVrWristHudOffX", -0.04f), CVarGetFloat("gVrWristHudOffY", 0.02f),
                     CVarGetFloat("gVrWristHudOffZ", 0.10f));
            ImGui::SetClipboardText(buf);
        });

    devPath.column = SECTION_COLUMN_2;
    AddWidget(devPath, "Sword Swing Speeds", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Arm Swing At: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysArmSpeed")
        .Options(FloatSliderOptions().Min(0.3f).Max(6.0f).DefaultValue(2.0f).Step(0.1f).Format("%.1f").Tooltip(
            "Blade tip speed (real meters/second) where a swing starts counting: "
            "the trail appears, the swing sound plays, and enemies begin their "
            "guard/dodge reactions. Below this the sword is inert."));
    AddWidget(devPath, "Hit At: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysHitSpeed")
        .Options(FloatSliderOptions().Min(0.5f).Max(10.0f).DefaultValue(5.0f).Step(0.1f).Format("%.1f").Tooltip(
            "Tip speed where the blade actually damages what it sweeps through, "
            "at the weapon's normal slash strength."));
    AddWidget(devPath, "Strong Hit At: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysHeavySpeed")
        .Options(FloatSliderOptions().Min(1.0f).Max(16.0f).DefaultValue(8.0f).Step(0.1f).Format("%.1f").Tooltip(
            "Tip speed for a committed swing: damage steps up to the weapon's "
            "jump-slash class (double against most enemies)."));
    AddWidget(devPath, "Re-Arm Below: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysReArmSpeed")
        .Options(FloatSliderOptions().Min(0.1f).Max(4.0f).DefaultValue(0.8f).Step(0.1f).Format("%.1f").Tooltip(
            "A swing ends (and the sword can strike again) once the tip slows "
            "below this. One strike lands per swing; follow-through and wind-up "
            "back up naturally re-arm you."));
    AddWidget(devPath, "Min Hand Speed To Damage: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysMinHandSpeed")
        .Options(FloatSliderOptions().Min(0.0f).Max(4.0f).DefaultValue(1.2f).Step(0.1f).Format("%.1f").Tooltip(
            "Anti-wiggle: the hand itself must move at least this fast for a "
            "swing to deal damage. Pure wrist flicks spin the blade quickly but "
            "shouldn't cut - real swings come from the arm. 0 disables."));

    AddWidget(devPath, "Blade Collider", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Kokiri Sword Length: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeLenKokiri")
        .Options(FloatSliderOptions().Min(10.0f).Max(60.0f).DefaultValue(18.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Blade collider length for the Kokiri Sword (game units, from the "
            "hilt). Default matches the visible blade. Unlike the base game, the "
            "collider is exactly one blade line - no invisible extra reach."));
    AddWidget(devPath, "Master Sword Length: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeLenMaster")
        .Options(FloatSliderOptions().Min(10.0f).Max(70.0f).DefaultValue(35.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Blade collider length for the Master Sword (game units). Default "
            "matches the visible blade."));
    AddWidget(devPath, "Biggoron Sword Length: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeLenBiggoron")
        .Options(FloatSliderOptions().Min(20.0f).Max(90.0f).DefaultValue(55.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Blade collider length for the Biggoron Sword / Giant's Knife (game "
            "units). Default matches the visible blade. Swings one-handed for "
            "now; real two-handed weight comes in a later update."));
    AddWidget(devPath, "Megaton Hammer Length: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeLenHammer")
        .Options(FloatSliderOptions().Min(10.0f).Max(45.0f).DefaultValue(25.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Collider length of the Megaton Hammer, from the grip to the head "
            "(game units)."));
    AddWidget(devPath, "Hammer Ground Hit At: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysHammerSlamSpeed")
        .Options(FloatSliderOptions().Min(1.0f).Max(10.0f).DefaultValue(4.0f).Step(0.1f).Format("%.1f").Tooltip(
            "Downward speed of the hammer head (meters/second) for a ground "
            "hit. A ground hit shakes the screen and stuns enemies near Link."));
    AddWidget(devPath, "Blade Width: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeWidth")
        .Options(FloatSliderOptions().Min(1.0f).Max(12.0f).DefaultValue(4.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Width of the blade's stab cross-section (game units). Only matters "
            "for straight thrusts - slashes get their hit area from the sweep "
            "itself."));

    AddWidget(devPath, "Blade Physics", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Blade Inertia & Collision", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysBladeInertia")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "The sword becomes a simulated object: it follows your hand on a stiff spring, "
            "STOPS and bounces on walls, armor and enemy shields (with impact buzz and sparks) "
            "while your real hand keeps going, and springs back as you pull away. Swings below "
            "damage speed also bounce off enemies instead of passing through. Off = the blade "
            "is glued to your hand and passes through everything (damage rules unchanged)."));
    AddWidget(devPath, "Sword Snappiness: %.0f Hz", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysSword1HFreq")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(4.0f).Max(30.0f).DefaultValue(14.0f).Step(1.0f).Format("%.0f").Tooltip(
            "How stiffly the virtual blade tracks your hand. High = near-1:1 "
            "and responsive (light sword); low = floaty and heavy. Two-handed "
            "weapons get their own weight in a later update."));
    AddWidget(devPath, "Sword Rotation Snappiness: %.0f Hz", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysSwordAngFreq")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(5.0f).Max(60.0f).DefaultValue(30.0f).Step(1.0f).Format("%.0f").Tooltip(
            "How fast the blade's ANGLE follows your wrist. Raise this if the "
            "sword lags behind during quick rotations; lower it for a heavier, "
            "slower-turning weapon."));
    AddWidget(devPath, "Weight Wiggle: %.0f ms", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysWeightLagMs")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(80.0f).DefaultValue(0.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Cosmetic weight: the visible sword trails a fast swing by this "
            "many milliseconds of rotation, then snaps back with a little "
            "overshoot. Collision, damage and aim never lag - the sword still "
            "moves exactly with your hand. 0 = off."));
    AddWidget(devPath, "Weight Wiggle Snap: %.1f Hz", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysWeightSnapHz")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(2.0f).Max(12.0f).DefaultValue(2.0f).Step(0.5f).Format("%.1f").Tooltip(
            "How quickly the trailing sword catches back up after a swing. "
            "Lower = heavier and floppier, higher = a tight little flick."));
    AddWidget(devPath, "Blade Thickness: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeThickness")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(4.0f).DefaultValue(0.4f).Step(0.1f).Format("%.1f").Tooltip(
            "Collision thickness of the blade (game units) - how far the steel "
            "rests off a surface it is pressed against. Lower = the blade "
            "visually touches walls more closely."));
    AddWidget(devPath, "Collide With Visual Meshes (Experimental)", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysVisualMesh")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "EXPERIMENTAL: the blade collides with the rendered geometry you "
            "actually see (harvested from the renderer, animated enemies "
            "included) instead of the simplified collision mesh. Turn off to "
            "fall back to collision-mesh physics."));
    AddWidget(devPath, "Blade Collider Roll: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeRoll")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(-90.0f).Step(5.0f).Format("%.0f").Tooltip(
            "Rotates the flat blade collider about the blade axis. Turn on the "
            "debug overlay and adjust until the cyan rectangle lies in the "
            "same plane as the visible blade."));
    AddWidget(devPath, "Collider Shift Along Blade: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeShiftFwd")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(-10.0f).Max(10.0f).DefaultValue(0.0f).Step(0.01f).Format("%.2f").Tooltip(
            "Slides the physical blade rectangle lengthwise (game units). "
            "Align the cyan debug outline with the visible steel."));
    AddWidget(devPath, "Collider Shift Along Edge: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeShiftEdge")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(-10.0f).Max(10.0f).DefaultValue(1.1f).Step(0.01f).Format("%.2f").Tooltip(
            "Slides the collider across the blade's width direction."));
    AddWidget(devPath, "Collider Shift Along Flat: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeShiftFlat")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(-10.0f).Max(10.0f).DefaultValue(0.0f).Step(0.01f).Format("%.2f").Tooltip(
            "Slides the collider perpendicular to the blade's flat plane."));
    AddWidget(devPath, "Limb Resistance: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysLimbResist")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(1.0f).DefaultValue(0.5f).Step(0.05f).Format("%.2f").Tooltip(
            "How much limbs fight back against the blade: 0 = ragdoll-loose, "
            "1 = they barely budge. Limbs lag behind your push and spring "
            "back firmly."));
    AddWidget(devPath, "Body Capsule Radius: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBodyCapsuleRadius")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(1.0f).Max(12.0f).DefaultValue(4.5f).Step(0.1f).Format("%.1f").Tooltip(
            "Thickness of the invisible capsules fitted to each enemy/NPC "
            "skeleton bone - the surfaces the sword actually rests on. Match "
            "to limb thickness: too big and the sword floats off bodies, too "
            "small and it sinks in before stopping."));
    AddWidget(devPath, "Blade Tip Taper: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeTipTaper")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(0.5f).DefaultValue(0.2f).Step(0.05f).Format("%.2f").Tooltip(
            "The blade collider is a flat rectangle as wide as Blade Width, "
            "converging to a point over this trailing fraction of its length. "
            "0 = square tip, 0.2 = pointed over the last 20%."));
    AddWidget(devPath, "Impact Tolerance: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysTouchTolerance")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.05f).Max(3.0f).DefaultValue(0.3f).Step(0.05f).Format("%.2f").Tooltip(
            "How close (game units) counts as a real hit for impact sounds, "
            "sparks and rumble. Because the blade is stopped exactly AT "
            "surfaces rather than inside them, a little tolerance is needed or "
            "impacts rarely register. Raise if hits feel like they get missed."));
    AddWidget(devPath, "Swing-Through Speed: %.1f m/s", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysPassthroughSpeed")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(8.0f).DefaultValue(2.2f).Step(0.1f).Format("%.1f").Tooltip(
            "Swings faster than this cut THROUGH surfaces instead of stopping "
            "on them; gentle contact still rests on the surface. 0 = the "
            "blade never passes through anything."));
    AddWidget(devPath, "Hit Flinch Amount: %.0f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysFlinchAmount")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(60.0f).DefaultValue(0.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Punching-bag hit reaction: how far a struck body caves toward "
            "the swing around the impact point before springing back. Purely "
            "visual - hitboxes and enemy AI never move. 0 = off."));
    AddWidget(devPath, "Knockback Strength: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysKnockbackScale")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(3.0f).DefaultValue(0.0f).Step(0.1f).Format("%.1f").Tooltip(
            "How hard landed hits shove enemies, scaled by swing speed. "
            "Bosses and rooted enemies never budge. 0 = off."));
    AddWidget(devPath, "Blade Push Strength: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysPressPush")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(6.0f).DefaultValue(0.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Enemies get nudged away when you press the blade against them "
            "(no damage - just steel insisting). Bosses and rooted enemies "
            "stay put. 0 = off."));
    AddWidget(devPath, "Cut Resistance (Flesh): %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysCutDragFlesh")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(0.97f).DefaultValue(0.73f).Step(0.01f).Format("%.2f").Tooltip(
            "How much enemy bodies hold the blade back while a fast swing "
            "cuts through them. The blade drags in the cut (with rumble) and "
            "catches up to your hand on exit. 0 = clean effortless cuts."));
    AddWidget(devPath, "Cut Resistance (World): %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysCutDragWorld")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(0.97f).DefaultValue(0.73f).Step(0.01f).Format("%.2f").Tooltip(
            "Drag while a fast swing passes through world geometry (walls, "
            "fences). Light by default so committed swings stay fluid."));
    AddWidget(devPath, "Blade Friction: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysBladeFriction")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysBladeInertia", 1); })
        .Options(FloatSliderOptions().Min(0.0f).Max(1.0f).DefaultValue(0.5f).Step(0.05f).Format("%.2f").Tooltip(
            "How much the blade drags while sliding along a surface. 0 = "
            "frictionless skating, higher = the blade angle sticks and trails "
            "as you drag it across walls and floors."));

    AddWidget(devPath, "Shield", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Physical Shield", WIDGET_CVAR_CHECKBOX)
        .CVar("gVrPhysShield")
        .Options(CheckboxOptions().DefaultValue(true).Tooltip(
            "On: the shield is in your off hand while the sword is in your "
            "sword hand - no button, no stance. Hold it up and whatever "
            "touches the shield is blocked; whatever gets around it hits you. "
            "Blocks never stagger you or push you back.\n"
            "Off: the shield of the original game comes back - hold R for the "
            "shield stance, with its animation and its advanced tricks "
            "(megaflip and others). By default L Grip is R. If no input is R, "
            "set one in Controls."));
    AddWidget(devPath, "Shield Facing Leniency: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldFacingDeg")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysShield", 1); })
        .Options(FloatSliderOptions().Min(30.0f).Max(180.0f).DefaultValue(90.0f).Step(1.0f).Format("%.0f").Tooltip(
            "How far the shield's face may be turned away from an attack "
            "and still block it. Outside this cone the hit doesn't count - "
            "no back-of-shield or corner deflections; if the attack also "
            "reached your body, it hurts. Lower = you must square up to the "
            "threat. 90 = anything in front of the shield. 180 = block from "
            "any angle."));
    // The size and shift sliders do nothing while the collider takes the shield mesh's size
    // (gVrPhysShieldFitMesh, console only: the fallback if the fit is wrong on some shield).
    auto hideWhenShieldFitted = [](WidgetInfo& info) {
        info.isHidden = !CVarGetInteger("gVrPhysShield", 1) || CVarGetInteger("gVrPhysShieldFitMesh", 1);
    };
    AddWidget(devPath, "Shield Width Top: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldWidthTop")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(5.0f).Max(90.0f).DefaultValue(21.2f).Step(0.5f).Format("%.1f").Tooltip(
            "Width of the block collider's TOP edge (game units). Turn on "
            "the debug overlay - the cyan quad on the shield is exactly what "
            "blocks. Anything outside it doesn't count: smaller = stricter."));
    AddWidget(devPath, "Shield Width Bottom: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldWidthBottom")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(5.0f).Max(90.0f).DefaultValue(11.9f).Step(0.5f).Format("%.1f").Tooltip(
            "Width of the block collider's BOTTOM edge. Set narrower than "
            "the top for a Hylian-style tapered shape."));
    AddWidget(devPath, "Shield Height: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldHeight")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(5.0f).Max(90.0f).DefaultValue(18.0f).Step(0.5f).Format("%.1f").Tooltip(
            "Height of the block collider (game units)."));
    AddWidget(devPath, "Shield Shift Across: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldShiftX")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(-40.0f).Max(40.0f).DefaultValue(-1.1f).Step(0.5f).Format("%.1f").Tooltip(
            "Slides the collider along its own width axis (game units) - the "
            "shifts follow the tilt sliders, so they always mean what they "
            "say. Center the cyan quad on the visible steel."));
    AddWidget(devPath, "Shield Shift Up/Down: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldShiftY")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(-40.0f).Max(40.0f).DefaultValue(-0.8f).Step(0.5f).Format("%.1f").Tooltip(
            "Slides the collider along the shield face's vertical axis."));
    AddWidget(devPath, "Shield Shift Out: %.1f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldShiftZ")
        .PreFunc(hideWhenShieldFitted)
        .Options(FloatSliderOptions().Min(-40.0f).Max(40.0f).DefaultValue(-2.5f).Step(0.5f).Format("%.1f").Tooltip(
            "Slides the collider along the shield's facing direction, until "
            "it lies in the same plane as the steel."));
    AddWidget(devPath, "Shield Pitch: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldPitch")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysShield", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(-4.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Tilts the collider plane forward/back relative to the grip."));
    AddWidget(devPath, "Shield Yaw: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldYaw")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysShield", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(0.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Turns the collider plane left/right relative to the grip."));
    AddWidget(devPath, "Shield Roll: %.0f deg", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gVrPhysShieldRoll")
        .PreFunc([](WidgetInfo& info) { info.isHidden = !CVarGetInteger("gVrPhysShield", 1); })
        .Options(FloatSliderOptions().Min(-180.0f).Max(180.0f).DefaultValue(-90.0f).Step(1.0f).Format("%.0f").Tooltip(
            "Spins the collider within the shield plane."));

    AddWidget(devPath, "Combat Export", WIDGET_SEPARATOR_TEXT);
    AddWidget(devPath, "Copy All Combat Tuning Values", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Copy every Physical Combat tuning value (speed tiers, "
                                         "blade collider, physics, hit feel — including the "
                                         "console-only knobs) so they can be handed to a "
                                         "developer to become the defaults."))
        .Callback([](WidgetInfo& info) {
            char buf[2048];
            snprintf(buf, sizeof(buf),
                     "gVrPhysArmSpeed=%.2f\n"
                     "gVrPhysHitSpeed=%.2f\n"
                     "gVrPhysHeavySpeed=%.2f\n"
                     "gVrPhysReArmSpeed=%.2f\n"
                     "gVrPhysMinHandSpeed=%.2f\n"
                     "gVrPhysBladeLenKokiri=%.2f\n"
                     "gVrPhysBladeLenMaster=%.2f\n"
                     "gVrPhysBladeLenBiggoron=%.2f\n"
                     "gVrPhysBladeLenHammer=%.2f\n"
                     "gVrPhysHammerSlamSpeed=%.2f\n"
                     "gVrPhysHammerSlamReach=%.2f\n"
                     "gVrPhysBladeWidth=%.2f\n"
                     "gVrPhysBladeThickness=%.2f\n"
                     "gVrPhysBladeRoll=%.1f\n"
                     "gVrPhysBladeShiftFwd=%.2f\n"
                     "gVrPhysBladeShiftEdge=%.2f\n"
                     "gVrPhysBladeShiftFlat=%.2f\n"
                     "gVrPhysBladeTipTaper=%.2f\n"
                     "gVrPhysBladeInertia=%d\n"
                     "gVrPhysSword1HFreq=%.2f\n"
                     "gVrPhysSword1HZeta=%.2f\n"
                     "gVrPhysSwordAngFreq=%.2f\n"
                     "gVrPhysWeightLagMs=%.1f\n"
                     "gVrPhysWeightSnapHz=%.1f\n"
                     "gVrPhysMaxAccel=%.1f\n"
                     "gVrPhysMaxAngAccel=%.1f\n"
                     "gVrPhysTouchTolerance=%.2f\n"
                     "gVrPhysBladeFriction=%.2f\n"
                     "gVrPhysPivotOnly=%d\n"
                     "gVrPhysVisualMesh=%d\n"
                     "gVrPhysMeshRadius=%.1f\n"
                     "gVrPhysPassthroughSpeed=%.2f\n"
                     "gVrPhysCutDragFlesh=%.2f\n"
                     "gVrPhysCutDragWorld=%.2f\n"
                     "gVrPhysKnockbackScale=%.2f\n"
                     "gVrPhysKnockbackCap=%.2f\n"
                     "gVrPhysPressPush=%.2f\n"
                     "gVrPhysFlinchAmount=%.1f\n"
                     "gVrPhysLimbResist=%.2f\n"
                     "gVrPhysLimbRadius=%.1f\n"
                     "gVrPhysLimbPushMax=%.1f\n"
                     "gVrPhysBodyCapsuleRadius=%.2f\n"
                     "gVrPhysSubQuads=%d\n"
                     "gVrPhysShield=%d\n"
                     "gVrPhysShieldWidthTop=%.1f\n"
                     "gVrPhysShieldWidthBottom=%.1f\n"
                     "gVrPhysShieldHeight=%.1f\n"
                     "gVrPhysShieldShiftX=%.1f\n"
                     "gVrPhysShieldShiftY=%.1f\n"
                     "gVrPhysShieldShiftZ=%.1f\n"
                     "gVrPhysShieldPitch=%.0f\n"
                     "gVrPhysShieldYaw=%.0f\n"
                     "gVrPhysShieldRoll=%.0f\n"
                     "gVrPhysShieldFacingDeg=%.0f\n"
                     "gVrPhysShieldFitMesh=%d\n",
                     CVarGetFloat("gVrPhysArmSpeed", 2.0f), CVarGetFloat("gVrPhysHitSpeed", 5.0f),
                     CVarGetFloat("gVrPhysHeavySpeed", 8.0f), CVarGetFloat("gVrPhysReArmSpeed", 0.8f),
                     CVarGetFloat("gVrPhysMinHandSpeed", 1.2f), CVarGetFloat("gVrPhysBladeLenKokiri", 18.0f),
                     CVarGetFloat("gVrPhysBladeLenMaster", 35.0f), CVarGetFloat("gVrPhysBladeLenBiggoron", 55.0f),
                     CVarGetFloat("gVrPhysBladeLenHammer", 25.0f), CVarGetFloat("gVrPhysHammerSlamSpeed", 4.0f),
                     CVarGetFloat("gVrPhysHammerSlamReach", 8.0f), CVarGetFloat("gVrPhysBladeWidth", 4.0f),
                     CVarGetFloat("gVrPhysBladeThickness", 0.4f), CVarGetFloat("gVrPhysBladeRoll", -90.0f),
                     CVarGetFloat("gVrPhysBladeShiftFwd", 0.0f), CVarGetFloat("gVrPhysBladeShiftEdge", 1.1f),
                     CVarGetFloat("gVrPhysBladeShiftFlat", 0.0f), CVarGetFloat("gVrPhysBladeTipTaper", 0.2f),
                     CVarGetInteger("gVrPhysBladeInertia", 1), CVarGetFloat("gVrPhysSword1HFreq", 14.0f),
                     CVarGetFloat("gVrPhysSword1HZeta", 1.0f), CVarGetFloat("gVrPhysSwordAngFreq", 30.0f),
                     CVarGetFloat("gVrPhysWeightLagMs", 0.0f), CVarGetFloat("gVrPhysWeightSnapHz", 2.0f),
                     CVarGetFloat("gVrPhysMaxAccel", 400.0f), CVarGetFloat("gVrPhysMaxAngAccel", 3000.0f),
                     CVarGetFloat("gVrPhysTouchTolerance", 0.3f), CVarGetFloat("gVrPhysBladeFriction", 0.5f),
                     CVarGetInteger("gVrPhysPivotOnly", 1), CVarGetInteger("gVrPhysVisualMesh", 1),
                     CVarGetFloat("gVrPhysMeshRadius", 150.0f), CVarGetFloat("gVrPhysPassthroughSpeed", 2.2f),
                     CVarGetFloat("gVrPhysCutDragFlesh", 0.73f), CVarGetFloat("gVrPhysCutDragWorld", 0.73f),
                     CVarGetFloat("gVrPhysKnockbackScale", 0.0f), CVarGetFloat("gVrPhysKnockbackCap", 8.0f),
                     CVarGetFloat("gVrPhysPressPush", 0.0f), CVarGetFloat("gVrPhysFlinchAmount", 0.0f),
                     CVarGetFloat("gVrPhysLimbResist", 0.5f), CVarGetFloat("gVrPhysLimbRadius", 9.0f),
                     CVarGetFloat("gVrPhysLimbPushMax", 22.0f), CVarGetFloat("gVrPhysBodyCapsuleRadius", 4.5f),
                     CVarGetInteger("gVrPhysSubQuads", 3), CVarGetInteger("gVrPhysShield", 1),
                     CVarGetFloat("gVrPhysShieldWidthTop", 21.2f), CVarGetFloat("gVrPhysShieldWidthBottom", 11.9f),
                     CVarGetFloat("gVrPhysShieldHeight", 18.0f), CVarGetFloat("gVrPhysShieldShiftX", -1.1f),
                     CVarGetFloat("gVrPhysShieldShiftY", -0.8f), CVarGetFloat("gVrPhysShieldShiftZ", -2.5f),
                     CVarGetFloat("gVrPhysShieldPitch", -4.0f), CVarGetFloat("gVrPhysShieldYaw", 0.0f),
                     CVarGetFloat("gVrPhysShieldRoll", -90.0f), CVarGetFloat("gVrPhysShieldFacingDeg", 90.0f),
                     CVarGetInteger("gVrPhysShieldFitMesh", 1));
            ImGui::SetClipboardText(buf);
        });
}

} // namespace SohGui
