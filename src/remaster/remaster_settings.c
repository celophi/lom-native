#include "lom_native_remaster.h"
#include "../gpu/hd_textures.h"
#include "../gpu/hd_movie.h"

#include <stdio.h>

static char s_root[4096];
static int s_found;
static int s_hd_backgrounds = 1;
static int s_music;
static int s_effects;
static int s_hd_ui = 1;
static int s_hd_movies = 1;
static int s_text;

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

int lom_native_remaster_hd_supported(void)
{
    return lom_hd_textures_set && lom_hd_textures_upload && lom_hd_textures_draw && lom_hd_textures_release;
}

void lom_native_remaster_set_hd_backgrounds(int enabled)
{
    s_hd_backgrounds = enabled != 0;
}

int lom_native_remaster_hd_backgrounds(void)
{
    return s_hd_backgrounds;
}

void lom_native_remaster_set_music(int source)
{
    s_music = source >= 0 && source < LOM_REMASTER_MUSIC_COUNT ? source : LOM_REMASTER_MUSIC_PS1;
}

int lom_native_remaster_music(void)
{
    return s_music;
}

void lom_native_remaster_set_effects(int enabled)
{
    s_effects = enabled != 0;
}

int lom_native_remaster_effects(void)
{
    return s_effects;
}

int lom_native_remaster_hd_ui_supported(void)
{
    return lom_hd_images_set && lom_hd_images_upload && lom_hd_images_draw && lom_hd_images_release;
}

void lom_native_remaster_set_hd_ui(int enabled)
{
    s_hd_ui = enabled != 0;
}

int lom_native_remaster_hd_ui(void)
{
    return s_hd_ui;
}

int lom_native_remaster_hd_movies_supported(void)
{
    return lom_hd_movie_begin && lom_hd_movie_frame && lom_hd_movie_end;
}

void lom_native_remaster_set_hd_movies(int enabled)
{
    s_hd_movies = enabled != 0;
}

int lom_native_remaster_hd_movies(void)
{
    return s_hd_movies;
}

void lom_native_remaster_set_text(int language)
{
    s_text = language >= 0 && language < LOM_REMASTER_TEXT_COUNT ? language : LOM_REMASTER_TEXT_PS1;
}

int lom_native_remaster_text(void)
{
    return s_text;
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
    return s_text_supported ? s_text : LOM_REMASTER_TEXT_PS1;
}
