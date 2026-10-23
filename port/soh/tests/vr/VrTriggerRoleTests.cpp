// Host unit tests for VrTriggerRole.h (pure rule, no game or OpenXR). The game build does not
// compile this file. Run it from the repository root:
//
//   g++ -std=c++17 -I port/soh/soh/Enhancements/vr-combat port/soh/tests/vr/VrTriggerRoleTests.cpp -o /tmp/vr_trigger_role_tests
//   /tmp/vr_trigger_role_tests

#include "VrTriggerRole.h"

#include <cstdio>

using namespace VrTriggerRole;

static int sFailures = 0;

#define EXPECT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

// Selector mode on, VR on, gameplay view, no ocarina. Each test changes one field.
static View Gameplay(bool firstPerson) {
    View view;
    view.selectorOn = true;
    view.vrInitialized = true;
    view.firstPerson = firstPerson;
    view.flatScreen = false;
    view.ocarinaInPlay = false;
    return view;
}

static void FirstPersonSwordHandIsZTarget() {
    EXPECT(For(Gameplay(true), true) == Role::ZTarget);
}

static void FirstPersonOffHandUsesHeldItem() {
    EXPECT(For(Gameplay(true), false) == Role::HeldItem);
}

// Issue #79: the sword-hand trigger keeps Z-target in third person.
static void ThirdPersonSwordHandIsZTarget() {
    EXPECT(For(Gameplay(false), true) == Role::ZTarget);
}

// Third person keeps stock item use: the C buttons pull out and use items there.
static void ThirdPersonOffHandHasNoRole() {
    EXPECT(For(Gameplay(false), false) == Role::None);
}

// Classic mode binds the triggers in padmgr, not here.
static void ClassicModeHasNoRole() {
    for (int firstPerson = 0; firstPerson <= 1; firstPerson++) {
        View view = Gameplay(firstPerson != 0);
        view.selectorOn = false;
        EXPECT(For(view, true) == Role::None);
        EXPECT(For(view, false) == Role::None);
    }
}

// The ocarina binding set puts notes on the triggers.
static void OcarinaHasNoRole() {
    for (int firstPerson = 0; firstPerson <= 1; firstPerson++) {
        View view = Gameplay(firstPerson != 0);
        view.ocarinaInPlay = true;
        EXPECT(For(view, true) == Role::None);
        EXPECT(For(view, false) == Role::None);
    }
}

// The pause menu and the file select show on the flat screen. Z there changes the page.
static void FlatScreenHasNoRole() {
    for (int firstPerson = 0; firstPerson <= 1; firstPerson++) {
        View view = Gameplay(firstPerson != 0);
        view.flatScreen = true;
        EXPECT(For(view, true) == Role::None);
        EXPECT(For(view, false) == Role::None);
    }
}

static void VrOffHasNoRole() {
    View view = Gameplay(true);
    view.vrInitialized = false;
    EXPECT(For(view, true) == Role::None);
    EXPECT(For(view, false) == Role::None);
}

int main() {
    FirstPersonSwordHandIsZTarget();
    FirstPersonOffHandUsesHeldItem();
    ThirdPersonSwordHandIsZTarget();
    ThirdPersonOffHandHasNoRole();
    ClassicModeHasNoRole();
    OcarinaHasNoRole();
    FlatScreenHasNoRole();
    VrOffHasNoRole();
    if (sFailures == 0) {
        std::printf("All VrTriggerRole tests passed.\n");
    }
    return sFailures == 0 ? 0 : 1;
}
