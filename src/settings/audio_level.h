#ifndef LOM_SETTINGS_AUDIO_LEVEL_H
#define LOM_SETTINGS_AUDIO_LEVEL_H

namespace lom::settings
{

struct AudioLevel
{
    static constexpr int maximum_volume = 100;

    int volume = maximum_volume;

    bool muted = false;

    bool operator==(const AudioLevel& other) const
    {
        return volume == other.volume && muted == other.muted;
    }
};

}
#endif
