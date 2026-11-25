#include "movie_audio_fifo.h"
#include <string.h>

int lom_movie_audio_fifo_put(LomMovieAudioFifo* fifo, const void* raw, size_t frames)
{
    if (frames > fifo->capacity - fifo->queued || (frames && !raw))
    {
        return -1;
    }

    if (!frames)
    {
        return 0;
    }

    const unsigned char* input = raw;
    size_t write = (fifo->read + fifo->queued) % fifo->capacity;

    for (size_t i = 0; i < frames; ++i)
    {
        memcpy(fifo->raw + write * 4, input + i * 4, 4);
        write = (write + 1) % fifo->capacity;
    }

    fifo->queued += frames;
    return 0;
}

size_t lom_movie_audio_fifo_pull(LomMovieAudioFifo* fifo, void* output, size_t frames)
{
    if (frames > fifo->queued)
    {
        frames = fifo->queued;
    }

    unsigned char* bytes = output;

    for (size_t i = 0; i < frames; ++i)
    {
        memcpy(bytes + i * 4, fifo->raw + fifo->read * 4, 4);
        fifo->read = (fifo->read + 1) % fifo->capacity;
    }

    fifo->queued -= frames;
    lom_movie_audio_apply(output, frames, &fifo->gains);
    return frames;
}
