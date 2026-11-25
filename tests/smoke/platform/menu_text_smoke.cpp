#include "../../../src/ui/menu_fonts.h"
#include "../../../src/ui/menu_glyphs.h"
#include "../../../src/ui/menu_model.h"
#include "../../../src/ui/menu_text.h"

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "imstb_truetype.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include "../../support/lom_check.h"

extern "C" void lom_native_menu_destroy();

namespace
{
void (*volatile menu_file)() = &lom_native_menu_destroy;
const char* (*volatile glyphs_file)(lom::settings::PadFamily) = &lom::settings::pad_family_name;

/** The printf blanks of a text, in order. */
std::string blanks(const std::string& text)
{
    std::string found;

    for (size_t i = 0; i + 1 < text.size(); ++i)
    {
        if (text[i] == '%')
        {
            found += text[i + 1];
            ++i;
        }
    }

    return found;
}

/** A text's characters, as code points. */
std::vector<int> characters(const std::string& text)
{
    std::vector<int> out;

    for (size_t i = 0; i < text.size();)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        const int length = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
        int code = length == 1 ? c : length == 2 ? c & 0x1F : length == 3 ? c & 0x0F : c & 0x07;

        for (int k = 1; k < length && i + k < text.size(); ++k)
        {
            code = code << 6 | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }

        out.push_back(code);
        i += static_cast<size_t>(length);
    }

    return out;
}

}

int main()
{
    int failures = 0;
    CHECK(menu_file != nullptr && glyphs_file != nullptr);

    for (const char* code : lom::ui::menu_languages())
    {
        if (std::string(code) == "en")
        {
            continue;
        }

        std::map<std::string, std::string> texts;

        for (const auto& [key, text] : lom::ui::menu_translation(code))
        {
            texts[key] = text;
        }

        for (const lom::ui::MenuText* text : lom::ui::menu_texts())
        {
            const auto found = texts.find(text->key());

            if (found == texts.end() || found->second.empty())
            {
                std::fprintf(stderr, "menu-text: %s has no %s\n", code, text->key());
                ++failures;
            }
            else if (blanks(found->second) != blanks(text->english()))
            {
                std::fprintf(stderr, "menu-text: %s's %s fills in %s, not %s\n", code, text->key(), blanks(found->second).c_str(),
                             blanks(text->english()).c_str());
                ++failures;
            }
            else
            {
                texts.erase(found);
            }
        }

        for (const auto& [key, unused] : texts)
        {
            std::fprintf(stderr, "menu-text: %s has %s, which the menu doesn't use\n", code, key.c_str());
            ++failures;
        }
    }

    CHECK(lom::ui::menu_language_for({"pt-BR", "fr-CA", "en-US"}) == "fr");
    CHECK(lom::ui::menu_language_for({"zh_Hant_TW"}) == "zh_TW");
    CHECK(lom::ui::menu_language_for({"zh-HK"}) == "zh_TW");
    CHECK(lom::ui::menu_language_for({"zh-CN"}) == "zh_CN");
    CHECK(lom::ui::menu_language_for({"it-IT"}) == "en");

    lom::ui::set_menu_language("de");
    CHECK(std::string(lom::settings::MenuModel::category_name(lom::settings::MenuCategory::Display)) == "Anzeige");
    lom::ui::set_menu_language("en");
    CHECK(std::string(lom::settings::MenuModel::category_name(lom::settings::MenuCategory::Display)) == "Display");
    lom::ui::set_menu_language("xx");
    CHECK(std::string(lom::ui::menu_language()) == "en");

    struct Font
    {
        const char* language;
        const unsigned char* data;
        size_t size;
    };

    const Font fonts[] = {
        {"ja", lom_font_notosanscjkjp_regular, lom_font_notosanscjkjp_regular_size},
        {"ja", lom_font_notosanscjkjp_bold, lom_font_notosanscjkjp_bold_size},
        {"ko", lom_font_notosanscjkkr_regular, lom_font_notosanscjkkr_regular_size},
        {"ko", lom_font_notosanscjkkr_bold, lom_font_notosanscjkkr_bold_size},
        {"zh_CN", lom_font_notosanscjksc_regular, lom_font_notosanscjksc_regular_size},
        {"zh_CN", lom_font_notosanscjksc_bold, lom_font_notosanscjksc_bold_size},
        {"zh_TW", lom_font_notosanscjktc_regular, lom_font_notosanscjktc_regular_size},
        {"zh_TW", lom_font_notosanscjktc_bold, lom_font_notosanscjktc_bold_size},
    };

    for (const Font& font : fonts)
    {
        stbtt_fontinfo info;
        CHECK(stbtt_InitFont(&info, font.data, stbtt_GetFontOffsetForIndex(font.data, 0)));
        int missing = 0;

        for (const auto& [key, text] : lom::ui::menu_translation(font.language))
        {
            for (int c : characters(text))
            {
                if (c > 0xFF && stbtt_FindGlyphIndex(&info, c) == 0 && missing++ < 5)
                {
                    std::fprintf(stderr, "menu-text: %s's font has no U+%04X (in %s); run tools/build/make_menu_fonts.py --cjk\n", font.language, c,
                                 key.c_str());
                }
            }
        }

        failures += missing;
    }

    CHECK(failures == 0);
    std::printf("menu-text: %zu texts, %zu languages\n", lom::ui::menu_texts().size(), lom::ui::menu_languages().size());
    return 0;
}
