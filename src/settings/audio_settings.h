#ifndef LOM_SETTINGS_AUDIO_SETTINGS_H
#define LOM_SETTINGS_AUDIO_SETTINGS_H

#include "audio_level.h"
#include "lom_native_audio_categories.h"
#include <array>

namespace lom::settings
{

struct AudioSettings
{
    AudioLevel master;

    std::array<AudioLevel, LOM_AUDIO_CATEGORY_COUNT> categories{};

    bool operator==(const AudioSettings& other) const
    {
        return master == other.master && categories == other.categories;
    }
};

}
#endif
