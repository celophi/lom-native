#include "common.h"
#include "lom_native_switches.h"
#include <libgpu.h>
#include <libetc.h>
#include <libpress.h>
#include "overlays/movie/movie_state.h"
#include "lom_native_cd.h"
#include "lom_native_overlay_entry.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "movie_decoder.h"
#include "movie_source.h"
#include "field_movie_native.h"

#if UINTPTR_MAX > UINT32_MAX
_Static_assert(sizeof(MovieState) == 200, "native shared movie state size");

_Static_assert(offsetof(MovieState, rects) == 64, "FIELD movie rectangle view");

_Static_assert(offsetof(MovieState, chunk_idx) == 192, "FIELD movie buffer view");

_Static_assert(offsetof(MovieState, frame_ready) == 197, "FIELD movie ready view");

_Static_assert(offsetof(MovieState, end_state) == 199, "FIELD movie completion view");
#else
_Static_assert(sizeof(MovieState) == 160, "native shared movie state size");

_Static_assert(offsetof(MovieState, rects) == 32, "FIELD movie rectangle view");

_Static_assert(offsetof(MovieState, chunk_idx) == 152, "FIELD movie buffer view");

_Static_assert(offsetof(MovieState, frame_ready) == 157, "FIELD movie ready view");

_Static_assert(offsetof(MovieState, end_state) == 159, "FIELD movie completion view");
#endif

_Static_assert(sizeof(MovieState) <= MEMORY_MAP_SPACING(0x100), "movie state must fit before controllers");

static struct
{
    LomPreparedMovie source;
    FILE* pipe;
    pid_t child;
    unsigned char* rgb;
    u16* pixels;
    unsigned int frame_count;
    unsigned int total_frames;
    unsigned int resource;
    unsigned int last_vblank;
} decoder;

void lom_native_field_movie_shutdown(void)
{
    if (decoder.pipe != NULL)
    {
        lom_movie_decoder_close(decoder.pipe, decoder.child, 1);
        decoder.pipe = NULL;
    }

    lom_movie_cleanup_source(&decoder.source);
    free(decoder.rgb);
    free(decoder.pixels);
    decoder.rgb = NULL;
    decoder.pixels = NULL;
}

/**
 * @brief Close the movie and stop FIELD.
 * @param reason Why.
 */
static _Noreturn void fail_movie(const char* reason)
{
    lom_native_field_movie_shutdown();
    lom_native_field_fail(reason);
}

/**
 * @brief Check a rectangle is inside VRAM.
 * @param rect The rectangle.
 * @return Nonzero when it is.
 */
static int valid_rect(const RECT* rect)
{
    return rect->x >= 0 && rect->y >= 0 && rect->w > 0 && rect->h > 0 && rect->x + rect->w <= 1024 && rect->y + rect->h <= 512;
}

void movie_init(s32 resource, s32 mode, s32 frames, s32 visible)
{
    char path[LOM_MOVIE_PATH_MAX];
    MovieState* state = MOVIE_STATE;
    lom_native_field_movie_shutdown();
    decoder.resource = (unsigned int)resource;
    decoder.frame_count = 0;

    if (lom_native_switch(LOM_SWITCH_SKIP_MOVIES) && frames > 0)
    {
        state->resource_index = (u32)resource;
        state->current_frame = (u32)frames;
        state->total_frames = (u32)frames;
        state->frame_ready = 0;
        state->end_of_stream = 1;
        state->end_state = 2;
        return;
    }

    if (mode != 1 || frames <= 0 || visible < 0 || visible > 1 || resource < 0x16A6 || resource > 0x16B0 || (resource & 1))
    {
        fail_movie("invalid in-scene movie configuration");
    }

    if (lom_native_cd_resolve_resource_path((unsigned int)resource, path, sizeof(path)) != 0 || lom_movie_prepare_file(path, &decoder.source) != 0)
    {
        fail_movie("in-scene movie resource preparation failed");
    }

    if (decoder.source.audio_frame_count != 0 || decoder.source.width != (unsigned int)state->rects[0].w ||
        decoder.source.height != (unsigned int)state->rects[0].h)
    {
        fail_movie("in-scene movie dimensions or audio unsupported");
    }

    state->rects[1] = state->rects[0];

    if (state->rects[0].x >= 768)
    {
        state->rects[1].x = 512;
        state->rects[1].y = 0;
    }
    else
    {
        state->rects[1].x += state->rects[0].w;
    }

    if (!valid_rect(&state->rects[0]) || !valid_rect(&state->rects[1]))
    {
        fail_movie("in-scene movie rectangle outside VRAM");
    }

    size_t pixels = (size_t)decoder.source.width * decoder.source.height;
    decoder.rgb = malloc(pixels * 3u);
    decoder.pixels = malloc(pixels * sizeof(*decoder.pixels));

    if (decoder.rgb == NULL || decoder.pixels == NULL)
    {
        fail_movie("in-scene movie allocation failed");
    }

    decoder.total_frames = (unsigned int)frames;
    decoder.pipe = lom_movie_decoder_open_sized(decoder.source.video_path, decoder.total_frames, decoder.source.width, decoder.source.height, &decoder.child);

    if (decoder.pipe == NULL)
    {
        fail_movie("in-scene movie decoder spawn failed");
    }

    decoder.last_vblank = (unsigned int)VSync(-1);

    state->resource_index = (u32)resource;
    state->current_frame = 0;
    state->total_frames = (u32)frames;
    state->gpu_mode = 1;
    state->use_cd_audio = 0;
    state->chunk_idx = (u8)visible;
    state->frame_ready = 0;
    state->end_state = 0;
    state->end_of_stream = 0;
    state->pending_mdec_decode = 0;
    state->pending_vram_upload = 0;
    state->mdec_busy = 0;
    state->rects[MDEC_OUTPUT_RECT_INDEX] = state->rects[visible];
}

void movie_update(void)
{
    MovieState* state = MOVIE_STATE;

    if (decoder.pipe == NULL || state->end_state >= 2 || state->frame_ready)
    {
        return;
    }

    unsigned int vblank = (unsigned int)VSync(-1);

    if (vblank - decoder.last_vblank < 4)
    {
        return;
    }

    decoder.last_vblank = vblank;
    size_t pixels = (size_t)decoder.source.width * decoder.source.height;
    LomMovieReadResult result = lom_movie_decoder_read_frame(decoder.pipe, decoder.rgb, pixels * 3u);

    if (result.bytes != pixels * 3u || result.error)
    {
        fprintf(stderr,
                "[lom-native] FIELD movie %04X stopped after %u/%u frames: "
                "RGB24 bytes=%zu/%zu eof=%d error=%d.\n",
                decoder.resource, decoder.frame_count, decoder.total_frames, result.bytes, pixels * 3u, result.eof, result.error);
        fail_movie("in-scene movie decoder read failed");
    }

    for (size_t i = 0; i < pixels; ++i)
    {
        unsigned red = decoder.rgb[3 * i] >> 3;
        unsigned green = decoder.rgb[3 * i + 1] >> 3;
        unsigned blue = decoder.rgb[3 * i + 2] >> 3;
        decoder.pixels[i] = (u16)(red | (green << 5) | (blue << 10));
    }

    if (state->chunk_idx > 1)
    {
        fail_movie("in-scene movie buffer index invalid");
    }

    RECT rect = state->rects[state->chunk_idx];

    if (!valid_rect(&rect) || rect.w != (s16)decoder.source.width || rect.h != (s16)decoder.source.height)
    {
        fail_movie("in-scene movie rectangle changed during playback");
    }

    LoadImage(&rect, (u_long*)decoder.pixels);
    DrawSync(0);
    ++decoder.frame_count;
    state->current_frame = decoder.frame_count;
    state->chunk_idx = 1 - state->chunk_idx;
    state->rects[MDEC_OUTPUT_RECT_INDEX] = state->rects[state->chunk_idx];
    state->frame_ready = 1;

    if (decoder.frame_count == decoder.total_frames)
    {
        unsigned char extra;
        result = lom_movie_decoder_read_frame(decoder.pipe, &extra, 1);
        LomMovieCloseResult closed = lom_movie_decoder_close(decoder.pipe, decoder.child, 0);
        decoder.pipe = NULL;

        if (result.bytes || result.error || !result.eof || !lom_movie_decoder_succeeded(closed))
        {
            fprintf(stderr,
                    "[lom-native] FIELD movie %04X decoder termination failed: "
                    "wait status=0x%X wait error=%d close error=%d.\n",
                    decoder.resource, closed.wait_status, closed.wait_error, closed.stream_error);
            fail_movie("in-scene movie decoder termination failed");
        }

        lom_native_field_movie_shutdown();
        state->end_of_stream = 1;
        state->end_state = 2;
    }
}

void movie_service_video_ops(void)
{
}

/**
 * @brief Psy-Q's DecDCTvlcBuild. ffmpeg decodes the movies, so the table isn't
 *        needed.
 * @param table Ignored.
 */
void DecDCTvlcBuild(u_short* table)
{
    (void)table;
}

void lom_movie_prepare_decoder(void* vlc_table)
{
    DecDCTReset(0);
    DecDCTvlcBuild((unsigned short*)vlc_table);
}
