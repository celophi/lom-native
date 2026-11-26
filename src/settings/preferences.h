#ifndef LOM_SETTINGS_PREFERENCES_H
#define LOM_SETTINGS_PREFERENCES_H

#include "control_bindings.h"
#include "audio_settings.h"
#include "display_settings.h"

#include <map>
#include <string>

namespace lom::settings
{

struct Preferences
{
    DisplaySettings display;

    AudioSettings audio;

    ControlBindings controls;

    bool pause_unfocused = false;

    bool hd_backgrounds = true;

    bool hd_ui = true;

    int text_language = 0;

    int soundtrack = 0;

    bool remaster_effects = false;

    std::map<std::string, std::string> mod_options;

    bool operator==(const Preferences& other) const
    {
        return display == other.display && audio == other.audio && controls == other.controls && pause_unfocused == other.pause_unfocused &&
               hd_backgrounds == other.hd_backgrounds && hd_ui == other.hd_ui && text_language == other.text_language &&
               soundtrack == other.soundtrack && remaster_effects == other.remaster_effects && mod_options == other.mod_options;
    }
};

}
#endif
