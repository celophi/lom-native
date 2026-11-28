#include "movie_native.c"
#include <limits.h>
#include "../support/lom_check.h"
static SDL_AudioStream* fake_stream = (SDL_AudioStream*)(uintptr_t)1;

static SDL_AudioStreamCallback callback;

static void* callback_userdata;

static unsigned alloc_calls, fail_alloc, live_allocs;

static int lock_depth, fail_lock, fail_callback_install, fail_put, fail_at_detach;

static unsigned put_calls, destroyed, maximum_put;

static short delivered[20000];

static size_t delivered_samples;
static unsigned observed_puts, observed_pulls;

static LomMovieAudioFifo* observed_fifo;

extern int __real_lom_movie_audio_fifo_put(LomMovieAudioFifo*, const void*, size_t);

extern size_t __real_lom_movie_audio_fifo_pull(LomMovieAudioFifo*, void*, size_t);

/**
 * @brief Note the queue the output writes to and count its writes.
 * @param fifo The queue.
 * @param raw The frames.
 * @param frames How many.
 * @return What the real one returns.
 */
int __wrap_lom_movie_audio_fifo_put(LomMovieAudioFifo* fifo, const void* raw, size_t frames)
{
    observed_fifo = fifo;
    ++observed_puts;
    return __real_lom_movie_audio_fifo_put(fifo, raw, frames);
}

/**
 * @brief Count the reads from the queue.
 * @param fifo The queue.
 * @param raw Where to write.
 * @param frames How many.
 * @return What the real one returns.
 */
size_t __wrap_lom_movie_audio_fifo_pull(LomMovieAudioFifo* fifo, void* raw, size_t frames)
{
    ++observed_pulls;
    return __real_lom_movie_audio_fifo_pull(fifo, raw, frames);
}

/**
 * @brief A malloc that fails at the allocation the test asks for, and counts
 *        the live ones.
 * @param bytes How many bytes.
 * @return The memory, or NULL.
 */
void* movie_test_malloc(size_t bytes)
{
    ++alloc_calls;

    if (fail_alloc == alloc_calls)
    {
        return NULL;
    }

    void* p = malloc(bytes);

    if (p)
    {
        ++live_allocs;
    }

    return p;
}

/**
 * @brief A calloc that fails at the allocation the test asks for, and counts
 *        the live ones.
 * @param n How many items.
 * @param bytes Each item's size.
 * @return The memory, or NULL.
 */
void* movie_test_calloc(size_t n, size_t bytes)
{
    ++alloc_calls;

    if (fail_alloc == alloc_calls)
    {
        return NULL;
    }

    void* p = calloc(n, bytes);

    if (p)
    {
        ++live_allocs;
    }

    return p;
}

/**
 * @brief A free that counts the live allocations.
 * @param p The memory, or NULL.
 */
void movie_test_free(void* p)
{
    if (p)
    {
        --live_allocs;
    }

    free(p);
}

/**
 * @brief Lock the fake stream, or fail when the test asks.
 * @param stream The fake stream.
 * @return Whether it locked.
 */
bool SDL_LockAudioStream(SDL_AudioStream* stream)
{
    CHECK_COUNTED(stream == fake_stream);

    if (fail_lock)
    {
        return false;
    }

    ++lock_depth;
    return true;
}

/**
 * @brief Unlock the fake stream.
 * @param stream The fake stream.
 * @return true.
 */
bool SDL_UnlockAudioStream(SDL_AudioStream* stream)
{
    CHECK_COUNTED(stream == fake_stream && lock_depth > 0);
    --lock_depth;
    return true;
}

/**
 * @brief Set the fake stream's callback, or fail when the test asks. Taking it
 *        off can run it once more, failing, to test a detach mid-callback.
 * @param stream The fake stream.
 * @param fn The callback, or NULL.
 * @param userdata Its data.
 * @return Whether it was set.
 */
bool SDL_SetAudioStreamGetCallback(SDL_AudioStream* stream, SDL_AudioStreamCallback fn, void* userdata)
{
    CHECK_COUNTED(stream == fake_stream);

    if (fail_callback_install)
    {
        return false;
    }

    if (!fn && fail_at_detach && callback)
    {
        fail_put = 1;
        callback(callback_userdata, stream, 4, 4);
        fail_put = 0;
    }

    callback = fn;
    callback_userdata = userdata;
    return true;
}

/**
 * @brief Keep what the output gives the fake stream, or fail when the test
 *        asks.
 * @param stream The fake stream.
 * @param data The samples.
 * @param bytes How many bytes, a whole number of frames.
 * @return Whether it took them.
 */
bool SDL_PutAudioStreamData(SDL_AudioStream* stream, const void* data, int bytes)
{
    CHECK_COUNTED(stream == fake_stream && lock_depth > 0 && bytes > 0 && !(bytes % 4));
    ++put_calls;

    if ((unsigned)bytes > maximum_put)
    {
        maximum_put = bytes;
    }

    if (fail_put)
    {
        return false;
    }

    CHECK_COUNTED(delivered_samples + (size_t)bytes / 2 <= sizeof(delivered) / sizeof(*delivered));
    memcpy(delivered + delivered_samples, data, (size_t)bytes);
    delivered_samples += (size_t)bytes / 2;
    return true;
}

/**
 * @brief Count the fake stream's destruction, which must come after its
 *        callback is off.
 * @param stream The fake stream.
 */
void SDL_DestroyAudioStream(SDL_AudioStream* stream)
{
    CHECK_COUNTED(stream == fake_stream && !callback);
    ++destroyed;
}

/**
 * @brief SDL's SetError, which the fakes don't keep.
 * @param format Ignored.
 * @return false.
 */
bool SDL_SetError(const char* format, ...)
{
    (void)format;
    return false;
}

/**
 * @brief SDL's GetError, for the messages.
 * @return What the fakes say failed.
 */
const char* SDL_GetError(void)
{
    return "CPU injected SDL boundary failure";
}

/**
 * @brief Have the fake stream ask the output for samples, as SDL's audio thread
 *        would.
 * @param bytes How many bytes it wants.
 */
static void request(int bytes)
{
    if (callback)
    {
        ++lock_depth;
        callback(callback_userdata, fake_stream, bytes, bytes);
        --lock_depth;
    }
}

/**
 * @brief Make an output on the fake stream at full volume.
 * @param frames Its queue's length.
 * @return The output.
 */
static LomMovieAudioOutput* create(size_t frames)
{
    return lom_movie_audio_output_create(fake_stream, frames, lom_movie_audio_gains(0, 0, 127u << 8));
}

int main(void)
{
    CHECK_COUNTED(!create(0));
    CHECK_COUNTED(!create(SIZE_MAX));
    CHECK_COUNTED(!live_allocs && !callback);
    unsigned previous_allocs = alloc_calls, previous_destroyed = destroyed;
    CHECK_COUNTED(!lom_movie_audio_output_create(NULL, 4, lom_movie_audio_gains(0, 0, 127u << 8)));
    CHECK_COUNTED(alloc_calls == previous_allocs && destroyed == previous_destroyed && !live_allocs && !callback);

    for (unsigned n = 1; n <= 2; ++n)
    {
        alloc_calls = 0;
        fail_alloc = n;
        CHECK_COUNTED(!create(8));
        CHECK_COUNTED(!live_allocs && !callback);
    }

    fail_alloc = 0;
    fail_callback_install = 1;
    CHECK_COUNTED(!create(8));
    CHECK_COUNTED(!live_allocs && !callback);
    fail_callback_install = 0;
    LomMovieAudioOutput* output = create(9000);
    CHECK_COUNTED(output && callback && live_allocs == 2);
    request(0);
    request(-1);
    request(INT_MAX);
    CHECK_COUNTED(!put_calls && !lom_movie_audio_output_failed(output));
    short raw[9000 * 2];

    for (unsigned i = 0; i < 18000; ++i)
    {
        raw[i] = (short)(i * 7 - 30000);
    }

    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 9000));
    request(INT_MAX);
    CHECK_COUNTED(put_calls == 3 && maximum_put == 4096 * 4 && delivered_samples == 18000);

    for (unsigned i = 0; i < 18000; ++i)
    {
        int product = raw[i] * 32512;
        int expected = product / 32768 - (product < 0 && product % 32768 != 0);
        CHECK_COUNTED(delivered[i] == expected);
    }

    CHECK_COUNTED(!lom_movie_audio_output_set_volume(output, 0));
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 2));
    size_t before = delivered_samples;
    request(5);
    CHECK_COUNTED(delivered_samples == before + 4 && !delivered[before] && !delivered[before + 1]);
    unsigned calls = put_calls;
    request(4);
    CHECK_COUNTED(put_calls == calls);
    CHECK_COUNTED(!lom_movie_audio_output_set_volume(output, 127));
    CHECK_COUNTED(lom_movie_audio_output_set_volume(output, 128) < 0);
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 2));
    fail_put = 1;
    request(4);
    CHECK_COUNTED(lom_movie_audio_output_failed(output));
    CHECK_COUNTED(lom_movie_audio_output_publish(output, raw, 1) < 0);
    calls = put_calls;
    request(100);
    CHECK_COUNTED(put_calls == calls);
    fail_put = 0;
    CHECK_COUNTED(lom_movie_audio_output_destroy(output) < 0);
    CHECK_COUNTED(!callback && !live_allocs && !lock_depth);
    output = create(4);
    CHECK_COUNTED(output);
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 4));
    request(8);
    CHECK_COUNTED(observed_fifo && observed_fifo->queued == 2 && observed_fifo->read == 2);
    LomMovieAudioFifo saved_fifo = *observed_fifo;
    unsigned char saved_raw[16];
    memcpy(saved_raw, observed_fifo->raw, sizeof(saved_raw));
    unsigned old_puts = observed_puts, old_pulls = observed_pulls, old_callbacks = put_calls;
    size_t old_delivered = delivered_samples;
    fail_lock = 1;
    CHECK_COUNTED(lom_movie_audio_output_publish(output, raw + 8, 1) < 0);
    CHECK_COUNTED(lom_movie_audio_output_set_volume(output, 0) < 0);
    CHECK_COUNTED(lom_movie_audio_output_failed(output));
    CHECK_COUNTED(!memcmp(&saved_fifo, observed_fifo, sizeof(saved_fifo)) && !memcmp(saved_raw, observed_fifo->raw, sizeof(saved_raw)));
    CHECK_COUNTED(observed_puts == old_puts && observed_pulls == old_pulls && put_calls == old_callbacks && delivered_samples == old_delivered && !lock_depth);
    fail_lock = 0;
    CHECK_COUNTED(!lom_movie_audio_output_failed(output));
    CHECK_COUNTED(!memcmp(&saved_fifo, observed_fifo, sizeof(saved_fifo)) && !memcmp(saved_raw, observed_fifo->raw, sizeof(saved_raw)));
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw + 8, 2));
    request(16);
    CHECK_COUNTED(delivered_samples == old_delivered + 8);

    for (unsigned i = 0; i < 8; ++i)
    {
        int product = raw[i + 4] * 32512;
        int expected = product / 32768 - (product < 0 && product % 32768 != 0);
        CHECK_COUNTED(delivered[old_delivered + i] == expected);
    }

    CHECK_COUNTED(!observed_fifo->queued && !lom_movie_audio_output_failed(output));
    CHECK_COUNTED(!lom_movie_audio_output_destroy(output));
    CHECK_COUNTED(!callback && !live_allocs && !lock_depth);
    output = create(8);
    CHECK_COUNTED(output);
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 1));
    fail_at_detach = 1;
    CHECK_COUNTED(lom_movie_audio_output_destroy(output) < 0);
    fail_at_detach = 0;
    CHECK_COUNTED(!callback && !live_allocs);
    output = create(8);
    CHECK_COUNTED(output);
    CHECK_COUNTED(!lom_movie_audio_output_publish(output, raw, 8));
    CHECK_COUNTED(lom_movie_audio_output_publish(output, raw, 1) < 0);
    CHECK_COUNTED(lom_movie_audio_output_failed(output));
    CHECK_COUNTED(lom_movie_audio_output_destroy(output) < 0);
    output = create(LOM_NATIVE_MOVIE_AUDIO_PREBUFFER_FRAMES * LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME);
    CHECK_COUNTED(output);
    FILE* file = tmpfile();
    CHECK_COUNTED(file);
    unsigned char partial[7] = {1, 2, 3, 4, 5, 6, 7}, chunk[LOM_NATIVE_MOVIE_AUDIO_BYTES_PER_VIDEO_FRAME];
    CHECK_COUNTED(fwrite(partial, 1, sizeof(partial), file) == sizeof(partial));
    rewind(file);
    calls = put_calls;
    CHECK_COUNTED(queue_movie_audio_frame(output, file, chunk, sizeof(chunk), 0) < 0);
    request(INT_MAX);
    CHECK_COUNTED(put_calls == calls);
    CHECK_COUNTED(!lom_movie_audio_output_failed(output));
    fclose(file);
    file = tmpfile();
    CHECK_COUNTED(file);
    CHECK_COUNTED(queue_movie_audio_frame(output, file, chunk, sizeof(chunk), 0) == 0);
    CHECK_COUNTED(prebuffer_movie_audio(output, file, chunk, sizeof(chunk), 0) < 0);
    fclose(file);
    file = tmpfile();
    CHECK_COUNTED(file);
    memset(chunk, 0, sizeof(chunk));

    for (unsigned i = 0; i < 4; ++i)
    {
        CHECK_COUNTED(fwrite(chunk, 1, sizeof(chunk), file) == sizeof(chunk));
    }

    rewind(file);
    CHECK_COUNTED(!prebuffer_movie_audio(output, file, chunk, sizeof(chunk), 0));
    CHECK_COUNTED(put_calls == calls);
    CHECK_COUNTED(queue_movie_audio_frame(output, file, chunk, sizeof(chunk), 0) == 0);
    fclose(file);
    CHECK_COUNTED(!lom_movie_audio_output_destroy(output));
    CHECK_COUNTED(!callback && !live_allocs && !lock_depth);
    output = create(4 * LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME);
    CHECK_COUNTED(output);
    file = tmpfile();
    CHECK_COUNTED(file);

    for (unsigned i = 0; i < 3; ++i)
    {
        CHECK_COUNTED(fwrite(chunk, 1, sizeof(chunk), file) == sizeof(chunk));
    }

    CHECK_COUNTED(fwrite(partial, 1, sizeof(partial), file) == sizeof(partial));
    rewind(file);
    calls = put_calls;
    CHECK_COUNTED(prebuffer_movie_audio(output, file, chunk, sizeof(chunk), 0) < 0);
    CHECK_COUNTED(put_calls == calls);
    fclose(file);
    CHECK_COUNTED(!lom_movie_audio_output_destroy(output));
    CHECK_COUNTED(!callback && !live_allocs && !lock_depth);
    output = create(4 * LOM_NATIVE_MOVIE_AUDIO_FRAMES_PER_VIDEO_FRAME);
    CHECK_COUNTED(output);
    file = tmpfile();
    CHECK_COUNTED(file);
    memset(chunk, 0, sizeof(chunk));

    for (unsigned i = 0; i < 3; ++i)
    {
        CHECK_COUNTED(fwrite(chunk, 1, sizeof(chunk), file) == sizeof(chunk));
    }

    CHECK_COUNTED(fwrite(partial, 1, sizeof(partial), file) == sizeof(partial));
    rewind(file);
    CHECK_COUNTED(!prebuffer_movie_audio(output, file, chunk, sizeof(chunk), 1));
    CHECK_COUNTED(!memcmp(chunk, partial, sizeof(partial)) && chunk[sizeof(partial)] == 0 && chunk[sizeof(chunk) - 1] == 0);
    CHECK_COUNTED(queue_movie_audio_frame(output, file, chunk, sizeof(chunk), 1) == 0);
    fclose(file);
    CHECK_COUNTED(!lom_movie_audio_output_destroy(output));
    CHECK_COUNTED(!callback && !live_allocs && !lock_depth);
    CHECK_COUNTED(!lom_movie_audio_output_destroy(NULL));
    CHECK_COUNTED(observed_puts > 0 && observed_pulls > 0);
    printf("FIFO wrappers: %u puts, %u pulls\n", observed_puts, observed_pulls);
    printf("movie output edges: %u checks, %u failures; largest callback %u bytes\n", lom_check_count, lom_check_failures, maximum_put);
    return lom_check_failures ? 1 : 0;
}
