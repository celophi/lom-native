#include <psyz/log.h>
#include <string.h>
unsigned spu_completion_unsupported_warnings;

/**
 * @brief Count PSY-Z's warnings about SPU settings it doesn't support, and drop
 *        its log.
 * @param level The log level.
 * @param file The source file.
 * @param line The line.
 * @param func The function.
 * @param format The message.
 */
void __wrap_psyz_log(unsigned level, const char* file, unsigned line, const char* func, const char* format, ...)
{
    (void)level;
    (void)file;
    (void)line;
    (void)func;

    if (strstr(format, "unsupported spec:"))
    {
        ++spu_completion_unsupported_warnings;
    }
}

/**
 * @brief Psy-Q's ResetCallback, which these tests don't need.
 * @return 0.
 */
int __wrap_ResetCallback(void)
{
    return 0;
}

/**
 * @brief Psy-Q's DMACallback, which these tests don't need.
 * @param channel Ignored.
 * @param callback Ignored.
 * @return NULL.
 */
void* __wrap_DMACallback(int channel, void (*callback)(void))
{
    (void)channel;
    (void)callback;
    return 0;
}

extern volatile int g_akao_spu_xfer_pending;

unsigned spu_completion_akao_callbacks, spu_completion_akao_armed;

typedef void (*TransferCallback)(void);

static TransferCallback akao_callback;

TransferCallback __real_SpuSetTransferCallback(TransferCallback callback);

/**
 * @brief Count an SPU transfer's completion, and whether AKAO was waiting for
 *        it, then run AKAO's callback.
 */
static void observe_akao_completion(void)
{
    ++spu_completion_akao_callbacks;
    spu_completion_akao_armed += g_akao_spu_xfer_pending == 1;
    TransferCallback callback = akao_callback;
    callback();
}

/**
 * @brief Put observe_akao_completion in front of the transfer callback AKAO
 *        sets.
 * @param callback AKAO's callback, or NULL.
 * @return The callback before, as AKAO set it.
 */
TransferCallback __wrap_SpuSetTransferCallback(TransferCallback callback)
{
    TransferCallback previous = akao_callback;
    akao_callback = callback;
    TransferCallback result = __real_SpuSetTransferCallback(callback ? observe_akao_completion : NULL);
    return result == observe_akao_completion ? previous : result;
}

unsigned spu_completion_reverb_audit, spu_completion_reverb_waits, spu_completion_reverb_waits_ok;

long __real_WaitEvent(long event);

/**
 * @brief Count the waits on events while the reverb is being audited, and how
 *        many succeeded.
 * @param event The event.
 * @return What the real WaitEvent returns.
 */
long __wrap_WaitEvent(long event)
{
    long result = __real_WaitEvent(event);

    if (spu_completion_reverb_audit)
    {
        ++spu_completion_reverb_waits;
        spu_completion_reverb_waits_ok += result == 1;
    }

    return result;
}
