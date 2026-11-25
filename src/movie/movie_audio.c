#include "lom_native_movie_audio.h"
#include <string.h>

/**
 * @brief Scale a sample by one of the SPU's gains, as the PS1 does: the gain's
 *        low 15 bits doubled and taken as signed, rounding toward minus
 *        infinity.
 * @param sample The sample.
 * @param gain The gain.
 * @return The scaled sample.
 */
static int scaled(int16_t sample, int16_t gain)
{
    int effective = ((uint16_t)gain & 0x7FFFu) << 1;

    if (effective >= 32768)
    {
        effective -= 65536;
    }

    int32_t product = (int32_t)sample * effective;
    return product / 32768 - (product < 0 && product % 32768 != 0);
}

/**
 * @brief Clip a sample to 16 bits.
 * @param sample The sample.
 * @return It, clipped.
 */
static int16_t clip(int sample)
{
    return sample > 32767 ? 32767 : sample < -32768 ? -32768 : (int16_t)sample;
}

void lom_movie_audio_apply(void* pcm, size_t frames, const LomMovieAudioGains* gains)
{
    unsigned char* bytes = pcm;

    for (size_t i = 0; i < frames; ++i)
    {
        int16_t pair[2];
        memcpy(pair, bytes + i * sizeof(pair), sizeof(pair));
        int left = scaled(pair[0], gains->left_to_left) + scaled(pair[1], gains->right_to_left);
        int right = scaled(pair[0], gains->left_to_right) + scaled(pair[1], gains->right_to_right);
        pair[0] = clip(left);
        pair[1] = clip(right);
        memcpy(bytes + i * sizeof(pair), pair, sizeof(pair));
    }
}
