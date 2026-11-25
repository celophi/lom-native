#undef cdrom_queue_read_with_callback
#undef bcopy
#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "main/audio/akao_cmd.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_cd.h"
#include "lom_native_akao.h"
#include "lom_native_ps1_memory.h"
#include <stdio.h>
#include <string.h>
#include "lom_native_sound.h"
#include "lom_native_boundary.h"
#include "lom_native_overlay_entry.h"
#include "main/audio/game_audio.h"
#include "overlays/field/internal/field_calls.h"
#include "overlays/field/internal/field_music_stream.h"

enum
{
    SONG_CAPACITY = 16384
};

static _Alignas(AkaoHeader) u8 incoming_song[SONG_CAPACITY];

static u32 incoming_song_bytes;

static int song_copy_active;

static CdCommandCallback owned_callback;

static s32 owned_resource;

static unsigned owned_epoch;

static int upload_started;

static u32 expected_bank_bytes, consumed_bank_bytes;

static u32 sample_tail_padding;

#define bank_buffer ((u8*)CD_STREAM_BUFFER_AT(0x1000))

/**
 * @brief Turn a pointer into a number to compare with others.
 * @param pointer The pointer.
 * @return Its address.
 */
static uintptr_t resident_address(const void* pointer)
{
    return (uintptr_t)pointer;
}

/**
 * @brief Check whether a pointer is into the resident song buffer, the one copy
 *        of the song FIELD's music plays from.
 * @param pointer The pointer.
 * @return 1 when it is.
 */
static int resident_reference(const void* pointer)
{
    uintptr_t value = resident_address(pointer), base = resident_address(g_resident_song_buffer);
    return value >= base && value - base < SONG_CAPACITY;
}

/**
 * @brief Check whether an AKAO channel still reads from the resident song
 *        buffer.
 * @param channel The channel.
 * @return 1 when it does.
 */
static int channel_resident_reference(const AkaoChannelState* channel)
{
    if (resident_reference(channel->seq_cursor) || resident_reference(channel->key_map) || resident_reference(channel->return_cursor))
    {
        return 1;
    }

    for (unsigned i = 0; i < 4; ++i)
    {
        if (resident_reference(channel->loop_cursor[i]))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Check whether a song, or any of its active or parked channels, still
 *        reads from the resident song buffer.
 * @param song The song, or NULL.
 * @param channels Its channels, or NULL.
 * @return 1 when it does.
 */
static int song_resident_owner(const AkaoSongState* song, const AkaoChannelState* channels)
{
    if (!song || !channels)
    {
        return 0;
    }

    u32 mask = (u32)song->masks.active_mask | (u32)song->parked_mask;

    if (!mask)
    {
        return 0;
    }

    if (resident_reference(song->song_data) || resident_reference(song->key_map_base) || resident_reference(song->note_table))
    {
        return 1;
    }

    for (unsigned i = 0; i < 32; ++i)
    {
        if ((mask & (1u << i)) && channel_resident_reference(&channels[i]))
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Check that the song that came in is whole, and that nothing still
 *        plays from the resident song buffer, before it's copied there. The
 *        AKAO data lock must be held.
 * @return NULL when it can be copied, otherwise why not.
 */
static const char* publication_error(void)
{
    if (incoming_song_bytes < 64 || incoming_song_bytes > SONG_CAPACITY || g_field_stream_song_remaining ||
        (u32)g_field_stream_song_bytes != incoming_song_bytes)
    {
        return "music song assembly incomplete";
    }

    const AkaoHeader* header = (const void*)incoming_song;

    if (header->magic != 0x4F414B41u || header->length < 64 || header->length > incoming_song_bytes)
    {
        return "music song header length";
    }

    if (song_resident_owner(g_akao_seq_channel1, g_akao_pending_channels))
    {
        return "music: the second song still plays from the song buffer";
    }

    u32 sfx = (u32)g_akao_sfx_control.active_mask | (u32)g_akao_sfx_control.paused_mask;

    for (unsigned i = 0, bit = AKAO_SFX_FIRST_CHANNEL_BIT; i < AKAO_SFX_CHANNEL_COUNT; ++i, bit <<= 1)
    {
        if ((sfx & bit) && channel_resident_reference(&g_sfx_channels[i]))
        {
            return "music: a sound effect still plays from the song buffer";
        }
    }

    int dedup = g_akao_seq_channel0->song_id == header->id && (!g_akao_seq_channel1 || g_akao_seq_channel1->song_id == header->id);

    if (dedup && song_resident_owner(g_akao_seq_channel0, g_akao_seq_channels) &&
        (resident_address(g_akao_seq_channel0->song_data) != resident_address(g_resident_song_buffer) ||
         memcmp(g_resident_song_buffer, incoming_song, header->length)))
    {
        return "music: a different song with the same ID is still playing";
    }

    return NULL;
}

/**
 * @brief Find a song's channels that still play from the resident song buffer.
 * @param song The song, or NULL.
 * @param channels Its channels, or NULL.
 * @return A bit for each of them.
 */
static unsigned live_resident_channels(const AkaoSongState* song, const AkaoChannelState* channels)
{
    if (!song || !channels)
    {
        return 0;
    }

    u32 mask = (u32)song->masks.active_mask | (u32)song->parked_mask, live = 0;

    for (unsigned i = 0; i < 32; ++i)
    {
        if ((mask & (1u << i)) && resident_reference(channels[i].seq_cursor))
        {
            live |= 1u << i;
        }
    }

    return live;
}

/**
 * @brief Report, up to 32 times, that something wrote over the resident song
 *        buffer while channels still played from it.
 */
static void note_resident_overwrite(void)
{
    static unsigned reported;
    lom_native_akao_data_lock();
    unsigned primary = live_resident_channels(g_akao_seq_channel0, g_akao_seq_channels);
    unsigned secondary = live_resident_channels(g_akao_seq_channel1, g_akao_pending_channels);
    int primary_id = g_akao_seq_channel0 ? g_akao_seq_channel0->song_id : -1;
    int secondary_id = g_akao_seq_channel1 ? g_akao_seq_channel1->song_id : -1;
    lom_native_akao_data_unlock();

    if ((primary || secondary) && reported++ < 32)
    {
        fprintf(stderr,
                "[lom-native] AKAO: resident song buffer overwritten while channels still execute it: "
                "primary song %d mask %08X, secondary song %d mask %08X (tick %u)\n",
                primary_id, primary, secondary_id, secondary, lom_native_akao_tick_count());
    }
}

/**
 * @brief Copy song data for the game. While the stream's update runs, a copy
 *        into the resident song buffer goes into the incoming song instead,
 *        which replaces the resident one once the sound bank is uploaded.
 * @param source What to copy.
 * @param destination Where the game copies it to.
 * @param count How many bytes.
 * @return Where it was copied to.
 */
static void* copy_song_data(const unsigned char* source, unsigned char* destination, int count)
{
    if (song_copy_active && resident_reference(destination))
    {
        uintptr_t offset = resident_address(destination) - resident_address(g_resident_song_buffer);

        if (count < 0 || (u32)count > incoming_song_bytes || offset > incoming_song_bytes - (u32)count)
        {
            lom_native_field_fail("music: a song copy runs past the incoming song buffer");
        }

        return memmove(incoming_song + offset, source, (size_t)count);
    }

    if (count > 0 && resident_reference(destination))
    {
        note_resident_overwrite();
    }

    return memmove(destination, source, (size_t)(u32)count);
}

void* lom_sound_copy_song_data(const void* source_bytes, void* destination_bytes, int count)
{
    const unsigned char* source = source_bytes;
    unsigned char* destination = destination_bytes;
    return copy_song_data(source, destination, count);
}

/**
 * @brief Run the game's music stream update, with its copies into the resident
 *        song buffer going into the incoming song.
 */
static void prepared_update(void)
{
    song_copy_active = 1;
    field_update_music_stream();
    song_copy_active = 0;
}

enum
{
    SECTOR = 1,
    FIRST = 2,
    WAIT = 3,
    LAST = 4
};

void lom_sound_reset_music_stream(void)
{
    lom_native_boundary_event(LOM_BOUNDARY_SOUND, LOM_SOUND_RESET_MUSIC_STREAM, 0, 0);

    if (owned_callback)
    {
        lom_native_cd_cancel_callback(owned_resource, owned_callback);
    }

    if (upload_started)
    {
        g_akao_driver_flags.upload_flags &= ~1u;
    }

    owned_callback = NULL;
    owned_resource = 0;
    upload_started = 0;
    expected_bank_bytes = consumed_bank_bytes = 0;
    sample_tail_padding = 0;
    incoming_song_bytes = 0;
    song_copy_active = 0;
    g_field_stream_sector_ready = 0;
    g_field_stream_resource = 0;
    g_field_stream_song_bytes = 0;
    g_field_stream_read_handle = 0;
    g_field_stream_song_remaining = 0;
    g_field_stream_bank_pending = 0;
    g_field_stream_bank_bytes = 0;
    g_field_stream_bytes_done = 0;
    g_field_stream_sector = NULL;
    g_field_stream_state = 0;
    g_field_stream_bytes_left = 0;
}

s32 lom_native_field_music_queue(s32 resource, CdCommandCallback callback)
{
    s32 result = cdrom_queue_read_with_callback(resource, callback);

    if (result < 0)
    {
        lom_sound_reset_music_stream();
        lom_native_field_fail("music callback read could not be queued");
    }

    owned_callback = callback;
    owned_resource = (u16)resource;
    owned_epoch = lom_native_cd_reset_epoch();
    return result;
}

void lom_sound_start_music_stream(s32 index)
{
    lom_native_boundary_event(LOM_BOUNDARY_SOUND, LOM_SOUND_START_MUSIC_STREAM, (unsigned)index, 0);

    if (owned_callback && owned_epoch != lom_native_cd_reset_epoch())
    {
        lom_sound_reset_music_stream();
    }

    if (g_field_stream_resource)
    {
        return;
    }

    if (index < 0 || index > 256)
    {
        lom_native_field_fail("music stream index");
    }

    lom_sound_reset_music_stream();
    field_start_music_stream(index);
}

/**
 * @brief Add bytes to the sound bank waiting to go to the SPU. FIELD fails when
 *        they don't fit in 4096 bytes.
 * @param bytes The bytes.
 * @param count How many.
 */
static void append_bank(const u8* bytes, u32 count)
{
    if (g_field_stream_bank_bytes < 0 || (u32)g_field_stream_bank_bytes > 4096 || count > 4096 - (u32)g_field_stream_bank_bytes)
    {
        lom_native_field_fail("music: the sound bank doesn't fit in 4096 bytes");
    }

    memcpy(bank_buffer + g_field_stream_bank_bytes, bytes, count);
    g_field_stream_bank_bytes += (s32)count;
}

/**
 * @brief Send the waiting sound bank to the SPU, 2048 bytes at a time, after
 *        checking its header the first time. Its samples are padded with zeros
 *        to the length the header says.
 * @param final Nonzero for the stream's last sector, which sends whatever is
 *        left.
 */
static void upload(int final)
{
    u32 available = (u32)g_field_stream_bank_bytes;

    if (!upload_started)
    {
        if (available < 64)
        {
            if (final)
            {
                lom_native_field_fail("music bank header truncated");
            }

            g_field_stream_sector_ready = 0;
            return;
        }

        const AkaoBankHeader* bank = (const void*)bank_buffer;
        uint64_t declared = 64 + bank->articulation_count * 16ull + bank->sample_size;

        if (bank->header.magic != 0x4F414B41u || declared < expected_bank_bytes || declared - expected_bank_bytes >= 64 ||
            (declared - expected_bank_bytes) % 16 ||
            (declared != expected_bank_bytes && (bank->sample_size % 64 || 64 + bank->articulation_count * 16ull > expected_bank_bytes)))
        {
            lom_native_field_fail("music bank declared length");
        }

        sample_tail_padding = (u32)(declared - expected_bank_bytes);
        expected_bank_bytes = (u32)declared;
        akao_reset_xfer_state();
        upload_started = 1;
    }

    if (final && sample_tail_padding)
    {
        const u8 zeros[64] = {0};
        append_bank(zeros, sample_tail_padding);
        available = (u32)g_field_stream_bank_bytes;
        sample_tail_padding = 0;
    }

    u32 chunk = final ? available : available >= 2048 ? 2048 : 0;

    if (!chunk)
    {
        g_field_stream_sector_ready = 0;
        return;
    }

    akao_streaming_upload_tick(bank_buffer, chunk, 1);
    consumed_bank_bytes += chunk;
    available -= chunk;
    memmove(bank_buffer, bank_buffer + chunk, available);
    g_field_stream_bank_bytes = (s32)available;
    g_field_stream_bank_pending = 0;
    g_field_stream_state = final ? LAST : WAIT;

    if (final && (consumed_bank_bytes != expected_bank_bytes || (g_akao_driver_flags.upload_flags & 1)))
    {
        lom_native_field_fail("music bank ended before upload completed");
    }
}

/**
 * @brief While the SPU takes the bank, keep the upload going; once the last of
 *        it is in, publish the new song where FIELD's music plays from, and let
 *        go of the stream when FIELD has nothing more queued.
 */
static void continue_bank_upload(void)
{
    s32 transfer = akao_get_xfer_state();

    if (transfer < 0 || transfer > 1)
    {
        lom_sound_reset_music_stream();
        lom_native_field_fail("music SPU transfer failed");
    }

    int final = g_field_stream_state == LAST;

    if (final && transfer == 0)
    {
        lom_native_akao_data_lock();
        const char* failure = publication_error();

        if (failure)
        {
            lom_native_akao_data_unlock();
            lom_sound_reset_music_stream();
            lom_native_field_fail(failure);
        }

        memcpy(g_resident_song_buffer, incoming_song, incoming_song_bytes);
        prepared_update();
        lom_native_akao_data_unlock();
    }
    else
    {
        prepared_update();
    }

    if (final && !g_field_stream_resource)
    {
        owned_callback = NULL;
        owned_resource = 0;
        upload_started = 0;
    }
}

/**
 * @brief Read a music container's first sector: check its header, copy the
 *        start of the song, and start the bank when the song ends in it.
 * @param sector The sector.
 * @param bytes How much of it is the container's.
 * @param final Whether it's the container's last sector.
 */
static void read_first_music_sector(const u8* sector, u32 bytes, int final)
{
    u32 header[3];

    if (bytes < 12)
    {
        lom_native_field_fail("music container header truncated");
    }

    memcpy(header, sector, 12);

    if (header[0] != 2 || header[1] < 12 || header[1] > bytes || header[2] < header[1] || header[2] > g_field_stream_bytes_left || (header[2] & 3) ||
        header[2] - header[1] > 16384 || g_field_stream_bytes_left - header[2] < 64)
    {
        lom_native_field_fail("music container offsets or song capacity");
    }

    expected_bank_bytes = g_field_stream_bytes_left - header[2];
    incoming_song_bytes = header[2] - header[1];
    g_field_stream_song_remaining = (s32)incoming_song_bytes;
    u32 song = bytes - header[1];

    if (song > (u32)g_field_stream_song_remaining)
    {
        song = (u32)g_field_stream_song_remaining;
    }

    memcpy(incoming_song, sector + header[1], song);
    g_field_stream_song_bytes = (s32)song;
    g_field_stream_song_remaining -= (s32)song;

    if (!g_field_stream_song_remaining)
    {
        g_field_stream_bank_pending = 1;
        append_bank(sector + header[1] + song, bytes - header[1] - song);
    }

    if (final)
    {
        upload(1);
    }
    else
    {
        g_field_stream_sector_ready = 0;
    }
}

/**
 * @brief Read one of a music container's later sectors: more of the song, or
 *        the bank once the song is all in.
 * @param sector The sector.
 * @param bytes How much of it is the container's.
 * @param final Whether it's the container's last sector.
 */
static void read_music_sector(u8* sector, u32 bytes, int final)
{
    if (!final && (g_field_stream_song_remaining >= 2048 || !g_field_stream_song_remaining))
    {
        int bank = !g_field_stream_song_remaining;

        if (bank && !upload_started)
        {
            append_bank(sector, bytes);
            upload(0);
            return;
        }

        prepared_update();

        if (bank)
        {
            consumed_bank_bytes += 2048;
        }

        return;
    }

    u32 song = (u32)g_field_stream_song_remaining;

    if (song > bytes)
    {
        song = bytes;
    }

    if (g_field_stream_song_bytes < 0 || g_field_stream_song_bytes > 16384 || song > 16384 - (u32)g_field_stream_song_bytes)
    {
        lom_native_field_fail("music song continuation capacity");
    }

    memcpy(incoming_song + g_field_stream_song_bytes, sector, song);
    g_field_stream_song_bytes += (s32)song;
    g_field_stream_song_remaining -= (s32)song;

    if (!g_field_stream_song_remaining)
    {
        g_field_stream_bank_pending = !upload_started;
        append_bank(sector + song, bytes - song);

        if (final)
        {
            upload(1);
        }
        else
        {
            g_field_stream_sector_ready = 0;
        }
    }
    else if (final)
    {
        lom_native_field_fail("music song truncated");
    }
    else
    {
        g_field_stream_sector_ready = 0;
    }
}

void lom_sound_update_music_stream(void)
{
    if (!owned_callback)
    {
        field_update_music_stream();
        return;
    }

    if (owned_epoch != lom_native_cd_reset_epoch())
    {
        lom_sound_reset_music_stream();
        return;
    }

    LomNativeCdError error = lom_native_cd_get_last_error();

    if (error != LOM_NATIVE_CD_ERROR_NONE && error != LOM_NATIVE_CD_ERROR_CALLBACK_BLOCKED)
    {
        lom_sound_reset_music_stream();
        lom_native_field_fail("music callback transport failed");
    }

    if (!g_field_stream_sector_ready)
    {
        if (!lom_native_cd_callback_pending(owned_resource, owned_callback))
        {
            lom_sound_reset_music_stream();
        }

        return;
    }

    if (g_field_stream_state == WAIT || g_field_stream_state == LAST)
    {
        continue_bank_upload();
        return;
    }

    u32 bytes = g_field_stream_bytes_left < 2048 ? g_field_stream_bytes_left : 2048;
    int final = g_field_stream_bytes_left <= 2048;
    u8* sector = g_field_stream_sector;

    if (!sector || g_field_stream_bytes_done < 0 || !bytes)
    {
        lom_native_field_fail("music callback sector state");
    }

    if (g_field_stream_state == FIRST)
    {
        read_first_music_sector(sector, bytes, final);
        return;
    }

    if (g_field_stream_state != SECTOR)
    {
        lom_native_field_fail("music stream state");
    }

    read_music_sector(sector, bytes, final);
}

void lom_native_field_music_drain(void)
{
    while (owned_callback)
    {
        s32 done = g_field_stream_bytes_done, ready = g_field_stream_sector_ready;
        u32 state = g_field_stream_state;
        cdrom_process_state();
        lom_sound_update_music_stream();

        if (owned_callback && done == g_field_stream_bytes_done && ready == g_field_stream_sector_ready && state == g_field_stream_state)
        {
            if ((state == WAIT || state == LAST) && akao_get_xfer_state() == 1)
            {
                akao_spu_wait();
                continue;
            }

            lom_native_field_fail("music callback drain made no progress");
        }
    }
}
