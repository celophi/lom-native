#include "internal/akao_driver.h"
#include "main/audio/akao.h"
#include "internal/akao_voice.h"
#include <libspu.h>
#include "lom_native_akao.h"

/**
 * @brief Relocates an AKAO articulation table by adding the SPU upload base
 *        address into each entry as it is copied.
 *
 * Streams @p count @ref AkaoArticulation entries from @p src to @p dst,
 * biasing @c sample_addr and @c loop_addr by @p spu_base so the in-RAM
 * table holds absolute SPU addresses. The @c adsr and @c pitch_misc fields
 * are copied verbatim.
 *
 * @param src       Source articulation table.
 * @param dst       Destination articulation table.
 * @param spu_base  SPU base address added to @c sample_addr and @c loop_addr.
 * @param count     Number of entries to relocate (must be > 0; do-while shape).
 *
 * @see AkaoArticulation
 * @see decomp.me (100%) https://decomp.me/scratch/CJTY6
 */
void akao_relocate_articulations(AkaoArticulation* src, AkaoArticulation* dst, s32 spu_base, s32 count)
{
    do
    {
        dst->sample_addr = src->sample_addr + spu_base;
        dst->loop_addr = src->loop_addr + spu_base;
        dst->adsr.word = src->adsr.word;
        dst->pitch_misc.word = src->pitch_misc.word;
        src++;
        dst++;
    } while (--count != 0);
}

/**
 * @brief Validates the 'AKAO' magic word at the head of an audio resource.
 *
 * The Square AKAO sound driver tags every bank/sequence with the four-byte
 * little-endian magic 0x4F414B41 ("AKAO"). Returns 0 iff @p header begins with
 * that magic, by subtracting and letting the result be zero on a match.
 *
 * @param header Candidate AKAO-tagged buffer.
 *
 * @return 0 if the magic matches; otherwise (header->magic - AKAO_MAGIC).
 *
 * @see decomp.me (100%) https://decomp.me/scratch/scY8u
 */
s32 akao_check_magic(AkaoHeader* header)
{
    return header->magic - AKAO_MAGIC;
}

/**
 * @brief SPU transfer-completion callback.
 *
 * Installed via @c SpuSetTransferCallback by akao_spu_arm_xfer; the SPU
 * library invokes this when a write/read transfer finishes. Tears down the
 * callback (passes NULL back to libspu) and clears the in-flight flag so
 * akao_spu_wait can release.
 *
 * @see decomp.me (100%) https://decomp.me/scratch/qI6jZ
 */
void akao_spu_xfer_done_cb(void)
{
    SpuSetTransferCallback(NULL);
    g_akao_spu_xfer_pending = 0;
}

/**
 * @brief Arms the SPU for an asynchronous transfer.
 *
 * Sets the in-flight flag (@c g_akao_spu_xfer_pending = 1) and registers
 * akao_spu_xfer_done_cb as the libspu transfer-callback. Used as the prelude
 * to either akao_spu_write or akao_spu_read.
 *
 * @see decomp.me (100%) https://decomp.me/scratch/oy7T9
 */
void akao_spu_arm_xfer(void)
{
    g_akao_spu_xfer_pending = 1;
    SpuSetTransferCallback(&akao_spu_xfer_done_cb);
}

/**
 * @brief Begins an asynchronous SPU write (SpuWrite + done-callback hookup).
 *
 * Installs the completion callback before submitting the source buffer.
 * Pair with akao_spu_wait when synchronous completion is needed.
 *
 * @param source     Source buffer in main RAM.
 * @param byte_count Number of bytes to upload.
 *
 * @see decomp.me (100%) https://decomp.me/scratch/D2YiT
 */
void akao_spu_write(void* source, s32 byte_count)
{
    g_akao_spu_xfer_pending = 1;
    SpuSetTransferCallback(&akao_spu_xfer_done_cb);
    lom_native_akao_spu_write_complete(source, byte_count);
}

/**
 * @brief Begins an asynchronous SPU read (SpuRead + done-callback hookup).
 *
 * Installs the completion callback before submitting the destination buffer.
 * Pair with akao_spu_wait before consuming the returned data.
 *
 * @param destination Destination buffer in main RAM.
 * @param byte_count Number of bytes to read back from the SPU.
 *
 * @see decomp.me (100%) https://decomp.me/scratch/lLOqn
 */
void akao_spu_read(void* destination, s32 byte_count)
{
    akao_spu_arm_xfer();
    lom_native_akao_spu_read_complete(destination, byte_count);
}

/**
 * @brief Spin-waits for an in-flight SPU transfer to complete.
 *
 * Polls the volatile in-flight flag @c g_akao_spu_xfer_pending until akao_spu_xfer_done_cb
 * clears it. Used wherever the AKAO upload paths need to synchronize before
 * issuing the next SPU operation.
 *
 * @see decomp.me (100%) https://decomp.me/scratch/fqPPO
 */
void akao_spu_wait(void)
{
    while (g_akao_spu_xfer_pending == 1);
}

s32 lom_native_akao_submit_bank_prepared(AkaoBankHeader* bank, s32 wait_for_completion)
{
    if (akao_check_magic(&bank->header) == 0)
    {
        lom_native_akao_upload_bank_prepared(bank, wait_for_completion, bank->bank_id, bank->spu_dest_addr);
        return 0;
    }

    return -1;
}

s32 lom_native_akao_upload_bank_prepared(void* bank, s32 wait_for_completion, s32 bank_id, s32 spu_base)
{
    AkaoBankHeader* header;
    AkaoArticulation* articulations;
    s32 result;
    uintptr_t header_address;

    akao_spu_wait();
    header_address = akao_check_magic(bank);

    if (header_address == 0)
    {
        header_address = (uintptr_t)bank;
        header = (AkaoBankHeader*)header_address;
        SpuSetTransferStartAddr(spu_base);
        bank = header + 1;
        articulations = bank;
        akao_spu_write(&articulations[header->articulation_count], header->sample_size);
        akao_relocate_articulations(articulations, &((AkaoArticulation*)g_akao_articulation_slots)[bank_id], spu_base, header->articulation_count);

        if (wait_for_completion != 0)
        {
            akao_spu_wait();
        }

        result = 0;
    }
    else
    {
        g_akao_spu_xfer_pending = -1;
        result = -1;
    }

    return result;
}
