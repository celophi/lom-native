#include "service.h"
#include "../input/binding_policy.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "lom_native_display.h"
#include "lom_native_host_audio.h"
#include "lom_native_widescreen.h"
#include "lom_native_switches.h"
#include "lom_native_remaster.h"
#include <utility>

namespace lom::settings
{
namespace
{
constexpr char application_directory[] = "lom-native";

constexpr char preferences_filename[] = "preferences.ini";
}

SettingsService& settings()
{
    static SettingsService service;
    return service;
}

void SettingsService::enable(const char* path)
{
    if (enabled_)
    {
        return;
    }

    std::string filename;

    if (path)
    {
        filename = path;
    }
    else
    {
        char* directory = SDL_GetPrefPath(nullptr, application_directory);

        if (directory)
        {
            filename = std::string(directory) + preferences_filename;
            SDL_free(directory);
        }
    }

    Preferences defaults;
    defaults.controls = default_bindings();
    values_ = store_.load(std::move(filename), defaults);
    enabled_ = true;
    lom_native_audio_set(values_.audio.master.volume, values_.audio.master.muted);
    lom_native_audio_apply_categories();

    const auto display = values_.display;
    lom_native_display_set(display.filter, display.resolution, display.fullscreen);
    lom_native_widescreen_set_enabled(display.widescreen);

    for (int id = 0; id < LOM_SWITCH_COUNT; ++id)
    {
        if (lom_native_switch_player_setting(static_cast<LomSwitch>(id)))
        {
            lom_native_switch_set_setting(static_cast<LomSwitch>(id), values_.switches[id]);
        }
    }

    lom_native_remaster_set_hd_backgrounds(values_.hd_backgrounds);
    lom_native_remaster_set_hd_ui(values_.hd_ui);
    lom_native_remaster_set_text(values_.text_language);
    lom_native_remaster_set_music(values_.soundtrack);
    lom_native_remaster_set_effects(values_.remaster_effects);
}

Preferences SettingsService::persistence_snapshot()
{
    values_.display.widescreen = lom_native_widescreen_enabled();

    for (int id = 0; id < LOM_SWITCH_COUNT; ++id)
    {
        if (lom_native_switch_player_setting(static_cast<LomSwitch>(id)))
        {
            values_.switches[id] = lom_native_switch_setting(static_cast<LomSwitch>(id)) != 0;
        }
    }

    values_.hd_backgrounds = lom_native_remaster_hd_backgrounds() != 0;
    values_.hd_ui = lom_native_remaster_hd_ui() != 0;
    values_.text_language = lom_native_remaster_text();
    values_.soundtrack = lom_native_remaster_music();
    values_.remaster_effects = lom_native_remaster_effects() != 0;
    Preferences snapshot = values_;

    if (lom_native_display_pending())
    {
        snapshot.display = store_.persisted().display;
        snapshot.display.widescreen = values_.display.widescreen;
    }
    else
    {
        values_.display = {lom_native_display_filter(), lom_native_display_resolution(), lom_native_display_fullscreen(), values_.display.widescreen};
        snapshot.display = values_.display;
    }

    return snapshot;
}

void SettingsService::update()
{
    if (enabled_)
    {
        store_.save(persistence_snapshot(), PreferenceStore::SaveReason::Changed);
    }
}

void SettingsService::flush()
{
    if (enabled_)
    {
        store_.save(persistence_snapshot(), PreferenceStore::SaveReason::ExplicitRetry);
    }
}

bool SettingsService::enabled() const
{
    return enabled_;
}

Preferences& SettingsService::values()
{
    return values_;
}

const Preferences& SettingsService::values() const
{
    return values_;
}

const PreferenceStore& SettingsService::store() const
{
    return store_;
}

}
