#pragma once

// Pure rule behind VrItemSelect_TriggerItemMask (VrItemSelect.cpp): the job of each VR trigger in
// selector mode. No game types, so the host tests in port/soh/tests/vr/VrTriggerRoleTests.cpp can
// build it alone.

namespace VrTriggerRole {

enum class Role {
    None,     // The trigger has no job here.
    ZTarget,  // The trigger is BTN_Z.
    HeldItem, // The trigger is the button of the held item.
};

struct View {
    bool selectorOn;    // gVrItemSelect
    bool vrInitialized;
    bool firstPerson;
    bool flatScreen;    // the pause menu or the file select
    bool ocarinaInPlay; // the ocarina binding set puts notes on the triggers
};

// The sword-hand trigger is Z-target in first person and in third person (#79), so a trigger
// keeps one role when the view changes. The off-hand trigger uses the held item in first person
// only: third person keeps stock item use on the C buttons.
inline Role For(const View& view, bool swordHand) {
    if (!view.selectorOn || !view.vrInitialized || view.flatScreen || view.ocarinaInPlay) {
        return Role::None;
    }
    if (swordHand) {
        return Role::ZTarget;
    }
    return view.firstPerson ? Role::HeldItem : Role::None;
}

} // namespace VrTriggerRole
