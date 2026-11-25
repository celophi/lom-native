#include "common.h"

#include "../internal/field_calls.h"
#include "../internal/field_records.h"
#include "common/golem_shape.h"
#include "common/saved_game.h"
#include "main/main.h"

#define GOLEM_COMMAND_REORDER 0x92BC
#define GOLEM_GRID_UPPER_CELL_COUNT (GOLEM_GRID_CELL_COUNT - GOLEM_GRID_WIDTH)
#define GOLEM_SLOT_STATUS_UNCHANGED 3

extern GolemShapeTable g_field_golem_stamp_shapes;

static void field_golem_rebuild_grid(s32 type);
static void field_golem_mark_grid_edges(void);
static void field_golem_stamp_block_shape(s32 index, s32 rotation, s32 x, s32 y);

void field_golem_commit_group_edit_unchecked(s32 command)
{
    s32 i;
    s32 found;
    s32 target;
    s8 joined;
    SavedGameLayout* layout;
    s32 group;

    i = 0;

    if (command == GOLEM_COMMAND_REORDER)
    {
        target = g_field_runtime_context.locals.golem.slot;

        if ((u32)target < GOLEM_RECORD_COUNT)
        {
            layout = &g_saved_game.layout;
            joined = layout->joined_golem;

            for (; i < GOLEM_RECORD_COUNT; i++)
            {
                if (*(i + layout->golem_order) == joined)
                {
                    found = i;
                }
            }

            g_saved_game.layout.golem_order[found] = g_saved_game.layout.golem_order[target];
            g_saved_game.layout.golem_order[target] = g_saved_game.layout.joined_golem;

            if (g_saved_game.layout.golem_order[found] == g_saved_game.layout.joined_golem)
            {
                g_field_runtime_context.locals.golem.slot_status[0] = GOLEM_SLOT_STATUS_UNCHANGED;
            }
            else
            {
                g_field_runtime_context.locals.golem.slot_status[0] = g_saved_game.layout.golem_order[found];
            }

            g_field_runtime_context.locals.golem.joined_group = g_saved_game.layout.joined_golem;
        }
    }
    else
    {
        for (; i < GOLEM_NAME_LENGTH; i++)
        {
            g_saved_game.layout.golem_records[g_saved_game.layout.joined_golem].name[i] = g_saved_game.layout.characters[FIELD_PARTY_COMPANION].name[i];
        }

        group = g_saved_game.layout.joined_golem;

        if (group != GOLEM_NO_GROUP)
        {
            SAVED_GAME_GOLEM_HEADER_WORD(g_saved_game.layout) =
                (SAVED_GAME_GOLEM_HEADER_WORD(g_saved_game.layout) & ~GOLEM_SAVED_GROUP_MASK) | ((group & 0xF) << GOLEM_SAVED_GROUP_SHIFT);
        }

        g_saved_game.layout.joined_golem = GOLEM_NO_GROUP;
    }
}

void field_golem_commit_group_edit(s32 command)
{
    s32 group = g_saved_game.layout.joined_golem;

    if ((u32)group >= GOLEM_RECORD_COUNT)
    {
        return;
    }

    if (command == 0x92BC)
    {
        unsigned slot;

        for (slot = 0; slot < GOLEM_RECORD_COUNT; ++slot)
        {
            if (g_saved_game.layout.golem_order[slot] == group)
            {
                break;
            }
        }

        if (slot == GOLEM_RECORD_COUNT)
        {
            return;
        }
    }

    field_golem_commit_group_edit_unchecked(command);
}

/**
 * @brief Show one logic type on the grid and rebuild the golem companion record.
 * @param type Logic type to show, or GOLEM_NO_GROUP to rejoin the saved group.
 */
void field_golem_select_logic_type(s32 type)
{
    if (type == GOLEM_NO_GROUP)
    {
        g_saved_game.layout.joined_golem = g_saved_game.layout.golem_count >> GOLEM_SAVED_GROUP_SHIFT;
    }
    else
    {
        g_saved_game.layout.joined_golem = type;
    }

    field_golem_rebuild_current_grid();
    field_golem_build_companion(g_saved_game.layout.joined_golem, &g_saved_game.layout.characters[FIELD_PARTY_COMPANION]);
}

/** @brief Rebuild the placement grid for the joined golem's logic type. */
void field_golem_rebuild_current_grid(void)
{
    field_golem_rebuild_grid(g_saved_game.layout.joined_golem);
}

/**
 * @brief Clear the placement grid and place every block of one logic type on it.
 * @param type Logic type whose placed blocks are stamped; GOLEM_NO_GROUP only clears the grid.
 */
static void field_golem_rebuild_grid(s32 type)
{
    s32 i;
    LogicBlock block;

    for (i = 0; i < GOLEM_GRID_CELL_COUNT; i++)
    {
        g_saved_game.layout.golem_grid[i].block_id = 0;
        g_saved_game.layout.golem_grid[i].detail = 0;
    }

    if (type != GOLEM_NO_GROUP)
    {
        for (i = 0; i < g_saved_game.layout.logic_block_count; i++)
        {
            block = g_saved_game.layout.logic_blocks[i];

            if ((block.f.placed == 1) && (block.f.logic_type == type))
            {
                field_golem_stamp_block_shape(i, block.f.rotation, block.f.grid_x, block.f.grid_y);
            }
        }

        field_golem_mark_grid_edges();
    }
}

/** @brief Mark each grid cell whose lower neighbour belongs to a different block. */
static void field_golem_mark_grid_edges(void)
{
    s32 i;
    u8 owner;
    u8 below;
    u8 empty;

    empty = GOLEM_GRID_EMPTY;

    for (i = GOLEM_GRID_CELL_COUNT - 1; i >= 0; i--)
    {
        g_saved_game.layout.golem_grid[i].edge = empty;
    }

    for (i = 0; i < GOLEM_GRID_UPPER_CELL_COUNT; i++)
    {
        owner = g_saved_game.layout.golem_grid[i].owner;

        if (owner != GOLEM_GRID_EMPTY)
        {
            below = g_saved_game.layout.golem_grid[i + GOLEM_GRID_WIDTH].owner;

            if ((below != GOLEM_GRID_EMPTY) && (owner != below))
            {
                g_saved_game.layout.golem_grid[i].edge = i + GOLEM_GRID_WIDTH;
            }
        }
    }
}

/**
 * @brief Stamp one logic block's shape onto the placement grid.
 * @param index Logic-block index written to each covered cell.
 * @param rotation Rotation of the shape to place.
 * @param x Grid column of the shape origin.
 * @param y Grid row of the shape origin.
 */
static void field_golem_stamp_block_shape(s32 index, s32 rotation, s32 x, s32 y)
{
    GolemShapeTable table;
    s32 i;
    s32 cell;

    table = g_field_golem_stamp_shapes;

    for (i = 0; i < table.shapes[g_saved_game.layout.logic_blocks[index].f.shape].count; i++)
    {
        cell = x + table.shapes[g_saved_game.layout.logic_blocks[index].f.shape].rotations[rotation].parts[i].x +
               (y + table.shapes[g_saved_game.layout.logic_blocks[index].f.shape].rotations[rotation].parts[i].y) * GOLEM_GRID_WIDTH;
        g_saved_game.layout.golem_grid[cell].owner = index;
        g_saved_game.layout.golem_grid[cell].block_id = g_saved_game.layout.logic_blocks[index].f.id;
        g_saved_game.layout.golem_grid[cell].detail = g_saved_game.layout.logic_blocks[index].f.level;
    }
}
