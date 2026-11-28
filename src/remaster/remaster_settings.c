#include "lom_native_remaster.h"
#include "../gpu/hd_textures.h"
#include "../gpu/hd_movie.h"

#include <stdio.h>

static char s_root[4096];
static int s_found;
static int s_switches[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1};
static int s_text_language = LOM_REMASTER_TEXT_ENGLISH;

_Static_assert(sizeof(s_switches) / sizeof(s_switches[0]) == LOM_REMASTER_SWITCH_COUNT, "a starting value for each switch");

static const int s_picture_switches[] = {LOM_REMASTER_SWITCH_FACES,      LOM_REMASTER_SWITCH_ENCYCLOPEDIA, LOM_REMASTER_SWITCH_TITLE,
                                         LOM_REMASTER_SWITCH_NAME_ENTRY, LOM_REMASTER_SWITCH_WORLD_SELECT, LOM_REMASTER_SWITCH_WORLD_MAP,
                                         LOM_REMASTER_SWITCH_MENU};

static int s_text_supported = 1;

void lom_remaster_set_found(const char* root)
{
    s_found = root != NULL;
    snprintf(s_root, sizeof(s_root), "%s", root ? root : "");
}

int lom_native_remaster_found(void)
{
    return s_found;
}

const char* lom_native_remaster_root(void)
{
    return s_root;
}

void lom_native_remaster_set_switch(int which, int on)
{
    if (which >= 0 && which < LOM_REMASTER_SWITCH_COUNT)
    {
        s_switches[which] = on != 0;
    }
}

int lom_native_remaster_switch(int which)
{
    return which >= 0 && which < LOM_REMASTER_SWITCH_COUNT ? s_switches[which] : 0;
}

int lom_native_remaster_uses(int which)
{
    if (!s_found || which <= LOM_REMASTER_SWITCH_ALL || which >= LOM_REMASTER_SWITCH_COUNT || !s_switches[LOM_REMASTER_SWITCH_ALL] || !s_switches[which])
    {
        return 0;
    }

    return which != LOM_REMASTER_SWITCH_ARRANGED || s_switches[LOM_REMASTER_SWITCH_MUSIC];
}

int lom_native_remaster_hd_supported(void)
{
    return lom_hd_textures_set && lom_hd_textures_upload && lom_hd_textures_draw && lom_hd_textures_release;
}

void lom_native_remaster_set_hd_backgrounds(int enabled)
{
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_BACKGROUNDS, enabled);
}

int lom_native_remaster_hd_backgrounds(void)
{
    return lom_native_remaster_uses(LOM_REMASTER_SWITCH_BACKGROUNDS);
}

void lom_native_remaster_set_music(int source)
{
    const int valid = source > LOM_REMASTER_MUSIC_PS1 && source < LOM_REMASTER_MUSIC_COUNT;
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_MUSIC, valid);

    if (valid)
    {
        lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_ARRANGED, source == LOM_REMASTER_MUSIC_ARRANGED);
    }
}

int lom_native_remaster_music(void)
{
    if (!lom_native_remaster_uses(LOM_REMASTER_SWITCH_MUSIC))
    {
        return LOM_REMASTER_MUSIC_PS1;
    }

    return lom_native_remaster_uses(LOM_REMASTER_SWITCH_ARRANGED) ? LOM_REMASTER_MUSIC_ARRANGED : LOM_REMASTER_MUSIC_ORIGINAL;
}

void lom_native_remaster_set_effects(int enabled)
{
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_EFFECTS, enabled);
}

int lom_native_remaster_effects(void)
{
    return lom_native_remaster_uses(LOM_REMASTER_SWITCH_EFFECTS);
}

int lom_native_remaster_hd_ui_supported(void)
{
    return lom_hd_images_set && lom_hd_images_upload && lom_hd_images_draw && lom_hd_images_release;
}

void lom_native_remaster_set_hd_ui(int enabled)
{
    for (unsigned i = 0; i < sizeof(s_picture_switches) / sizeof(s_picture_switches[0]); ++i)
    {
        lom_native_remaster_set_switch(s_picture_switches[i], enabled);
    }
}

int lom_native_remaster_hd_movies_supported(void)
{
    return lom_hd_movie_begin && lom_hd_movie_frame && lom_hd_movie_end;
}

void lom_native_remaster_set_hd_movies(int enabled)
{
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_MOVIES, enabled);
}

int lom_native_remaster_hd_movies(void)
{
    return lom_native_remaster_uses(LOM_REMASTER_SWITCH_MOVIES);
}

void lom_native_remaster_set_text(int language)
{
    const int valid = language > LOM_REMASTER_TEXT_PS1 && language < LOM_REMASTER_TEXT_COUNT;
    lom_native_remaster_set_switch(LOM_REMASTER_SWITCH_TEXT, valid);

    if (valid)
    {
        s_text_language = language;
    }
}

int lom_native_remaster_text(void)
{
    return lom_native_remaster_uses(LOM_REMASTER_SWITCH_TEXT) ? s_text_language : LOM_REMASTER_TEXT_PS1;
}

void lom_native_remaster_set_text_language(int language)
{
    if (language > LOM_REMASTER_TEXT_PS1 && language < LOM_REMASTER_TEXT_COUNT)
    {
        s_text_language = language;
    }
}

int lom_native_remaster_text_language(void)
{
    return s_text_language;
}

void lom_native_remaster_set_text_supported(int supported)
{
    s_text_supported = supported != 0;
}

int lom_native_remaster_text_supported(void)
{
    return s_text_supported;
}

int lom_native_remaster_text_in_use(void)
{
    return s_text_supported ? lom_native_remaster_text() : LOM_REMASTER_TEXT_PS1;
}
