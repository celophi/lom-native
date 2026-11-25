#include "common.h"

#include "../internal/field_calls.h"
#include "../internal/field_records.h"
#include "../internal/field_script.h"
#include "../internal/field_japanese_rules.h"
#include "overlays/field/field_overlay_handoff.h"

#define FIELD_ITEM_TABLE 4

#define FIELD_ITEM_GRID_TABLE 0xF

#define FIELD_ITEM_RESULT_VARIABLE 0x7100

#define FIELD_ITEM_RESULT_FULL 0xFE

#define FIELD_ITEM_RESULT_NONE 0xFF

#define FIELD_CREATE_KIND_TEMPER 3

#define FIELD_DEFAULT_STAT_MODIFIER 4

#define FIELD_STAGING_BOUNDS_MASK 0xF0

#define FIELD_SLOT_CLASS_NONE 0xF

#define FIELD_SLOT_CLASS_LIMIT 0x10

#define FIELD_GRID_MAX 7

static void field_create_instrument_item(FieldItemRecord* record, s32 category, s32 item_type, s32 material, s32 secondary_item);
static void field_generate_staged_item(void);
static void field_load_staged_material(void);

void field_create_item_from_gosub_unchecked(s32 kind)
{
    FieldItemRecord* record;

    g_field_gosub_state = 0;
    field_copy_words(NULL, g_field_item_staging, sizeof(FieldItemStaging));

    if (g_gosub_result_count != 0)
    {
        switch (kind)
        {
        case FIELD_ITEM_CATEGORY_INSTRUMENT:
            record = field_find_free_inventory_record();

            if (record != NULL)
            {
                field_create_instrument_item(record, FIELD_ITEM_CATEGORY_INSTRUMENT, g_gosub_result_values[0], g_gosub_result_values[1],
                                             g_gosub_result_values[2]);
                field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, record - g_field_game_state->items);
            }
            else
            {
                field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, FIELD_ITEM_RESULT_FULL);
            }

            break;
        case FIELD_CREATE_KIND_TEMPER:
            field_temper_item(&g_field_game_state->items[g_gosub_result_values[0]], g_gosub_result_values[1]);
            field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, g_gosub_result_values[0]);
            break;
        default:
            record = field_find_free_inventory_record();

            if (record != NULL)
            {
                field_create_equipment_item(record, kind, g_gosub_result_values[0], g_gosub_result_values[1]);
                field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, record - g_field_game_state->items);
            }
            else
            {
                field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, FIELD_ITEM_RESULT_FULL);
            }

            break;
        }
    }
    else
    {
        field_set_script_var(0, FIELD_ITEM_RESULT_VARIABLE, FIELD_ITEM_RESULT_NONE);
    }
}

/**
 * @brief Create a new weapon or armor, consuming one of its material, and generate it.
 * @param record Item record that receives the item.
 * @param category FIELD_ITEM_CATEGORY_WEAPON or FIELD_ITEM_CATEGORY_ARMOR.
 * @param item_type Weapon or armor type.
 * @param material Material item kind; one is consumed.
 */
void field_create_equipment_item(FieldItemRecord* record, s32 category, s32 item_type, s32 material)
{
    s32 i;

    field_consume_item(material);

    FIELD_SET_STAGING_RECORD(g_field_item_staging, record);
    g_field_item_staging->category = category;
    g_field_item_staging->item_type = item_type;
    g_field_item_staging->material = material;
    g_field_item_staging->secondary_item = FIELD_NO_SECONDARY_ITEM;

    for (i = 0; i < FIELD_STAGING_STAT_COUNT; i++)
    {
        g_field_item_staging->stats.bytes[i] = (g_field_item_staging->stats.bytes[i] & FIELD_STAGING_BOUNDS_MASK) | FIELD_DEFAULT_STAT_MODIFIER;
        g_field_item_staging->base_stats[i] = FIELD_DEFAULT_STAT_MODIFIER;
    }

    for (i = 0; i < FIELD_STAGING_SLOT_COUNT; i++)
    {
        g_field_item_staging->slots[i] = FIELD_STAGING_SLOT_EMPTY;
    }

    g_field_item_staging->properties[0] = g_field_item_staging->item_type << 4;
    g_field_item_staging->properties[1] = (g_field_item_staging->item_type << 4) + 0xC;
    g_field_item_staging->properties[2] = (g_field_item_staging->item_type << 4) + 0xD;
    g_field_item_staging->properties[3] = (g_field_item_staging->item_type << 4) + 0xE;
    g_field_item_staging->properties[4] = (g_field_item_staging->item_type << 4) + 0xF;
    g_field_item_staging->properties[5] = 0xFF;

    field_generate_staged_item();
}

/**
 * @brief Create a new instrument from a material and a secondary material, and pick its spell.
 *
 * The material and instrument type give the start cell and power; the
 * secondary material's script moves the cell, which is clamped to the grid
 * and selects the spell.
 *
 * @param record Item record that receives the item.
 * @param category Item category (FIELD_ITEM_CATEGORY_INSTRUMENT).
 * @param item_type Instrument type.
 * @param material Material item kind; one is consumed.
 * @param secondary_item Secondary material item kind; one is consumed.
 */
static void field_create_instrument_item(FieldItemRecord* record, s32 category, s32 item_type, s32 material, s32 secondary_item)
{
    FieldScriptState* saved_script;
    s32 column;
    s32 grid_row;
    s32 i;

    field_consume_item(material);
    field_consume_item(secondary_item);
    FIELD_SET_STAGING_RECORD(g_field_item_staging, record);
    g_field_item_staging->category = category;
    g_field_item_staging->item_type = item_type;
    g_field_item_staging->material = material;
    g_field_item_staging->secondary_item = secondary_item;

    for (i = 0; i < FIELD_STAGING_STAT_COUNT; i++)
    {
        g_field_item_staging->stats.bytes[i] = (g_field_item_staging->stats.bytes[i] & FIELD_STAGING_BOUNDS_MASK) | FIELD_DEFAULT_STAT_MODIFIER;
    }

    for (i = 0; i < FIELD_STAGING_SLOT_COUNT; i++)
    {
        g_field_item_staging->slots[i] = FIELD_STAGING_SLOT_EMPTY;
    }

    g_field_item_tables = field_find_resource(FIELD_ITEM_GRID_TABLE);
    g_field_item_staging->properties[1] = g_field_item_tables->grid.pairs[material][item_type][0] & 7;
    g_field_item_staging->properties[2] = g_field_item_tables->grid.pairs[material][item_type][0] >> 3;
    g_field_item_staging->properties[3] = g_field_item_tables->grid.pairs[material][item_type][1];

    saved_script = g_field_script;
    g_field_script = &g_field_runtime->events[0].script;
    field_run_item_script(g_field_item_tables->grid.secondary_scripts[secondary_item - FIELD_SECONDARY_ITEM_FIRST]);
    g_field_script = saved_script;
    field_write_staged_item();

    record->derived.instrument.spirit = g_field_item_staging->properties[0];

    if ((s8)g_field_item_staging->properties[1] >= 0)
    {
        column = FIELD_GRID_MAX;

        if (g_field_item_staging->properties[1] <= FIELD_GRID_MAX)
        {
            column = g_field_item_staging->properties[1];
        }
    }
    else
    {
        column = 0;
    }

    if ((s8)g_field_item_staging->properties[2] >= 0)
    {
        grid_row = FIELD_GRID_MAX;

        if (g_field_item_staging->properties[2] <= FIELD_GRID_MAX)
        {
            grid_row = g_field_item_staging->properties[2];
        }
    }
    else
    {
        grid_row = 0;
    }

    record->derived.instrument.spell = g_field_item_tables->grid.grid[grid_row][column];
    record->derived.instrument.power = g_field_item_staging->properties[3];
}

/**
 * @brief Temper an existing weapon or armor with one more item and regenerate it.
 * @param record Item record to temper.
 * @param secondary_item Tempering item kind; one is consumed.
 * @note JP reuses the current generation table. JP, and the US game with the
 *       Japanese balance on, keeps the item's power and element flags.
 */
void field_temper_item(FieldItemRecord* record, s32 secondary_item)
{
    s32 i;

#if !defined(VERSION_JP)
    g_field_item_tables = field_find_resource(FIELD_ITEM_TABLE);
#endif
    field_consume_item(secondary_item);

    FIELD_SET_STAGING_RECORD(g_field_item_staging, record);
    g_field_item_staging->category = record->info.bits.category;
    g_field_item_staging->item_type = record->info.bits.item_type;
    g_field_item_staging->material = record->info.bits.material;
    g_field_item_staging->secondary_item = secondary_item;
    g_field_item_staging->pool += g_field_item_tables->item.secondary_items[secondary_item - FIELD_SECONDARY_ITEM_FIRST].pool_bonus;

    g_field_item_staging->levels[0].level = record->bonus_nibbles.bits.n0;
    g_field_item_staging->levels[1].level = record->bonus_nibbles.bits.n1;
    g_field_item_staging->levels[2].level = record->bonus_nibbles.bits.n2;
    g_field_item_staging->levels[3].level = record->bonus_nibbles.bits.n3;
    g_field_item_staging->levels[4].level = record->bonus_nibbles.bits.n4;
    g_field_item_staging->levels[5].level = record->bonus_nibbles.bits.n5;
    g_field_item_staging->levels[6].level = record->bonus_nibbles.bits.n6;
    g_field_item_staging->levels[7].level = record->bonus_nibbles.bits.n7;
    g_field_item_staging->effect_index = record->effect_index;

    g_field_item_staging->stats.words[0].modifier0 = record->stat_nibbles.bits.n0;
    g_field_item_staging->stats.words[0].modifier1 = record->stat_nibbles.bits.n1;
    g_field_item_staging->stats.words[0].modifier2 = record->stat_nibbles.bits.n2;
    g_field_item_staging->stats.words[0].modifier3 = record->stat_nibbles.bits.n3;
    g_field_item_staging->stats.words[1].modifier0 = record->stat_nibbles.bits.n4;
    g_field_item_staging->stats.words[1].modifier1 = record->stat_nibbles.bits.n5;
    g_field_item_staging->stats.words[1].modifier2 = record->stat_nibbles.bits.n6;
    g_field_item_staging->stats.words[1].modifier3 = record->stat_nibbles.bits.n7;

    for (i = 0; i < FIELD_STAGING_STAT_COUNT; i++)
    {
        g_field_item_staging->base_stats[i] = FIELD_DEFAULT_STAT_MODIFIER;
    }

    g_field_item_staging->slots[0] = FIELD_STAGING_SLOT_EMPTY;
    g_field_item_staging->slots[1] = record->special_ids[3];

    for (i = 0; i < FIELD_ITEM_SPECIAL_COUNT - 1; i++)
    {
        g_field_item_staging->slots[i + 2] = record->special_ids[i];
    }

    g_field_item_staging->slots[5] = FIELD_STAGING_SLOT_EMPTY;

    switch (g_field_item_staging->category)
    {
    case FIELD_ITEM_CATEGORY_WEAPON:
        for (i = 0; i < FIELD_STAGING_PROPERTY_COUNT; i++)
        {
            g_field_item_staging->properties[i] = record->derived.weapon.stats[i];
        }
        g_field_item_staging->power_flags = FIELD_JAPANESE_RULES() ? record->status_flags : 0;
        break;
    case FIELD_ITEM_CATEGORY_ARMOR:
        g_field_item_staging->element_flags = FIELD_JAPANESE_RULES() ? record->element_flags : 0;
        g_field_item_staging->immunity_flags = record->status_flags;
        break;
    }

    field_generate_staged_item();
}

/**
 * @brief Run the generation scripts of the staged item and write it back.
 *
 * The type, material, secondary material and slot scripts run on the event script
 * context; pending levels, flags and stat clamping are applied, the item is
 * written back and its weapon or armor derived values are computed.
 * @note JP, and the US game with the Japanese balance on, applies the
 *       pending levels in field_finish_staged_item instead.
 */
static void field_generate_staged_item(void)
{
    FieldScriptState* saved_script;

    saved_script = g_field_script;
    g_field_script = &g_field_runtime->events[0].script;
    field_load_staged_material();

    if (g_field_item_staging->category == FIELD_ITEM_CATEGORY_WEAPON)
    {
        field_run_item_script(g_field_item_tables->item.weapon_types[g_field_item_staging->item_type].scripts[0]);
    }
    else
    {
        field_run_item_script(g_field_item_tables->item.armor_types[g_field_item_staging->item_type].scripts[0]);
    }

    field_run_item_script(g_field_item_tables->item.materials[g_field_item_staging->material].script);
    field_run_item_script(g_field_item_tables->item.secondary_items[g_field_item_staging->secondary_item - FIELD_SECONDARY_ITEM_FIRST].script);
    field_run_slot_scripts();
    if (!FIELD_JAPANESE_RULES())
    {
        field_apply_pending_levels();
    }

    if (g_field_item_staging->category == FIELD_ITEM_CATEGORY_WEAPON)
    {
        field_run_item_script(g_field_item_tables->item.weapon_types[g_field_item_staging->item_type].scripts[1]);
    }
    else
    {
        field_run_item_script(g_field_item_tables->item.armor_types[g_field_item_staging->item_type].scripts[1]);
    }

    field_finish_staged_item();
    g_field_script = saved_script;
    field_write_staged_item();

    switch (g_field_item_staging->category)
    {
    case FIELD_ITEM_CATEGORY_WEAPON:
        field_derive_weapon_values(FIELD_STAGING_RECORD(g_field_item_staging));
        return;
    case FIELD_ITEM_CATEGORY_ARMOR:
        field_derive_armor_values(FIELD_STAGING_RECORD(g_field_item_staging));
        return;
    }
}

/**
 * @brief Load the item table and copy the staged material's entry into the staging block.
 */
static void field_load_staged_material(void)
{
    s32 i;

    g_field_item_tables = field_find_resource(FIELD_ITEM_TABLE);
    g_field_item_staging->divisor = g_field_item_tables->item.materials[g_field_item_staging->material].divisor;

    for (i = 0; i < FIELD_STAGING_FACTOR_COUNT; i++)
    {
        g_field_item_staging->weights[i] = g_field_item_tables->item.materials[g_field_item_staging->material].weights[i];
    }

    for (i = 0; i < FIELD_STAGING_FACTOR_COUNT; i++)
    {
        g_field_item_staging->multipliers[i] = g_field_item_tables->item.materials[g_field_item_staging->material].multipliers[i];
    }

    for (i = 0; i < FIELD_STAGING_LEVEL_COUNT; i++)
    {
        g_field_item_staging->levels[i].cost = g_field_item_tables->item.materials[g_field_item_staging->material].costs[i];
        g_field_item_staging->pending_levels[i] = 0;
    }

    g_field_item_staging->flags.bits.slot_class = FIELD_SLOT_CLASS_NONE;

    for (i = 1; i < FIELD_STAGING_SLOT_COUNT - 1; i++)
    {
        if (g_field_item_staging->slots[i] < FIELD_SLOT_CLASS_LIMIT)
        {
            g_field_item_staging->flags.bits.slot_class = g_field_item_staging->slots[i];
        }
    }
}
