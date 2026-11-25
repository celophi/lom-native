#define _GNU_SOURCE
#include "lom_native_switches.h"
#include "lom_native_mod_toggles.h"
#include "lom_native_session.h"
#include "../../support/platform/lom_test_posix.h"

#include <ftw.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../support/lom_check.h"

#define PAD_BYTES 34
#define PAD_RECORD (1 + 2 * PAD_BYTES)
#define HEADER_BYTES 12

static unsigned char s_port0[PAD_BYTES], s_port1[PAD_BYTES];
static unsigned char* s_ports[2] = {s_port0, s_port1};

/**
 * @brief Have the session take a pad sample, as the controllers do each frame.
 */
static void sample(void)
{
    lom_native_session_pad_sample(s_ports);
}

/**
 * @brief A pad script that changes nothing, so the replay reads its own input.
 * @param vblank Ignored.
 * @param port_buffers Ignored.
 */
static void pad_script(unsigned long long vblank, unsigned char* port_buffers[2])
{
    (void)vblank;
    (void)port_buffers;
}

/**
 * @brief Remove a file or an empty folder, for nftw().
 * @param path The file or folder.
 * @param st Ignored.
 * @param flag What it is.
 * @param ftw Ignored.
 * @return What remove() or rmdir() returns.
 */
static int remove_entry(const char* path, const struct stat* st, int flag, struct FTW* ftw)
{
    (void)st;
    (void)ftw;
    return flag == FTW_DP ? rmdir(path) : remove(path);
}

/**
 * @brief Write a session file's header.
 * @param out Where to.
 * @param version The file's version.
 * @return How many bytes it wrote.
 */
static size_t put_header(unsigned char* out, uint32_t version)
{
    memcpy(out, "LOMSESS1", 8);
    memcpy(out + 8, &version, sizeof(version));
    return HEADER_BYTES;
}

/**
 * @brief Write a pad sample record, nothing held.
 * @param out Where to.
 * @return How many bytes it wrote.
 */
static size_t put_pad(unsigned char* out)
{
    memset(out, 0, PAD_RECORD);
    out[0] = 'P';
    return PAD_RECORD;
}

/**
 * @brief Write a version 4 Always run record.
 * @param out Where to.
 * @param value On or off.
 * @return How many bytes it wrote.
 */
static size_t put_always_run(unsigned char* out, unsigned char value)
{
    out[0] = 'A';
    out[1] = value;
    return 2;
}

/**
 * @brief Write a version 5 switch record.
 * @param out Where to.
 * @param number The switch's recorded number.
 * @param value On or off.
 * @return How many bytes it wrote.
 */
static size_t put_switch(unsigned char* out, unsigned char number, unsigned char value)
{
    out[0] = 'S';
    out[1] = number;
    out[2] = value;
    return 3;
}

/**
 * @brief Write a version 6 record of a mod turned on or off in F1.
 * @param out Where to.
 * @param index The mod's place among those with a switch in F1.
 * @param value On or off.
 * @return How many bytes it wrote.
 */
static size_t put_mod(unsigned char* out, unsigned char index, unsigned char value)
{
    out[0] = 'M';
    out[1] = index;
    out[2] = value;
    return 3;
}

/**
 * @brief Put every switch's setting back to off, forget mods' values, and
 *        leave one mod with a switch in F1, on.
 */
static void reset_switches(void)
{
    lom_native_switch_clear_mods();
    lom_native_mod_toggles_reset();
    lom_native_mod_toggles_add("test-mod", "Test mod", "");

    for (int i = 0; i < LOM_SWITCH_COUNT; i++)
    {
        lom_native_switch_set_setting((LomSwitch)i, 0);
    }
}

/**
 * @brief Write a file.
 * @param path The file.
 * @param data Its bytes.
 * @param size How many.
 * @return 0, or -1.
 */
static int write_file(const char* path, const unsigned char* data, size_t size)
{
    FILE* file = fopen(path, "wb");

    if (!file)
    {
        return -1;
    }

    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    return written == size ? 0 : -1;
}

/**
 * @brief Check a session's manifest lists the mods, none here, and every
 *        switch's value at the start.
 * @param folder The session's folder.
 */
static void check_manifest(const char* folder)
{
    char path[8192];
    char text[4096];
    snprintf(path, sizeof(path), "%s/manifest.json", folder);
    FILE* file = fopen(path, "r");
    CHECK(file != NULL);
    const size_t size = fread(text, 1, sizeof(text) - 1, file);
    fclose(file);
    text[size] = 0;
    CHECK_DETAIL(strstr(text, "\n  \"mods\": [],\n  \"refused_mods\": [],\n"
                              "  \"switches\": {\"always_run\": true, \"skip_battle_results\": false, \"no_natural_healing\": false, \"skip_movies\": false, "
                              "\"skip_battles\": false, \"japanese_balance\": false},\n  \"remaster\": \"") != NULL,
                 text);
}

/**
 * @brief Check recording a session saves each switch when it first differs from
 *        off and each time it changes, counting one a mod sets, and that the
 *        game follows the recorded value until the next pad sample.
 * @param root A scratch folder.
 * @return 0. A failed check stops the test.
 */
static int record(const char* root)
{
    char base[4096 + 16], cards[4096 + 16], inputs[8192];
    snprintf(base, sizeof(base), "%s/sessions", root);
    snprintf(cards, sizeof(cards), "%s/cards", root);
    CHECK(setenv("LOM_RECORD", "1", 1) == 0);
    CHECK(setenv("LOM_SESSION_DIR", base, 1) == 0);
    CHECK(setenv("LOM_CARD_ROOT", cards, 1) == 0);

    reset_switches();
    lom_native_switch_set_setting(LOM_SWITCH_ALWAYS_RUN, 1);
    CHECK(lom_native_session_record_start(NULL) == 0);
    CHECK(lom_native_session_folder() != NULL);
    snprintf(inputs, sizeof(inputs), "%s/inputs.bin", lom_native_session_folder());
    check_manifest(lom_native_session_folder());
    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && !lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    sample();

    lom_native_switch_set_setting(LOM_SWITCH_ALWAYS_RUN, 0);
    lom_native_switch_set_by_mod(LOM_SWITCH_SKIP_BATTLE_RESULTS, 1, "test-mod");
    lom_native_mod_toggles_set(0, 0);
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && !lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS) && lom_native_mod_toggles_on(0));
    sample();
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS) && !lom_native_mod_toggles_on(0));
    sample();
    lom_native_session_finish("test");

    FILE* file = fopen(inputs, "rb");
    CHECK(file != NULL);
    unsigned char data[1024];
    size_t size = fread(data, 1, sizeof(data), file);
    fclose(file);

    const unsigned char always_run = (unsigned char)lom_native_switch_record_number(LOM_SWITCH_ALWAYS_RUN);
    const unsigned char skip_results = (unsigned char)lom_native_switch_record_number(LOM_SWITCH_SKIP_BATTLE_RESULTS);
    uint32_t version;
    memcpy(&version, data + 8, sizeof(version));
    CHECK(memcmp(data, "LOMSESS1", 8) == 0 && version == 6);
    size_t at = HEADER_BYTES;
    CHECK(data[at] == 'S' && data[at + 1] == always_run && data[at + 2] == 1);
    at += 3;
    CHECK(data[at] == 'P');
    at += PAD_RECORD;
    CHECK(data[at] == 'P');
    at += PAD_RECORD;
    CHECK(data[at] == 'M' && data[at + 1] == 0 && data[at + 2] == 0);
    at += 3;
    CHECK(data[at] == 'S' && data[at + 1] == always_run && data[at + 2] == 0);
    at += 3;
    CHECK(data[at] == 'S' && data[at + 1] == skip_results && data[at + 2] == 1);
    at += 3;
    CHECK(data[at] == 'P');
    at += PAD_RECORD;
    CHECK(data[at] == 'P');
    at += PAD_RECORD;
    CHECK(at == size);
    return 0;
}

/**
 * @brief Check replaying a session puts each switch, and each mod turned on or
 *        off in F1, back as it was recorded, whatever the live settings are,
 *        then hands them back to the settings when the replay ends.
 * @param root A scratch folder.
 * @return 0. A failed check stops the test.
 */
static int replay(const char* root)
{
    char inputs[4096 + 16];
    snprintf(inputs, sizeof(inputs), "%s/inputs.bin", root);
    unsigned char data[1024];
    size_t size = put_header(data, 6);
    size += put_switch(data + size, (unsigned char)lom_native_switch_record_number(LOM_SWITCH_ALWAYS_RUN), 1);
    size += put_pad(data + size);
    size += put_pad(data + size);
    size += put_mod(data + size, 0, 0);
    size += put_switch(data + size, (unsigned char)lom_native_switch_record_number(LOM_SWITCH_ALWAYS_RUN), 0);
    size += put_switch(data + size, (unsigned char)lom_native_switch_record_number(LOM_SWITCH_SKIP_BATTLE_RESULTS), 1);
    size += put_pad(data + size);
    CHECK(write_file(inputs, data, size) == 0);

    reset_switches();
    lom_native_switch_set_setting(LOM_SWITCH_ALWAYS_RUN, 1);
    lom_native_switch_set_setting(LOM_SWITCH_SKIP_BATTLE_RESULTS, 1);
    lom_native_session_set_pad_script(pad_script);
    CHECK(lom_native_session_replay_start(inputs) == 0);
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && !lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && !lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && !lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS) && lom_native_mod_toggles_on(0));
    sample();
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS) && !lom_native_mod_toggles_on(0));
    CHECK(lom_native_mod_toggles_wanted(0));
    CHECK(lom_native_session_replay_state() == 1);

    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) && lom_native_switch(LOM_SWITCH_SKIP_BATTLE_RESULTS));
    return 0;
}

/**
 * @brief Check an older recording still replays: version 4's Always run records
 *        hold Always run, and version 3, which has none, leaves it off until the
 *        replay ends.
 * @param root A scratch folder.
 * @param version 3 or 4.
 * @return 0. A failed check stops the test.
 */
static int replay_older(const char* root, uint32_t version)
{
    char inputs[4096 + 16];
    snprintf(inputs, sizeof(inputs), "%s/inputs.bin", root);
    unsigned char data[1024];
    size_t size = put_header(data, version);
    const int has_always_run = version == 4;

    if (has_always_run)
    {
        size += put_always_run(data + size, 1);
    }

    size += put_pad(data + size);
    size += put_pad(data + size);

    if (has_always_run)
    {
        size += put_always_run(data + size, 0);
    }

    size += put_pad(data + size);
    CHECK(write_file(inputs, data, size) == 0);

    reset_switches();
    lom_native_switch_set_setting(LOM_SWITCH_ALWAYS_RUN, 1);
    lom_native_session_set_pad_script(pad_script);
    CHECK(lom_native_session_replay_start(inputs) == 0);
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN));
    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) == has_always_run);
    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN) == has_always_run);
    sample();
    CHECK(!lom_native_switch(LOM_SWITCH_ALWAYS_RUN));
    CHECK(lom_native_session_replay_state() == 1);

    sample();
    CHECK(lom_native_switch(LOM_SWITCH_ALWAYS_RUN));
    return 0;
}

/**
 * @brief Check a recording that names a switch this build doesn't have, as one
 *        from a newer build would, stops the replay as drifted.
 * @param root A scratch folder.
 * @return 0. A failed check stops the test.
 */
static int replay_unknown(const char* root)
{
    char inputs[4096 + 16];
    snprintf(inputs, sizeof(inputs), "%s/inputs.bin", root);
    unsigned char data[1024];
    size_t size = put_header(data, 5);
    size += put_switch(data + size, 200, 1);
    size += put_pad(data + size);
    CHECK(write_file(inputs, data, size) == 0);

    reset_switches();
    lom_native_session_set_pad_script(pad_script);
    CHECK(lom_native_session_replay_start(inputs) == 0);
    sample();
    CHECK(lom_native_session_replay_state() == 2);
    return 0;
}

int main(int argc, char** argv)
{
    char root[4096];
    int result;

    CHECK(argc == 2);
    CHECK(lom_test_make_temp_folder(root, sizeof(root), "lom-session-switches-test") != NULL);

    if (strcmp(argv[1], "record") == 0)
    {
        result = record(root);
    }
    else if (strcmp(argv[1], "replay") == 0)
    {
        result = replay(root);
    }
    else if (strcmp(argv[1], "v4") == 0)
    {
        result = replay_older(root, 4);
    }
    else if (strcmp(argv[1], "old") == 0)
    {
        result = replay_older(root, 3);
    }
    else if (strcmp(argv[1], "unknown") == 0)
    {
        result = replay_unknown(root);
    }
    else
    {
        fprintf(stderr, "unknown case %s\n", argv[1]);
        result = 1;
    }

    nftw(root, remove_entry, 16, FTW_DEPTH | FTW_PHYS);

    if (result == 0)
    {
        printf("session switches %s: ok\n", argv[1]);
    }

    return result;
}
