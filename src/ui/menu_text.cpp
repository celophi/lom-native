#include "menu_text.h"
#include "menu_translations.h"
#include "lom_native_env.h"

#include <SDL3/SDL.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace lom::ui
{

namespace
{

std::vector<const MenuText*>& registry()
{
    static std::vector<const MenuText*> texts;
    return texts;
}

/**
 * @brief The language the menu shows, as its code.
 * @return It, which can be changed.
 */
std::string& current_language()
{
    static std::string language = "en";
    return language;
}

/**
 * @brief The menu's texts in the current language, by name.
 * @return The table, which can be changed.
 */
std::unordered_map<std::string, const char*>& current_texts()
{
    static std::unordered_map<std::string, const char*> texts;
    return texts;
}

}

MenuText::MenuText(const char* key, const char* english) : key_(key), english_(english)
{
    registry().push_back(this);
}

MenuText::operator const char*() const
{
    const auto found = current_texts().find(key_);
    return found != current_texts().end() ? found->second : english_;
}

const std::vector<const MenuText*>& menu_texts()
{
    return registry();
}

const std::vector<const char*>& menu_languages()
{
    static const std::vector<const char*> languages = {"en", "de", "fr", "es", "ja", "ko", "zh_CN", "zh_TW"};
    return languages;
}

std::string menu_language_for(const std::vector<std::string>& preferred)
{
    for (const std::string& entry : preferred)
    {
        std::vector<std::string> parts(1);

        for (char c : entry)
        {
            if (c == '-' || c == '_')
            {
                parts.emplace_back();
            }
            else
            {
                parts.back() += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }

        if (parts[0] == "zh")
        {
            for (size_t i = 1; i < parts.size(); ++i)
            {
                if (parts[i] == "hant" || parts[i] == "tw" || parts[i] == "hk" || parts[i] == "mo")
                {
                    return "zh_TW";
                }
            }

            return "zh_CN";
        }

        for (const char* code : menu_languages())
        {
            if (parts[0] == code)
            {
                return code;
            }
        }
    }

    return "en";
}

void set_menu_language(const char* code)
{
    std::string chosen = code && *code ? code : "";

    if (chosen.empty())
    {
        const char* launcher = lom_native_env_value(LOM_ENV_LANGUAGE);

        if (launcher)
        {
            chosen = launcher;
        }
        else
        {
            std::vector<std::string> preferred;
            int count = 0;
            SDL_Locale** locales = SDL_GetPreferredLocales(&count);

            for (int i = 0; locales && i < count; ++i)
            {
                preferred.push_back(std::string(locales[i]->language ? locales[i]->language : "") +
                                    (locales[i]->country ? std::string("-") + locales[i]->country : std::string()));
            }

            SDL_free(locales);
            chosen = menu_language_for(preferred);
        }
    }

    bool known = false;

    for (const char* language : menu_languages())
    {
        known = known || chosen == language;
    }

    current_language() = known ? chosen : "en";
    current_texts().clear();

    for (unsigned i = 0; i < lom_menu_translation_count; ++i)
    {
        if (current_language() == lom_menu_translations[i].language && *lom_menu_translations[i].text)
        {
            current_texts()[lom_menu_translations[i].key] = lom_menu_translations[i].text;
        }
    }
}

const char* menu_language()
{
    return current_language().c_str();
}

std::vector<std::pair<std::string, std::string>> menu_translation(const char* code)
{
    std::vector<std::pair<std::string, std::string>> texts;

    for (unsigned i = 0; i < lom_menu_translation_count; ++i)
    {
        if (std::strcmp(lom_menu_translations[i].language, code) == 0)
        {
            texts.emplace_back(lom_menu_translations[i].key, lom_menu_translations[i].text);
        }
    }

    return texts;
}

}
