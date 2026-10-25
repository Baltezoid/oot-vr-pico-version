#if defined(__ANDROID__) || defined(__IOS__)
#include "ship/port/mobile/MobileImpl.h"
#include <SDL2/SDL.h>

#include <imgui_internal.h>

// SOH [Quest] Starts false: the first call with a text field active must show the keyboard. The
// field keeps its text (no ClearText), so the player can edit a name or a room ID.
static bool isShowingVirtualKeyboard = false;

void Ship::Mobile::ImGuiProcessEvent(bool wantsTextInput) {
    if (wantsTextInput) {
        if (!isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = true;
            SDL_StartTextInput();
        }
    } else {
        if (isShowingVirtualKeyboard) {
            isShowingVirtualKeyboard = false;
            SDL_StopTextInput();
        }
    }
}
#endif
