#include "lom_native_cd.h"
#include "lom_native_env.h"
#include "lom_native_remaster.h"
#include "lom_native_text.h"

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
#define WORKSHOP_NAMES 0x38
#define TYPES 36
#define MATERIALS 64

typedef struct
{
    const char* key;
    const char* words;
} RemasterWord;

static RemasterFilesProviderFake s_files_fake;
static DiscFilesProviderFake s_disc_fake;
static HdRendererProviderFake s_renderer_fake;

static const char* s_disc_type = "Knife";
static const char* s_disc_material = "Menos ";
static RemasterWord s_english_named[] = {{"wpn_10", "Earthsplitter"}, {"arm_05", "Pegasus Helm"}};
static RemasterWord s_german_words[] = {{"wpn_00:", "Messer"}, {"mtr_00:", "Menos"}};
static RemasterWord s_german_named[] = {{"wpn_10", "Erdspalter"}, {"arm_05", "Pegasushelm"}};
static RemasterWord s_french_words[] = {{"wpn_00:", "Couteau"}, {"mtr_00:", "Menos"}};
static RemasterWord s_spanish_words[] = {{"wpn_00:", "Daga"}, {"mtr_00:", "Menos"}};
static int s_text = LOM_REMASTER_TEXT_GERMAN;
static const char* s_saved_name = "Menos Knife";
static unsigned char s_workshop[0x120];
static const unsigned char* s_name;

/**
 * @brief Put in a fake of each provider the items' names use, with the happy path's defaults: the Remaster in Steam's
 *        library with its English, German, French and Spanish text, and the disc with the workshop's names of
 *        materials and types.
 */
static void setup(void)
{
    remaster_files_provider_fake_start(&s_files_fake);
    remaster_files_provider_fake_add_folder(&s_files_fake, REMASTER_FOLDER "/windata/hdmap");
    disc_files_provider_fake_start(&s_disc_fake);
    hd_renderer_provider_fake_start(&s_renderer_fake);
}

/**
 * @brief Give the fake one of the Remaster's text files: the words for made items' materials and types in its "name"
 *        table, and the names of the weapons and armour the game names itself in theirs.
 * @param language The file's language, as the Remaster names it.
 * @param words The material and type words.
 * @param word_count How many.
 * @param named The weapons' and armour's names.
 * @param named_count How many.
 */
static void the_remaster_has_text(const char* language, const RemasterWord* words, int word_count, const RemasterWord* named, int named_count)
{
    static RemasterPsbBuilder s_builder;
    char path[256];
    size_t size = 0;
    remaster_psb_start(&s_builder);
    RemasterPsbNode* root = remaster_psb_object(&s_builder);
    RemasterPsbNode* name = remaster_psb_add(root, "name", remaster_psb_object(&s_builder));
    RemasterPsbNode* weapons = remaster_psb_add(root, "wpn", remaster_psb_object(&s_builder));
    RemasterPsbNode* armour = remaster_psb_add(root, "arm", remaster_psb_object(&s_builder));

    for (int i = 0; i < word_count; ++i)
    {
        remaster_psb_add(name, words[i].key, remaster_psb_text(&s_builder, words[i].words));
    }

    for (int i = 0; i < named_count; ++i)
    {
        remaster_psb_add(named[i].key[0] == 'w' ? weapons : armour, named[i].key, remaster_psb_text(&s_builder, named[i].words));
    }

    unsigned char* file = remaster_psb_build(&s_builder, root, &size);
    snprintf(path, sizeof(path), DATA "text/%s.psb.m", language);
    remaster_files_provider_fake_add(&s_files_fake, path, file, size);
}

/**
 * @brief Write a little-endian number.
 * @param at Where.
 * @param value The number.
 * @param bytes How many bytes it takes.
 */
static void put(unsigned char* at, unsigned value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
    {
        at[i] = (unsigned char)(value >> (8 * i));
    }
}

/**
 * @brief Give the disc fake the workshop's scene file, laid out as the PS1's: one record, its table of the types'
 *        names then the materials', the first type and the first material named, the rest pointing at nothing they
 *        have words for.
 */
static void the_disc_has_the_workshop(void)
{
    const unsigned record = 0x34, names = 0x100;
    memset(s_workshop, 0, sizeof(s_workshop));
    put(s_workshop + 0x10, 0x30, 4);
    put(s_workshop + 0x14, names, 4);
    put(s_workshop + 0x30, 4, 4);
    put(s_workshop + record, 8, 2);
    put(s_workshop + WORKSHOP_NAMES, names - WORKSHOP_NAMES, 2);
    const unsigned material = names + (unsigned)strlen(s_disc_type) + 1;
    put(s_workshop + WORKSHOP_NAMES + TYPES * 2, material - WORKSHOP_NAMES, 2);
    memcpy(s_workshop + names, s_disc_type, strlen(s_disc_type));
    memcpy(s_workshop + material, s_disc_material, strlen(s_disc_material));
    disc_files_provider_fake_add(&s_disc_fake, DISC "/ANA/MAPINFO/MHM_ALC0.IMG", s_workshop, material + strlen(s_disc_material) + 1);
}

/**
 * @brief Start the game with the launcher's settings and the disc, then have the game draw the name a saved item has,
 *        which the port looks up first, as every screen that shows an item's name does.
 */
static void when_the_game_shows_an_items_name(void)
{
    char text[8];
    the_remaster_has_text("EN_US", NULL, 0, s_english_named, 2);
    the_remaster_has_text("DE_DE", s_german_words, 2, s_german_named, 2);
    the_remaster_has_text("FR_FR", s_french_words, 2, NULL, 0);
    the_remaster_has_text("ES_ES", s_spanish_words, 2, NULL, 0);
    the_disc_has_the_workshop();
    snprintf(text, sizeof(text), "%d", s_text);
    setenv(LOM_ENV_REMASTER_ROOT, REMASTER_FOLDER, 1);
    setenv(LOM_ENV_REMASTER_TEXT, text, 1);
    lom_native_cd_set_disc_root(DISC);
    lom_native_remaster_initialize();
    s_name = lom_native_text_item_name((const unsigned char*)s_saved_name);
}

/**
 * @brief Given the defaults, German text and a knife made of Menos; when the game shows its name; then it's the
 *        Remaster's German, material and type joined the German way.
 */
SPECIFICATION(names_made_items_in_german, "Item name should join a made item's material and type the German way when the text is German")
{
    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Menos-Messer") == 0);
}

/**
 * @brief Given French text; when the game shows a Menos knife's name; then it's the type, then "de" and the material.
 */
SPECIFICATION(names_made_items_in_french, "Item name should put a made item's type before its material when the text is French")
{
    s_text = LOM_REMASTER_TEXT_FRENCH;

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Couteau de Menos") == 0);
}

/**
 * @brief Given French text and a material starting with a vowel; when the game shows the item's name; then it's
 *        joined with "d'".
 */
SPECIFICATION(elides_before_a_vowel_in_french, "Item name should join with d' before a material starting with a vowel when the text is French")
{
    s_text = LOM_REMASTER_TEXT_FRENCH;
    s_french_words[1].words = "Argent";
    s_disc_material = "Silver ";
    s_saved_name = "Silver Knife";

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Couteau d'Argent") == 0);
}

/**
 * @brief Given Spanish text; when the game shows a Menos knife's name; then it's the type, then the material.
 */
SPECIFICATION(names_made_items_in_spanish, "Item name should put a made item's type before its material when the text is Spanish")
{
    s_text = LOM_REMASTER_TEXT_SPANISH;

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Daga Menos") == 0);
}

/**
 * @brief Given a weapon the game names itself, saved with its English name; when the game shows it; then it's the
 *        Remaster's German name for it.
 */
SPECIFICATION(names_named_weapons, "Item name should be the Remaster's name for a weapon the game names itself")
{
    s_saved_name = "Earthsplitter";

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Erdspalter") == 0);
}

/**
 * @brief Given armour the game names itself, saved with its spaces taken out to fit; when the game shows it; then it's
 *        still the Remaster's German name for it.
 */
SPECIFICATION(names_squeezed_names, "Item name should find a named item saved without its spaces")
{
    s_saved_name = "PegasusHelm";

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Pegasushelm") == 0);
}

/**
 * @brief Given a name the game didn't give, one the player chose; when the game shows it; then it's shown as saved.
 */
SPECIFICATION(keeps_names_the_game_didnt_give, "Item name should keep a name the game didn't give")
{
    s_saved_name = "Mr Pointy";

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Mr Pointy") == 0);
}

/**
 * @brief Given the PS1's text, the Remaster's text turned off; when the game shows a Menos knife's name; then it's shown
 *        as saved.
 */
SPECIFICATION(keeps_the_ps1_names_with_the_ps1_text, "Item name should keep the saved name when the text is the PS1's")
{
    s_text = LOM_REMASTER_TEXT_PS1;

    when_the_game_shows_an_items_name();

    CHECK(strcmp((const char*)s_name, "Menos Knife") == 0);
}

int main(int argc, char** argv)
{
    return lom_specifications_main(argc, argv, setup);
}
