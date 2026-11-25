#include "route_runner.h"

static unsigned s_world_map_since;

static int s_world_map_idle;

static int s_carousel_turns;

static unsigned s_world_map_watch_since;

/**
 * @brief Whether a step is one the world map phase plays.
 * @param index Step index.
 * @return Non-zero for a land or place step.
 */
int is_world_map_step(int index)
{
    return index < g_run.step_count && (g_run.steps[index].kind == STEP_LAND || g_run.steps[index].kind == STEP_PLACE);
}

/**
 * @brief On the world map with g_run.step at the next step: record its checkpoints there, then play the world map
 *        steps that follow, or end the route.
 * @param frame Current FIELD frame.
 */
void continue_on_world_map(unsigned frame)
{
    for (; g_run.step < g_run.step_count && g_run.steps[g_run.step].kind == STEP_CHECKPOINT; g_run.step++)
    {
        route_record_checkpoint(g_run.steps[g_run.step].name, frame, WORLD_MAP_SCENE);
    }

    g_run.held = 0;

    if (is_world_map_step(g_run.step) ||
        (g_run.step < g_run.step_count &&
         (g_run.steps[g_run.step].kind == STEP_WATCH || g_run.steps[g_run.step].kind == STEP_PRESS || g_run.steps[g_run.step].kind == STEP_WAIT)))
    {
        s_world_map_since = g_run.vblanks;
        s_world_map_idle = 0;
        s_carousel_turns = 0;
        enter_phase(PHASE_WORLD_MAP, frame);
        return;
    }

    if (g_run.step < g_run.step_count)
    {
        fail(frame, "the route continues on the world map with a step that isn't a land, place, watch, press or wait step");
        return;
    }

    enter_phase(PHASE_DONE, frame);
    request_quit();
}

/**
 * @brief The exit step's walk has handed over to the world map: record the exit and go on there.
 * @param frame Current FIELD frame.
 */
void leave_for_world_map(unsigned frame)
{
    route_record_cover(ROUTE_COVER_EXIT, (unsigned)(g_field_scene_id & SCENE_MASK), (unsigned)g_run.steps[g_run.step].args[EXIT_ARG_LABEL], WORLD_MAP_SCENE);
    g_run.world_map_frames_before = g_wmap_frame_count;
    g_run.world_map_ready = 0;
    g_run.step++;
    continue_on_world_map(frame);
}

/**
 * @brief The land an artifact becomes when it's put on the world map.
 * @param artifact Artifact number.
 * @return Its land number.
 */
static int artifact_land(int artifact)
{
    return artifact == ARTIFACT_22 ? ARTIFACT_22_LAND : artifact;
}

/**
 * @brief The cell of a land on the world map.
 * @param land Land index.
 * @return Its cell (column + row * WMAP_GRID_SIZE), or -1 when it isn't on the map.
 */
static int world_map_land_cell(int land)
{
    for (int i = 0; i < WMAP_GRID_SIZE * WMAP_GRID_SIZE; i++)
    {
        if (g_wmap_land_lookup[i] == land)
        {
            return i;
        }
    }

    return -1;
}

/**
 * @brief The d-pad press that moves the world map cursor a cell toward a cell.
 * @param target Cell (column + row * WMAP_GRID_SIZE).
 * @return Hardware button bits, or 0 when the cursor is on it.
 */
static unsigned short world_map_cursor_toward(int target)
{
    int x = g_wmap_view.x / WMAP_CELL_SPACING + g_wmap_cursor_column;
    int y = g_wmap_view.y / WMAP_CELL_SPACING + g_wmap_cursor_row;

    if (x != target % WMAP_GRID_SIZE)
    {
        return x < target % WMAP_GRID_SIZE ? BUTTON_RIGHT : BUTTON_LEFT;
    }

    if (y != target / WMAP_GRID_SIZE)
    {
        return y < target / WMAP_GRID_SIZE ? BUTTON_DOWN : BUTTON_UP;
    }

    return 0;
}

/**
 * @brief Whether a chain of lands joins two cells, side by side, as the party travels.
 * @param from Cell (column + row * WMAP_GRID_SIZE).
 * @param to Cell.
 * @return Non-zero when it does.
 */
static int world_map_cells_joined(int from, int to)
{
    int seen[WMAP_GRID_SIZE * WMAP_GRID_SIZE] = {0}, queue[WMAP_GRID_SIZE * WMAP_GRID_SIZE], head = 0, tail = 0;

    queue[tail++] = from;
    seen[from] = 1;

    while (head < tail)
    {
        int cell = queue[head++], cx = cell % WMAP_GRID_SIZE, cy = cell / WMAP_GRID_SIZE;
        int next[4] = {cx > 0 ? cell - 1 : -1, cx < WMAP_GRID_SIZE - 1 ? cell + 1 : -1, cy > 0 ? cell - WMAP_GRID_SIZE : -1,
                       cy < WMAP_GRID_SIZE - 1 ? cell + WMAP_GRID_SIZE : -1};

        if (cell == to)
        {
            return 1;
        }

        for (int i = 0; i < 4; i++)
        {
            if (next[i] >= 0 && !seen[next[i]] && g_wmap_land_lookup[next[i]] != WMAP_NO_LAND)
            {
                seen[next[i]] = 1;
                queue[tail++] = next[i];
            }
        }
    }

    return 0;
}

/**
 * @brief A land step's next press: move the cursor onto the land's cell, then Cross, which sends the party there,
 *        and Cross again once the party stands on it, which enters the land.
 * @param step The land step.
 * @return Hardware button bits.
 */
static unsigned short land_buttons(const Step* step)
{
    int target = world_map_land_cell(step->args[LAND_ARG_LAND]);
    unsigned short toward;

    if ((g_wmap_input_locked && !g_wmap_script_wait) || g_wmap_view_scroll_mode || g_wmap_selection_phase || g_wmap_sequence_count || g_wmap_party_moving ||
        g_wmap_travelers[0].moving || g_wmap_exit_frame)
    {
        return 0;
    }

    if (target < 0)
    {
        char reason[64];

        snprintf(reason, sizeof(reason), "land %d isn't on the world map", step->args[LAND_ARG_LAND]);
        fail(lom_native_field_draw_count(), reason);
        return 0;
    }

    toward = world_map_cursor_toward(target);

    if (!toward && !world_map_cells_joined((int)(g_wmap_travelers[0].cell_x + g_wmap_travelers[0].cell_y * WMAP_GRID_SIZE), target))
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "no chain of lands joins land %d to the party's", step->args[LAND_ARG_LAND]);
        fail(lom_native_field_draw_count(), reason);
        return 0;
    }

    return toward ? toward : BUTTON_CROSS;
}

/**
 * @brief A place step's next press, as a player puts an artifact on the map: Square opens the artifacts, Right
 *        turns them until the step's is in front, Cross takes it, the d-pad moves the cursor to the nearest cell
 *        it may go in, and Cross puts it there. The step ends once its land is on the map and the world map has
 *        finished the land's effect.
 * @param step The place step.
 * @return Hardware button bits.
 */
static unsigned short place_buttons(const Step* step)
{
    int artifact = step->args[PLACE_ARG_ARTIFACT], best = -1, best_distance = 0, x, y;

    if (world_map_land_cell(artifact_land(artifact)) >= 0)
    {
        return 0;
    }

    if ((g_wmap_input_locked && !g_wmap_script_wait) || g_wmap_view_scroll_mode || g_wmap_sequence_count || g_wmap_carousel_turn_frames ||
        g_wmap_party_moving || g_wmap_travelers[0].moving || g_wmap_artifact_placement_frame || g_wmap_exit_frame)
    {
        return 0;
    }

    switch (g_wmap_selection_phase)
    {
    case WMAP_SELECTION_ARTIFACTS:
        if (wmap_get_selected_artifact() == artifact)
        {
            return BUTTON_CROSS;
        }

        if (++s_carousel_turns > CAROUSEL_TURN_LIMIT)
        {
            char reason[64];

            snprintf(reason, sizeof(reason), "artifact %d isn't among the artifacts in hand", artifact);
            fail(lom_native_field_draw_count(), reason);
            return 0;
        }

        return BUTTON_RIGHT;
    case WMAP_SELECTION_MAP:
        if (!g_wmap_artifact_placement_active || g_wmap_selected_artifact != artifact)
        {
            return g_wmap_selected_artifact != -1 && g_wmap_selected_artifact != artifact ? BUTTON_CIRCLE : BUTTON_SQUARE;
        }

        x = g_wmap_view.x / WMAP_CELL_SPACING + g_wmap_cursor_column;
        y = g_wmap_view.y / WMAP_CELL_SPACING + g_wmap_cursor_row;

        for (int i = 0; i < WMAP_GRID_SIZE * WMAP_GRID_SIZE; i++)
        {
            int cx = i % WMAP_GRID_SIZE, cy = i / WMAP_GRID_SIZE;
            int joined = (cx > 0 && g_wmap_land_lookup[i - 1] != WMAP_NO_LAND) || (cx < WMAP_GRID_SIZE - 1 && g_wmap_land_lookup[i + 1] != WMAP_NO_LAND) ||
                         (cy > 0 && g_wmap_land_lookup[i - WMAP_GRID_SIZE] != WMAP_NO_LAND) ||
                         (cy < WMAP_GRID_SIZE - 1 && g_wmap_land_lookup[i + WMAP_GRID_SIZE] != WMAP_NO_LAND);
            int distance = abs(cx - x) + abs(cy - y) + (joined ? 0 : WMAP_GRID_SIZE * 2);

            if (g_wmap_cells[cx][cy].placement_allowed && (best < 0 || distance < best_distance))
            {
                best = i;
                best_distance = distance;
            }
        }

        if (best < 0)
        {
            char reason[64];

            snprintf(reason, sizeof(reason), "no cell on the world map takes artifact %d", artifact);
            fail(lom_native_field_draw_count(), reason);
            return 0;
        }

        return world_map_cursor_toward(best) ? world_map_cursor_toward(best) : BUTTON_CROSS;
    default:
        return 0;
    }
}

/**
 * @brief Fail a world map step that ran out of time, logging the world map's
 *        state and its lands to say why.
 * @param step The step.
 */
static void fail_world_map_step(const Step* step)
{
    char reason[96];

    snprintf(reason, sizeof(reason), "the world map step didn't finish (%s %d; party at cell %d, %d)", route_step_names[step->kind], step->args[LAND_ARG_LAND],
             (int)g_wmap_travelers[0].cell_x, (int)g_wmap_travelers[0].cell_y);
    fprintf(stderr,
            "route-runner: world map: ready %d, input locked %d, scrolling %d, selection %d, sequences %d, party moving %d %d, exit %d, "
            "cursor %d, %d, view %d, %d, artifact %d, placing %d %d; lands by cell:",
            g_run.world_map_ready, (int)g_wmap_input_locked, (int)g_wmap_view_scroll_mode, (int)g_wmap_selection_phase, (int)g_wmap_sequence_count,
            (int)g_wmap_party_moving, (int)g_wmap_travelers[0].moving, (int)g_wmap_exit_frame, (int)g_wmap_cursor_column, (int)g_wmap_cursor_row,
            (int)g_wmap_view.x, (int)g_wmap_view.y, (int)g_wmap_selected_artifact, (int)g_wmap_artifact_placement_active, (int)g_wmap_artifact_placement_frame);

    for (int i = 0; i < WMAP_GRID_SIZE * WMAP_GRID_SIZE; i++)
    {
        fprintf(stderr, "%s%d", i % WMAP_GRID_SIZE ? " " : " | ", (int)g_wmap_land_lookup[i]);
    }

    fprintf(stderr, "\n");
    fail(lom_native_field_draw_count(), reason);
}

/**
 * @brief Wait for the world map to take input: it has to have started over
 *        since the route arrived, and run a few frames.
 * @return Nonzero once it does.
 */
static int world_map_ready(void)
{
    if (!g_run.world_map_ready && g_wmap_frame_count != g_run.world_map_frames_before && g_wmap_frame_count <= WMAP_FRAME_COUNT_RESTARTED)
    {
        g_run.world_map_ready = -1;
    }

    if (g_run.world_map_ready == -1 && g_wmap_frame_count >= WMAP_INPUT_GRACE_FRAMES)
    {
        g_run.world_map_ready = 1;
    }

    return g_run.world_map_ready == 1;
}

/**
 * @brief Count a timed step's frames on the world map, and go on to the next
 *        step when they're done.
 * @param frames How many frames the step lasts.
 * @param note A note to record when it's done, or NULL.
 * @return Nonzero once it's done.
 */
static int world_map_timed_step(unsigned frames, const char* note)
{
    if (!s_world_map_watch_since)
    {
        s_world_map_watch_since = g_run.vblanks;
    }

    if (g_run.vblanks - s_world_map_watch_since < frames)
    {
        return 0;
    }

    if (note)
    {
        route_record_note(note);
    }

    s_world_map_watch_since = 0;
    g_run.step++;
    continue_on_world_map(lom_native_field_draw_count());
    return 1;
}

/**
 * @brief Check whether the artifact a place step put down is on the map and
 *        the map has settled, and go on to the next step if so.
 * @param step The step.
 * @return Nonzero once it's placed.
 */
static int artifact_placed(const Step* step)
{
    int land = artifact_land(step->args[PLACE_ARG_ARTIFACT]);

    s_world_map_idle = world_map_land_cell(land) >= 0 && !g_wmap_input_locked && !g_wmap_view_scroll_mode && !g_wmap_sequence_count &&
                               g_wmap_selection_phase == WMAP_SELECTION_MAP && !g_wmap_event_active
                           ? s_world_map_idle + 1
                           : 0;

    if (s_world_map_idle >= WORLD_MAP_IDLE_VBLANKS)
    {
        char note[64];

        snprintf(note, sizeof(note), "placed artifact %d on the world map as land %d", step->args[PLACE_ARG_ARTIFACT], land);
        route_record_note(note);
        g_run.step++;
        continue_on_world_map(lom_native_field_draw_count());
        return 1;
    }

    return 0;
}

/**
 * @brief The buttons for the world map steps, one tap at a time as the cursor moves a cell per press.
 * @return Hardware button bits.
 */
unsigned short world_map_buttons(void)
{
    const Step* step = &g_run.steps[g_run.step];

    if (g_game_state != GAME_STATE_WORLD_MAP)
    {
        return 0;
    }

    if (g_run.vblanks - s_world_map_since > WORLD_MAP_LIMIT)
    {
        fail_world_map_step(step);
        return 0;
    }

    if (!world_map_ready())
    {
        return 0;
    }

    if (step->kind == STEP_PRESS || step->kind == STEP_WAIT)
    {
        unsigned frames =
            step->kind == STEP_PRESS ? (unsigned)(step->args[PRESS_ARG_FRAMES] > 0 ? step->args[PRESS_ARG_FRAMES] : 1) : (unsigned)step->args[WAIT_ARG_FRAMES];

        if (world_map_timed_step(frames, NULL))
        {
            return 0;
        }

        return step->kind == STEP_PRESS ? (unsigned short)step->args[PRESS_ARG_BUTTONS] : 0;
    }

    if (step->kind == STEP_WATCH)
    {
        world_map_timed_step((unsigned)step->args[WATCH_ARG_FRAMES], "watched the world map");
        return 0;
    }

    if (step->kind == STEP_PLACE && artifact_placed(step))
    {
        return 0;
    }

    if (g_run.vblanks % WORLD_MAP_TAP_PERIOD >= LOAD_TAP_VBLANKS)
    {
        return 0;
    }

    return step->kind == STEP_PLACE ? place_buttons(step) : land_buttons(step);
}

/**
 * @brief FIELD is back after a land step: check the scene the land led to and go on.
 * @param frame Current FIELD frame.
 * @param scene The scene FIELD started in.
 */
void arrive_from_world_map(unsigned frame, int scene)
{
    const Step* step = &g_run.steps[g_run.step];
    char note[128];

    if (step->kind == STEP_PLACE)
    {
        int land = artifact_land(step->args[PLACE_ARG_ARTIFACT]);

        if (!(g_saved_game.layout.lands[land].flags & SAVED_LAND_ON_MAP))
        {
            snprintf(note, sizeof(note), "the world map handed over to the field (scene %d) before artifact %d was placed", scene,
                     step->args[PLACE_ARG_ARTIFACT]);
            fail(frame, note);
            return;
        }

        snprintf(note, sizeof(note), "placed artifact %d as land %d; its effect handed over to the field (scene %d)", step->args[PLACE_ARG_ARTIFACT], land,
                 scene);
        route_record_note(note);

        if (is_world_map_step(g_run.step + 1))
        {
            fail(frame, "the world map handed over to the field during the place step, before the world map steps after it");
            return;
        }

        next_step(frame);
        return;
    }

    snprintf(note, sizeof(note), "entered land %d from the world map at scene %d", step->args[LAND_ARG_LAND], scene);
    route_record_note(note);

    if (step->args[LAND_ARG_SCENE] != ANY_SCENE && scene != step->args[LAND_ARG_SCENE])
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "land %d led to scene %d, not %d", step->args[LAND_ARG_LAND], scene, step->args[LAND_ARG_SCENE]);
        fail(frame, reason);
        return;
    }

    next_step(frame);
}
