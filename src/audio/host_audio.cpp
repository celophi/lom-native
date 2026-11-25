#include "lom_native_host_audio.h"
#include "lom_native_audio_categories.h"
#include <SDL3/SDL.h>
#include "../settings/service.h"

namespace
{
constexpr int maximum_volume = lom::settings::AudioLevel::maximum_volume;

constexpr float percentage_divisor = static_cast<float>(maximum_volume);

constexpr char master_gain_error_format[] = "Could not set master volume: %s";

struct AudioStreams
{
    SDL_AudioStream* mixed = nullptr;

    SDL_AudioStream* movie = nullptr;

    bool resume_movie = false;
} streams;

/**
 * @brief Access the audio settings, on the SDL thread.
 * @return Borrowed audio settings from the application model.
 */
lom::settings::AudioSettings& audio_settings()
{
    return lom::settings::settings().values().audio;
}

/**
 * @brief Apply master volume and mute to a borrowed stream, logging SDL
 *        failures.
 * @param stream Borrowed stream, or null to unregister; creators unregister
 *        before destruction.
 */
void apply_master_gain(SDL_AudioStream* stream)
{
    const auto& master = audio_settings().master;

    if (stream && !SDL_SetAudioStreamGain(stream, master.muted ? 0.0f : master.volume / percentage_divisor))
    {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, master_gain_error_format, SDL_GetError());
    }
}

/**
 * @brief Check an audio category is one of LOM_AUDIO_*.
 * @param category The category.
 * @return Whether it is.
 */
bool valid_category(int category)
{
    return category >= 0 && category < LOM_AUDIO_CATEGORY_COUNT;
}
}

namespace
{
void (*category_sink)(int music, int sfx);
bool categories_pending;
}

extern "C" void lom_native_audio_apply_categories()
{
    const auto& music = audio_settings().categories[LOM_AUDIO_MUSIC];
    const auto& sfx = audio_settings().categories[LOM_AUDIO_SFX];

    if (!category_sink)
    {
        categories_pending = true;
        return;
    }

    categories_pending = false;
    category_sink(music.muted ? 0 : music.volume, sfx.muted ? 0 : sfx.volume);
}

extern "C" void lom_native_audio_set_category_sink(void (*sink)(int music, int sfx))
{
    category_sink = sink;

    if (sink && categories_pending)
    {
        lom_native_audio_apply_categories();
    }
}

extern "C" int lom_native_audio_category_volume(int category)
{
    return valid_category(category) ? audio_settings().categories[category].volume : maximum_volume;
}

extern "C" bool lom_native_audio_category_muted(int category)
{
    return valid_category(category) && audio_settings().categories[category].muted;
}

extern "C" void lom_native_audio_category_set(int category, int volume, bool muted)
{
    if (!valid_category(category) || volume < 0 || volume > maximum_volume)
    {
        return;
    }

    audio_settings().categories[category] = {volume, muted};
    lom_native_audio_apply_categories();
}

extern "C" int lom_native_audio_volume()
{
    return audio_settings().master.volume;
}

extern "C" bool lom_native_audio_muted()
{
    return audio_settings().master.muted;
}

extern "C" void lom_native_audio_set(int volume, bool muted)
{
    if (volume < 0 || volume > maximum_volume)
    {
        return;
    }

    audio_settings().master = {volume, muted};
    apply_master_gain(streams.mixed);
    apply_master_gain(streams.movie);
}

extern "C" void lom_native_menu_audio_stream(SDL_AudioStream* stream)
{
    streams.mixed = stream;
    apply_master_gain(stream);
}

extern "C" void lom_native_menu_movie_stream(SDL_AudioStream* stream)
{
    streams.movie = stream;
    streams.resume_movie = false;
    apply_master_gain(stream);
}

extern "C" void lom_native_menu_pause_movie(bool paused)
{
    if (!streams.movie)
    {
        return;
    }

    if (paused)
    {
        streams.resume_movie |= !SDL_AudioStreamDevicePaused(streams.movie);
        SDL_PauseAudioStreamDevice(streams.movie);
    }
    else if (streams.resume_movie)
    {
        SDL_ResumeAudioStreamDevice(streams.movie);
        streams.resume_movie = false;
    }
}
