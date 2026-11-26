#include "menu_backend.h"

#include "../settings/service.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_controls.h"
#include "lom_native_display.h"
#include "lom_native_host_audio.h"
#include "lom_native_remaster.h"
#include "lom_native_widescreen.h"
#include "menu_glyphs.h"
#include "menu_runtime.h"
#include "menu_text.h"

#include <string>

namespace lom::settings
{
namespace
{
LOM_MENU_TEXT(unbound_text, "Unbound");
}

DisplaySettings GameBackend::display() const
{
    return DisplaySettings{lom_native_display_filter(), lom_native_display_resolution(), lom_native_display_fullscreen(), lom_native_widescreen_enabled() != 0};
}

void GameBackend::set_display(const DisplaySettings& value)
{
    lom_native_display_set(value.filter, value.resolution, value.fullscreen);
    lom_native_widescreen_set_enabled(value.widescreen);
}

std::string GameBackend::display_error() const
{
    return lom_native_display_error();
}

AudioLevel GameBackend::audio() const
{
    return AudioLevel{lom_native_audio_volume(), lom_native_audio_muted()};
}

void GameBackend::set_audio(const AudioLevel& value)
{
    lom_native_audio_set(value.volume, value.muted);
}

AudioLevel GameBackend::category_audio(int category) const
{
    return AudioLevel{lom_native_audio_category_volume(category), lom_native_audio_category_muted(category)};
}

void GameBackend::set_category_audio(int category, const AudioLevel& value)
{
    lom_native_audio_category_set(category, value.volume, value.muted);
}

bool GameBackend::pause_unfocused() const
{
    return settings().values().pause_unfocused;
}

void GameBackend::set_pause_unfocused(bool value)
{
    menu_runtime().set_pause_unfocused(value);
}

bool GameBackend::remaster_found() const
{
    return lom_native_remaster_found() != 0;
}

bool GameBackend::hd_backgrounds_available() const
{
    return lom_native_remaster_hd_supported() != 0;
}

bool GameBackend::hd_backgrounds() const
{
    return lom_native_remaster_hd_backgrounds() != 0;
}

void GameBackend::set_hd_backgrounds(bool value)
{
    lom_native_remaster_set_hd_backgrounds(value);
}

bool GameBackend::hd_ui_available() const
{
    return lom_native_remaster_hd_ui_supported() != 0;
}

bool GameBackend::hd_ui() const
{
    return lom_native_remaster_hd_ui() != 0;
}

void GameBackend::set_hd_ui(bool value)
{
    lom_native_remaster_set_hd_ui(value);
}

int GameBackend::text_language() const
{
    return lom_native_remaster_text();
}

bool GameBackend::text_available() const
{
    return lom_native_remaster_text_supported() != 0;
}

int GameBackend::mod_toggle_count() const
{
    return lom_native_mod_toggles_count();
}

std::string GameBackend::mod_toggle_name(int index) const
{
    const char* name = lom_native_mod_toggles_name(index);
    return name ? name : "";
}

std::string GameBackend::mod_toggle_description(int index) const
{
    const char* description = lom_native_mod_toggles_description(index);
    return description ? description : "";
}

bool GameBackend::mod_toggle_on(int index) const
{
    return lom_native_mod_toggles_wanted(index) != 0;
}

void GameBackend::set_mod_toggle(int index, bool value)
{
    lom_native_mod_toggles_set(index, value);
}

void GameBackend::set_text_language(int value)
{
    lom_native_remaster_set_text(value);
}

int GameBackend::soundtrack() const
{
    return lom_native_remaster_music();
}

void GameBackend::set_soundtrack(int value)
{
    lom_native_remaster_set_music(value);
}

bool GameBackend::remaster_effects() const
{
    return lom_native_remaster_effects() != 0;
}

void GameBackend::set_remaster_effects(bool value)
{
    lom_native_remaster_set_effects(value);
}

void GameBackend::resume()
{
    menu_runtime().set_open(false);
}

void GameBackend::request_quit()
{
    menu_runtime().request_quit();
}

bool GameBackend::confirming_quit() const
{
    return menu_runtime().confirming_quit();
}

bool GameBackend::confirm_quit()
{
    return menu_runtime().confirm_quit();
}

void GameBackend::cancel_quit()
{
    menu_runtime().cancel_quit();
}

std::string GameBackend::quit_error() const
{
    return menu_runtime().quit_error();
}

std::string GameBackend::binding_name(int port, int button, bool pad) const
{
    const auto& bindings = menu_runtime().controls().bindings();
    const int value = (pad ? bindings.pads : bindings.keys)[port][button];

    if (!pad)
    {
        return value != SDL_SCANCODE_UNKNOWN ? SDL_GetScancodeName(static_cast<SDL_Scancode>(value)) : static_cast<const char*>(unbound_text);
    }

    SDL_JoystickID id = lom_native_psyz_pad_device_id(port);
    return pad_binding_name(id ? SDL_GetGamepadTypeForID(id) : SDL_GAMEPAD_TYPE_UNKNOWN, value);
}

void GameBackend::begin_binding(int port, int button, bool pad)
{
    menu_runtime().controls().begin(port, button, pad);
}

bool GameBackend::binding_active() const
{
    return menu_runtime().controls().capture().active();
}

int GameBackend::binding_port() const
{
    return menu_runtime().controls().capture().port;
}

int GameBackend::binding_button() const
{
    return menu_runtime().controls().capture().button;
}

bool GameBackend::binding_pad() const
{
    return menu_runtime().controls().capture().pad;
}

void GameBackend::restore_bindings(int port)
{
    menu_runtime().controls().restore(port);
}

std::string GameBackend::binding_message() const
{
    return menu_runtime().controls().message();
}

std::string GameBackend::gamepad_name(int port) const
{
    SDL_JoystickID id = lom_native_psyz_pad_device_id(port);
    SDL_Gamepad* pad = id ? SDL_GetGamepadFromID(id) : nullptr;
    const char* name = pad ? SDL_GetGamepadName(pad) : nullptr;
    return name ? name : "";
}

}
