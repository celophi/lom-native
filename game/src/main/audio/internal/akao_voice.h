#ifndef _AKAO_VOICE_H
#define _AKAO_VOICE_H

#include "akao_driver.h"

#define AKAO_CH_PITCH_LFO 0x01
#define AKAO_CH_VOLUME_LFO 0x02
#define AKAO_CH_PAN_LFO 0x04
#define AKAO_CH_PITCH_SIDECHAIN 0x10
#define AKAO_CH_PITCH_VOLUME_SIDECHAIN 0x20

#define AKAO_CHANNEL_VOLUME_MASK (AKAO_VOLUME_MAX << AKAO_Q8_SHIFT)
#define AKAO_Q8_LEVEL_MASK 0xFF00
#define AKAO_MASTER_LEVEL_MASK (AKAO_MASTER_VOLUME_MASK << AKAO_Q16_SHIFT)

#define AKAO_PITCH_LFO_DEPTH_MASK 0x7F00
#define AKAO_PITCH_LFO_ABSOLUTE_FLAG 0x8000
#define AKAO_PITCH_LFO_RELATIVE_SCALE 15

#define AKAO_REVERB_DEPTH_FRACTION_BITS 12

#define SPU_UPDATE_VOLUME 0x3

#define SPU_UPDATE_PITCH 0x10

#define SPU_UPDATE_START_ADDR 0x80
#define SPU_UPDATE_REPEAT_ADDR 0x10000

#define SPU_UPDATE_ADSR_HIGH 0x6600
#define SPU_UPDATE_ADSR_LOW 0x9900

#define SPU_CD_VOLUME_LEFT (*(s16*)SPU_REGISTER(0x1F801DB0))
#define SPU_CD_VOLUME_RIGHT (*(s16*)SPU_REGISTER(0x1F801DB2))

#define SPU_ADSR_DECAY_SUSTAIN_MASK 0x00FF
#define SPU_ADSR_ATTACK_RATE_SHIFT 8
#define SPU_ADSR_DECAY_RATE_SHIFT 4
#define SPU_ADSR_DECAY_RATE_MASK 0x00F0
#define SPU_ADSR_SUSTAIN_LEVEL_MASK 0x000F
#define SPU_ADSR_SUSTAIN_RATE_SHIFT 6

typedef struct
{
    s16 left;
    s16 right;
} SpuVoiceVolume;

typedef struct
{
    s32 voice;
    s32 update_flags;
    u32 sample_start_addr;
    u32 sample_repeat_addr;
    u16 pitch;
    u16 adsr_low;
    u16 adsr_high;
    u16 volume_scale;
    s16 volume_left;
    s16 volume_right;
} SpuVoiceParams;

extern AkaoSongState* g_akao_voice_owners[];

void spu_set_key_on(u32 voice_mask);
void spu_set_key_off(u32 voice_mask);
void spu_set_reverb_enable(u32 voice_mask);
void spu_set_noise_enable(u32 voice_mask);
void spu_set_pitch_modulation_enable(u32 voice_mask);

void spu_set_voice_volume(s32 voice, u32 vol_l, u32 vol_r, s32 scale);
void spu_set_voice_pitch(s32 voice, s32 pitch);
void spu_set_voice_start_addr(s32 voice, u32 addr);
void spu_set_voice_repeat_addr(s32 voice, u32 addr);
void spu_set_voice_adsr_low(s32 voice, u16 adsr_low);
void spu_set_voice_adsr_high(s32 voice, u16 adsr_high);
void spu_set_voice_attack(s32 voice, s32 attack_shift, u32 mode_bits);
void spu_set_voice_decay_shift(s32 voice, s32 decay_shift);
void spu_set_voice_sustain_level(s32 voice, s32 sustain_level);
void spu_set_voice_sustain_mode(s32 voice, s32 sustain_bits, u32 mode_bits);
void spu_set_voice_release_mode(s32 voice, s32 release_shift, u32 mode_bit);
void spu_write_voice_params(s32 voice, SpuVoiceParams* params, s32 scale);
void spu_apply_voice_updates(s32 voice, SpuVoiceParams* params, s32 flags);
void akao_tick_channel_effects(AkaoChannelState* channel, s32 channel_bit, s32 is_sfx);
void akao_flush_voice_updates(s32 sfx_update_mask);
void akao_flush_voice_key_offs(void);

#endif
