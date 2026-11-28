#include "service.h"
#include "../input/binding_policy.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "lom_native_display.h"
#include "lom_native_host_audio.h"
#include "lom_native_mod_options.h"
#include "lom_native_widescreen.h"
#include "lom_native_remaster.h"
#include <iterator>
#include <utility>

namespace lom::settings
{
namespace
{
constexpr char application_directory[] = "lom-native";

constexpr char preferences_filename[] = "preferences.ini";

struct RemasterSwitch
{
    int which;
    bool Preferences::* value;
};

constexpr RemasterSwitch remaster_switches[] = {
    {LOM_REMASTER_SWITCH_ALL, &Preferences::remaster},
    {LOM_REMASTER_SWITCH_BACKGROUNDS, &Preferences::hd_backgrounds},
    {LOM_REMASTER_SWITCH_FACES, &Preferences::hd_faces},
    {LOM_REMASTER_SWITCH_ENCYCLOPEDIA, &Preferences::hd_encyclopedia},
    {LOM_REMASTER_SWITCH_TITLE, &Preferences::hd_title},
    {LOM_REMASTER_SWITCH_NAME_ENTRY, &Preferences::hd_name_entry},
    {LOM_REMASTER_SWITCH_WORLD_SELECT, &Preferences::hd_world_select},
    {LOM_REMASTER_SWITCH_WORLD_MAP, &Preferences::hd_world_map},
    {LOM_REMASTER_SWITCH_MOVIES, &Preferences::hd_movies},
    {LOM_REMASTER_SWITCH_MUSIC, &Preferences::remaster_music},
    {LOM_REMASTER_SWITCH_ARRANGED, &Preferences::arranged_music},
    {LOM_REMASTER_SWITCH_EFFECTS, &Preferences::remaster_effects},
    {LOM_REMASTER_SWITCH_TEXT, &Preferences::remaster_text},
    {LOM_REMASTER_SWITCH_MENU, &Preferences::hd_menu},
    {LOM_REMASTER_SWITCH_SAVES, &Preferences::hd_saves},
};

static_assert(std::size(remaster_switches) == LOM_REMASTER_SWITCH_COUNT, "a preference for each of the Remaster's switches");
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

    for (const auto& [key, choice] : values_.mod_options)
    {
        const std::size_t colon = key.find(':');
        lom_native_mod_options_prefer(key.substr(0, colon).c_str(), key.substr(colon + 1).c_str(), choice.c_str());
    }

    for (const RemasterSwitch& entry : remaster_switches)
    {
        lom_native_remaster_set_switch(entry.which, values_.*entry.value);
    }

    lom_native_remaster_set_text_language(values_.text_language);
}

Preferences SettingsService::persistence_snapshot()
{
    values_.display.widescreen = lom_native_widescreen_enabled();

    for (int i = 0; i < lom_native_mod_options_count(); ++i)
    {
        const std::string key = std::string(lom_native_mod_options_mod(i)) + ':' + lom_native_mod_options_id(i);
        values_.mod_options[key] = lom_native_mod_options_choice_id(i, lom_native_mod_options_chosen(i));
    }

    for (const RemasterSwitch& entry : remaster_switches)
    {
        values_.*entry.value = lom_native_remaster_switch(entry.which) != 0;
    }

    values_.text_language = lom_native_remaster_text_language();
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
