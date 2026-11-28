#include "lom_native_env.h"
#include "lom_native_remaster.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../support/behavior/specification.h"
#include "../../support/gpu/hd_renderer_provider_fake.h"
#include "../../support/lom_check.h"
#include "../../support/platform/lom_test_posix.h"
#include "../../support/platform/remaster_files_provider_fake.h"

#define CHOSEN_FOLDER "/games/Legend of Mana"
#define HOME "/home/player"

#if defined(_WIN32)
#define STEAM_GAME "C:/Program Files (x86)/Steam/steamapps/common/Legend of Mana"
#elif defined(__APPLE__)
#define STEAM_GAME HOME "/Library/Application Support/Steam/steamapps/common/Legend of Mana"
#else
#define STEAM_GAME HOME "/.local/share/Steam/steamapps/common/Legend of Mana"
#endif

static RemasterFilesProviderFake s_files_fake;
static HdRendererProviderFake s_renderer_fake;

static const char* s_remaster_folder_found = STEAM_GAME "/windata/hdmap";
static const char* s_chosen_folder = NULL;
static const char* s_hd_backgrounds = NULL;
static const char* s_hd_screens = NULL;
static const char* s_music = NULL;
static const char* s_text = NULL;

/**
 * @brief Put in a fake of each provider starting the game uses, with the happy path's defaults: the Remaster
 *        installed in Steam's library, nothing chosen in the launcher, and a renderer that can draw everything HD.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Set an environment variable the launcher passes, or leave it out.
 * @param name The variable.
 * @param value Its value, or NULL when the launcher doesn't pass it.
 */
static void launcher_passes(const char* name, const char* value)
{
    if (value)
    {
        setenv(name, value, 1);
    }
    else
    {
        unsetenv(name);
    }
}

/**
 * @brief Start the game the way the launcher does: with its settings in the environment, and the player's home
 *        folder.
 */
static void when_the_game_starts(void)
{
    remaster_files_provider_fake_add_folder(&s_files_fake, s_remaster_folder_found);
    setenv("HOME", HOME, 1);
    launcher_passes(LOM_ENV_REMASTER_ROOT, s_chosen_folder);
    launcher_passes(LOM_ENV_REMASTER_HD, s_hd_backgrounds);
    launcher_passes(LOM_ENV_REMASTER_UI, s_hd_screens);
    launcher_passes(LOM_ENV_REMASTER_MUSIC, s_music);
    launcher_passes(LOM_ENV_REMASTER_TEXT, s_text);
    launcher_passes(LOM_ENV_REMASTER_MOVIES, NULL);
    lom_native_remaster_initialize();
}

/**
 * @brief Given the defaults, the Remaster in Steam's library; when the game starts; then it finds the Remaster's data
 *        there and uses all of it: HD backgrounds, HD screens, the original soundtrack and the English text.
 */
SPECIFICATION(finds_the_remaster_in_steam, "Starting the game should find the Remaster in Steam's library and use all of it")
{
    when_the_game_starts();

    CHECK(lom_native_remaster_found());
    CHECK(strcmp(lom_native_remaster_root(), STEAM_GAME "/windata") == 0);
    CHECK(lom_native_remaster_hd_backgrounds());
    CHECK(lom_native_remaster_uses(LOM_REMASTER_SWITCH_TITLE));
    CHECK(lom_native_remaster_music() == LOM_REMASTER_MUSIC_ORIGINAL);
    CHECK(lom_native_remaster_text_in_use() == LOM_REMASTER_TEXT_ENGLISH);
}

/**
 * @brief Given the player chose a folder in the launcher, with the Remaster's data in its windata folder; when the
 *        game starts; then it uses that folder's data rather than Steam's.
 */
SPECIFICATION(uses_the_chosen_folder, "Starting the game should use the Remaster folder the player chose over Steam's")
{
    s_chosen_folder = CHOSEN_FOLDER;
    remaster_files_provider_fake_add_folder(&s_files_fake, CHOSEN_FOLDER "/windata/hdmap");

    when_the_game_starts();

    CHECK(lom_native_remaster_found());
    CHECK(strcmp(lom_native_remaster_root(), CHOSEN_FOLDER "/windata") == 0);
}

/**
 * @brief Given the player chose the Remaster's data folder itself, which has no windata folder in it; when the game
 *        starts; then it uses that folder.
 */
SPECIFICATION(uses_a_chosen_data_folder, "Starting the game should use the chosen folder itself when it's the Remaster's data folder")
{
    s_chosen_folder = CHOSEN_FOLDER;
    remaster_files_provider_fake_add_folder(&s_files_fake, CHOSEN_FOLDER "/hdmap");

    when_the_game_starts();

    CHECK(lom_native_remaster_found());
    CHECK(strcmp(lom_native_remaster_root(), CHOSEN_FOLDER) == 0);
}

/**
 * @brief Given the player chose a folder without the Remaster in it; when the game starts; then it doesn't look in
 *        Steam's library instead, and nothing of the Remaster is used.
 */
SPECIFICATION(trusts_the_chosen_folder, "Starting the game should not use Steam's Remaster when the chosen folder doesn't have it")
{
    s_chosen_folder = CHOSEN_FOLDER;

    when_the_game_starts();

    CHECK(!lom_native_remaster_found());
}

/**
 * @brief Given no Remaster anywhere; when the game starts; then nothing of the Remaster is used: the PS1's
 *        backgrounds, screens, music and text.
 */
SPECIFICATION(plays_the_ps1_without_the_remaster, "Starting the game should use only the PS1's art, music and text when the Remaster isn't installed")
{
    s_remaster_folder_found = "/somewhere/else";

    when_the_game_starts();

    CHECK(!lom_native_remaster_found());
    CHECK(!lom_native_remaster_hd_backgrounds());
    CHECK(!lom_native_remaster_uses(LOM_REMASTER_SWITCH_TITLE));
    CHECK(lom_native_remaster_music() == LOM_REMASTER_MUSIC_PS1);
    CHECK(lom_native_remaster_text_in_use() == LOM_REMASTER_TEXT_PS1);
}

/**
 * @brief Given the launcher says HD backgrounds off, HD screens off, the arranged soundtrack and French text; when the
 *        game starts; then each part of the Remaster is used as it says.
 */
SPECIFICATION(uses_the_launchers_settings, "Starting the game should use the parts of the Remaster the launcher's settings turn on")
{
    s_hd_backgrounds = "0";
    s_hd_screens = "0";
    s_music = "2";
    s_text = "3";

    when_the_game_starts();

    CHECK(!lom_native_remaster_hd_backgrounds());
    CHECK(!lom_native_remaster_uses(LOM_REMASTER_SWITCH_TITLE));
    CHECK(!lom_native_remaster_uses(LOM_REMASTER_SWITCH_MENU));
    CHECK(lom_native_remaster_music() == LOM_REMASTER_MUSIC_ARRANGED);
    CHECK(lom_native_remaster_text_in_use() == LOM_REMASTER_TEXT_FRENCH);
}

/**
 * @brief Given a renderer that can't draw the Remaster's HD art; when the game starts; then it says neither the HD
 *        screens nor the HD backgrounds can be shown, while the Remaster's text still can.
 */
SPECIFICATION(says_hd_art_cant_be_shown, "Starting the game should say HD screens and backgrounds can't be shown when the renderer can't draw HD art")
{
    hd_renderer_provider_fake_without_hd(&s_renderer_fake);

    when_the_game_starts();

    CHECK(!lom_native_remaster_hd_ui_supported());
    CHECK(!lom_native_remaster_hd_supported());
    CHECK(lom_native_remaster_text_in_use() == LOM_REMASTER_TEXT_ENGLISH);
}

/**
 * @brief Given the defaults, a renderer that can draw HD art; when the game starts; then it says the HD screens and
 *        the HD backgrounds can both be shown.
 */
SPECIFICATION(says_hd_art_can_be_shown, "Starting the game should say HD screens and backgrounds can be shown when the renderer can draw HD art")
{
    when_the_game_starts();

    CHECK(lom_native_remaster_hd_ui_supported());
    CHECK(lom_native_remaster_hd_supported());
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
