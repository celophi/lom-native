#include "lom_native_akao.h"
#include "lom_native_audio_categories.h"
#include "lom_native_sound.h"
#include "lom_native_movie_audio.h"
#include "lom_native_resident.h"
#include "lom_native_spu_shadow.h"

#include "common.h"
#include "main/audio/akao.h"
#include "main/audio/internal/akao_driver.h"
#include "main/audio/internal/akao_voice.h"
#include "main/audio/internal/akao_control.h"
#include <libspu.h>

#include "lom_native_psyz_sdk.h"
#include <psyz/audio.h>
#include <psyz/spu.h>

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AKAO_ROOT_COUNTER 0xF2000002
#define AKAO_ROOT_COUNTER_TARGET 0x44E8
#define AKAO_ROOT_COUNTER_MODE 0x1000

static atomic_flag s_driver_lock = ATOMIC_FLAG_INIT;
static _Thread_local unsigned s_data_lock_depth;
static _Thread_local unsigned s_event_lock_depth;
static atomic_uint s_tick_count;
static int s_initialized;
static int s_audio_started;

/**
 * @brief Find PSY-Z's SPU registers.
 * @return The first register.
 */
static volatile uint16_t* spu_registers(void)
{
    return (volatile uint16_t*)lom_native_spu_registers;
}

/**
 * @brief Tell the SPU which voices the sound effects have, then write the
 *        register changes the driver made since the shadow began.
 */
static void commit_registers(void)
{
    lom_native_spu_set_sfx_mask((unsigned)g_akao_sfx_control.active_mask | (unsigned)g_akao_sfx_control.paused_mask | (unsigned)g_akao_xa_tracker.voice_mask);
    lom_native_spu_shadow_commit(spu_registers());
}

/**
 * @brief Take the lock between the driver's tick and the game's thread,
 *        spinning until it's free.
 */
static void driver_lock(void)
{
    while (atomic_flag_test_and_set_explicit(&s_driver_lock, memory_order_acquire))
    {
    }
}

/**
 * @brief Give back the lock between the driver's tick and the game's thread.
 */
static void driver_unlock(void)
{
    atomic_flag_clear_explicit(&s_driver_lock, memory_order_release);
}

void lom_native_akao_data_lock(void)
{
    if (s_data_lock_depth++ == 0)
    {
        Psyz_AudioLock();
        driver_lock();
    }
}

void lom_native_akao_data_unlock(void)
{
    if (--s_data_lock_depth == 0)
    {
        driver_unlock();
        Psyz_AudioUnlock();
    }
}

long lom_native_akao_disable_event(long event)
{
    (void)event;
    lom_native_akao_data_lock();

    if (s_event_lock_depth++ == 0)
    {
        lom_native_spu_shadow_begin(spu_registers());
    }

    return 1;
}

long lom_native_akao_enable_event(long event)
{
    (void)event;

    if (--s_event_lock_depth == 0)
    {
        commit_registers();
    }

    lom_native_akao_data_unlock();
    return 1;
}

/**
 * @brief The root counter's interrupt: run one tick of the AKAO driver, with
 *        its register writes made together at the end.
 */
static void akao_tick(void)
{
    driver_lock();
    lom_native_spu_shadow_begin(spu_registers());
    akao_irq_handler();
    commit_registers();
    atomic_fetch_add_explicit(&s_tick_count, 1, memory_order_relaxed);
    driver_unlock();
}

int lom_native_akao_initialize(const char* disc_root)
{
    if (s_initialized)
    {
        return 0;
    }

    if (disc_root == NULL || lom_native_load_resident(disc_root) != 0)
    {
        return -1;
    }

    Psyz_SpuInit();

    SpuInit();
    SpuStart();
    SpuInitMalloc(4, g_akao_spu_malloc_table);
    SpuSetTransferMode(0);
    SpuSetTransferStartAddr(0x1010);
    akao_spu_write(&g_akao_spu_zero_primer, 0x40);
    akao_spu_wait();

    lom_native_spu_shadow_begin(spu_registers());
    akao_driver_init_state();
    commit_registers();

    SpuSetIRQ(0);
    SpuSetIRQCallback(0);

    if (SetRCnt(AKAO_ROOT_COUNTER, AKAO_ROOT_COUNTER_TARGET, AKAO_ROOT_COUNTER_MODE) == 0)
    {
        return -1;
    }

    InterruptCallback(0, akao_tick);

    if (StartRCnt(AKAO_ROOT_COUNTER) == 0)
    {
        return -1;
    }

    atomic_store_explicit(&s_tick_count, 0, memory_order_relaxed);

    if (Psyz_AudioInit() != 0)
    {
        StopRCnt(AKAO_ROOT_COUNTER);
        InterruptCallback(0, NULL);
        SpuQuit();
        fprintf(stderr, "[lom-native] Failed to initialize PSY-Z host audio output.\n");
        return -1;
    }

    s_audio_started = 1;
    s_initialized = 1;
    lom_native_sound_started();
    return 0;
}

void lom_native_akao_shutdown(void)
{
    if (!s_initialized)
    {
        return;
    }

    lom_native_akao_data_lock();
    StopRCnt(AKAO_ROOT_COUNTER);
    InterruptCallback(0, NULL);
    lom_native_spu_shadow_begin(spu_registers());
    spu_set_key_off(0xFFFFFFu);
    commit_registers();
    SpuQuit();
    lom_native_akao_data_unlock();

    if (s_audio_started)
    {
        Psyz_AudioDestroy();
        s_audio_started = 0;
    }

    s_initialized = 0;
}

unsigned lom_native_akao_tick_count(void)
{
    return atomic_load_explicit(&s_tick_count, memory_order_relaxed);
}

s32 lom_native_akao_start_song_channels(void* sequence, s32 channel_mask, s32 unused)
{
    (void)unused;
    g_akao_cmd_params[0].buffer = sequence;
    g_akao_cmd_params[1].value = channel_mask;
    g_akao_cmd_params[2].value = 0;
    return akao_send_command(AKAO_CMD_START_SONG_CHANNELS);
}

LomMovieAudioGains lom_native_akao_movie_audio_gains(void)
{
    LomMovieAudioGains gains;
    lom_native_akao_data_lock();
    gains = lom_movie_audio_gains(g_akao_driver_flags.output_mode & 2, g_akao_pan_gain_table[0x80], 0x7F00);
    lom_native_akao_data_unlock();
    return gains;
}

void lom_native_akao_runaway(const AkaoChannelState* channel, const void* cursor, const char* walk)
{
    static unsigned reported;
    const u8* bytes = cursor;

    if (reported++ >= 16)
    {
        return;
    }

    fprintf(stderr,
            "[lom-native] AKAO %s found no note: channel %p cursor %p depth %u "
            "loops %p/%p/%p/%p counts %u/%u/%u/%u bytes %02X %02X %02X %02X (tick %u)\n",
            walk, (const void*)channel, cursor, (unsigned)channel->loop_depth, (void*)channel->loop_cursor[0], (void*)channel->loop_cursor[1],
            (void*)channel->loop_cursor[2], (void*)channel->loop_cursor[3], channel->loop_count[0], channel->loop_count[1], channel->loop_count[2],
            channel->loop_count[3], bytes ? bytes[0] : 0, bytes ? bytes[1] : 0, bytes ? bytes[2] : 0, bytes ? bytes[3] : 0, lom_native_akao_tick_count());
}

/**
 * @brief The game's akao_submit_bank, holding the AKAO data lock.
 * @param bank The sound bank.
 * @param wait Nonzero to wait for it to reach the SPU.
 * @return What the driver returns.
 */
s32 akao_submit_bank(AkaoBankHeader* bank, s32 wait)
{
    lom_native_akao_data_lock();
    s32 result = lom_native_akao_submit_bank_prepared(bank, wait);
    lom_native_akao_data_unlock();
    return result;
}

/**
 * @brief The game's akao_upload_bank, holding the AKAO data lock.
 * @param bank The sound bank.
 * @param wait Nonzero to wait for it to reach the SPU.
 * @param slot Which slot it goes in.
 * @param spu_base Where in SPU RAM it goes.
 * @return What the driver returns.
 */
s32 akao_upload_bank(void* bank, s32 wait, s32 slot, s32 spu_base)
{
    lom_native_akao_data_lock();
    s32 result = lom_native_akao_upload_bank_prepared(bank, wait, slot, spu_base);
    lom_native_akao_data_unlock();
    return result;
}

_Noreturn void lom_native_akao_stream_fail(const char* reason)
{
    fprintf(stderr, "[lom-native] Invalid AKAO bank stream: %s\n", reason);
    abort();
}

static long s_reverb_mode_type;

unsigned long lom_native_akao_spu_write_complete(unsigned char* source, unsigned long size)
{
    return SpuWrite(source, size);
}

unsigned long lom_native_akao_spu_read_complete(unsigned char* destination, unsigned long size)
{
    return SpuRead(destination, size);
}

/**
 * @brief Psy-Q's SpuSetIRQ. The port has no SPU interrupt, so it does nothing.
 * @param on_off Ignored.
 * @return 0.
 */
long SpuSetIRQ(long on_off)
{
    (void)on_off;
    return 0;
}

/**
 * @brief Psy-Q's SpuSetIRQAddr: set the SPU's interrupt address register.
 * @param addr The address in SPU RAM.
 * @return @p addr.
 */
u_long SpuSetIRQAddr(u_long addr)
{
    Psyz_SpuWrite(0x1A4u, (uint16_t)(addr >> 3));
    return addr;
}

/**
 * @brief Psy-Q's SpuSetTransferCallback.
 * @param callback Run when a transfer finishes, or NULL.
 * @return The one before.
 */
SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc callback)
{
    SpuTransferCallbackProc previous = (SpuTransferCallbackProc)_spu_transferCallback;
    _spu_transferCallback = callback;
    return previous;
}

/**
 * @brief Psy-Q's SpuGetReverbModeType.
 * @param mode Set to the reverb type last set.
 */
void SpuGetReverbModeType(long* mode)
{
    if (mode != NULL)
    {
        *mode = s_reverb_mode_type;
    }
}

/**
 * @brief Psy-Q's SpuSetReverbModeType. It only remembers the type, for
 *        SpuGetReverbModeType.
 * @param mode The reverb type.
 */
void SpuSetReverbModeType(long mode)
{
    s_reverb_mode_type = mode;
}

void lom_native_akao_get_voice_envelope(int voice, short* envelope)
{
    *envelope = spu_registers()[(voice * 16 + 0xC) / 2];
}

void lom_native_akao_set_reverb_depth(short left, short right)
{
    spu_registers()[0x184 / 2] = left;
    spu_registers()[0x186 / 2] = right;
}

long lom_native_akao_set_noise_clock(long clock)
{
    if (clock < 0)
    {
        clock = 0;
    }

    if (clock > 0x3F)
    {
        clock = 0x3F;
    }

    spu_registers()[0x1AA / 2] = (spu_registers()[0x1AA / 2] & 0xC0FF) | (clock << 8);
    return clock;
}
