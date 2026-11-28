#include "lom_native_cd.h"
#include "lom_native_env.h"
#include "lom_native_ps1_memory.h"
#include "lom_native_remaster.h"
#include "main/cdrom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../support/behavior/specification.h"
#include "../../support/cd/disc_files_provider_fake.h"
#include "../../support/gpu/hd_renderer_provider_fake.h"
#include "../../support/lom_check.h"
#include "../../support/platform/lom_test_posix.h"
#include "../../support/platform/remaster_files_provider_fake.h"
#include "../../support/remaster/remaster_psb_builder.h"

#define REMASTER_FOLDER "/steam/steamapps/common/Legend of Mana"
#define DATA REMASTER_FOLDER "/windata/"
#define DISC "/disc"
#define MENU_SCENE 0x060E
#define MENU_SCENE_FILE DISC "/ANA/MAPINFO/MID_MENU.IMG"
#define HEADER 40
#define STRINGS_START 2
#define STRINGS_END 3
#define AFTER_STRINGS 4
#define NEWLINE "\x01"

static RemasterFilesProviderFake s_files_fake;
static DiscFilesProviderFake s_disc_fake;
static HdRendererProviderFake s_renderer_fake;

static const char* s_ps1_strings[2] = {"Hello", "Bye"};
static const char* s_remaster_strings[3] = {"Hallo", "Auf Wiedersehen", NULL};
static const char* s_remaster_table = "mid_menu";
static int s_text = LOM_REMASTER_TEXT_GERMAN;
static unsigned char s_scene[256];
static size_t s_scene_size;
static unsigned char* s_loaded;
static int s_loaded_size;

/**
 * @brief Put in a fake of each provider a scene's text uses, with the happy path's defaults: the Remaster in Steam's
 *        library with its German text for the menu's scene, and the disc with the scene.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    disc_files_provider_fake_start(&s_disc_fake);
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Read a little-endian word.
 * @param at Where it is.
 * @return The word.
 */
static uint32_t word_at(const unsigned char* at)
{
    return (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24;
}

/**
 * @brief Write a little-endian word.
 * @param at Where it goes.
 * @param value The word.
 */
static void put_word(unsigned char* at, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
    {
        at[i] = (unsigned char)(value >> (8 * i));
    }
}

/**
 * @brief Give the disc fake the menu's scene file, laid out as the PS1's: a header of words, its strings' table (their
 *        offsets, then each string), and four bytes after the strings that the header points at.
 */
static void the_disc_has_the_scene(void)
{
    size_t at = HEADER + 4;
    memset(s_scene, 0, sizeof(s_scene));

    for (int i = 0; i < 2; ++i)
    {
        s_scene[HEADER + i * 2] = (unsigned char)(at - HEADER);
        memcpy(s_scene + at, s_ps1_strings[i], strlen(s_ps1_strings[i]) + 1);
        at += strlen(s_ps1_strings[i]) + 1;
    }

    at = (at + 3) & ~(size_t)3;
    put_word(s_scene + STRINGS_START * 4, HEADER);
    put_word(s_scene + STRINGS_END * 4, (uint32_t)at);
    put_word(s_scene + AFTER_STRINGS * 4, (uint32_t)at);
    memcpy(s_scene + at, "TAIL", 4);
    s_scene_size = at + 4;
    disc_files_provider_fake_add(&s_disc_fake, MENU_SCENE_FILE, s_scene, s_scene_size);
}

/**
 * @brief Give the fake the Remaster's German text, with a table of strings for the menu's scene.
 */
static void the_remaster_has_its_german_text(void)
{
    static RemasterPsbBuilder s_builder;
    static const char* const keys[3] = {"0", "1", "2"};
    size_t size = 0;
    remaster_psb_start(&s_builder);
    RemasterPsbNode* root = remaster_psb_object(&s_builder);
    RemasterPsbNode* table = remaster_psb_add(root, s_remaster_table, remaster_psb_object(&s_builder));

    for (int i = 0; i < 3 && s_remaster_strings[i]; ++i)
    {
        remaster_psb_add(table, keys[i], remaster_psb_text(&s_builder, s_remaster_strings[i]));
    }

    unsigned char* file = remaster_psb_build(&s_builder, root, &size);
    remaster_files_provider_fake_add(&s_files_fake, DATA "text/DE_DE.psb.m", file, size);
}

/**
 * @brief Start the game with the launcher's settings and the disc, then have the game read the menu's scene into its
 *        memory, as it does when the menu loads.
 */
static void when_the_game_reads_a_scene(void)
{
    char text[8];
    the_disc_has_the_scene();
    the_remaster_has_its_german_text();
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_cd_set_disc_root(DISC);
    lom_native_remaster_initialize();
    s_loaded = lom_native_ram + 0x80000;
    s_loaded_size = cdrom_queue_read(MENU_SCENE, s_loaded);
}

/**
 * @brief The string a scene in memory has at an index, from its strings' table.
 * @param index Which.
 * @return The string.
 */
static const char* string_at(int index)
{
    const uint32_t strings = word_at(s_loaded + STRINGS_START * 4);
    const unsigned offset = s_loaded[strings + index * 2] | s_loaded[strings + index * 2 + 1] << 8;
    return (const char*)s_loaded + strings + offset;
}

/**
 * @brief Given the defaults, German text; when the game reads the menu's scene; then its strings are the Remaster's
 *        German, and what came after them is still there, moved along with what the header says about it.
 */
SPECIFICATION(puts_in_the_remasters_text, "Scene text should be the Remaster's German when the text is German")
{
    when_the_game_reads_a_scene();

    CHECK(strcmp(string_at(0), "Hallo") == 0);
    CHECK(strcmp(string_at(1), "Auf Wiedersehen") == 0);
    CHECK(s_loaded_size == (int)s_scene_size + 12);
    CHECK(word_at(s_loaded + AFTER_STRINGS * 4) == word_at(s_loaded + STRINGS_END * 4));
    CHECK(memcmp(s_loaded + word_at(s_loaded + AFTER_STRINGS * 4), "TAIL", 4) == 0);
}

/**
 * @brief Given the PS1's string starts on a new line; when the game reads the scene; then the Remaster's string starts
 *        on a new line too.
 */
SPECIFICATION(keeps_the_ps1s_leading_newlines, "Scene text should keep the PS1's new line before a string")
{
    s_ps1_strings[0] = NEWLINE "Hello";

    when_the_game_reads_a_scene();

    CHECK(strcmp(string_at(0), NEWLINE "Hallo") == 0);
}

/**
 * @brief Given one of the Remaster's strings has a letter the PS1 can't show; when the game reads the scene; then that
 *        string stays the PS1's and the other is the Remaster's.
 */
SPECIFICATION(keeps_strings_the_ps1_cant_show, "Scene text should keep the PS1's string where the Remaster's can't be shown")
{
    s_remaster_strings[0] = "Hallo \xF0\x9F\x98\x80";

    when_the_game_reads_a_scene();

    CHECK(strcmp(string_at(0), "Hello") == 0);
    CHECK(strcmp(string_at(1), "Auf Wiedersehen") == 0);
}

/**
 * @brief Given the Remaster has more strings for the scene than the PS1, so the two don't line up; when the game reads
 *        the scene; then it's left as the disc has it.
 */
SPECIFICATION(leaves_scenes_that_dont_line_up, "Scene text should leave the PS1's scene when the Remaster's strings don't line up with it")
{
    s_remaster_strings[2] = "Noch einer";

    when_the_game_reads_a_scene();

    CHECK(s_loaded_size == (int)s_scene_size);
    CHECK(memcmp(s_loaded, s_scene, s_scene_size) == 0);
}

/**
 * @brief Given the Remaster has no text for the scene; when the game reads it; then it's left as the disc has it.
 */
SPECIFICATION(leaves_scenes_without_text, "Scene text should leave the PS1's scene when the Remaster has no text for it")
{
    s_remaster_table = "another_scene";

    when_the_game_reads_a_scene();

    CHECK(memcmp(s_loaded, s_scene, s_scene_size) == 0);
}

/**
 * @brief Given the PS1's text, the Remaster's turned off; when the game reads the scene; then it's left as the disc
 *        has it.
 */
SPECIFICATION(leaves_scenes_with_the_ps1_text, "Scene text should leave the PS1's scene when the text is the PS1's")
{
    s_text = LOM_REMASTER_TEXT_PS1;

    when_the_game_reads_a_scene();

    CHECK(s_loaded_size == (int)s_scene_size);
    CHECK(memcmp(s_loaded, s_scene, s_scene_size) == 0);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
