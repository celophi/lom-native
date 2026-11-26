#include "main/audio/game_audio.h"
#include "main/audio/akao_cmd.h"
#include "common.h"
#include "../internal/field_calls.h"
#include "../internal/field_actor.h"
#include "../internal/field_records.h"
#include "main/main.h"
#include "lom_native_sound.h"
#include "lom_native_mods.h"
#include "lom_native_switches.h"

#define FIELD_BATTLE_MUSIC_FADE_TICKS 64

#define FIELD_MUSIC_BOSS_BATTLE 3
#define FIELD_MUSIC_BOSS_BATTLE_2 34
#define FIELD_MUSIC_BOSS_BATTLE_3 35
#define FIELD_MUSIC_ANOTHER_DIMENSION 37
#define FIELD_MUSIC_SPARKLING_VORTEX 43
#define FIELD_MUSIC_BOSS_BATTLE_4 45
#define FIELD_MUSIC_LEAVING_SORROW_BEHIND 46
#define FIELD_MUSIC_SANCTUARY_OF_MANA 47

/**
 * @brief Look up the object state of every non-party actor record.
 * @note The results are discarded; called instead of building a battle for group 0.
 */
void field_battle_scan_actor_objects(void)
{
    s32 i;

    for (i = FIELD_PARTY_SIZE; i < g_field_runtime->state.actor_count; i++)
    {
        field_find_object_state(g_field_runtime->actors[i].id);
    }
}

/**
 * @brief Start a battle: bind the group's actors, run their battle event and hand the party to battle control.
 * @param group Trigger group (monster group) matched against each actor record.
 * @note Native: then raises the mods' battle_start hook.
 */
void field_battle_start(s32 group)
{
    s32 i;
    FieldRuntimeContext* context;

    for (i = FIELD_PARTY_SIZE; i < g_field_runtime->state.actor_count; i++)
    {
        if (g_field_runtime->actors[i].flags.bits.trigger_group == group)
        {
            field_set_actor_group(g_field_runtime->actors[i].id, group);
            field_run_actor_event(g_field_runtime->actors[i].id, FIELD_EVENT_NOTIFY, FIELD_BATTLE_PHASE_START);
        }
    }

    context = g_field_runtime;
    context->state.bits.battle_active = 1;
    context->state.bits.trigger_group = group;
    field_set_battle_group(group, context);

    for (i = 0; i < FIELD_PARTY_SIZE; i++)
    {
        if (g_field_game_state->characters[i].info.bits.pad_controlled)
        {
            field_set_actor_control_mode(i, FIELD_CONTROL_PLAYER);
        }
        else
        {
            g_field_runtime->actors[i].enabled_events = FIELD_ALL_EVENTS;
            field_set_actor_control_mode(i, FIELD_CONTROL_SCRIPTED);
            field_run_actor_event(g_field_runtime->actors[i].id, FIELD_EVENT_START, 0);
        }
    }

    field_run_actor_event(FIELD_EVENT_ACTOR_ID_BASE, FIELD_EVENT_NOTIFY, FIELD_BATTLE_PHASE_START);
    lom_native_mods_hook(LOM_MOD_HOOK_BATTLE_START, 0);
}

/**
 * @brief Native: check whether the battle has a boss in it.
 * @return 1 when one of its monsters is a boss, otherwise 0.
 */
static s32 field_battle_has_boss(void)
{
    s32 i;
    FieldStatusRecord* record;

    for (i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
    {
        record = &g_field_battle->records[i];

        if ((record->side & FIELD_RECORD_MONSTER_SIDE) && !(record->meta.packed & FIELD_STATUS_META_ALLY) && record->meta.bits.active &&
            record->state != NULL && record->state != FIELD_STATUS_STATE_NONE && record->state->hp_display.bits.boss)
        {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Native: when a switch says to win this kind of battle at once
 *        (skip_battles for any, win_boss_battles for one with a boss in it,
 *        win_regular_battles for the rest), defeat every monster still
 *        standing in it, the way the game's own DEFEAT_ACTOR script command
 *        does (field_battle_defeat_record), so drops, the enemy count, the
 *        battle's end and a boss's death events all run as they would. A
 *        monster a script is holding (a boss's opening, say) waits until it's
 *        let go. The field runs this every frame.
 */
void field_skip_battle(void)
{
    s32 i;
    FieldStatusRecord* record;
    struct FieldActorRecord* actor;

    if (g_field_runtime == NULL || !g_field_runtime->state.bits.battle_active || g_field_battle == NULL)
    {
        return;
    }

    if (!lom_native_switch(LOM_SWITCH_SKIP_BATTLES) &&
        !lom_native_switch(field_battle_has_boss() ? LOM_SWITCH_WIN_BOSS_BATTLES : LOM_SWITCH_WIN_REGULAR_BATTLES))
    {
        return;
    }

    for (i = 0; i < FIELD_BATTLE_RECORD_COUNT && !(g_field_battle->state.flags & FIELD_BATTLE_FINISHED); i++)
    {
        record = &g_field_battle->records[i];

        if (!(record->side & FIELD_RECORD_MONSTER_SIDE) || (record->meta.packed & FIELD_STATUS_META_ALLY) || !record->meta.bits.active ||
            record->state == NULL || record->state == FIELD_STATUS_STATE_NONE || record->state->current_hp.word == 0)
        {
            continue;
        }

        actor = field_find_actor_record(record->meta.bytes.id);

        if (actor != NULL && actor->flags.bits.script_only)
        {
            continue;
        }

        record->state->current_hp.word = 0;
        field_battle_defeat_record(record->meta.bytes.id);
    }
}

/**
 * @brief Suspend the battle, run the suspend event for the active group and refill the party's HP.
 * @note Native: with the No natural healing switch on, the party's HP isn't
 *       refilled.
 */
void field_battle_suspend(void)
{
    s32 i;
    FieldObjectState* state;

    g_field_battle->state.flags |= FIELD_BATTLE_FINISHED;
    field_set_battle_group(0, g_field_battle);
    field_run_actor_event(FIELD_EVENT_ACTOR_ID_BASE, FIELD_EVENT_NOTIFY, FIELD_BATTLE_PHASE_SUSPEND);

    for (i = FIELD_PARTY_SIZE; i < g_field_runtime->state.actor_count; i++)
    {
        if (g_field_runtime->actors[i].flags.bits.trigger_group == g_field_runtime->state.bits.trigger_group)
        {
            field_run_actor_event(g_field_runtime->actors[i].id, FIELD_EVENT_NOTIFY, FIELD_BATTLE_PHASE_SUSPEND);
        }
    }

    if (lom_native_switch(LOM_SWITCH_NO_NATURAL_HEALING))
    {
        return;
    }

    for (i = 0; i < FIELD_PARTY_SIZE; i++)
    {
        state = field_find_object_state(i);

        if (state != FIELD_STATUS_STATE_NONE)
        {
            state->current_hp.word = state->maximum_hp;
        }
    }
}

/**
 * @brief End the battle: return the party to field control, run the end event and fade the battle music.
 * @note Only boss music fades, and not after a duel (g_field_duel_mode).
 * @note Native: then raises the mods' battle_end hook, before the results window.
 */
void field_battle_end(void)
{
    s32 i;
    s32 j;

    g_field_battle = NULL;
    g_field_runtime->state.bits.battle_active = 0;
    g_field_runtime->state.bits.trigger_group = 0;

    for (i = 0; i < FIELD_PARTY_SIZE; i++)
    {
        if (g_field_game_state->characters[i].info.bits.pad_controlled)
        {
            field_set_actor_control_mode(i, FIELD_CONTROL_PLAYER);
        }
        else
        {
            g_field_runtime->actors[i].enabled_events = 0;

            for (j = 0; j < FIELD_ACTOR_SCRIPT_COUNT; j++)
            {
                g_field_runtime->actors[i].scripts[j] = FIELD_NO_SCRIPT;
            }

            field_set_actor_control_mode(i, FIELD_CONTROL_FOLLOWER);
            field_stop_actor_script(i, 0);
        }
    }

    for (i = 0; i < g_field_runtime->state.actor_count; i++)
    {
        field_run_actor_event(g_field_runtime->actors[i].id, FIELD_EVENT_NOTIFY, FIELD_BATTLE_PHASE_END);
    }

    if (g_field_duel_mode != 0)
    {
        g_field_duel_mode = 0;
    }
    else
    {
        switch (g_field_music_id)
        {
        case FIELD_MUSIC_BOSS_BATTLE:
        case FIELD_MUSIC_BOSS_BATTLE_2:
        case FIELD_MUSIC_BOSS_BATTLE_3:
        case FIELD_MUSIC_ANOTHER_DIMENSION:
        case FIELD_MUSIC_SPARKLING_VORTEX:
        case FIELD_MUSIC_BOSS_BATTLE_4:
        case FIELD_MUSIC_LEAVING_SORROW_BEHIND:
        case FIELD_MUSIC_SANCTUARY_OF_MANA:
            lom_sound_fade_song_volume(0, FIELD_BATTLE_MUSIC_FADE_TICKS, 0);
            break;
        }
    }

    lom_native_mods_hook(LOM_MOD_HOOK_BATTLE_END, 0);
}

/**
 * @brief Find a monster template by id.
 * @param table Template table of the battle resource.
 * @param id Template id; only the low byte is compared.
 * @return The matching template, or NULL (after a diagnostic) when absent.
 */
FieldActorTemplate* field_find_actor_template(FieldActorTemplateTable* table, s32 id)
{
    u32 i;
    FieldActorTemplate* template;

    id &= 0xFF;

    for (i = 0; i < table->count; i++)
    {
        template = (FieldActorTemplate*)((u8*)table + table->offsets[i]);

        if (template->id == id)
        {
            return template;
        }
    }

    record_game_diagnostic(DIAG_ERROR, DIAG_NO_MONSTER_TEMPLATE, id, -1);
    return NULL;
}
