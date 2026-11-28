#include "main/controller.h"
#include "main/controller_internal.h"
#include "lom_native_ps1_memory.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <psyz/input.h>
#include "../support/lom_check.h"

extern intptr_t lom_native_VSyncCallback(void (*)(void));

extern intptr_t __real_lom_native_VSyncCallback(void (*)(void));

static void (*observed_callback)(void);

static char events[128];

#define CHECK_EVENTS(test) CHECK_DETAIL(test, events)

static unsigned event_count, setter_errors, low_calls, backend_calls, pad_reads, pad_initializations;

static void (*backend_hook)(void);

static unsigned backend_ran;

/**
 * @brief The vertical blank hook PSY-Z had before the controllers': count its
 *        calls.
 */
static void previous_backend_hook(void)
{
    ++backend_calls;
    backend_ran = 1;
}

/**
 * @brief Note one call in the order they happened, as a letter.
 * @param code The letter.
 */
static void event(char code)
{
    if (event_count + 1 < sizeof(events))
    {
        events[event_count++] = code;
        events[event_count] = 0;
    }
    else
    {
        ++setter_errors;
    }
}

/**
 * @brief Forget the calls noted so far.
 */
static void clear_events(void)
{
    event_count = 0;
    events[0] = 0;
}

/**
 * @brief A game vertical blank callback: count its calls, and check PSY-Z's
 *        hook ran first.
 */
static void low_callback(void)
{
    if (!backend_ran)
    {
        ++setter_errors;
    }

    ++low_calls;
}

/**
 * @brief Count the calls to low_callback.
 * @return The count.
 */
static unsigned low_callback_calls(void)
{
    return low_calls;
}

/**
 * @brief Note the game setting its vertical blank callback, and check the one
 *        it replaces is the last one set.
 * @param callback The new callback.
 * @return What the real one returns.
 */
intptr_t __wrap_lom_native_VSyncCallback(void (*callback)(void))
{
    event('C');
    intptr_t previous = __real_lom_native_VSyncCallback(callback);

    if ((uintptr_t)previous != (uintptr_t)observed_callback)
    {
        ++setter_errors;
    }

    observed_callback = callback;
    return previous;
}

void (*Psyz_SetVSyncCb(void (*next)(void)))(void)
{
    void (*previous)(void) = backend_hook;
    backend_hook = next;
    return previous;
}

/**
 * @brief A PadInit that counts its calls.
 * @param mode Ignored.
 */
void PadInit(int mode)
{
    (void)mode;
    ++pad_initializations;
}

/**
 * @brief A pad read with nothing plugged in: fill the buffer with 0xFF and note
 *        it.
 * @param port 0 or 1.
 * @param buffer Filled.
 * @param length PSYZ_PAD_BUF_LEN.
 */
void Psyz_PadsGet(int port, char* buffer, int length)
{
    if (port < 0 || port > 1 || length != PSYZ_PAD_BUF_LEN)
    {
        ++setter_errors;
    }

    memset(buffer, 255, (size_t)length);
    ++pad_reads;
    event('R');
}

/**
 * @brief Say nothing is plugged in.
 * @param port 0 or 1.
 * @param channel 0.
 * @param kind PSYZ_CTRL_QUERY_KIND.
 * @return PSYZ_CTRL_DISCONNECTED.
 */
PsyzControllerKind Psyz_PadsSetKind(int port, int channel, PsyzControllerKind kind)
{
    if (port < 0 || port > 1 || channel != 0 || kind != PSYZ_CTRL_QUERY_KIND)
    {
        ++setter_errors;
        return PSYZ_CTRL_ERROR;
    }

    return PSYZ_CTRL_DISCONNECTED;
}

/**
 * @brief A vertical blank: note it and run the hook, which must run once.
 * @param mode Ignored.
 * @return 0.
 */
int VSync(int mode)
{
    (void)mode;
    unsigned before = backend_calls;
    backend_ran = 0;
    event('V');

    if (backend_hook)
    {
        backend_hook();
    }

    if (backend_calls != before + 1)
    {
        ++setter_errors;
    }

    return 0;
}

extern void __real_lom_native_PadInitDirect(unsigned char*, unsigned char*);

extern void __real_lom_native_PadStartCom(void);

extern void __real_lom_native_PadStopCom(void);

extern int __real_lom_native_PadChkVsync(void);

extern int __real_lom_native_PadGetState(int);

int lom_native_PadChkVsync(void);

void lom_native_PadStartCom(void);

void lom_native_PadStopCom(void);

/**
 * @brief Note PadInitDirect, and check it's given the controller state's
 *        receive buffers.
 * @param first Port 1's buffer.
 * @param second Port 2's.
 */
void __wrap_lom_native_PadInitDirect(unsigned char* first, unsigned char* second)
{
    event('P');

    if (first != CONTROLLER_STATE->receive_buffers[0].bytes || second != CONTROLLER_STATE->receive_buffers[1].bytes)
    {
        ++setter_errors;
    }

    __real_lom_native_PadInitDirect(first, second);
}

/**
 * @brief Note PadStartCom.
 */
void __wrap_lom_native_PadStartCom(void)
{
    event('S');
    __real_lom_native_PadStartCom();
}

/**
 * @brief Note PadStopCom.
 */
void __wrap_lom_native_PadStopCom(void)
{
    event('T');
    __real_lom_native_PadStopCom();
}

/**
 * @brief Note PadChkVsync.
 * @return What the real one returns.
 */
int __wrap_lom_native_PadChkVsync(void)
{
    event('K');
    return __real_lom_native_PadChkVsync();
}

/**
 * @brief Note PadGetState.
 * @param port The port.
 * @return What the real one returns.
 */
int __wrap_lom_native_PadGetState(int port)
{
    event('G');
    return __real_lom_native_PadGetState(port);
}

#define STATE_ROOM MEMORY_MAP_SPACING(0x200)
#define GAP_START (offsetof(ControllerState, vsync_accumulation_interval) + 1)
#define GAP_END offsetof(ControllerState, previous_vsync_callback)

/**
 * @brief Check nothing wrote outside the controller state, or into the gap
 *        inside it that the PS1 leaves unused.
 * @return 0. A failed check stops the test.
 */
static int sentinels(void)
{
    const unsigned char* base = (const unsigned char*)CONTROLLER_STATE;

    for (unsigned i = 0; i < 32; ++i)
    {
        CHECK_EVENTS(base[-32 + (int)i] == 0x5A);
    }

    for (unsigned i = sizeof(ControllerState); i < STATE_ROOM + 32; ++i)
    {
        CHECK_EVENTS(base[i] == 0xA5);
    }

    for (unsigned i = GAP_START; i < GAP_END; ++i)
    {
        CHECK_EVENTS(base[i] == 0xD3);
    }

    return 0;
}

/**
 * @brief Start the controllers, read them and shut them down, with a game
 *        callback already set, checking the calls come in the PS1's order and
 *        the callbacks are put back.
 * @param prior The game's callback, or NULL.
 * @param prior_calls Counts its calls, or NULL.
 * @return 0. A failed check stops the test.
 */
static int cycle(void (*prior)(void), unsigned (*prior_calls)(void))
{
    unsigned char* base = (unsigned char*)CONTROLLER_STATE;
    memset(base - 32, 0x5A, 32);
    memset(base, 0xA5, STATE_ROOM + 32);
    memset(base, 0, sizeof(ControllerState));
    memset(base + GAP_START, 0xD3, GAP_END - GAP_START);
    unsigned before_game = prior_calls ? prior_calls() : 0;
    unsigned before_reads = pad_reads;
    lom_native_VSyncCallback(prior);
    VSync(0);
    CHECK_EVENTS(!prior_calls || prior_calls() == before_game + 1);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == before_reads);
    clear_events();
    initialize_controllers(0);
    CHECK_EVENTS(!strcmp(events, "PCSVKRRGG"));
    CHECK_EVENTS(!observed_callback);
    CHECK_EVENTS(!prior_calls || prior_calls() == before_game + 1);
    CHECK_EVENTS(CONTROLLER_STATE->previous_vsync_callback.handler == prior);
    CHECK_EVENTS(g_previous_controller_vsync_callback.handler == prior);
    CHECK_EVENTS(&g_previous_controller_vsync_callback == &CONTROLLER_STATE->previous_vsync_callback);
    CHECK_EVENTS(&g_controller_pending_sample_count == &CONTROLLER_STATE->pending_sample_count);
    CHECK_EVENTS(&g_controller_vsync_accumulation_count == &CONTROLLER_STATE->vsync_accumulation_count);
    CHECK_EVENTS(!sentinels());

    clear_events();
    initialize_controller_vsync();
    CHECK_EVENTS(!strcmp(events, "VC"));
    CHECK_EVENTS(observed_callback && observed_callback != prior);
    CHECK_EVENTS(CONTROLLER_STATE->pending_sample_count == 0);
    CHECK_EVENTS(CONTROLLER_STATE->vsync_accumulation_count == 0);
    update_controllers();
    VSync(0);
    CHECK_EVENTS(CONTROLLER_STATE->pending_sample_count == 1);
    CHECK_EVENTS(CONTROLLER_STATE->vsync_accumulation_count == 2);
    update_controllers();
    CHECK_EVENTS(CONTROLLER_STATE->published_sample_count == 1);
    CHECK_EVENTS(CONTROLLER_STATE->pending_sample_count == 0);
    CHECK_EVENTS(CONTROLLER_STATE->ports[0].published_sample.device_type == CONTROLLER_DEVICE_DISCONNECTED);

    clear_events();
    shutdown_controllers();
    CHECK_EVENTS(!strcmp(events, "VKRRGGTC"));
    CHECK_EVENTS(observed_callback == prior);
    CHECK_EVENTS(!setter_errors && !sentinels());
    before_reads = pad_reads;
    VSync(0);
    CHECK_EVENTS(!prior_calls || prior_calls() == before_game + 2);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == before_reads);
    intptr_t restored = lom_native_VSyncCallback(NULL);
    CHECK_EVENTS((uintptr_t)restored == (uintptr_t)prior);
    CHECK_EVENTS(backend_hook == previous_backend_hook);
    before_reads = pad_reads;
    VSync(0);
    CHECK_EVENTS(!prior_calls || prior_calls() == before_game + 2);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == before_reads);
    CHECK_EVENTS(!setter_errors);
    return 0;
}

int main(int argc, char** argv)
{
    if (argc != 2 || lom_native_map_ps1_ram())
    {
        return 2;
    }

#ifdef _WIN32
    HMODULE library = LoadLibraryA(argv[1]);

    if (!library)
    {
        fprintf(stderr, "LoadLibrary %s failed: error %lu\n", argv[1], (unsigned long)GetLastError());
        return 2;
    }

    void (*high_callback)(void) = (void (*)(void))GetProcAddress(library, "lom_test_controller_prior_callback");
    unsigned (*high_calls)(void) = (unsigned (*)(void))GetProcAddress(library, "lom_test_controller_prior_callback_calls");
#else
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);

    if (!library)
    {
        fprintf(stderr, "%s\n", dlerror());
        return 2;
    }

    void (*high_callback)(void) = (void (*)(void))dlsym(library, "lom_test_controller_prior_callback");
    unsigned (*high_calls)(void) = (unsigned (*)(void))dlsym(library, "lom_test_controller_prior_callback_calls");
#endif
    CHECK_EVENTS(high_callback && high_calls);
    backend_hook = previous_backend_hook;

    for (unsigned i = 0; i < 3; ++i)
    {
        CHECK_EVENTS(!cycle(NULL, NULL));
        CHECK_EVENTS(!cycle(low_callback, low_callback_calls));
        CHECK_EVENTS(!cycle(high_callback, high_calls));
        CHECK_EVENTS(low_calls == 2 * (i + 1) && high_calls() == 2 * (i + 1));
    }

    clear_events();
    unsigned reads = pad_reads, calls = low_calls;
    lom_native_VSyncCallback(low_callback);
    lom_native_PadStartCom();
    lom_native_PadStartCom();
    VSync(0);
    CHECK_EVENTS(low_calls == calls + 1);
    CHECK_EVENTS(lom_native_PadChkVsync() == 1 && pad_reads == reads + 2);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == reads + 2);
    CHECK_EVENTS(lom_native_VSyncCallback(NULL) == (intptr_t)low_callback);
    CHECK_EVENTS(backend_hook != previous_backend_hook);
    VSync(0);
    CHECK_EVENTS(low_calls == calls + 1);
    CHECK_EVENTS(lom_native_PadChkVsync() == 1 && pad_reads == reads + 4);
    lom_native_PadStopCom();
    lom_native_PadStopCom();
    CHECK_EVENTS(backend_hook == previous_backend_hook);
    VSync(0);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == reads + 4);
    lom_native_VSyncCallback(low_callback);
    lom_native_PadStartCom();
    lom_native_PadStopCom();
    CHECK_EVENTS(backend_hook != previous_backend_hook);
    VSync(0);
    CHECK_EVENTS(low_calls == calls + 2);
    CHECK_EVENTS(lom_native_PadChkVsync() == 0 && pad_reads == reads + 4);
    CHECK_EVENTS(lom_native_VSyncCallback(NULL) == (intptr_t)low_callback);
    CHECK_EVENTS(backend_hook == previous_backend_hook);
    CHECK_EVENTS(pad_initializations == 1 && !setter_errors && !sentinels());
#ifdef _WIN32
    FreeLibrary(library);
#else
    dlclose(library);
#endif
    puts("Controller callback: 9 original lifecycle cycles, real low/high callbacks restored; adjacent RAM intact");
    return 0;
}
