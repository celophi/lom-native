#include "common/card_directory.h"
#include "common/card_menu.h"

/**
 * @brief Remove both placeholder save filenames from the active card.
 */
inline void card_erase_placeholder_files(void)
{
    CardFilePath card_path;

    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    card_path.device.characters.slot += g_card_slot;
    lom_native_niki_concat(card_path.text, g_lom_save_dummy_filename);
    lom_native_card_erase_libapi(card_path.text);

    lom_native_niki_string_copy(card_path.text, CARD_DEVICE_PREFIX);
    card_path.device.characters.slot += g_card_slot;
    lom_native_niki_concat(card_path.text, g_lom_pocketstation_dummy_filename);
    lom_native_card_erase_libapi(card_path.text);
}
