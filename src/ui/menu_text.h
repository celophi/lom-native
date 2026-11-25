#ifndef LOM_UI_MENU_TEXT_H
#define LOM_UI_MENU_TEXT_H

#include <string>
#include <vector>

namespace lom::ui
{
class MenuText
{
  public:
    /**
     * @brief Declare one of the menu's texts, in English, and register it so it
     *        follows the menu's language.
     * @param key Its name in src/ui/translations/<code>.json.
     * @param english The English text.
     */
    MenuText(const char* key, const char* english);

    /**
     * @brief The text in the menu's language.
     * @return The text, which stays valid until the language changes.
     */
    operator const char*() const;

    /**
     * @brief The text in the menu's language, as a string.
     * @return A copy of the text.
     */
    operator std::string() const
    {
        return static_cast<const char*>(*this);
    }

    const char* key() const
    {
        return key_;
    }

    const char* english() const
    {
        return english_;
    }

  private:
    const char* key_;
    const char* english_;
};

inline bool operator==(const std::string& shown, const MenuText& text)
{
    return shown == static_cast<const char*>(text);
}

inline bool operator!=(const std::string& shown, const MenuText& text)
{
    return !(shown == text);
}

/** Every text, in the order declared. */
const std::vector<const MenuText*>& menu_texts();

/** The menu's languages, English first: their codes ("de", "zh_TW"). */
const std::vector<const char*>& menu_languages();

/** Use a language: one of menu_languages(), or empty or NULL for LOM_LANGUAGE or else the system's. */
void set_menu_language(const char* code);

/** The language in use. */
const char* menu_language();

/** The first of some languages (best first, as "fr-CA" or "zh_Hant_TW") the menu has, or English. */
std::string menu_language_for(const std::vector<std::string>& preferred);

/** A language's texts by name, as its translation has them (for checking them). */
std::vector<std::pair<std::string, std::string>> menu_translation(const char* code);

}

#define LOM_MENU_TEXT(name, english)                                                                                                                           \
    const ::lom::ui::MenuText name                                                                                                                             \
    {                                                                                                                                                          \
        #name, english                                                                                                                                         \
    }

#endif
