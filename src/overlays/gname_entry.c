#include "lom_native_gname.h"
#include "common/saved_game.h"
#include "lom_native_ps1_memory.h"
#include "overlays/gname/gname.h"

s32 lom_native_run_gname(intptr_t render_buffers, intptr_t initial_name, intptr_t active_name, s32 source_mode, s32 history_index, intptr_t custom_name,
                         s32 allow_empty_cancel)
{
    static const u8 empty_custom[48];

    if (!custom_name)
    {
        custom_name = (intptr_t)empty_custom;
    }

    if (source_mode == 3)
    {
        if (history_index < 0)
        {
            return 2;
        }

        u8* active = (u8*)active_name;
        u8* companion = g_saved_game_ctx->characters[2].name;

        if (active && active == companion)
        {
            int golem = (g_saved_game_ctx->characters[2].info.word & FIELD_CHARACTER_TYPE_MASK) == FIELD_CHARACTER_GOLEM;

            if (golem ? (u32)g_saved_game_ctx->joined_golem >= GOLEM_RECORD_COUNT : (u32)g_saved_game_ctx->joined_pet >= PET_RECORD_COUNT)
            {
                return 2;
            }

            active_name = (intptr_t)g_saved_game_ctx->characters[2].name;
        }
    }

    return gname_run((void*)render_buffers, (const u8*)initial_name, (u8*)active_name, source_mode, history_index, (const u8*)custom_name, allow_empty_cancel);
}
