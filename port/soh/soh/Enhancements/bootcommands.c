#include <stdbool.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include "bootcommands.h"
#include "soh/cvar_prefixes.h"

void BootCommands_Init() {
    // Clears vars to prevent randomizer menu from being disabled
    CVarClear(CVAR_GENERAL("RandoGenerating")); // Clear when a crash happened during rando seed generation
    CVarClear(CVAR_GENERAL("NewSeedGenerated"));
    CVarClear(CVAR_GENERAL("OnFileSelectNameEntry")); // Clear when soh is killed on the file name entry page
    CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQMode"));
    CVarClear(CVAR_GENERAL("BetterDebugWarpScreenMQModeScene"));
#if defined(__SWITCH__) || defined(__WIIU__)
    CVarRegisterInteger(CVAR_IMGUI_CONTROLLER_NAV, 1); // always enable controller nav on switch/wii u
#endif
#ifdef __ANDROID__
    // SOH [Quest] Default speed and comfort settings for the headset. A value that the player set
    // in the menu stays: CVarRegisterInteger only sets a CVar that does not exist.
    CVarRegisterInteger(CVAR_ENHANCEMENT("TextSpeed"), 5);
    CVarRegisterInteger(CVAR_ENHANCEMENT("FastChests"), 1);
    CVarRegisterInteger(CVAR_ENHANCEMENT("FasterPauseMenu"), 1);
    CVarRegisterInteger(CVAR_ENHANCEMENT("FasterBlockPush"), 5);
    CVarRegisterInteger(CVAR_ENHANCEMENT("CrawlSpeed"), 4);
    CVarRegisterInteger(CVAR_ENHANCEMENT("ClimbSpeed"), 6);
    CVarRegisterInteger(CVAR_ENHANCEMENT("Autosave"), 1);
    CVarRegisterInteger(CVAR_ENHANCEMENT("EnemyHealthBar"), 1);
    CVarRegisterInteger(CVAR_ENHANCEMENT("HookshotableReticle"), 1);
    CVarRegisterInteger(CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Entrances"), 1);
#endif
}
