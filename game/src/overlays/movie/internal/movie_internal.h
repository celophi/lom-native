#ifndef LOM_MOVIE_INTERNAL_H
#define LOM_MOVIE_INTERNAL_H

#include "common.h"
#include <libcd.h>
#include <libgpu.h>
#include <libgte.h>
#include <libpress.h>

#include "common/pad.h"
#include "main/audio/akao_cmd.h"
#include "main/cdrom.h"
#include "main/controller.h"
#include "overlays/movie/movie.h"
#include "overlays/movie/movie_state.h"

typedef enum
{
    END_STATE_RUNNING,
    END_STATE_NEAR_END,
    END_STATE_DONE
} MovieEndState;

typedef enum
{
    MDEC_STATE_IDLE,
    MDEC_STATE_ACTIVE,
    MDEC_STATE_CHAINED
} MovieMdecState;

#define MOVIE_GPU_MODE_STANDARD 0

#define STANDARD_VLC_DECODE_SIZE 4096
#define ALTERNATE_VLC_DECODE_SIZE 5802
#define STANDARD_VLC_RETRY_COUNT 3
#define ALTERNATE_VLC_RETRY_COUNT 1
#define AUDIO_STREAM_STATE_IDLE 0
#define AUDIO_STREAM_STATE_SECTOR_READY 1
#define AUDIO_STREAM_STATE_PRIMED 2
#define AKAO_COMMAND_SELECTOR_9E 3
#define AKAO_XA_POSITION_UNAVAILABLE (-1)
#define AUDIO_SECTORS_PER_XA_FRAME 2

#define DRAW_SYNC_MODE_POLL 1
#define DRAW_SYNC_DEFER_THRESHOLD 2
#define BREAK_DRAW_FAILURE ((u_long*)-1)

#define SECTOR_TYPE_VIDEO 0x8001
#define SECTOR_TYPE_AUDIO 1

#define CONTINUATION_VIDEO 0
#define CONTINUATION_AUDIO 1

#define CD_HEADER_WORDS 8
#define CD_HEADER_BYTES (CD_HEADER_WORDS * sizeof(u32))
#define CD_PAYLOAD_BYTES (CD_DATA_SECTOR_SIZE - CD_HEADER_BYTES)
#define CD_PAYLOAD_WORDS (CD_PAYLOAD_BYTES / sizeof(u32))

typedef struct
{
    u16 magic;
    u16 sector_type;
    u16 chunk_sector_idx;
    u16 sector_count;
    u32 frame_number;
} SectorEntry;

typedef union VideoSectorEntry
{
    SectorEntry header;
    u32 words[CD_HEADER_WORDS];
} VideoSectorEntry;

typedef struct AudioSector
{
    VideoSectorEntry header_block;
    u8 payload[CD_PAYLOAD_BYTES];
} AudioSector;

typedef union VideoVlcPayload
{
    u8 data[CD_PAYLOAD_BYTES];
    u_long words[CD_PAYLOAD_WORDS];
} VideoVlcPayload;

void movie_mdec_out_callback(void);
u8* cd_sector_callback(s32 bytes_transferred, u32 bytes_remaining);
s32 get_next_audio_entry(AudioSector** out_entry);
void draw_sync_callback(void);
s32 get_next_video_entry(VideoVlcPayload** out_vlc_data, VideoSectorEntry** out_entry_header);
void advance_audio_read(void);
void advance_video_read(void);

#endif
