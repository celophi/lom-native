#include "main/audio/akao.h"
#include "main/audio/internal/akao_driver.h"
#include "main/audio/akao_cmd.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const int sfx_volume_opcode = 0xA8;

static const int sfx_fade_opcode = 0xA9;

static const int sfx_volume_handler_address = 0x80027D08;

static const int sfx_fade_handler_address = 0x80027D84;

static const int reverb_unity_coefficient = 0x4000;

static const int reverb_wall_coefficient = 0x6000;

static const int retained_master_volume = 37;

static const int suppressed_channel_step = 17;

static const int wrapped_command_volume = 0x182;

static const int expected_fade_step_magnitude = 3968;

static const int maximum_command_byte = 0xFF;

static const int expected_zero_tick_step = 16128;

static const int expected_wrapped_volume = 0x0200;

static const char sfx_probe_error[] = "AKAO global SFX volume/fade state failed.\n";

static const char sfx_probe_success[] = "  SFX volume: real A8/A9 commands, masks, zero ticks, suppressed/inactive channels passed.";

static const int fixture_channel_count = 2;

static const int active_channel_mask = 3;

static const int fade_ticks = 4;

static const int inactive_command_value = 9;

/**
 * @brief Exercise real A8/A9 dispatch with suppressed, inactive and zero-tick channels.
 * @return Zero on success; -1 when an observed state differs.
 */
int lom_native_akao_sfx_volume_probe(void)
{
    SfxControl saved_control = g_akao_sfx_control;
    AkaoChannelState saved_channels[fixture_channel_count];
    memcpy(saved_channels, g_sfx_channels, sizeof(saved_channels));
    memset(g_sfx_channels, 0, sizeof(saved_channels));
    g_akao_sfx_control.active_mask = AKAO_SFX_FIRST_CHANNEL_BIT * active_channel_mask;
    g_sfx_channels[0].volume_scale = reverb_unity_coefficient;
    g_sfx_channels[1].volume_scale = reverb_wall_coefficient;
    g_sfx_channels[1].sfx_tag = AKAO_SFX_FLAG_SUPPRESS;
    g_sfx_channels[1].volume_scale_fade_ticks = retained_master_volume;
    g_sfx_channels[1].volume_scale_step = suppressed_channel_step;
    akao_fade_all_sfx_volume(fade_ticks, wrapped_command_volume);
    int failed = g_sfx_channels[0].volume_scale != reverb_unity_coefficient || g_sfx_channels[0].volume_scale_fade_ticks != fade_ticks ||
                 (s16)g_sfx_channels[0].volume_scale_step != -expected_fade_step_magnitude;
    akao_fade_all_sfx_volume(0, maximum_command_byte);
    failed |= g_sfx_channels[0].volume_scale_fade_ticks != 1 || g_sfx_channels[0].volume_scale_step != expected_zero_tick_step;
    akao_set_all_sfx_volume(wrapped_command_volume);
    failed |= g_sfx_channels[0].volume_scale != expected_wrapped_volume || g_sfx_channels[0].volume_scale_fade_ticks != 0 ||
              (g_sfx_channels[0].update_flags & active_channel_mask) != active_channel_mask;
    failed |= g_sfx_channels[1].volume_scale != reverb_wall_coefficient || g_sfx_channels[1].volume_scale_fade_ticks != retained_master_volume ||
              g_sfx_channels[1].volume_scale_step != suppressed_channel_step;
    g_akao_sfx_control.active_mask = 0;
    akao_fade_all_sfx_volume(inactive_command_value, inactive_command_value);
    akao_set_all_sfx_volume(inactive_command_value);
    failed |= g_sfx_channels[0].volume_scale != expected_wrapped_volume || g_sfx_channels[0].volume_scale_fade_ticks != 0;
    memcpy(g_sfx_channels, saved_channels, sizeof(saved_channels));
    g_akao_sfx_control = saved_control;

    if (failed)
    {
        fprintf(stderr, sfx_probe_error);
    }
    else
    {
        puts(sfx_probe_success);
    }

    return failed ? -1 : 0;
}
