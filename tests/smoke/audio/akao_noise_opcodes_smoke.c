#include "main/audio/internal/akao_driver.h"
#include "lom_native_akao.h"
#include "lom_native_ps1_memory.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../support/lom_check.h"

extern unsigned short Psyz_SpuRead(unsigned int reg_offset);

extern long StopRCnt(long counter);

extern void akao_seq_step_opcode(AkaoChannelState* channel, s32 mask);

extern void akao_flush_voice_updates(s32 unused);

extern void (*g_akao_opcode_handlers[])(AkaoChannelState*, s32);
extern void akao_seq_op_enable_noise(AkaoChannelState* channel, s32 mask);
extern void akao_seq_op_disable_noise(AkaoChannelState* channel, s32 mask);
#define HANDLER_IS(opcode, function, ps1_address) (g_akao_opcode_handlers[(opcode) - 0xA0] == (function))
#define TEST_STREAM ((u8*)MEMORY_MAP_RAM(0x80060000))

#define BIT(n) (1u << (n))

/**
 * @brief Read a 24-voice mask from a pair of SPU registers.
 * @param offset The low register's offset.
 * @return The mask.
 */
static u32 spu_mask(unsigned offset)
{
    return Psyz_SpuRead(offset) | ((u32)Psyz_SpuRead(offset + 2) << 16);
}

/**
 * @brief Clear the driver's songs, channels and effects, and point the channel
 *        tables at the song or the suspended song.
 * @param secondary Nonzero to use the suspended song.
 */
static void reset_fixture(int secondary)
{
    memset(&g_akao_seq_master_state, 0, sizeof(g_akao_seq_master_state));
    memset(&g_akao_suspended_song, 0, sizeof(g_akao_suspended_song));
    memset(g_akao_seq_channels, 0, sizeof(*g_akao_seq_channels) * AKAO_CHANNEL_COUNT);
    memset(g_akao_suspended_channels, 0, sizeof(*g_akao_suspended_channels) * AKAO_CHANNEL_COUNT);
    memset(g_sfx_channels, 0, sizeof(*g_sfx_channels) * AKAO_SFX_CHANNEL_COUNT);
    memset(&g_akao_sfx_control, 0, sizeof(g_akao_sfx_control));
    memset(&g_akao_xa_tracker, 0, sizeof(g_akao_xa_tracker));
    g_akao_driver_mode_flags = 0;
    g_akao_driver_flags.update_flags = 0;
    g_akao_seq_channel0 = &g_akao_seq_master_state;
    g_akao_seq_channel1 = secondary ? &g_akao_suspended_song : NULL;
    g_akao_pending_channels = g_akao_suspended_channels;
    g_akao_seq_master_state.masks.active_mask = BIT(1) | BIT(7);
    g_akao_seq_master_state.noise_mask = BIT(7);
    g_akao_seq_master_state.reverb_mask = BIT(7);
    g_akao_seq_master_state.noise_freq = 24;
    g_akao_seq_channels[1].voice = 3;
    g_akao_seq_channels[7].voice = 19;
    g_akao_suspended_song.masks.active_mask = BIT(1) | BIT(7);
    g_akao_suspended_song.noise_mask = BIT(7);
    g_akao_suspended_song.reverb_mask = BIT(7);
    g_akao_suspended_channels[1].voice = 5;
    g_akao_suspended_channels[7].voice = 17;
    g_akao_sfx_control.active_mask = BIT(12) | BIT(13) | BIT(20);
    g_akao_sfx_control.noise_mask = BIT(12);
    g_akao_sfx_control.reverb_mask = BIT(13);
    g_akao_sfx_control.noise_freq = 33;
    g_sfx_channels[8].is_sfx_channel = 1;
    g_sfx_channels[8].voice = 20;
}

/**
 * @brief Run one opcode on a channel, as the sequencer would.
 * @param channel The channel.
 * @param mask The channel's bit.
 * @param opcode The opcode.
 * @param secondary Nonzero for the suspended song.
 */
static void dispatch(AkaoChannelState* channel, u32 mask, u8 opcode, int secondary)
{
    u8* stream = TEST_STREAM;
    stream[0] = opcode;
    stream[1] = stream[2] = 0x8F;
    channel->seq_cursor = stream;

    if (secondary)
    {
        g_akao_seq_channel0 = &g_akao_suspended_song;
    }

    akao_seq_step_opcode(channel, (s32)mask);
    g_akao_seq_channel0 = &g_akao_seq_master_state;
    CHECK(channel->seq_cursor == stream + 2);
}

/**
 * @brief Check the noise and reverb opcodes set and clear the right bits, and
 *        the SPU's registers follow, for a song channel, a suspended song's or
 *        a sound effect's.
 * @param kind 0 for the song, 1 for the suspended song, 2 for a sound effect.
 */
static void exercise(int kind)
{
    int secondary = kind == 1;
    u32 bit = kind == 2 ? BIT(20) : BIT(1);
    u32 noise_base = BIT(19) | BIT(12) | (secondary ? BIT(17) : 0);
    u32 reverb_base = BIT(19) | BIT(13) | (secondary ? BIT(17) : 0);
    u32 voice = BIT(kind == 2 ? 20 : secondary ? 5 : 3);
    u32 *noise, *reverb;
    AkaoChannelState* channel;

    lom_native_akao_disable_event(0);
    reset_fixture(secondary);
    channel = kind == 2 ? &g_sfx_channels[8] : secondary ? &g_akao_suspended_channels[1] : &g_akao_seq_channels[1];
    noise = kind == 2 ? &g_akao_sfx_control.noise_mask : secondary ? &g_akao_suspended_song.noise_mask : &g_akao_seq_master_state.noise_mask;
    reverb = kind == 2 ? &g_akao_sfx_control.reverb_mask : secondary ? &g_akao_suspended_song.reverb_mask : &g_akao_seq_master_state.reverb_mask;
    u32 initial_noise = *noise, initial_reverb = *reverb;
    channel->noise_toggle_ticks = 9;
    dispatch(channel, bit, 0xC4, secondary);
    CHECK(*noise == (initial_noise | bit));
    CHECK(*reverb == initial_reverb && channel->noise_toggle_ticks == 9);
    CHECK((g_akao_driver_flags.update_flags & 0x110) == 0x110);
    akao_flush_voice_updates(0);
    lom_native_akao_enable_event(0);
    CHECK(spu_mask(0x194) == (noise_base | voice));
    CHECK(spu_mask(0x198) == reverb_base);
    CHECK(((Psyz_SpuRead(0x1AA) >> 8) & 63) == 33);

    lom_native_akao_disable_event(0);
    dispatch(channel, bit, 0xC5, secondary);
    CHECK(*noise == initial_noise && *reverb == initial_reverb);
    CHECK(channel->noise_toggle_ticks == 0);
    CHECK((g_akao_driver_flags.update_flags & 0x110) == 0x110);
    akao_flush_voice_updates(0);
    lom_native_akao_enable_event(0);
    CHECK(spu_mask(0x194) == noise_base && spu_mask(0x198) == reverb_base);

    lom_native_akao_disable_event(0);
    dispatch(channel, bit, 0xC2, secondary);
    CHECK(*reverb == (initial_reverb | bit) && *noise == initial_noise);
    akao_flush_voice_updates(0);
    lom_native_akao_enable_event(0);
    CHECK(spu_mask(0x198) == (reverb_base | voice) && spu_mask(0x194) == noise_base);
    lom_native_akao_disable_event(0);
    dispatch(channel, bit, 0xC3, secondary);
    CHECK(*reverb == initial_reverb && *noise == initial_noise);
    akao_flush_voice_updates(0);
    lom_native_akao_enable_event(0);
    CHECK(spu_mask(0x198) == reverb_base && spu_mask(0x194) == noise_base);
}

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    CHECK(lom_native_map_ps1_ram() == 0);
    CHECK(lom_native_akao_initialize(argv[1]) == 0);
    StopRCnt(0xF2000002ul);
    CHECK(HANDLER_IS(0xC4, akao_seq_op_enable_noise, 0x8002C418u));
    CHECK(HANDLER_IS(0xC5, akao_seq_op_disable_noise, 0x8002C478u));

    for (int kind = 0; kind < 3; ++kind)
    {
        exercise(kind);
    }

    lom_native_akao_shutdown();
    puts("AKAO retail-table noise/reverb dispatch and SPU publication passed (primary, secondary, SFX).");
    return 0;
}
