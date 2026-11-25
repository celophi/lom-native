#include "preference_store.h"
#include "../input/binding_policy.h"
#include <SDL3/SDL.h>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace lom::settings
{
namespace
{
constexpr int decimal_radix = 10;

constexpr int stored_gamepad_value_count = right_trigger + 2;

constexpr int boolean_value_count = 2;

constexpr int filter_value_count = 4;

constexpr int resolution_value_count = 3;

constexpr int volume_value_count = AudioLevel::maximum_volume + 1;

constexpr char file_heading[] = "# Legend of Mana native preferences\n";

constexpr char filter_key[] = "filter=";

constexpr char resolution_key[] = "resolution=";

constexpr char fullscreen_key[] = "fullscreen=";

constexpr char widescreen_key[] = "widescreen=";

constexpr char pause_unfocused_key[] = "pause_unfocused=";

constexpr char hd_backgrounds_key[] = "hd_backgrounds=";
constexpr char hd_ui_key[] = "hd_pictures=";
constexpr char text_key[] = "text=";
constexpr int text_value_count = 9;

constexpr char soundtrack_key[] = "soundtrack=";
constexpr char remaster_effects_key[] = "remaster_effects=";

constexpr int soundtrack_value_count = 3;

constexpr char master_volume_key[] = "master_volume=";

constexpr char master_mute_key[] = "mute=";

constexpr char music_volume_key[] = "music_volume=";

constexpr char music_mute_key[] = "music_mute=";

constexpr char sfx_volume_key[] = "sfx_volume=";

constexpr char sfx_mute_key[] = "sfx_mute=";

constexpr char keyboard_prefix[] = "key";

constexpr char gamepad_prefix[] = "pad";

constexpr char temporary_suffix[] = ".tmp";

constexpr char missing_directory_error[] = "Preferences could not be saved: no settings directory.";

constexpr char save_error[] = "Preferences could not be saved. Changes still work this session.";

constexpr char save_error_log_format[] = "%s %s";

struct Field
{
    const char* key;
    int limit;
    int (*get)(const Preferences&);
    void (*set)(Preferences&, int);
};

constexpr Field fields[] = {
    {filter_key, filter_value_count, [](const Preferences& p) { return p.display.filter; }, [](Preferences& p, int v) { p.display.filter = v; }},
    {resolution_key, resolution_value_count, [](const Preferences& p) { return p.display.resolution; },
     [](Preferences& p, int v) { p.display.resolution = v; }},
    {fullscreen_key, boolean_value_count, [](const Preferences& p) { return int{p.display.fullscreen}; },
     [](Preferences& p, int v) { p.display.fullscreen = v != 0; }},
    {widescreen_key, boolean_value_count, [](const Preferences& p) { return int{p.display.widescreen}; },
     [](Preferences& p, int v) { p.display.widescreen = v != 0; }},
    {pause_unfocused_key, boolean_value_count, [](const Preferences& p) { return int{p.pause_unfocused}; },
     [](Preferences& p, int v) { p.pause_unfocused = v != 0; }},
    {hd_backgrounds_key, boolean_value_count, [](const Preferences& p) { return int{p.hd_backgrounds}; },
     [](Preferences& p, int v) { p.hd_backgrounds = v != 0; }},
    {hd_ui_key, boolean_value_count, [](const Preferences& p) { return int{p.hd_ui}; }, [](Preferences& p, int v) { p.hd_ui = v != 0; }},
    {text_key, text_value_count, [](const Preferences& p) { return p.text_language; }, [](Preferences& p, int v) { p.text_language = v; }},
    {soundtrack_key, soundtrack_value_count, [](const Preferences& p) { return p.soundtrack; }, [](Preferences& p, int v) { p.soundtrack = v; }},
    {remaster_effects_key, boolean_value_count, [](const Preferences& p) { return int{p.remaster_effects}; },
     [](Preferences& p, int v) { p.remaster_effects = v != 0; }},
    {master_volume_key, volume_value_count, [](const Preferences& p) { return p.audio.master.volume; },
     [](Preferences& p, int v) { p.audio.master.volume = v; }},
    {master_mute_key, boolean_value_count, [](const Preferences& p) { return int{p.audio.master.muted}; },
     [](Preferences& p, int v) { p.audio.master.muted = v != 0; }},
    {music_volume_key, volume_value_count, [](const Preferences& p) { return p.audio.categories[LOM_AUDIO_MUSIC].volume; },
     [](Preferences& p, int v) { p.audio.categories[LOM_AUDIO_MUSIC].volume = v; }},
    {music_mute_key, boolean_value_count, [](const Preferences& p) { return int{p.audio.categories[LOM_AUDIO_MUSIC].muted}; },
     [](Preferences& p, int v) { p.audio.categories[LOM_AUDIO_MUSIC].muted = v != 0; }},
    {sfx_volume_key, volume_value_count, [](const Preferences& p) { return p.audio.categories[LOM_AUDIO_SFX].volume; },
     [](Preferences& p, int v) { p.audio.categories[LOM_AUDIO_SFX].volume = v; }},
    {sfx_mute_key, boolean_value_count, [](const Preferences& p) { return int{p.audio.categories[LOM_AUDIO_SFX].muted}; },
     [](Preferences& p, int v) { p.audio.categories[LOM_AUDIO_SFX].muted = v != 0; }},
};

/**
 * @brief Parse a complete bounded decimal field after its preference key.
 * @param line Complete preference line to inspect.
 * @param name Expected key prefix including its equals sign.
 * @param limit Exclusive nonnegative upper bound.
 * @param result Receives the parsed value only on success.
 * @return True on success; leaves the output unchanged on rejection.
 */
bool parse_number(const std::string& line, const std::string& name, int limit, int& result)
{
    if (line.compare(0, name.size(), name) != 0)
    {
        return false;
    }

    const char* start = line.c_str() + name.size();
    char* end;
    errno = 0;
    long value = std::strtol(start, &end, decimal_radix);

    if (end == start || errno || value < 0 || value >= limit)
    {
        return false;
    }

    while (*end == ' ' || *end == '\t' || *end == '\r')
    {
        ++end;
    }

    if (*end)
    {
        return false;
    }

    result = static_cast<int>(value);
    return true;
}

/**
 * @brief Encode a complete preference snapshot in the existing text format.
 * @param preferences Snapshot to encode.
 * @return Serialized settings and per-player mappings.
 */
std::string serialize(const Preferences& preferences)
{
    std::ostringstream output;
    output << file_heading;

    for (const Field& field : fields)
    {
        output << field.key << field.get(preferences) << '\n';
    }

    for (int id = 0; id < LOM_SWITCH_COUNT; ++id)
    {
        if (lom_native_switch_player_setting(static_cast<LomSwitch>(id)))
        {
            output << lom_native_switch_name(static_cast<LomSwitch>(id)) << '=' << int{preferences.switches[id]} << '\n';
        }
    }

    for (int port = 0; port < player_count; ++port)
    {
        for (int button = 0; button < button_count; ++button)
        {
            output << keyboard_prefix << port + 1 << '_' << button << '=' << preferences.controls.keys[port][button] << '\n'
                   << gamepad_prefix << port + 1 << '_' << button << '=' << preferences.controls.pads[port][button] + 1 << '\n';
        }
    }

    return output.str();
}
}

Preferences PreferenceStore::load(std::string path, const Preferences& defaults)
{
    path_ = std::move(path);
    persisted_ = defaults;
    attempted_save_ = false;
    error_.clear();
    std::ifstream input(path_);
    BindingPresence keys{}, pads{};
    std::string line;

    while (std::getline(input, line))
    {
        auto number = [&](const std::string& name, int limit, int& result) { return parse_number(line, name, limit, result); };
        int value;

        for (const Field& field : fields)
        {
            if (number(field.key, field.limit, value))
            {
                field.set(persisted_, value);
            }
        }

        for (int id = 0; id < LOM_SWITCH_COUNT; ++id)
        {
            if (lom_native_switch_player_setting(static_cast<LomSwitch>(id)) &&
                number(std::string(lom_native_switch_name(static_cast<LomSwitch>(id))) + '=', boolean_value_count, value))
            {
                persisted_.switches[id] = value != 0;
            }
        }

        for (int port = 0; port < player_count; ++port)
        {
            for (int button = 0; button < button_count; ++button)
            {
                std::string suffix = std::to_string(port + 1) + '_' + std::to_string(button) + '=';

                if (number(keyboard_prefix + suffix, SDL_SCANCODE_COUNT, value) && valid_key(value))
                {
                    persisted_.controls.keys[port][button] = value;
                    keys[port][button] = true;
                }

                if (number(gamepad_prefix + suffix, stored_gamepad_value_count, value) && valid_pad(value - 1))
                {
                    persisted_.controls.pads[port][button] = value - 1;
                    pads[port][button] = true;
                }
            }
        }
    }

    normalize_bindings(persisted_.controls, keys, pads);
    return persisted_;
}

bool PreferenceStore::save(const Preferences& preferences, SaveReason reason)
{
    if (preferences == persisted_)
    {
        error_.clear();
        attempted_save_ = false;
        return true;
    }

    if (reason == SaveReason::Changed && attempted_save_ && preferences == attempted_)
    {
        return error_.empty();
    }

    attempted_ = preferences;
    attempted_save_ = true;

    if (path_.empty())
    {
        error_ = missing_directory_error;
        return false;
    }

    const std::string temporary = path_ + temporary_suffix;
    const std::string contents = serialize(preferences);

    if (!SDL_SaveFile(temporary.c_str(), contents.data(), contents.size()) || !SDL_RenamePath(temporary.c_str(), path_.c_str()))
    {
        error_ = save_error;
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, save_error_log_format, error_.c_str(), SDL_GetError());
        SDL_RemovePath(temporary.c_str());
        return false;
    }

    persisted_ = preferences;
    error_.clear();
    return true;
}

const Preferences& PreferenceStore::persisted() const
{
    return persisted_;
}

const std::string& PreferenceStore::path() const
{
    return path_;
}

const std::string& PreferenceStore::error() const
{
    return error_;
}

}
