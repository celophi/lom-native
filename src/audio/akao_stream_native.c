#include "main/audio/akao_cmd.h"
#include "main/audio/internal/akao_driver.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_akao.h"

#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(AkaoBankHeader) == 64, "streamed bank header size");

_Static_assert(sizeof(AkaoArticulation) == 16, "streamed articulation size");

/**
 * @brief Give back the AKAO data lock and stop the game because a streamed
 *        sound bank is bad.
 * @param reason What's wrong with it.
 */
static _Noreturn void stream_fail(const char* reason)
{
    lom_native_akao_data_unlock();
    lom_native_akao_stream_fail(reason);
}

/**
 * @brief Check a chunk of a streamed bank is at a real address and doesn't wrap
 *        around memory.
 * @param source The chunk.
 * @param avail Its length.
 * @return @p source.
 */
static u8* checked_source(u8* source, u32 avail)
{
    uintptr_t address = (uintptr_t)source;
    u32 narrow = (u32)address;

    if (address == (uintptr_t)(intptr_t)(s32)narrow)
    {
        address = narrow;
    }

    if (!address || avail > UINTPTR_MAX - address)
    {
        stream_fail("source span");
    }

    return source;
}

/**
 * @brief Check a sound bank's samples fit in SPU RAM after the driver's area
 *        and its articulations fit in the 256 slots.
 * @param bank The bank's header.
 */
static void check_bank(const AkaoBankHeader* bank)
{
    if (bank->spu_dest_addr < 0x1010 || (bank->spu_dest_addr & 7) || bank->spu_dest_addr > 0x7FFF8 || bank->sample_size > 0x80000 - bank->spu_dest_addr)
    {
        stream_fail("SPU sample span");
    }

    if (bank->bank_id > 256 || bank->articulation_count > 256 - bank->bank_id)
    {
        stream_fail("articulation table span");
    }
}

/**
 * @brief Check one chunk of a streamed sound bank before the driver uploads it:
 *        its header on the first chunk, that the upload is where the last chunk
 *        left it, and that each part starts aligned.
 * @param source The chunk.
 * @param avail Its length.
 * @param wait_for_spu Nonzero to wait for the SPU transfer.
 * @return What the driver's upload returns.
 */
static s32 checked_stream_tick(u8* source, u32 avail, s32 wait_for_spu)
{
    if (!(g_akao_driver_flags.upload_flags & 1))
    {
        return lom_native_akao_streaming_upload_prepared(source, avail, wait_for_spu);
    }

    if (!avail && g_akao_streaming_state.spu_addr)
    {
        return lom_native_akao_streaming_upload_prepared(source, avail, wait_for_spu);
    }

    source = checked_source(source, avail);
    const AkaoBankHeader* bank = &g_akao_bank_staging;
    u32 articulation_remaining = g_akao_streaming_state.articulation_remaining;
    u32 sample_remaining = g_akao_streaming_state.sample_remaining;
    u32 sample_address = g_akao_streaming_state.spu_addr;

    if (!sample_address)
    {
        if (avail < sizeof(AkaoBankHeader) || ((uintptr_t)source & 3))
        {
            stream_fail("first tick requires aligned complete header");
        }

        bank = (const AkaoBankHeader*)source;

        if (bank->header.magic != 0x4F414B41u)
        {
            return lom_native_akao_streaming_upload_prepared(source, avail, wait_for_spu);
        }

        check_bank(bank);
        source += sizeof(*bank);
        avail -= sizeof(*bank);
        articulation_remaining = bank->articulation_count * sizeof(AkaoArticulation);
        sample_remaining = bank->sample_size;
        sample_address = bank->spu_dest_addr;
    }
    else
    {
        check_bank(bank);
        u32 articulation_size = bank->articulation_count * sizeof(AkaoArticulation);

        if (articulation_remaining > articulation_size || sample_remaining > bank->sample_size ||
            sample_address != bank->spu_dest_addr + bank->sample_size - sample_remaining ||
            (uintptr_t)g_akao_streaming_state.articulation_dst !=
                (uintptr_t)g_akao_articulation_slots + bank->bank_id * sizeof(AkaoArticulation) + articulation_size - articulation_remaining)
        {
            stream_fail("resident upload progression");
        }
    }

    u32 articulation_chunk = articulation_remaining < avail ? articulation_remaining : avail;

    if (articulation_chunk && (((uintptr_t)source & 3) || (articulation_chunk & 3)))
    {
        stream_fail("articulation fragment must contain whole aligned words");
    }

    if (avail > articulation_chunk && sample_remaining && (sample_address & 7))
    {
        stream_fail("SPU transfer start alignment");
    }

    if (!g_akao_streaming_state.spu_addr)
    {
        source -= sizeof(*bank);
        avail += sizeof(*bank);
    }

    return lom_native_akao_streaming_upload_prepared(source, avail, wait_for_spu);
}

/**
 * @brief The game's akao_streaming_upload_tick, checked, holding the AKAO data
 *        lock.
 * @param source The next chunk of the bank.
 * @param avail Its length.
 * @param wait_for_spu Nonzero to wait for the SPU transfer.
 * @return What the driver's upload returns.
 */
s32 akao_streaming_upload_tick(u8* source, u32 avail, s32 wait_for_spu)
{
    lom_native_akao_data_lock();
    s32 result = checked_stream_tick(source, avail, wait_for_spu);
    lom_native_akao_data_unlock();
    return result;
}

/**
 * @brief The game's akao_reset_xfer_state, holding the AKAO data lock.
 * @return What the driver returns.
 */
s32 akao_reset_xfer_state(void)
{
    lom_native_akao_data_lock();
    s32 result = lom_native_akao_reset_xfer_state_prepared();
    lom_native_akao_data_unlock();
    return result;
}

/**
 * @brief The game's akao_get_xfer_state, holding the AKAO data lock.
 * @return 0 when the SPU transfer is done, 1 while it runs.
 */
s32 akao_get_xfer_state(void)
{
    lom_native_akao_data_lock();
    s32 result = lom_native_akao_get_xfer_state_prepared();
    lom_native_akao_data_unlock();
    return result;
}
