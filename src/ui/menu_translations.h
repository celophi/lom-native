#ifndef LOM_UI_MENU_TRANSLATIONS_H
#define LOM_UI_MENU_TRANSLATIONS_H

struct LomMenuTranslation
{
    const char* language;

    const char* key;

    const char* text;
};

extern "C" const LomMenuTranslation lom_menu_translations[];

extern "C" const unsigned lom_menu_translation_count;

#endif
