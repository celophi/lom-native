#include "overlays/title/internal/title_internal.h"
#include "common/pad.h"
void handle_title_menu_input(void);
#define LOM_TITLE_DEVICE_REGS ((volatile u8*)SCD_REGS)
#define RESET_DEBOUNCE()

/**
 * @brief Play no sound effects.
 * @param sound_id Ignored.
 * @param arg1 Ignored.
 * @param pan Ignored.
 * @param volume Ignored.
 */
void __wrap_akao_play_sfx(s32 sound_id, s32 arg1, s32 pan, s32 volume)
{
    (void)sound_id;
    (void)arg1;
    (void)pan;
    (void)volume;
}

#include "lom_native_ps1_memory.h"

extern int printf(const char* format, ...);

/**
 * @brief Set the buttons TITLE reads straight from the controller's registers.
 * @param raw_buttons The buttons, low when held.
 */
static void set_raw_buttons(u16 raw_buttons)
{
    LOM_TITLE_DEVICE_REGS[0] = 0;
    *((volatile u16*)(LOM_TITLE_DEVICE_REGS + 2)) = raw_buttons;
}

int main(void)
{
    if (lom_native_map_ps1_ram() != 0)
    {
        return 10;
    }

    g_title_menu_item_flags[0] = 1;
    g_title_menu_item_flags[2] = 1;
    g_title_menu_item_flags[4] = 1;
    g_title_selected_item = 0;
    g_title_visible_item_rank = 0;
    g_title_menu_exit_state = 0;
    g_title_idle_countdown = 60;
    RESET_DEBOUNCE();

    set_raw_buttons(0x0040);
    handle_title_menu_input();

    if (g_title_selected_item != 1 || g_title_visible_item_rank != 1 || g_title_menu_exit_state != 0)
    {
        printf("TITLE input down transition failed: selected=%u rank=%u exit=%d\n", (unsigned int)g_title_selected_item,
               (unsigned int)g_title_visible_item_rank, g_title_menu_exit_state);
        return 11;
    }

    set_raw_buttons(0);
    handle_title_menu_input();

    set_raw_buttons(0x0080);
    handle_title_menu_input();

    if (g_title_selected_item != 0 || g_title_visible_item_rank != 0 || g_title_menu_exit_state != 0)
    {
        printf("TITLE input up transition failed: selected=%u rank=%u exit=%d\n", (unsigned int)g_title_selected_item, (unsigned int)g_title_visible_item_rank,
               g_title_menu_exit_state);
        return 12;
    }

    printf("LoM TITLE input state smoke passed.\n");
    printf("  selected item: %u\n", (unsigned int)g_title_selected_item);
    printf("  visible rank:  %u\n", (unsigned int)g_title_visible_item_rank);
    return 0;
}
