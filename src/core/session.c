#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "lom_native_session.h"
#include "session_folder.h"
#include "lom_native_switches.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_card.h"
#include "lom_native_fs.h"
#include "lom_native_pipe.h"
#include "lom_native_psyz.h"
#include "lom_native_remaster.h"
#include "lom_native_env.h"
#include <SDL3/SDL_events.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PAD_BYTES 34
#define PAD_RECORD (1 + 2 * PAD_BYTES)
#define ALWAYS_RUN_RECORD 2
#define SWITCH_RECORD 3
#define MOD_RECORD 3
#define MOD_RECORD_LIMIT 64
#define COUNT_RECORD 5
#define FLUSH_BLANKS 60
static const char k_magic[8] = {'L', 'O', 'M', 'S', 'E', 'S', 'S', '1'};

static const uint32_t k_version = 6;

static uint32_t s_input_version = 3;

enum
{
    MODE_OFF,
    MODE_RECORD,
    MODE_REPLAY
};

static int s_mode;

static char s_dir[4096];

static int s_inputs_fd = -1, s_end_fd = -1;

static unsigned char s_out[1 << 16];

static size_t s_out_len;

static unsigned s_blanks;

static unsigned long long s_last_pulled;

static char s_stop[512];

static int s_finished;

static int s_saved_stderr = -1, s_pipe_read = -1, s_log_fd = -1;

static pthread_t s_tee;

static int s_tee_started;

static volatile int s_tee_stop;

static unsigned char* s_replay;

static size_t s_replay_size, s_replay_pos;

static int s_replay_state;

static unsigned long long s_vblank;

static int s_quit_seen;

static unsigned char s_recorded_switches[LOM_SWITCH_COUNT];

static unsigned char s_recorded_mods[MOD_RECORD_LIMIT];

static void (*s_pad_script)(unsigned long long, unsigned char* [2]);

void lom_native_session_set_pad_script(void (*script)(unsigned long long, unsigned char* [2]))
{
    s_pad_script = script;
}

/**
 * @brief Write bytes to a file, retrying partial writes. Safe in a signal
 *        handler.
 * @param fd The file.
 * @param data The bytes.
 * @param size How many.
 */
static void write_all(int fd, const void* data, size_t size)
{
    const unsigned char* bytes = data;

    while (size > 0)
    {
        ssize_t n = write(fd, bytes, size);

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n <= 0)
        {
            return;
        }

        bytes += n;
        size -= (size_t)n;
    }
}

/**
 * @brief Write the buffered records to inputs.bin.
 */
static void flush_records(void)
{
    if (s_inputs_fd >= 0)
    {
        write_all(s_inputs_fd, s_out, s_out_len);
    }

    s_out_len = 0;
}

/**
 * @brief Add bytes to the recording's buffer, writing it out when it's full.
 * @param data The bytes.
 * @param size How many.
 */
static void put(const void* data, size_t size)
{
    if (s_out_len + size > sizeof(s_out))
    {
        flush_records();
    }

    memcpy(s_out + s_out_len, data, size);
    s_out_len += size;
}

/**
 * @brief Copy whatever the game has printed so far to the session's log and the
 *        real stderr.
 */
static void drain_stderr_pipe(void)
{
    char buffer[4096];

    for (;;)
    {
        ssize_t n = lom_native_pipe_read(s_pipe_read, buffer, sizeof(buffer));

        if (n <= 0)
        {
            return;
        }

        write_all(s_log_fd, buffer, (size_t)n);
        write_all(s_saved_stderr, buffer, (size_t)n);
    }
}

/**
 * @brief The thread that copies everything the game prints to both the
 *        session's log and the real stderr, until the pipe closes.
 * @param unused Unused.
 * @return NULL.
 */
static void* tee_stderr(void* unused)
{
    (void)unused;
    char buffer[4096];

    for (;;)
    {
        ssize_t n = lom_native_pipe_read(s_pipe_read, buffer, sizeof(buffer));

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n < 0 && errno == EAGAIN)
        {
            if (__atomic_load_n(&s_tee_stop, __ATOMIC_ACQUIRE))
            {
                return NULL;
            }

            struct timespec pause = {0, 2000000};
            nanosleep(&pause, NULL);
            continue;
        }

        if (n <= 0)
        {
            return NULL;
        }

        write_all(s_log_fd, buffer, (size_t)n);
        write_all(s_saved_stderr, buffer, (size_t)n);
    }
}

/**
 * @brief Write how the run ended into the session's end file, replacing
 *        "running".
 * @param reason How it ended.
 */
static void write_end(const char* reason)
{
    if (s_end_fd < 0)
    {
        return;
    }

    if (lseek(s_end_fd, 0, SEEK_SET) == 0 && ftruncate(s_end_fd, 0) == 0)
    {
        write_all(s_end_fd, reason, strlen(reason));
        write_all(s_end_fd, "\n", 1);
    }
}

void lom_native_session_crashed(const char* reason)
{
    if (s_mode == MODE_RECORD && !s_finished)
    {
        flush_records();
        write_end(s_stop[0] ? s_stop : reason);

        if (s_pipe_read >= 0)
        {
            drain_stderr_pipe();
        }
    }
}

/**
 * @brief Ask the game to quit, as the end of a replay does.
 */
static void request_quit(void)
{
    SDL_Event quit = {.type = SDL_EVENT_QUIT};
    SDL_PushEvent(&quit);
}

/**
 * @brief End a replay that no longer matches its recording, so it exits with 2.
 */
static void drift(void)
{
    s_replay_state = 2;
    request_quit();
}

/**
 * @brief Note that a replay has finished once every recorded input has played
 *        back.
 */
static void check_replay_end(void)
{
    if (s_replay_state || s_replay_pos < s_replay_size)
    {
        return;
    }

    s_replay_state = 1;

    if (s_pad_script)
    {
        fprintf(stderr, "[lom-native] replay: recording consumed at vblank %llu; continuing with the pad script\n", s_vblank);
        s_mode = MODE_OFF;
        Psyz_SetLegacyVSyncReplayProfile(0);
        return;
    }

    request_quit();
}

/**
 * @brief Check whether the recording's next record is a whole one of a kind.
 * @param tag The record's first byte.
 * @param size The record's size, tag included.
 * @return 1 when it is, otherwise 0.
 */
static int next_record_is(char tag, size_t size)
{
    return s_replay_size - s_replay_pos >= size && s_replay[s_replay_pos] == tag;
}

/**
 * @brief Read the number a count record carries after its tag.
 * @return The number.
 */
static uint32_t next_record_count(void)
{
    uint32_t count;
    memcpy(&count, s_replay + s_replay_pos + 1, sizeof(count));
    return count;
}

/**
 * @brief Check whether what's left of a recording is only timing (blanks and
 *        queries) with no more controller input, which a replay can skip.
 * @param pos Where to start looking.
 * @return 1 when only timing is left, otherwise 0.
 */
static int only_timing_remains(size_t pos)
{
    while (pos < s_replay_size)
    {
        if (s_replay[pos] == 'P')
        {
            return 0;
        }

        if (s_replay[pos] == 'V' || s_replay[pos] == 'E' || s_replay[pos] == 'R' || s_replay[pos] == 'C')
        {
            if (s_replay_size - pos < COUNT_RECORD)
            {
                return 0;
            }

            pos += COUNT_RECORD;
        }
        else if (s_replay[pos] == 'Q')
        {
            ++pos;
        }
        else
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief End a replay that has run out of input, where only the timing differed
 *        after it.
 * @param event What differed, for the message.
 */
static void finish_replay_early(const char* event)
{
    fprintf(stderr, "[lom-native] replay: all recorded input consumed; %s at byte %zu after it\n", event, s_replay_pos);
    s_replay_pos = s_replay_size;
    check_replay_end();
}

/**
 * @brief Check whether the recording quits before its next blank: whether a
 *        quit comes after the pad, switch and query records that follow.
 * @param pos Where to start looking.
 * @return 1 when it does, otherwise 0.
 */
static int quit_before_next_blank(size_t pos)
{
    while (pos < s_replay_size)
    {
        if (s_replay[pos] == 'P')
        {
            pos += PAD_RECORD;
        }
        else if (s_replay[pos] == 'A')
        {
            pos += ALWAYS_RUN_RECORD;
        }
        else if (s_replay[pos] == 'S')
        {
            pos += SWITCH_RECORD;
        }
        else if (s_replay[pos] == 'M')
        {
            pos += MOD_RECORD;
        }
        else if (s_replay[pos] == 'R' || s_replay[pos] == 'C')
        {
            pos += COUNT_RECORD;
        }
        else
        {
            break;
        }
    }

    return pos < s_replay_size && s_replay[pos] == 'Q';
}

/**
 * @brief Record a blank: a quit first if the game has just asked to, then the
 *        blank's kind and the audio frames pulled since the last.
 * @param tag 'V' for a blank that showed a frame, 'E' for one that didn't.
 */
static void record_vblank(char tag)
{
    if (!s_quit_seen && Psyz_QuitRequested())
    {
        s_quit_seen = 1;
        put("Q", 1);
    }

    unsigned long long pulled = Psyz_AudioPulledFrames();
    uint32_t frames = (uint32_t)(pulled - s_last_pulled);
    s_last_pulled = pulled;
    put(&tag, 1);
    put(&frames, sizeof(frames));

    if (++s_blanks >= FLUSH_BLANKS)
    {
        flush_records();
        s_blanks = 0;
    }
}

/**
 * @brief Play back a blank: check it's the kind the recording has, give the
 *        audio the frames it pulled, and quit where the recording did.
 * @param tag 'V' for a blank that showed a frame, 'E' for one that didn't.
 */
static void replay_vblank(char tag)
{
    if (s_replay_pos < s_replay_size && s_replay[s_replay_pos] == 'Q')
    {
        s_replay_pos++;
    }

    if (!next_record_is(tag, COUNT_RECORD) && only_timing_remains(s_replay_pos))
    {
        finish_replay_early("frame timing differs");
        return;
    }

    if (!next_record_is(tag, COUNT_RECORD))
    {
        fprintf(stderr, "[lom-native] replay diverged at byte %zu: game serviced a vblank with a different presentation kind, recording has '%c'\n",
                s_replay_pos, s_replay_pos < s_replay_size ? s_replay[s_replay_pos] : '-');
        drift();
        return;
    }

    Psyz_AudioSetVirtualBlankFrames((int)next_record_count());
    s_replay_pos += COUNT_RECORD;
    check_replay_end();

    if (!s_quit_seen && quit_before_next_blank(s_replay_pos))
    {
        s_quit_seen = 1;
        request_quit();
    }
}

/**
 * @brief Run on every vertical blank: record the blank and the audio frames
 *        pulled since the last, or check the recording's blank against this
 *        one when replaying. A blank that doesn't match ends the replay as
 *        drifted.
 */
static void vblank(void)
{
    char tag = Psyz_VideoVBlankWasPresented() ? 'V' : 'E';
    s_vblank++;

    if (s_mode == MODE_RECORD)
    {
        record_vblank(tag);
    }
    else if (s_mode == MODE_REPLAY && !s_replay_state)
    {
        replay_vblank(tag);
    }
}

/**
 * @brief Play back the blanks a version 3 recording noted as passing without a
 *        frame shown.
 * @return 1 for each, 0 when there are none, or -1 when the replay drifted.
 */
static int replay_elapsed(void)
{
    if (s_mode != MODE_REPLAY || s_replay_state || s_input_version < 3)
    {
        return 0;
    }

    size_t next = s_replay_pos;

    if (next < s_replay_size && s_replay[next] == 'Q')
    {
        ++next;
    }

    return next < s_replay_size && s_replay[next] == 'E';
}

/**
 * @brief Record the horizontal count the game read, or give it the recorded one
 *        when replaying.
 * @param live The real count.
 * @return The count the game should see.
 */
static unsigned horizontal_query(unsigned live)
{
    if (s_mode == MODE_RECORD)
    {
        put("R", 1);
        put(&live, 4);
        return live;
    }

    if (s_input_version < 3 && Psyz_LegacyVSyncReplayProfileActive())
    {
        return 0;
    }

    if (s_mode != MODE_REPLAY || s_input_version < 3 || s_replay_state)
    {
        return live;
    }

    if (!next_record_is('R', COUNT_RECORD) && only_timing_remains(s_replay_pos))
    {
        finish_replay_early("horizontal timing differs");
        return live;
    }

    if (!next_record_is('R', COUNT_RECORD))
    {
        fprintf(stderr, "[lom-native] replay: horizontal query diverged at byte %zu\n", s_replay_pos);
        drift();
        return live;
    }

    uint32_t recorded = next_record_count();
    s_replay_pos += COUNT_RECORD;
    check_replay_end();
    return recorded;
}

/**
 * @brief Record a vertical blank count the game read, or check it against the
 *        recording when replaying.
 * @param live The real count.
 * @return 0, or -1 when the replay drifted. PSY-Z ignores it.
 */
static int vertical_query(unsigned live)
{
    if (s_mode == MODE_RECORD)
    {
        put("C", 1);
        put(&live, 4);
        return 0;
    }

    if (s_mode != MODE_REPLAY || s_replay_state || s_input_version < 3)
    {
        return 0;
    }

    if (!next_record_is('C', COUNT_RECORD) && only_timing_remains(s_replay_pos))
    {
        finish_replay_early("vertical query timing differs");
        return 0;
    }

    if (!next_record_is('C', COUNT_RECORD) || next_record_count() != live)
    {
        fprintf(stderr, "[lom-native] replay: vertical query diverged at byte %zu\n", s_replay_pos);
        drift();
        return -1;
    }

    s_replay_pos += COUNT_RECORD;
    check_replay_end();
    return 0;
}

/**
 * @brief Record the pads the game sampled, and each switch and each mod the
 *        player turned on or off in F1 since the last sample. The game follows
 *        the recorded values until the next.
 * @param port_buffers The two ports' buffers; either may be NULL.
 */
static void record_pads(unsigned char* port_buffers[2])
{
    static const unsigned char empty[PAD_BYTES];

    for (int i = 0; i < lom_native_mod_toggles_count() && i < MOD_RECORD_LIMIT; i++)
    {
        const unsigned char value = (unsigned char)lom_native_mod_toggles_wanted(i);

        if (value != s_recorded_mods[i])
        {
            const unsigned char index = (unsigned char)i;
            put("M", 1);
            put(&index, 1);
            put(&value, 1);
            s_recorded_mods[i] = value;
        }

        lom_native_mod_toggles_hold(i, value);
    }

    for (int i = 0; i < LOM_SWITCH_COUNT; i++)
    {
        const unsigned char value = (unsigned char)lom_native_switch_wanted((LomSwitch)i);

        if (value != s_recorded_switches[i])
        {
            const unsigned char number = (unsigned char)lom_native_switch_record_number((LomSwitch)i);
            put("S", 1);
            put(&number, 1);
            put(&value, 1);
            s_recorded_switches[i] = value;
        }

        lom_native_switch_hold((LomSwitch)i, value);
    }

    put("P", 1);
    put(port_buffers[0] ? port_buffers[0] : empty, PAD_BYTES);
    put(port_buffers[1] ? port_buffers[1] : empty, PAD_BYTES);
}

/**
 * @brief Give the game the recording's switches before its pads: version 5's
 *        switch records, version 4's Always run records, and version 6's
 *        records of mods turned on or off in F1.
 * @return 0, or -1 when a switch record names a switch this build doesn't have.
 */
static int replay_switches(void)
{
    for (;;)
    {
        if (next_record_is('A', ALWAYS_RUN_RECORD))
        {
            lom_native_switch_hold(LOM_SWITCH_ALWAYS_RUN, s_replay[s_replay_pos + 1]);
            s_replay_pos += ALWAYS_RUN_RECORD;
        }
        else if (next_record_is('S', SWITCH_RECORD))
        {
            const int id = lom_native_switch_from_record_number(s_replay[s_replay_pos + 1]);

            if (id < 0)
            {
                fprintf(stderr, "[lom-native] replay: switch %u at byte %zu isn't one this build has\n", s_replay[s_replay_pos + 1], s_replay_pos);
                return -1;
            }

            lom_native_switch_hold((LomSwitch)id, s_replay[s_replay_pos + 2]);
            s_replay_pos += SWITCH_RECORD;
        }
        else if (next_record_is('M', MOD_RECORD))
        {
            lom_native_mod_toggles_hold(s_replay[s_replay_pos + 1], s_replay[s_replay_pos + 2]);
            s_replay_pos += MOD_RECORD;
        }
        else
        {
            return 0;
        }
    }
}

/**
 * @brief Give the game the recording's pads, and its switches as they were then.
 * @param port_buffers The two ports' buffers; either may be NULL.
 */
static void replay_pads(unsigned char* port_buffers[2])
{
    if (replay_switches())
    {
        drift();
        return;
    }

    if (!next_record_is('P', PAD_RECORD) && only_timing_remains(s_replay_pos))
    {
        finish_replay_early("the game sampled the pads");
        return;
    }

    if (!next_record_is('P', PAD_RECORD))
    {
        fprintf(stderr, "[lom-native] replay diverged at byte %zu: game sampled the pads, recording has '%c'\n", s_replay_pos, s_replay[s_replay_pos]);
        drift();
        return;
    }

    for (int port = 0; port < 2; port++)
    {
        if (port_buffers[port])
        {
            memcpy(port_buffers[port], s_replay + s_replay_pos + 1 + port * PAD_BYTES, PAD_BYTES);
        }
    }

    s_replay_pos += PAD_RECORD;
    check_replay_end();
}

void lom_native_session_pad_sample(unsigned char* port_buffers[2])
{
    if (s_mode != MODE_REPLAY && s_pad_script)
    {
        s_pad_script(s_vblank, port_buffers);
    }

    if (s_mode == MODE_RECORD)
    {
        record_pads(port_buffers);
    }
    else if (s_mode == MODE_REPLAY)
    {
        if (!s_replay_state)
        {
            replay_pads(port_buffers);
        }
    }
    else
    {
        lom_native_switch_release();
        lom_native_mod_toggles_release();
    }
}

int lom_native_session_replay_state(void)
{
    return s_replay_state;
}

/**
 * @brief atexit's handler for a recording: finish it with "quit" or "exit".
 */
static void finish_at_exit(void)
{
    lom_native_session_finish(Psyz_QuitRequested() ? "quit" : "exit");
}

/**
 * @brief atexit's handler for a replay: exit with 2 when it drifted and 1 when
 *        it didn't finish, so a test can tell.
 */
static void replay_at_exit(void)
{
    if (s_replay_state == 2)
    {
        _exit(2);
    }

    if (s_replay_state == 0)
    {
        _exit(1);
    }
}

/**
 * @brief Open the session's files: inputs.bin for the inputs, end for how the
 *        run ended, and log.txt for what the game printed.
 * @return 0, or -1 when any of them can't be opened.
 */
static int open_session_files(void)
{
    char path[4096 + 32];
    snprintf(path, sizeof(path), "%s/inputs.bin", s_dir);
    s_inputs_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | LOM_NATIVE_O_PRIVATE, 0600);
    snprintf(path, sizeof(path), "%s/end", s_dir);
    s_end_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | LOM_NATIVE_O_PRIVATE, 0600);
    snprintf(path, sizeof(path), "%s/log.txt", s_dir);
    s_log_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | LOM_NATIVE_O_PRIVATE, 0600);
    return s_inputs_fd < 0 || s_end_fd < 0 || s_log_fd < 0 ? -1 : 0;
}

/**
 * @brief Send stderr through a pipe that a thread copies to the terminal and
 *        to the session's log. Without a pipe, stderr stays as it is.
 */
static void start_stderr_tee(void)
{
    int pipe_fds[2];

    if (lom_native_pipe_cloexec(pipe_fds) != 0)
    {
        return;
    }

    s_saved_stderr = dup(STDERR_FILENO);
    s_pipe_read = pipe_fds[0];
    lom_native_pipe_set_nonblocking(s_pipe_read);

    if (s_saved_stderr >= 0 && dup2(pipe_fds[1], STDERR_FILENO) >= 0)
    {
        setvbuf(stderr, NULL, _IONBF, 0);
        s_tee_started = pthread_create(&s_tee, NULL, tee_stderr, NULL) == 0;
    }

    close(pipe_fds[1]);
}

int lom_native_session_record_start(const char* disc_root)
{
    Psyz_SetLegacyVSyncReplayProfile(0);
    if (!lom_native_env_on(LOM_ENV_RECORD))
    {
        if (s_pad_script)
        {
            Psyz_VideoSetVBlankHook(vblank);
        }

        return 0;
    }

    char base[3072];

    if (lom_session_base_folder(base, sizeof(base)))
    {
        return -1;
    }

    if (lom_native_make_folders(base))
    {
        return -1;
    }

    lom_session_prune(base);

    time_t now = time(NULL);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", gmtime(&now));
    snprintf(s_dir, sizeof(s_dir), "%s/session-%s-%d", base, stamp, (int)getpid());

    if (lom_native_make_folders(s_dir) || lom_session_write_manifest(s_dir, disc_root, now))
    {
        goto fail;
    }

    if (lom_session_snapshot_card(s_dir))
    {
        fprintf(stderr, "[lom-native] session: card snapshot failed; replay will start without it\n");
    }

    if (open_session_files())
    {
        goto fail;
    }

    s_mode = MODE_RECORD;
    s_input_version = k_version;
    memset(s_recorded_switches, 0, sizeof(s_recorded_switches));
    memset(s_recorded_mods, 1, sizeof(s_recorded_mods));
    put(k_magic, sizeof(k_magic));
    put(&k_version, sizeof(k_version));
    flush_records();
    write_end("running");
    s_last_pulled = Psyz_AudioPulledFrames();

    start_stderr_tee();
    Psyz_VideoSetVBlankHook(vblank);
    Psyz_VideoSetHCountHook(horizontal_query);
    Psyz_VideoSetCountHook(vertical_query);
    atexit(finish_at_exit);
    return 0;
fail:
    fprintf(stderr, "[lom-native] session recording unavailable: %s\n", strerror(errno));
    s_mode = MODE_OFF;
    return -1;
}

const char* lom_native_session_folder(void)
{
    return s_mode == MODE_RECORD ? s_dir : NULL;
}

void lom_native_session_note_stop(const char* reason)
{
    if (s_mode != MODE_RECORD || s_stop[0])
    {
        return;
    }

    snprintf(s_stop, sizeof(s_stop), "field-stop: %s", reason);
}

void lom_native_session_finish(const char* end_reason)
{
    if (s_mode != MODE_RECORD || s_finished)
    {
        return;
    }

    s_finished = 1;
    Psyz_VideoSetVBlankHook(NULL);
    Psyz_VideoSetHCountHook(NULL);
    Psyz_VideoSetCountHook(NULL);
    Psyz_VideoSetElapsedReplayHook(NULL);
    flush_records();
    write_end(s_stop[0] ? s_stop : end_reason);

    if (s_saved_stderr >= 0)
    {
        fflush(stderr);
        dup2(s_saved_stderr, STDERR_FILENO);
        __atomic_store_n(&s_tee_stop, 1, __ATOMIC_RELEASE);

        if (s_tee_started)
        {
            pthread_join(s_tee, NULL);
        }

        close(s_pipe_read);
        s_pipe_read = -1;
    }

    close(s_inputs_fd);
    close(s_end_fd);
    close(s_log_fd);
    s_inputs_fd = s_end_fd = s_log_fd = -1;
    fprintf(stderr, "[lom-native] session recorded: %s\n", s_dir);
}

/**
 * @brief Read a whole recording into memory.
 * @param path The recording's inputs.bin.
 * @param size Set to how many bytes it has.
 * @return The bytes, to free, or NULL when it can't be read.
 */
static unsigned char* read_recording(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    long length = fseek(file, 0, SEEK_END) ? -1 : ftell(file);
    unsigned char* data = length > 0 ? malloc((size_t)length) : NULL;
    rewind(file);

    if (data && fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        data = NULL;
    }

    fclose(file);
    *size = data ? (size_t)length : 0;
    return data;
}

int lom_native_session_replay_start(const char* inputs_path)
{
    size_t size;
    unsigned char* recording = read_recording(inputs_path, &size);

    if (!recording || size < sizeof(k_magic) + sizeof(k_version))
    {
        free(recording);
        return -1;
    }

    uint32_t version;
    memcpy(&version, recording + sizeof(k_magic), sizeof(version));

    if (memcmp(recording, k_magic, sizeof(k_magic)) || version < 1 || version > k_version)
    {
        fprintf(stderr, "[lom-native] %s is not a supported session version 1-%u\n", inputs_path, k_version);
        free(recording);
        return -1;
    }

    if (version < 3)
    {
        if (!Psyz_AudioIsVirtualClock())
        {
            fprintf(stderr, "[lom-native] legacy session version %u requires virtual replay\n", version);
            free(recording);
            return -1;
        }

        fprintf(
            stderr,
            "[lom-native] legacy session version %u: historical SDK/presentation callback profile, virtual H queries zero; elapsed/query history unavailable\n",
            version);
    }

    if (Psyz_SetLegacyVSyncReplayProfile(version < 3))
    {
        free(recording);
        return -1;
    }

    s_replay = recording;
    s_input_version = version;
    s_replay_size = size;
    s_replay_pos = sizeof(k_magic) + sizeof(k_version);
    s_mode = MODE_REPLAY;

    for (int i = 0; i < LOM_SWITCH_COUNT; i++)
    {
        lom_native_switch_hold((LomSwitch)i, 0);
    }

    for (int i = 0; i < lom_native_mod_toggles_count(); i++)
    {
        lom_native_mod_toggles_hold(i, 1);
    }

    Psyz_VideoSetVBlankHook(vblank);
    Psyz_VideoSetHCountHook(horizontal_query);
    Psyz_VideoSetCountHook(vertical_query);
    Psyz_VideoSetElapsedReplayHook(replay_elapsed);
    atexit(replay_at_exit);
    return 0;
}
