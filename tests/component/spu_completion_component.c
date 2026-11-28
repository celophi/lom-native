#include <kernel.h>
#include <libapi.h>
#include <libspu.h>
#include <psyz.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../support/lom_check.h"

extern unsigned spu_completion_unsupported_warnings;

extern unsigned spu_completion_akao_callbacks, spu_completion_akao_armed;

extern volatile int g_akao_spu_xfer_pending;

extern void akao_spu_write(void*, int);

extern void akao_spu_read(void*, int);

extern void akao_spu_wait(void);

extern unsigned spu_completion_reverb_audit, spu_completion_reverb_waits, spu_completion_reverb_waits_ok;

extern int _spu_transMode, _spu_trans_mode;

extern int _spu_EVdma;

extern int _spu_isCalled;

extern int _spu_inTransfer;

extern void (*volatile _spu_transferCallback)(void);

extern void Psyz_KernelRaise(unsigned desc, unsigned spec);

static unsigned callbacks, event_callbacks;

static unsigned char* callback_destination;

static const unsigned char* callback_expected;

static size_t callback_size;

static int callback_unregister, callback_redirect_address;

static atomic_int waiter_started, waiter_finished;

static long waiter_event;

static int waiter_result;

/**
 * @brief An SPU transfer callback that counts its calls, and unregisters itself
 *        or starts another transfer when the case asks.
 */
static void transfer_callback(void)
{
    ++callbacks;

    if (callback_unregister)
    {
        _spu_transferCallback = NULL;
    }

    if (callback_redirect_address)
    {
        SpuSetTransferStartAddr(0x3000);
    }

    if (callback_destination)
    {
        CHECK_COUNTED(!memcmp(callback_destination, callback_expected, callback_size));
    }
}

/**
 * @brief An event handler that counts its calls.
 * @return 0.
 */
static long interrupt_event_callback(void)
{
    ++event_callbacks;
    return 0;
}

/**
 * @brief Wait for an event on another thread, noting when it starts and
 *        finishes.
 * @param unused Nothing.
 * @return NULL.
 */
static void* wait_for_event(void* unused)
{
    (void)unused;
    atomic_store(&waiter_started, 1);
    waiter_result = (int)WaitEvent(waiter_event);
    atomic_store(&waiter_finished, 1);
    return NULL;
}

/**
 * @brief Check SPU completion events behave as Psy-Q's kernel's do: delivered
 *        only once enabled, and not inside a critical section.
 */
static void event_contract(void)
{
    long event = OpenEvent(HwSPU, EvSpCOMP, EvMdNOINTR, NULL);
    CHECK_COUNTED(event != -1);
    CHECK_COUNTED(TestEvent(event) == 0);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 0);
    CHECK_COUNTED(EnableEvent(event) == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    EnterCriticalSection();
    Psyz_KernelRaise(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 0);
    ExitCriticalSection();
    CHECK_COUNTED(TestEvent(event) == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    DeliverEvent(HwSPU, EvSpCOMP);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    DeliverEvent(HwSPU, EvSpCOMP);
    UnDeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 0);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(WaitEvent(event) == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    CHECK_COUNTED(DisableEvent(event) == 1);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 0);
    CHECK_COUNTED(EnableEvent(event) == 1);
    pthread_t thread;
    waiter_event = event;
    CHECK_COUNTED(pthread_create(&thread, NULL, wait_for_event, NULL) == 0);

    while (!atomic_load(&waiter_started))
    {
    }
    struct timespec delay = {0, 5000000};
    nanosleep(&delay, NULL);
    CHECK_COUNTED(!atomic_load(&waiter_finished));
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(pthread_join(thread, NULL) == 0);
    CHECK_COUNTED(waiter_result == 1);
    CHECK_COUNTED(TestEvent(event) == 0);
    CHECK_COUNTED(CloseEvent(event) == 1);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(TestEvent(event) == 0);
    long callback = OpenEvent(HwSPU, EvSpCOMP, EvMdINTR, interrupt_event_callback);
    CHECK_COUNTED(callback != -1);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(event_callbacks == 0);
    CHECK_COUNTED(EnableEvent(callback) == 1);
    DeliverEvent(HwSPU, EvSpCOMP);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(event_callbacks == 2);
    CHECK_COUNTED(TestEvent(callback) == 0);
    CHECK_COUNTED(DisableEvent(callback) == 1);
    DeliverEvent(HwSPU, EvSpCOMP);
    CHECK_COUNTED(event_callbacks == 2);
    CHECK_COUNTED(CloseEvent(callback) == 1);
    long card = OpenEvent(SwCARD, EvSpIOE, EvMdNOINTR, NULL);
    CHECK_COUNTED(card != -1);
    CHECK_COUNTED(TestEvent(card) == 0);
    CHECK_COUNTED(EnableEvent(card) == 1);
    CHECK_COUNTED(TestEvent(card) == 0);
    CHECK_COUNTED(_card_info(0) == 1);
    CHECK_COUNTED(TestEvent(card) == 1);
    CHECK_COUNTED(TestEvent(card) == 0);
    CHECK_COUNTED(CloseEvent(card) == 1);
}

/**
 * @brief Check SPU transfers write and read SPU RAM, call the transfer callback
 *        once when they finish, and raise the completion event.
 */
static void transfer_contract(void)
{
    unsigned char sample[64], readback[64];

    for (unsigned i = 0; i < sizeof sample; i++)
    {
        sample[i] = (unsigned char)(i * 17 + 3);
    }

    Psyz_SpuInit();
    SpuStart();
    CHECK_COUNTED(_spu_isCalled == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    long original_event = _spu_EVdma;
    CHECK_COUNTED(SpuSetTransferMode(SPU_TRANSFER_BY_DMA) == 0);
    CHECK_COUNTED(SpuSetTransferStartAddr(0x1010) == 0x1010);
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam() + 0x1010, sample, sizeof sample));
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_WAIT) == 1);
    memset(readback, 0, sizeof readback);
    CHECK_COUNTED(SpuRead(readback, sizeof readback) == sizeof readback);
    CHECK_COUNTED(!memcmp(readback, sample, sizeof sample));
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_WAIT) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    _spu_transferCallback = transfer_callback;
    callback_destination = Psyz_SpuGetRam() + 0x1010;
    callback_expected = sample;
    callback_size = sizeof sample;
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    memset(readback, 0, sizeof readback);
    callback_destination = readback;
    CHECK_COUNTED(SpuRead(readback, sizeof readback) == sizeof readback);
    CHECK_COUNTED(callbacks == 2);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    callback_destination = NULL;
    CHECK_COUNTED(SpuWrite(sample, 0) == 0);
    CHECK_COUNTED(callbacks == 3);
    CHECK_COUNTED(SpuRead(readback, 0) == 0);
    CHECK_COUNTED(callbacks == 4);
    callback_unregister = 1;
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 5);
    CHECK_COUNTED(_spu_transferCallback == NULL);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    _spu_transferCallback = transfer_callback;
    CHECK_COUNTED(SpuRead(readback, sizeof readback) == sizeof readback);
    CHECK_COUNTED(callbacks == 6);
    CHECK_COUNTED(_spu_transferCallback == NULL);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_WAIT) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    _spu_transferCallback = transfer_callback;
    CHECK_COUNTED(SpuWritePartly(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 7);
    CHECK_COUNTED(_spu_transferCallback == NULL);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam() + 0x1010, sample, sizeof sample));
    _spu_transferCallback = transfer_callback;
    CHECK_COUNTED(SpuWritePartly(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 8);
    CHECK_COUNTED(_spu_transferCallback == NULL);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam() + 0x1050, sample, sizeof sample));
    CHECK_COUNTED(SpuSetTransferStartAddr(0x1010) == 0x1010);
    callback_unregister = 0;
    _spu_transferCallback = transfer_callback;
    CHECK_COUNTED(SpuSetTransferMode(SPU_TRANSFER_BY_IO) == 1);
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 8);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam() + 0x1010, sample, sizeof sample));
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    memset(readback, 0, sizeof readback);
    CHECK_COUNTED(SpuRead(readback, sizeof readback) == sizeof readback);
    CHECK_COUNTED(callbacks == 9);
    CHECK_COUNTED(!memcmp(readback, sample, sizeof sample));
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuSetTransferMode(SPU_TRANSFER_BY_DMA) == 0);
    _spu_transferCallback = NULL;
    CHECK_COUNTED(SpuWrite(sample, 0) == 0);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuRead(readback, 0) == 0);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuSetTransferStartAddr(0x1010) == 0x1010);
    memset(Psyz_SpuGetRam() + 0x1050, 0, sizeof sample);
    _spu_transferCallback = transfer_callback;
    callback_redirect_address = 1;
    CHECK_COUNTED(SpuWritePartly(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(callbacks == 10);
    callback_redirect_address = 0;
    _spu_transferCallback = NULL;
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam() + 0x1050, sample, sizeof sample));
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
    const size_t cap = 0x7EFF0;
    unsigned char* large = malloc(cap + 16);
    unsigned char* result = malloc(cap + 16);
    CHECK_COUNTED(large && result);

    if (large && result)
    {
        memset(large, 0x5B, cap + 16);
        memset(result, 0xA6, cap + 16);
        CHECK_COUNTED(SpuSetTransferStartAddr(0x1010) == 0x1010);
        CHECK_COUNTED(SpuWrite(large, cap + 16) == cap);
        CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
        CHECK_COUNTED(Psyz_SpuGetRam()[0x80000 - 1] == 0x5B);
        CHECK_COUNTED(SpuRead(result, cap + 16) == cap);
        CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
        CHECK_COUNTED(!memcmp(result, large, cap));

        for (size_t i = cap; i < cap + 16; i++)
        {
            CHECK_COUNTED(result[i] == 0xA6);
        }
    }

    free(large);
    free(result);
    CHECK_COUNTED(SpuSetTransferStartAddr(0x1010) == 0x1010);
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    akao_spu_write(sample, sizeof sample);
    CHECK_COUNTED(spu_completion_akao_callbacks == 1);
    CHECK_COUNTED(spu_completion_akao_armed == 1);
    CHECK_COUNTED(g_akao_spu_xfer_pending == 0 && _spu_transferCallback == NULL);
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_PEEK) == 1);
    akao_spu_wait();
    memset(readback, 0, sizeof readback);
    akao_spu_read(readback, sizeof readback);
    CHECK_COUNTED(spu_completion_akao_callbacks == 2);
    CHECK_COUNTED(spu_completion_akao_armed == 2);
    CHECK_COUNTED(g_akao_spu_xfer_pending == 0 && _spu_transferCallback == NULL);
    CHECK_COUNTED(!memcmp(readback, sample, sizeof sample));
    CHECK_COUNTED(SpuIsTransferCompleted(SPU_TRANSFER_WAIT) == 1);
    akao_spu_wait();
    DeliverEvent(HwSPU, EvSpCOMP);
    SpuQuit();
    CHECK_COUNTED(_spu_isCalled == 0);
    CHECK_COUNTED(TestEvent(original_event) == 0);
    SpuStart();
    CHECK_COUNTED(_spu_isCalled == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuWrite(sample, sizeof sample) == sizeof sample);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 1);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    SpuQuit();
    CHECK_COUNTED(_spu_isCalled == 0);
}

/**
 * @brief Check clearing the reverb's work area clears it in SPU RAM as the
 *        PS1 does, waiting for its transfer, without running the transfer
 *        callback or leaving the completion event set.
 */
static void reverb_contract(void)
{
    unsigned char* expected = malloc(0x80000);
    CHECK_COUNTED(expected != NULL);

    if (!expected)
    {
        return;
    }

    SpuStart();
    CHECK_COUNTED(SpuSetTransferMode(SPU_TRANSFER_BY_IO) == 1);
    _spu_transferCallback = transfer_callback;
    unsigned prior_callbacks = callbacks;
    spu_completion_reverb_audit = 1;
    memset(expected, 0xD7, 0x80000);
    memset(expected + 0x7FF80, 0, 0x80);
    memset(Psyz_SpuGetRam(), 0xD7, 0x80000);
    fprintf(stderr, "SPU reverb: original OFF clear and blocking completion wait\n");
    CHECK_COUNTED(SpuClearReverbWorkArea(SPU_REV_MODE_OFF) == 0);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam(), expected, 0x80000));
    CHECK_COUNTED(spu_completion_reverb_waits == 1 && spu_completion_reverb_waits_ok == 1);
    CHECK_COUNTED(_spu_transMode == 1 && _spu_trans_mode == SPU_TRANSFER_BY_IO);
    CHECK_COUNTED(_spu_transferCallback == transfer_callback && callbacks == prior_callbacks);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    memset(expected, 0xE3, 0x80000);
    memset(expected + 0x7D940, 0, 0x26C0);
    memset(Psyz_SpuGetRam(), 0xE3, 0x80000);
    CHECK_COUNTED(SpuClearReverbWorkArea(SPU_REV_MODE_ROOM) == 0);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam(), expected, 0x80000));
    CHECK_COUNTED(spu_completion_reverb_waits == 11 && spu_completion_reverb_waits_ok == 11);
    CHECK_COUNTED(_spu_transMode == 1 && _spu_trans_mode == SPU_TRANSFER_BY_IO);
    CHECK_COUNTED(_spu_transferCallback == transfer_callback && callbacks == prior_callbacks);
    CHECK_COUNTED(TestEvent(_spu_EVdma) == 0);
    CHECK_COUNTED(SpuClearReverbWorkArea(-1) == -1);
    CHECK_COUNTED(SpuClearReverbWorkArea(SPU_REV_MODE_MAX) == -1);
    CHECK_COUNTED(!memcmp(Psyz_SpuGetRam(), expected, 0x80000));
    CHECK_COUNTED(spu_completion_reverb_waits == 11 && _spu_transferCallback == transfer_callback);
    spu_completion_reverb_audit = 0;
    SpuQuit();
    free(expected);
}

int main(void)
{
    event_contract();
    transfer_contract();
    reverb_contract();
    CHECK_COUNTED(spu_completion_unsupported_warnings == 0);
    return lom_check_summary("SPU completion");
}
