#ifndef _AKAO_H
#define _AKAO_H

#include "common.h"

typedef union
{
    s32 value;
    void* buffer;
} AkaoCommandParam;

extern AkaoCommandParam g_akao_cmd_params[];

#define HALF_LOW_U16(word) (((u16*)&(word))[0])
#define HALF_HIGH_U16(word) (((u16*)&(word))[1])
#define HALF_HIGH_S16(word) (((s16*)&(word))[1])

#define AKAO_VOICE_COUNT 24

#define AKAO_VOICE_MASK ((1 << AKAO_VOICE_COUNT) - 1)

#define AKAO_CHANNEL_COUNT 32

#define AKAO_SFX_CHANNEL_COUNT 12

#define AKAO_SFX_FIRST_VOICE 12

#define AKAO_SFX_FIRST_CHANNEL_BIT 0x1000

#define AKAO_SFX_ID_MASK 0x3FF

#define AKAO_SFX_TAG_MASK 0xFFFFFF

#define AKAO_SFX_FLAG_SUPPRESS 0x02000000

#define AKAO_SFX_STOP_OLDEST 0x40000000

#define AKAO_VOLUME_MAX 0x7F

#define AKAO_PAN_MASK 0xFF

#define AKAO_PAN_CENTER 0x80

#define AKAO_SFX_DEFAULT_VOLUME 0x80

#define AKAO_PITCH_BEND_MASK 0xFF

#define AKAO_MASTER_VOLUME_MASK 0xFF

#define AKAO_MASTER_TEMPO_MODIFIER_MASK 0xFF

#define AKAO_MASTER_TEMPO_VOLUME_MASK 0xFF

typedef enum AkaoCmd
{
    AKAO_CMD_IGNORE = 0x00,
    AKAO_CMD_PLAY_SONG = 0x10,
    AKAO_CMD_STOP_SONG = 0x11,
    AKAO_CMD_PLAY_SONG_WITH_TICKS = 0x12,
    AKAO_CMD_START_SONG_CHANNELS = 0x14,
    AKAO_CMD_SWITCH_SONG = 0x19,
    AKAO_CMD_PLAY_SFX = 0x20,
    AKAO_CMD_STOP_SFX = 0x21,
    AKAO_CMD_PLAY_SFX_LIST = 0x24,
    AKAO_CMD_PLAY_SOUND = 0x30,
    AKAO_CMD_SUSPEND_SONG = 0x40,
    AKAO_CMD_SELECT_STEREO = 0x80,
    AKAO_CMD_SELECT_MONO = 0x81,
    AKAO_CMD_MUTE_SONG_CHANNELS = 0x90,
    AKAO_CMD_SET_SONG_CONDITION = 0x92,
    AKAO_CMD_RESUME_ALL = 0x98,
    AKAO_CMD_PAUSE_ALL = 0x99,
    AKAO_CMD_RESUME_SONG = 0x9A,
    AKAO_CMD_PAUSE_SONG = 0x9B,
    AKAO_CMD_RESUME_SFX = 0x9C,
    AKAO_CMD_PAUSE_SFX = 0x9D,
    AKAO_CMD_RESUME_XA = 0x9E,
    AKAO_CMD_PAUSE_XA = 0x9F,
    AKAO_CMD_SET_SFX_VOLUME = 0xA0,
    AKAO_CMD_FADE_SFX_VOLUME = 0xA1,
    AKAO_CMD_SET_SFX_PAN = 0xA2,
    AKAO_CMD_FADE_SFX_PAN = 0xA3,
    AKAO_CMD_SET_SFX_PITCH_BEND = 0xA4,
    AKAO_CMD_FADE_SFX_PITCH_BEND = 0xA5,
    AKAO_CMD_SET_ALL_SFX_VOLUME = 0xA8,
    AKAO_CMD_FADE_ALL_SFX_VOLUME = 0xA9,
    AKAO_CMD_SET_ALL_SFX_PAN = 0xAA,
    AKAO_CMD_FADE_ALL_SFX_PAN = 0xAB,
    AKAO_CMD_SET_ALL_SFX_PITCH_BEND = 0xAC,
    AKAO_CMD_FADE_ALL_SFX_PITCH_BEND = 0xAD,
    AKAO_CMD_SET_SONG_VOLUME = 0xC0,
    AKAO_CMD_FADE_SONG_VOLUME = 0xC1,
    AKAO_CMD_FADE_SONG_VOLUME_FROM = 0xC2,
    AKAO_CMD_SET_CD_VOLUME = 0xC8,
    AKAO_CMD_FADE_CD_VOLUME = 0xC9,
    AKAO_CMD_FADE_CD_VOLUME_FROM = 0xCA,
    AKAO_CMD_SET_MASTER_TEMPO_MODIFIER = 0xD0,
    AKAO_CMD_FADE_MASTER_TEMPO_MODIFIER = 0xD1,
    AKAO_CMD_FADE_MASTER_TEMPO_MODIFIER_FROM = 0xD2,
    AKAO_CMD_SET_MASTER_VOLUME = 0xD4,
    AKAO_CMD_FADE_MASTER_VOLUME = 0xD5,
    AKAO_CMD_FADE_MASTER_VOLUME_FROM = 0xD6,
    AKAO_CMD_SET_MASTER_TEMPO_MODIFIER_AND_VOLUME = 0xD8,
    AKAO_CMD_FADE_MASTER_TEMPO_MODIFIER_AND_VOLUME = 0xD9,
    AKAO_CMD_FADE_MASTER_TEMPO_MODIFIER_AND_VOLUME_FROM = 0xDA,
    AKAO_CMD_PLAY_XA_BUFFER = 0xE0,
    AKAO_CMD_STOP_XA = 0xE2,
    AKAO_CMD_SET_XA_VOLUME = 0xE4,
    AKAO_CMD_FADE_XA_VOLUME = 0xE5,
    AKAO_CMD_SET_XA_PAN = 0xE6,
    AKAO_CMD_PREPARE_XA_RING = 0xE8,
    AKAO_CMD_PLAY_XA_ONE_SHOT = 0xEC,
    AKAO_CMD_PLAY_STAGED_XA = 0xED,
    AKAO_CMD_STOP_ALL_SONGS = 0xF0,
    AKAO_CMD_RELEASE_ALL_SFX = 0xF1
} AkaoCmd;

typedef struct AkaoTimeStamp
{
    u8 year_bcd;
    u8 month_bcd;
    u8 day_bcd;
    u8 hours_bcd;
    u8 minutes_bcd;
    u8 seconds_bcd;
} AkaoTimeStamp;

typedef struct AkaoHeader
{
    u32 magic;
    u16 id;
    u16 length;
    u16 reverb_type;
    AkaoTimeStamp timestamp;
} AkaoHeader;

typedef struct AkaoBankHeader
{
    AkaoHeader header;
    u32 spu_dest_addr;
    u32 sample_size;
    u32 bank_id;
    u32 articulation_count;
    u32 unused_20;
    u8 reserved[0x1C];
} AkaoBankHeader;

typedef struct
{
    u32 section_count;
    u32 section_offsets[2];
} AkaoContainerHeader;

#define AKAO_CONTAINER_DATA_AT(container, offset) ((u8*)(container) + (offset))

#define AKAO_CONTAINER_RESIDENT_SECTION 0

#define AKAO_CONTAINER_BANK_SECTION 1

#define LOAD_BUFFER_AKAO_CONTAINER ((AkaoContainerHeader*)LOAD_BUFFER_ADDRESS)

s32 akao_register_bank(AkaoHeader* bank);
s32 akao_play_song(AkaoHeader* sequence);
s32 akao_set_song_volume(s32 song_handle, s32 volume);
void akao_upload_bank_blocking(AkaoBankHeader* bank, s32 wait_for_completion);

typedef struct AkaoArticulation
{
    u32 sample_addr;
    u32 loop_addr;

    union
    {
        u32 word;

        struct
        {
            s16 lo;
            s16 hi;
        } half;
    } adsr;

    union
    {
        u32 word;

        struct
        {
            u16 lo;
            u16 hi;
        } half;
    } pitch_misc;
} AkaoArticulation;

#define AKAO_SONG_VOICE_DROPPED 0x1
#define AKAO_SONG_VOICE_STOLEN 0x2
#define AKAO_SONG_LOWER_XA_AREA_FLAG 0x40
#define AKAO_SONG_DESCRIPTOR_MISMATCH 0x20
#define AKAO_SONG_DESCRIPTOR_FLAGS 0x60

typedef struct AkaoSongDescriptor
{
    AkaoHeader header;
    u32 unused_10;
    s32 match_value;
    u8 _pad18[8];
    s32 channel_mask;
    s32 voice_alloc_low_mask;
    s32 static_voice_mask;
    u32 unused_2c;
    s32 key_map_offset;
    s32 note_table_offset;
    u8 _pad38[8];
    u16 channel_offsets[1];
} AkaoSongDescriptor;

typedef struct AkaoSongMasks
{
    u32 active_mask;
    u32 voice_alloc_low_mask;
    u32 static_voice_mask;
    u32 key_on_mask;
} AkaoSongMasks;

#define AKAO_CH_FULL_GATE 0x40

#define AKAO_CH_SFX_PITCH_MOD 0x10000

#define AKAO_CH_DEFERRED_STOP 0x100000
#define AKAO_CH_STOP_PENDING 0x200000

typedef struct AkaoSongState
{
    u32 flags;
    AkaoSongMasks masks;
    u32 note_on_mask;
    u32 key_off_mask;
    s32 parked_mask;
    u32 tempo;
    s32 tempo_step;
    u32 tempo_acc;
    u8* song_data;
    u8* key_map_base;
    u8* note_table;
    s32 voice_alloc_base;
    u32 noise_mask;
    u32 reverb_mask;
    u32 pitch_mod_mask;
    s32 reverb_depth;
    s32 reverb_depth_step;
    s32 volume;
    s32 volume_step;
    u16 volume_fade_ticks;
    s16 reverb_depth_fade_ticks;
    u16 tempo_fade_ticks;
    u16 song_id;
    u16 condition;
    u16 noise_freq;
    u16 beats_per_measure;
    u16 beat;
    u16 ticks_per_beat;
    u16 tick;
    u16 measure;
    u8 _pad6E[2];
} AkaoSongState;

typedef struct
{
    u16 scaled;
    u16 raw;
} AkaoPitchLfoDepth;

typedef struct AkaoChannelState
{
    u8* seq_cursor;
    u8* loop_cursor[4];
    u8* return_cursor;
    u8* key_map;
    s16* pitch_lfo_cursor;
    s16* volume_lfo_cursor;
    s16* pan_lfo_cursor;
    u32 sfx_tag;
    s32 pitch;
    s32 pitch_slide_acc;
    s32 flags;
    s32 sfx_bank;
    u32 sfx_id;
    u32 sfx_pitch_bend;
    u32 sfx_pitch_bend_step;
    s32 expression;
    s32 expression_step;
    s32 pitch_slide_step;
    s32 detune_pitch_delta;
    s32 sfx_age;
    s32 expression_preset;
    s32 expression_preset_step;
    u16 is_sfx_channel;
    u16 note_ticks;
    u16 gate_ticks;
    u16 articulation;
    u16 _pad6C;

    u16 pan_bias;
    u16 pan_bias_fade_ticks;
    u16 opcode_count;
    u16 loop_count[4];
    u16 loop_opcode_count[4];
    u16 volume;
    u16 volume_fade_ticks;
    u16 sfx_pitch_bend_fade_ticks;
    u16 expression_fade_ticks;
    u16 note_expression_ticks;
    u16 volume_scale_fade_ticks;
    u16 pan;
    u16 pan_fade_ticks;
    u16 pitch_slide_ticks;
    u16 octave;
    u16 pitch_slide_duration;
    u16 prev_key;
    u16 portamento_speed;
    u16 note_flags;
    u16 _padA0;
    u16 pitch_lfo_delay;
    u16 pitch_lfo_delay_ticks;
    s16 pitch_lfo_period;
    u16 pitch_lfo_restart;
    u16 pitch_lfo_waveform;
    AkaoPitchLfoDepth pitch_lfo_depth;
    u16 pitch_lfo_depth_fade_ticks;
    u16 pitch_lfo_depth_step;
    u16 _padB4;
    u16 volume_lfo_delay;
    u16 volume_lfo_delay_ticks;
    s16 volume_lfo_period;
    u16 volume_lfo_restart;
    u16 volume_lfo_waveform;
    u16 volume_lfo_depth;
    u16 volume_lfo_depth_fade_ticks;
    u16 volume_lfo_depth_step;
    u16 _padC6;
    u16 pan_lfo_period;
    u16 pan_lfo_restart;
    u16 pan_lfo_waveform;
    u16 pan_lfo_depth;
    u16 pan_lfo_depth_fade_ticks;
    u16 pan_lfo_depth_step;
    u16 noise_toggle_ticks;
    u16 pitch_mod_toggle_ticks;
    u16 loop_depth;
    u16 pitch_scale;
    s16 note_duration;
    u16 note_duration_adjust;
    s16 volume_step;
    s16 pan_bias_step;
    u16 volume_scale;
    u16 volume_scale_step;
    s16 pan_step;
    u16 transpose;
    s16 detune;
    u16 note_key;
    u16 pitch_slide_delta;
    s16 prev_transpose;
    s16 pitch_lfo_value;
    s16 volume_lfo_value;
    s16 pan_lfo_value;
    u16 _padFA;
    u32 voice;
    s32 update_flags;
    s32 spu_sample_addr;
    s32 spu_loop_addr;
    u16 spu_pitch;
    u16 spu_adsr_low;
    u16 spu_adsr_high;
    u16 spu_volume_scale;
    s16 spu_volume_left;
    s16 spu_volume_right;
} AkaoChannelState;

#endif
