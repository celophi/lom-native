#ifndef _AKAO_DRIVER_H
#define _AKAO_DRIVER_H

#include "common.h"

#include "main/audio/akao.h"

#define AKAO_MAGIC 0x4F414B41

#define AKAO_Q7_SHIFT 7
#define AKAO_Q8_SHIFT 8
#define AKAO_Q16_SHIFT 16

#define AKAO_TICK_PERIOD 0x44E8

#define AKAO_SPU_PRIMER_ADDR 0x1010

#define AKAO_BANK_SLOT_COUNT 6

#define AKAO_BANK_FIRST_ARTICULATION 0x80
#define AKAO_BANK_SLOT_ARTICULATIONS 16
#define AKAO_BANK_SLOT_ARTICULATION_INDEX(slot) (AKAO_BANK_FIRST_ARTICULATION + (slot) * AKAO_BANK_SLOT_ARTICULATIONS)

#define AKAO_BANK_FIRST_UPPER_SLOT 3

#define AKAO_UPLOAD_STREAMING 0x1

#define AKAO_OUTPUT_STEREO 1
#define AKAO_OUTPUT_MONO 2

#define AKAO_NOISE_CLOCK_UPDATE_PENDING 0x10
#define AKAO_REVERB_DEPTH_UPDATE_PENDING 0x80
#define AKAO_EFFECT_MASKS_UPDATE_PENDING 0x100

#define AKAO_MODE_SONG_PAUSED 0x1
#define AKAO_MODE_SFX_PAUSED 0x2
#define AKAO_MODE_FORCE_TICK 0x4

#define AKAO_XA_RING_BLOCK_SHIFT 12
#define AKAO_XA_RING_BLOCK_BYTES (1 << AKAO_XA_RING_BLOCK_SHIFT)

typedef struct
{
    u32 upload_flags;
    u32 output_mode;
    u32 update_flags;
} AkaoDriverFlags;

typedef struct
{
    u32 active_mask;
    s32 key_on_mask;
    u32 note_on_mask;
    u32 key_off_mask;
    u32 paused_mask;
    u32 tempo;
    u32 tempo_acc;
    u32 noise_mask;
    u32 reverb_mask;
    u32 pitch_mod_mask;
    u16 noise_freq;
} SfxControl;

#define XA_FLAG_STEREO 0x1
#define XA_FLAG_LOOP 0x2
#define XA_FLAG_RING_STREAM 0x1000000

typedef struct
{
    u32 magic;
    u32 key;
    u8 _pad08[8];
    u32 sample_size;
    u32 loop_offset;
    u32 flags;
    u16 pitch;
    u8 _pad1E[2];
    u32 spu_addr;
    u32 right_offset;
    s32 fade_in_ticks;
    u8 _pad2C[0x14];
} AkaoXaProgramHeader;

typedef struct
{
    AkaoXaProgramHeader header;
    u8 sample_prefix[0x10];
} AkaoXaProgramStaging;

typedef struct
{
    u8* data_cursor;
    u8* loop_cursor;
    u32 flags;
    s32 voice_mask;
    s32 first_voice;
    u32 bytes_remaining;
    s32 last_program_spu_addr;
    u32 loop_bytes;
    s32 last_ring_block_key;
    s32 filled_blocks;
    s32 uploaded_blocks;

    union
    {
        s32 spu_addr;
        u8* ring_base;
    } source;

    u32 ring_size;
    u32 upload_block;
    u32 fill_block;
    u32 ring_block_count;
    s32 volume;
    s32 volume_step;
    s32 volume_fade_ticks;
    s32 pan;
    u8 _pad50[8];
    s32 pitch;
} AkaoXaTracker;

extern volatile s32 g_akao_spu_xfer_pending;
extern u8 g_akao_articulation_slots[];
extern AkaoChannelState g_sfx_channels[];
extern s32 g_akao_driver_mode_flags;
extern s32 g_akao_muted_channel_mask;
extern s32 g_akao_seq_pending_ticks;
extern AkaoXaTracker g_akao_xa_tracker;
extern AkaoXaProgramStaging g_akao_xa_program_staging;
extern s16 g_akao_cdvol_fade_ticks;
extern s32 g_akao_master_tempo_modifier_acc;
extern s16 g_akao_master_tempo_modifier_fade_ticks;
extern s32 g_akao_mastervol_acc;
extern s16 g_akao_mastervol_fade_ticks;
extern s32 g_akao_cdvol_tick;
extern s32 g_akao_cdvol_acc;
extern AkaoChannelState* g_akao_pending_channels;
extern AkaoSongState* g_akao_seq_channel1;
extern AkaoSongState* g_akao_seq_channel0;
extern void* g_akao_seq_channels_base;
extern AkaoSongState g_akao_suspended_song;
extern AkaoChannelState g_akao_suspended_channels[];
extern s16 g_akao_pan_gain_table[256];
extern SfxControl g_akao_sfx_control;
extern u32 g_akao_effect_voice_masks[3];
extern AkaoDriverFlags g_akao_driver_flags;
extern s32 g_akao_bank_slot_keys[6];
extern u32 g_akao_song_match_words[2];
extern AkaoChannelState g_akao_seq_channels[];
extern AkaoSongState g_akao_seq_master_state;
extern char g_akao_spu_malloc_table[];
extern char g_akao_spu_zero_primer[];
extern s32 g_akao_rcnt2_event;
extern u8* g_akao_bank_prog_base;
extern u8* g_akao_bank_region_b;
extern u8* g_akao_bank_region_c;

void akao_driver_init(void);
void akao_driver_shutdown(void);
void akao_driver_init_state(void);
void akao_set_bank_data_ptrs(u8* base);
void akao_relocate_articulations(AkaoArticulation* src, AkaoArticulation* dst, s32 spu_base, s32 count);
void akao_spu_wait(void);
extern s32 akao_check_magic(AkaoHeader* header);
extern s32 akao_submit_bank(AkaoBankHeader* bank, s32 wait_for_completion);
extern s32 akao_upload_bank(void* bank, s32 wait_for_completion, s32 bank_id, s32 spu_base);
void akao_spu_write(void* source, s32 byte_count);
void akao_spu_read(void* destination, s32 byte_count);
void akao_spu_arm_xfer(void);
extern long akao_irq_handler(void);

typedef struct
{
    u8* articulation_dst;
    u32 spu_addr;
    u32 sample_remaining;
    u32 articulation_remaining;
} AkaoStreamingState;

extern AkaoStreamingState g_akao_streaming_state;
extern AkaoBankHeader g_akao_bank_staging;

s32 lom_native_akao_submit_bank_prepared(AkaoBankHeader* bank, s32 wait_for_completion);
s32 lom_native_akao_upload_bank_prepared(void* bank, s32 wait_for_completion, s32 bank_id, s32 spu_base);
s32 lom_native_akao_get_xfer_state_prepared(void);
s32 lom_native_akao_reset_xfer_state_prepared(void);
s32 lom_native_akao_streaming_upload_prepared(u8* source, u32 available_bytes, s32 wait_for_spu);

#endif
