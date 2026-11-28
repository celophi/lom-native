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

    bool remaster = true;

    bool hd_backgrounds = true;

    bool hd_faces = true;

    bool hd_encyclopedia = true;

    bool hd_title = true;

    bool hd_name_entry = true;

    bool hd_world_select = true;

    bool hd_world_map = true;

    bool hd_menu = true;

    bool hd_saves = true;

    bool hd_movies = true;

    bool remaster_music = true;

    bool arranged_music = false;

    bool remaster_effects = true;

    bool remaster_text = true;

    int text_language = 1;

    std::map<std::string, std::string> mod_options;

    bool operator==(const Preferences& other) const
    {
        return display == other.display && audio == other.audio && controls == other.controls && pause_unfocused == other.pause_unfocused &&
               remaster == other.remaster && hd_backgrounds == other.hd_backgrounds && hd_faces == other.hd_faces && hd_encyclopedia == other.hd_encyclopedia &&
               hd_title == other.hd_title && hd_name_entry == other.hd_name_entry && hd_world_select == other.hd_world_select &&
               hd_world_map == other.hd_world_map && hd_menu == other.hd_menu && hd_saves == other.hd_saves && hd_movies == other.hd_movies &&
               remaster_music == other.remaster_music && arranged_music == other.arranged_music && remaster_effects == other.remaster_effects &&
               remaster_text == other.remaster_text && text_language == other.text_language && mod_options == other.mod_options;
    }
};

}
#endif
