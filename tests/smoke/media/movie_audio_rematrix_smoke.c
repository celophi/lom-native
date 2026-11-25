#include "movie_audio_fifo.h"
#include "../../support/lom_check.h"
#include "../../support/oracles/movie_audio_original_voice_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_THAT(passed, message) lom_check_counted((passed) ? 1 : 0, __FILE__, __LINE__, (message))

/**
 * @brief Make movie audio gains from four numbers.
 * @param g Left to left, right to left, left to right, right to right.
 * @return The gains.
 */
static LomMovieAudioGains from_raw(int g[4])
{
    return (LomMovieAudioGains){g[0], g[1], g[2], g[3]};
}

/**
 * @brief Scale a sample by one of the SPU's gains, as the PS1 does.
 * @param s The sample.
 * @param g The gain.
 * @return The scaled sample.
 */
static int scale(int s, int g)
{
    int eff = (g & 32767) * 2;

    if (eff >= 32768)
    {
        eff -= 65536;
    }

    long long p = (long long)s * eff;
    return p >= 0 ? p / 32768 : -((-p + 32767) / 32768);
}

/**
 * @brief Clip a sample to 16 bits.
 * @param x The sample.
 * @return It, clipped.
 */
static int clip(int x)
{
    return x > 32767 ? 32767 : x < -32768 ? -32768 : x;
}

int main(int argc, char** argv)
{
    int baseline = argc > 1 && !strcmp(argv[1], "baseline");
    short pattern[8][2] = {{16000, 0}, {0, 16000}, {-1, 0}, {0, -1}, {32767, 32767}, {-32768, -32768}, {12345, -2222}, {16000, -16000}};
    unsigned char storage[64 * 4 + 1], initial[32 * 4], output[8 * 4];
    int raw[4];
    original_movie_gains(1, 17418, 127u << 8, raw);
    LomMovieAudioFifo fifo = {storage + 1, 64, 0, 0, from_raw(raw)};

    for (int n = 0; n < 4; ++n)
    {
        memcpy(initial + n * sizeof(pattern), pattern, sizeof(pattern));
    }

    if (baseline)
    {
        lom_movie_audio_apply(initial, 32, &fifo.gains);
    }

    CHECK_THAT(!lom_movie_audio_fifo_put(&fifo, initial, 32), "four raw prebuffer blocks accepted");

    if (baseline)
    {
        fifo.gains = (LomMovieAudioGains){16383, 0, 0, 16383};
    }

    CHECK_THAT(lom_movie_audio_fifo_pull(&fifo, output, 8) == 8, "first block consumed once");
    original_skip_gains(112u << 8, raw);
    fifo.gains = from_raw(raw);

    if (baseline)
    {
        memcpy(output, fifo.raw + fifo.read * 4, sizeof(output));
        fifo.read += 8;
        fifo.queued -= 8;
        short pair[8][2];
        memcpy(pair, output, sizeof(pair));

        for (int i = 0; i < 8; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                pair[i][c] = (short)(pair[i][c] * (112.0f / 127.0f));
            }
        }

        memcpy(output, pair, sizeof(pair));
    }
    else
    {
        CHECK_THAT(lom_movie_audio_fifo_pull(&fifo, output, 8) == 8, "already buffered block survives switch");
    }

    short actual[8][2];
    memcpy(actual, output, sizeof(actual));

    for (int n = 0; n < 8; ++n)
    {
        CHECK_THAT(actual[n][0] == clip(scale(pattern[n][0], raw[0]) + scale(pattern[n][1], raw[2])), "post-prime left follows original E4");
        CHECK_THAT(actual[n][1] == clip(scale(pattern[n][0], raw[1]) + scale(pattern[n][1], raw[3])), "post-prime right follows original E4");
    }

    if (!baseline)
    {
        for (int level = 112; level >= 0; level -= 16)
        {
            original_skip_gains((unsigned)level << 8, raw);
            fifo.gains = from_raw(raw);
            CHECK_THAT(!lom_movie_audio_fifo_put(&fifo, pattern, 8), "append raw after fade update");
            CHECK_THAT(lom_movie_audio_fifo_pull(&fifo, output, 8) == 8, "fade drains one synchronized block");
            memcpy(actual, output, sizeof(actual));

            for (int n = 0; n < 8; ++n)
            {
                CHECK_THAT(actual[n][0] == clip(scale(pattern[n][0], raw[0]) + scale(pattern[n][1], raw[2])), "exact left fade register level");
                CHECK_THAT(actual[n][1] == clip(scale(pattern[n][0], raw[1]) + scale(pattern[n][1], raw[3])), "exact right fade register level");
            }
        }

        unsigned char saved[sizeof(storage)];
        memcpy(saved, storage, sizeof(storage));
        size_t queued = fifo.queued;
        CHECK_THAT(lom_movie_audio_fifo_put(&fifo, initial, 64) < 0, "overflow rejected atomically");
        CHECK_THAT(fifo.queued == queued && !memcmp(saved, storage, sizeof(storage)), "overflow does not corrupt queued samples");

        while (fifo.queued)
        {
            lom_movie_audio_fifo_pull(&fifo, output, 8);
        }

        CHECK_THAT(lom_movie_audio_fifo_pull(&fifo, output, 8) == 0, "empty queue has no fabricated samples");
    }

    printf("output-time rematrix: %u checks, %u failures (%s)\n", lom_check_count, lom_check_failures, baseline ? "baseline" : "fixed");
    return lom_check_failures ? 1 : 0;
}
