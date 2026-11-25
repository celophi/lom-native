#include "lom_native_remaster.h"
#include "lom_native_text.h"
#include "remaster_text.h"
#include "lom_native_cd.h"
#include "common/memory_map.h"
#include "common/saved_game.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char* const remaster_text_languages[REMASTER_TEXT_LANGUAGES] = {NULL, "EN_US", "DE_DE", "FR_FR", "ES_ES", "JA_JP", "KO_KR", "ZH_CN", "ZH_TW"};
const char* const remaster_text_fonts[REMASTER_TEXT_LANGUAGES] = {NULL, NULL, NULL, NULL, NULL, "jp", "kr", "cn", "tw"};

static unsigned char* s_data;
LomPsb remaster_text_psb;
int remaster_text_language = -1;

int remaster_text_open(int language)
{
    if (language != remaster_text_language)
    {
        char path[PATH_SIZE];
        size_t size = 0;

        if (s_data)
        {
            lom_psb_close(&remaster_text_psb);
            free(s_data);
            s_data = NULL;
        }

        remaster_text_language = language;

        if (!remaster_text_font(language))
        {
            remaster_text_close_font();
        }

        if (language <= 0 || language >= (int)(sizeof(remaster_text_languages) / sizeof(remaster_text_languages[0])))
        {
            return 0;
        }

        snprintf(path, sizeof(path), "%s/text/%s.psb.m", lom_native_remaster_root(), remaster_text_languages[language]);
        s_data = lom_remaster_load(path, &size);

        if (s_data && lom_psb_open(&remaster_text_psb, s_data, size) != 0)
        {
            free(s_data);
            s_data = NULL;
        }
    }

    return s_data != NULL;
}

int remaster_text_active(void)
{
    return s_data != NULL;
}
