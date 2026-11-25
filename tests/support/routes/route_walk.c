#include "route_runner.h"

static FieldPathPoint s_path[PATH_CAPACITY];

static int s_path_length;

static int s_path_index;

static int s_target_x;

static int s_target_y;

static int s_target_z;

static int s_sidestep_frames;

static int s_sidestep_side;

static int s_heading_via;

static unsigned s_interrupted_since;

static int s_blocker_frames;

static int s_walk_scene;

/**
 * @brief The hero's position in map pixels.
 * @param x Receives x.
 * @param z Receives z.
 */
void hero_position(int* x, int* z)
{
    *x = g_field_actors[0].x >> POSITION_SHIFT;
    *z = g_field_actors[0].z >> POSITION_SHIFT;
}

/**
 * @brief Whether the player has control: no script, text, modal screen or scene change running.
 * @return Non-zero when the pad moves the hero.
 */
int player_has_control(void)
{
    return !g_field_interaction_active && !g_field_text_session_active && !(g_field_actors[0].control.word & FIELD_CONTROL_MODE_MASK) &&
           !g_field_scene_request_pending;
}

/**
 * @brief Ask the game's pathfinder for a route between two points, starting at the hero's height.
 * @param from_x Start x, in map pixels.
 * @param from_z Start z.
 * @param to_x Goal x.
 * @param to_y Goal height; the pathfinder uses the floor at or below it.
 * @param to_z Goal z.
 * @param out Receives up to LEG_CAPACITY waypoints.
 * @return The number of waypoints, or a negative pathfinder error.
 */
static int plan_leg(int from_x, int from_z, int to_x, int to_y, int to_z, FieldPathPoint* out)
{
    FieldCollisionQuery start = {0}, goal = {0};

    start.x = from_x << POSITION_SHIFT;
    start.y = g_field_actors[0].y;
    start.z = from_z << POSITION_SHIFT;
    start.width = goal.width = FIELD_FOOTPRINT_WIDTH;
    start.depth = goal.depth = FIELD_FOOTPRINT_DEPTH;
    start.height_tolerance = goal.height_tolerance = FIELD_FOOTPRINT_STEP;
    field_collision_dilate_query(&start);
    goal.x = to_x << POSITION_SHIFT;
    goal.y = to_y;
    goal.z = to_z << POSITION_SHIFT;
    return field_collision_find_path(&start, &goal, out, 0);
}

/**
 * @brief Plan a route from the hero to the current target.
 * @return The number of waypoints, or a negative pathfinder error.
 */
static int plan_path(void)
{
    int x, z;

    hero_position(&x, &z);
    s_path_length = plan_leg(x, z, s_target_x, s_target_y, s_target_z, s_path);
    s_path_index = 0;
    return s_path_length;
}

/**
 * @brief The marker with a label in the current scene.
 * @param label Marker label (the exit's script).
 * @return The marker, or NULL.
 */
static FieldMarker* find_marker(int label)
{
    for (FieldMarker* marker = g_field_scene.scene ? g_field_scene.scene->markers : NULL; marker != NULL; marker = marker->next)
    {
        if (marker->def->label == label)
        {
            return marker;
        }
    }

    return NULL;
}

/**
 * @brief The marker label a hero standing at a point and height would touch, using the game's own walk-in probe.
 * @param x Map x.
 * @param y Height, in the actors' units.
 * @param z Map z.
 * @return The label, or a negative value for none.
 */
static int marker_at_height(int x, int y, int z)
{
    FieldCollisionQuery probe = {0};

    probe.x = x << POSITION_SHIFT;
    probe.y = y;
    probe.z = z << POSITION_SHIFT;
    probe.width = CONTACT_WIDTH;
    probe.height_tolerance = CONTACT_HEIGHT;
    probe.depth = CONTACT_DEPTH;
    return field_collision_hit_markers(&probe);
}

/**
 * @brief The marker label a hero standing at a point would touch, at the hero's height.
 * @param x Map x.
 * @param z Map z.
 * @return The label, or a negative value for none.
 */
static int marker_at(int x, int z)
{
    return marker_at_height(x, g_field_actors[0].y, z);
}

/**
 * @brief Describe why the game might not run an exit's script, in the terms of its own marker contact checks.
 * @param out Receives the description.
 * @param size Size of @p out.
 */
void describe_contact(char* out, size_t size)
{
    FieldActorRecord* hero = field_find_actor_record(0);
    int x, z;

    hero_position(&x, &z);
    snprintf(out, size,
             "contact: marker %d here, player command %d, drop menu %d, battle zone %d, scene request %d, latched %d, transition %d, interactions blocked %d, "
             "script running %d, hero record %s",
             marker_at(x, z), (int)g_field_player_command, (int)g_field_item_drop_menu_open, (int)g_field_active_group, (int)g_field_scene_request_pending,
             (int)g_field_scene_contact_latched, (int)g_field_runtime->transition.bits.requested, (int)field_get_script_var(0, SCRIPT_VAR_INTERACTIONS_BLOCKED),
             g_field_runtime->script.frames[g_field_runtime->script.depth].pc != NULL,
             hero == NULL                   ? "missing"
             : hero->flags.bits.script_only ? "script-only"
             : hero->flags.bits.spawned     ? "spawned"
                                            : "ok");
}

/**
 * @brief Another exit's marker at a point, or within the exit step's clearance of it.
 * @param x Map x.
 * @param z Map z.
 * @param wanted Label of the exit we're heading for.
 * @param start_label Label of the marker the hero started on, which doesn't count.
 * @return The label, or a negative value for none.
 */
static int other_marker_near(int x, int z, int wanted, int start_label)
{
    int clearance = g_run.steps[g_run.step].kind == STEP_EXIT   ? g_run.steps[g_run.step].args[EXIT_ARG_CLEARANCE]
                    : g_run.steps[g_run.step].kind == STEP_TALK ? g_run.steps[g_run.step].args[TALK_ARG_CLEARANCE]
                                                                : 0;
    static const int offsets[][2] = {{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int probes = clearance > 0 ? (int)(sizeof(offsets) / sizeof(offsets[0])) : 1;

    for (int i = 0; i < probes; i++)
    {
        int px = x + offsets[i][0] * clearance, pz = z + offsets[i][1] * clearance;
        int label = marker_at(px, pz);

        if (label >= 0 && label != wanted && label != start_label)
        {
            return label;
        }

        if (g_run.steps[g_run.step].kind == STEP_TALK && g_run.steps[g_run.step].args[TALK_ARG_REGIONS] && g_field_runtime->trigger_table)
        {
            const FieldTriggerTable* table = g_field_runtime->trigger_table;

            for (int r = 0; r < table->count; r++)
            {
                const FieldTriggerRegion* region = &table->regions[r];

                if ((region->command & FIELD_TRIGGER_SCRIPT) && !(g_field_runtime->triggered_regions & (1u << r)) && px >= region->min_x &&
                    px <= region->max_x && pz >= region->min_z && pz <= region->max_z)
                {
                    return TRIGGER_REGION_LABEL + r;
                }
            }
        }
    }

    return -1;
}

/**
 * @brief Whether walking a route would step on a marker other than the one we want.
 * @param points The route's waypoints.
 * @param count Number of waypoints.
 * @param goal_x Where the route ends, after its waypoints.
 * @param goal_z Where the route ends.
 * @param wanted Label of the exit we're heading for.
 * @param cross_x Receives where it first crosses another marker.
 * @param cross_z Receives where it first crosses another marker.
 * @return Non-zero when the route crosses another marker.
 */
static int route_crosses_other_marker(const FieldPathPoint* points, int count, int goal_x, int goal_z, int wanted, int* cross_x, int* cross_z)
{
    int from_x, from_z, start_label;

    hero_position(&from_x, &from_z);
    start_label = marker_at(from_x, from_z);

    for (int i = 0; i <= count; i++)
    {
        int to_x = i < count ? points[i].x >> POSITION_SHIFT : goal_x;
        int to_z = i < count ? points[i].z >> POSITION_SHIFT : goal_z;
        int dx = to_x - from_x, dz = to_z - from_z;
        int length = abs(dx) > abs(dz) ? abs(dx) : abs(dz);

        for (int step = 0; step <= length; step += PATH_SAMPLE_STEP)
        {
            int x = from_x + (length ? dx * step / length : 0);
            int z = from_z + (length ? dz * step / length : 0);
            int label = other_marker_near(x, z, wanted, start_label);

            if (label >= 0)
            {
                *cross_x = x;
                *cross_z = z;
                return 1;
            }
        }

        from_x = to_x;
        from_z = to_z;
    }

    return 0;
}

/**
 * @brief Plan to the target through a point beside where the direct route crosses another exit.
 * @param wanted Label of the exit we're heading for.
 * @param cross_x Where the direct route crosses another marker.
 * @param cross_z Where the direct route crosses another marker.
 * @return Non-zero when a clean detour was found and is now the path.
 */
static int plan_detour(int wanted, int cross_x, int cross_z)
{
    FieldPathPoint first[LEG_CAPACITY], second[LEG_CAPACITY];
    int x, z, dx, dz, length;

    hero_position(&x, &z);
    dx = s_target_x - x;
    dz = s_target_z - z;
    length = abs(dx) > abs(dz) ? abs(dx) : abs(dz);

    if (length == 0)
    {
        return 0;
    }

    for (int offset = 1; offset <= DETOUR_OFFSETS; offset++)
    {
        for (int side = -1; side <= 1; side += 2)
        {
            int via_x = cross_x - side * offset * DETOUR_STEP * dz / length;
            int via_z = cross_z + side * offset * DETOUR_STEP * dx / length;
            int first_count = plan_leg(x, z, via_x, g_field_actors[0].y, via_z, first);
            int second_count, ignored_x, ignored_z;

            if (first_count <= 0)
            {
                continue;
            }

            second_count = plan_leg(via_x, via_z, s_target_x, s_target_y, s_target_z, second);

            if (second_count <= 0)
            {
                continue;
            }

            memcpy(s_path, first, sizeof(first[0]) * (size_t)first_count);
            s_path[first_count].x = via_x << POSITION_SHIFT;
            s_path[first_count].z = via_z << POSITION_SHIFT;
            memcpy(s_path + first_count + 1, second, sizeof(second[0]) * (size_t)second_count);
            s_path_length = first_count + 1 + second_count;
            s_path_index = 0;

            if (!route_crosses_other_marker(s_path, s_path_length, s_target_x, s_target_z, wanted, &ignored_x, &ignored_z))
            {
                return 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Look for the exit's goal on the scene's other floors, for exits up or down stairs.
 * @details The pathfinder searches the floor at or below the goal's height, so a goal
 * given at the hero's height lands on the wrong floor when the exit is higher up. Only
 * floors where the game's own walk-in probe finds the marker count.
 * @param label The exit's marker label.
 * @param goals The candidate goal points inside the marker.
 * @return 0 when a goal was set, -1 when no other floor has a clean route.
 */
static int choose_exit_goal_on_other_floor(int label, const int goals[GOAL_CANDIDATES][2])
{
    const FieldScene* scene = g_field_scene.scene;
    int hero_y = g_field_actors[0].y;
    int hero_floor = hero_y < 0 ? (hero_y + FLOOR_HEIGHT - 1) >> FLOOR_SHIFT : hero_y >> FLOOR_SHIFT;

    for (int floor = 0; scene != NULL && floor < scene->group_count; floor++)
    {
        int y = scene->group_ids[floor] * FLOOR_HEIGHT;

        if (scene->group_ids[floor] == hero_floor)
        {
            continue;
        }

        for (int i = 0; i < GOAL_CANDIDATES; i++)
        {
            int cross_x, cross_z;

            if (marker_at_height(goals[i][0], y, goals[i][1]) != label)
            {
                continue;
            }

            s_target_x = goals[i][0];
            s_target_y = y;
            s_target_z = goals[i][1];

            if (plan_path() > 0 &&
                (!route_crosses_other_marker(s_path, s_path_length, s_target_x, s_target_z, label, &cross_x, &cross_z) || plan_detour(label, cross_x, cross_z)))
            {
                char note[96];

                snprintf(note, sizeof(note), "exit goal on another floor (height %d)", scene->group_ids[floor]);
                route_record_note(note);
                return 0;
            }
        }
    }

    if (scene != NULL)
    {
        char note[256];
        int length = snprintf(note, sizeof(note), "route-runner: no clean route to exit %d on any floor: hero height %d, floors", label, hero_floor);

        for (int floor = 0; floor < scene->group_count && length < (int)sizeof(note) - 16; floor++)
        {
            length += snprintf(note + length, sizeof(note) - (size_t)length, " %d:%d", scene->group_ids[floor],
                               marker_at_height(goals[0][0], scene->group_ids[floor] * FLOOR_HEIGHT, goals[0][1]));
        }

        fprintf(stderr, "%s\n", note);
    }

    return -1;
}

/**
 * @brief Pick a goal inside an exit marker that the pathfinder can reach without crossing other exits.
 * @param label The exit's marker label.
 * @return 0 when a goal was set, -1 when the marker isn't in this scene.
 */
static int choose_exit_goal(int label)
{
    FieldMarker* marker = find_marker(label);
    const FieldMarkerDef* def;
    int px[4], pz[4], cx, cz, goals[GOAL_CANDIDATES][2], fallback = -1;

    if (marker == NULL)
    {
        return -1;
    }

    def = marker->def;
    px[0] = def->x0;
    pz[0] = def->y0;
    px[1] = def->x1;
    pz[1] = def->y1;
    px[2] = def->x0 + (s16)def->offset_x;
    pz[2] = def->y0 + (s16)def->offset_y;
    px[3] = def->x1 + (s16)def->offset_x;
    pz[3] = def->y1 + (s16)def->offset_y;
    cx = (px[0] + px[1] + px[2] + px[3]) / 4;
    cz = (pz[0] + pz[1] + pz[2] + pz[3]) / 4;
    goals[0][0] = cx;
    goals[0][1] = cz;

    for (int i = 0; i < 4; i++)
    {
        static const int edges[4][2] = {{0, 1}, {2, 3}, {0, 2}, {1, 3}};

        goals[1 + i][0] = cx + (px[i] - cx) / 3;
        goals[1 + i][1] = cz + (pz[i] - cz) / 3;
        goals[5 + i][0] = cx + ((px[edges[i][0]] + px[edges[i][1]]) / 2 - cx) / 3;
        goals[5 + i][1] = cz + ((pz[edges[i][0]] + pz[edges[i][1]]) / 2 - cz) / 3;
    }

    s_target_y = g_field_actors[0].y;

    for (int i = 0; i < GOAL_CANDIDATES; i++)
    {
        int cross_x, cross_z;

        s_target_x = goals[i][0];
        s_target_z = goals[i][1];

        if (plan_path() > 0)
        {
            if (!route_crosses_other_marker(s_path, s_path_length, s_target_x, s_target_z, label, &cross_x, &cross_z) || plan_detour(label, cross_x, cross_z))
            {
                return 0;
            }

            if (fallback < 0)
            {
                fallback = i;
            }
        }
    }

    if (choose_exit_goal_on_other_floor(label, goals) == 0)
    {
        return 0;
    }

    s_target_y = g_field_actors[0].y;
    s_target_x = goals[fallback >= 0 ? fallback : 0][0];
    s_target_z = goals[fallback >= 0 ? fallback : 0][1];
    plan_path();
    route_record_note(fallback >= 0 ? "no route to the exit avoids other exits; taking one that crosses one" : "no route to the exit; walking straight at it");
    return 0;
}

/**
 * @brief The d-pad toward a point. Up is +z and right is +x on the map.
 * @param x Hero x.
 * @param z Hero z.
 * @param to_x Point x.
 * @param to_z Point z.
 * @return Buttons to hold.
 */
unsigned short toward(int x, int z, int to_x, int to_z)
{
    unsigned short held = 0;

    held |= to_x > x + 1 ? BUTTON_RIGHT : to_x < x - 1 ? BUTTON_LEFT : 0;
    held |= to_z > z + 1 ? BUTTON_UP : to_z < z - 1 ? BUTTON_DOWN : 0;
    return held;
}

/**
 * @brief Start walking toward the target.
 * @param frame Current FIELD frame.
 */
void start_walk(unsigned frame)
{
    char note[96];

    s_sidestep_frames = 0;
    g_run.exit_script_started = 0;
    g_run.hero_script_started = 0;
    g_run.person_started = 0;
    s_walk_scene = g_field_scene_id & SCENE_MASK;
    snprintf(note, sizeof(note), "walk to (%d, %d): %d waypoints", s_target_x, s_target_z, s_path_length);
    route_record_note(note);
    enter_phase(PHASE_WALK, frame);
}

/**
 * @brief Plan toward the step's next target: its next via point, or its own goal once those are done.
 * @param z Hero z, for walks that keep it.
 * @return 0, or -1 when an exit's marker isn't in this scene.
 */
int set_next_target(int z)
{
    const Step* step = &g_run.steps[g_run.step];

    s_target_y = g_field_actors[0].y;

    if (g_run.via_index < step->via_count)
    {
        s_heading_via = 1;
        s_target_x = step->via[g_run.via_index][0];
        s_target_z = step->via[g_run.via_index][1];
        plan_path();
        return 0;
    }

    s_heading_via = 0;

    switch (step->kind)
    {
    case STEP_EXIT:
        return choose_exit_goal(step->args[EXIT_ARG_LABEL]);
    case STEP_TALK:
    {
        static const int sides[TALK_SIDES][2] = {{0, -TALK_OFFSET}, {0, TALK_OFFSET}, {-TALK_OFFSET, 0}, {TALK_OFFSET, 0}};
        int fallback = -1;
        Vec3i position;

        if (step->args[TALK_ARG_FOLLOW] && step->args[TALK_ARG_RECORD] != NO_RECORD &&
            field_get_actor_position(step->args[TALK_ARG_RECORD] + FIELD_ACTION_ID_BASE, &position) >= 0)
        {
            g_run.talk_x = position.x >> POSITION_SHIFT;
            g_run.talk_z = position.z >> POSITION_SHIFT;
        }

        for (int tries = 0; tries < TALK_SIDES; tries++, g_run.talk_side = (g_run.talk_side + 1) % TALK_SIDES)
        {
            int cross_x, cross_z;

            s_target_x = g_run.talk_x + sides[g_run.talk_side][0];
            s_target_z = g_run.talk_z + sides[g_run.talk_side][1];

            if (plan_path() <= 0)
            {
                continue;
            }

            if (step->args[TALK_ARG_CLEARANCE] <= 0 || !route_crosses_other_marker(s_path, s_path_length, s_target_x, s_target_z, -1, &cross_x, &cross_z) ||
                plan_detour(-1, cross_x, cross_z))
            {
                return 0;
            }

            if (fallback < 0)
            {
                fallback = g_run.talk_side;
            }
        }

        if (fallback >= 0)
        {
            g_run.talk_side = fallback;
            s_target_x = g_run.talk_x + sides[g_run.talk_side][0];
            s_target_z = g_run.talk_z + sides[g_run.talk_side][1];
            plan_path();
            route_record_note("no route to talk from avoids the exits; taking one that crosses one");
        }

        return 0;
    }
    default:
        s_target_x = step->args[WALK_ARG_X];
        s_target_z = step->args[WALK_ARG_Z] < 0 ? z : step->args[WALK_ARG_Z];
        plan_path();
        return 0;
    }
}

/**
 * @brief Fail the step if the scene changed under the hero while walking:
 *        one starting, or one that already happened unseen.
 * @param step The step.
 * @param frame Current FIELD frame.
 * @param x Hero x.
 * @param z Hero z.
 * @return 1 when it failed the step.
 */
static int scene_changed_under_walk(const Step* step, unsigned frame, int x, int z)
{
    if (step->kind != STEP_EXIT && g_field_runtime->transition.bits.requested)
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "a scene change to scene %d started while walking, at (%d, %d)",
                 (int)(g_field_runtime->transition.fields.scene_id & SCENE_MASK), x, z);
        g_run.held = 0;
        fail(frame, reason);
        return 1;
    }

    if ((int)(g_field_scene_id & SCENE_MASK) != s_walk_scene)
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "the hero is in scene %d, not scene %d, and the scene change went unseen", (int)(g_field_scene_id & SCENE_MASK),
                 s_walk_scene);
        g_run.held = 0;
        fail(frame, reason);
        return 1;
    }

    return 0;
}

/**
 * @brief Follow a scene change the walk led to: check it's the exit the step
 *        wanted, then wait for the scene or leave for the world map.
 * @param step The step.
 * @param frame Current FIELD frame.
 * @param x Hero x.
 * @param z Hero z.
 */
static void follow_scene_change(const Step* step, unsigned frame, int x, int z)
{
    int arrived = g_pending_game_state == GAME_STATE_WORLD_MAP ? WORLD_MAP_SCENE : g_field_pending_scene_id & SCENE_MASK;

    g_run.held = 0;

    if (step->kind != STEP_EXIT)
    {
        fail(frame, "a scene change started while walking");
        return;
    }

    int taken = marker_at(x, z);

    if (arrived != WORLD_MAP_SCENE && taken >= 0 && taken != step->args[EXIT_ARG_LABEL] && g_run.exit_script_started)
    {
        char note[96];

        snprintf(note, sizeof(note), "exit %d's script took the hero out through exit %d", step->args[EXIT_ARG_LABEL], taken);
        route_record_note(note);
    }
    else if (arrived != WORLD_MAP_SCENE && taken >= 0 && taken != step->args[EXIT_ARG_LABEL])
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "took exit %d, not exit %d, at (%d, %d)", taken, step->args[EXIT_ARG_LABEL], x, z);
        fail(frame, reason);
        return;
    }

    if (step->args[EXIT_ARG_SCENE] != ANY_SCENE && arrived != step->args[EXIT_ARG_SCENE] && taken < 0 && !g_run.exit_script_started)
    {
        char reason[112];

        snprintf(reason, sizeof(reason), "an event moved the hero from scene %d to scene %d while walking, from (%d, %d)", (int)(g_field_scene_id & SCENE_MASK),
                 arrived, x, z);

        if (!step->args[EXIT_ARG_EVENT] || arrived == WORLD_MAP_SCENE)
        {
            fail(frame, reason);
            return;
        }

        route_record_note(reason);
    }
    else if (step->args[EXIT_ARG_SCENE] != ANY_SCENE && arrived != step->args[EXIT_ARG_SCENE])
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "the exit led to scene %d, not %d, from (%d, %d)", arrived, step->args[EXIT_ARG_SCENE], x, z);
        fail(frame, reason);
        return;
    }

    if (arrived == WORLD_MAP_SCENE)
    {
        leave_for_world_map(frame);
        return;
    }

    g_run.wait_scene = arrived;
    enter_phase(PHASE_WAIT_SCENE, frame);
}

/**
 * @brief Something took control away mid-walk, like text or an event: a talk
 *        that started early goes on as the talk, and anything else is pressed
 *        through until control comes back, or fails if it never does.
 * @param step The step.
 * @param frame Current FIELD frame.
 */
static void walk_without_control(const Step* step, unsigned frame)
{
    if (step->kind == STEP_TALK && !s_heading_via && !g_run.hero_script_started)
    {
        g_run.talk_busy = 1;
        g_run.controllable = 0;
        enter_phase(PHASE_TALK, frame);
        return;
    }

    if (!g_run.interrupted)
    {
        g_run.interrupted = 1;
        s_interrupted_since = frame;
    }
    else if (frame - s_interrupted_since > INTERRUPTION_LIMIT)
    {
        fail(frame, "something took control away while walking and never gave it back");
        return;
    }

    g_run.held = confirm_buttons(frame - g_run.phase_start, TEXT_CONFIRM_PERIOD, TEXT_CONFIRM_PERIOD / 2);
    restart_progress(frame);
}

/**
 * @brief The hero reached the point they were heading for: go on to the next
 *        waypoint or via point, or finish the walk.
 * @param step The step.
 * @param frame Current FIELD frame.
 * @param z Hero z.
 * @return 1 when that settled the frame; 0 to keep walking.
 */
static int arrive(const Step* step, unsigned frame, int z)
{
    if (s_path_index < s_path_length)
    {
        s_path_index++;
        restart_progress(frame);
        return 1;
    }

    if (s_heading_via)
    {
        g_run.via_index++;
        set_next_target(z);
        restart_progress(frame);
        return 1;
    }

    if (step->kind == STEP_TALK)
    {
        enter_phase(PHASE_FACE, frame);
        return 1;
    }

    if (step->kind == STEP_WALK)
    {
        next_step(frame);
        return 1;
    }

    return 0;
}

/**
 * @brief Note whether the hero got closer; when they haven't for a while,
 *        sidestep, wait for whatever's in the way, or give up.
 * @param step The step.
 * @param frame Current FIELD frame.
 * @param x Hero x.
 * @param z Hero z.
 * @param to_x Where they're heading.
 * @param to_z Where they're heading.
 * @param distance How far away that is, squared.
 * @return 1 when that settled the frame; 0 to keep walking.
 */
static int check_progress(const Step* step, unsigned frame, int x, int z, int to_x, int to_z, int distance)
{
    if (distance < g_run.best - PROGRESS_DISTANCE_SQUARED)
    {
        g_run.best = distance;
        g_run.last_progress = frame;
    }
    else if (frame - g_run.last_progress >= STUCK_FRAMES)
    {
        if (++g_run.replans >= PLAN_ATTEMPTS)
        {
            if (step->kind == STEP_TALK && !s_heading_via)
            {
                enter_phase(PHASE_FACE, frame);
                return 1;
            }

            if (g_run.blocker_waits < BLOCKER_WAITS)
            {
                g_run.blocker_waits++;
                s_blocker_frames = BLOCKER_WAIT_FRAMES;
                g_run.held = 0;
                return 1;
            }

            char reason[128];

            snprintf(reason, sizeof(reason), "stuck at (%d, %d) heading for (%d, %d), waypoint %d of %d", x, z, to_x, to_z, s_path_index, s_path_length);
            fail(frame, reason);
            return 1;
        }

        s_sidestep_frames = SIDESTEP_FRAMES;
        s_sidestep_side ^= 1;
        restart_progress(frame);
    }

    return 0;
}

/**
 * @brief One frame of walking toward the target, waypoint by waypoint.
 * @param frame Current FIELD frame.
 * @param x Hero x.
 * @param z Hero z.
 */
void walk_frame(unsigned frame, int x, int z)
{
    const Step* step = &g_run.steps[g_run.step];
    int to_x, to_z, distance;

    if (scene_changed_under_walk(step, frame, x, z))
    {
        return;
    }

    if (g_field_scene_request_pending || g_pending_game_state == GAME_STATE_WORLD_MAP)
    {
        follow_scene_change(step, frame, x, z);
        return;
    }

    if (g_field_runtime->state.bits.battle_active && step->kind == STEP_WALK)
    {
        next_step(frame);
        return;
    }

    if (g_field_runtime->state.bits.battle_active && step->kind == STEP_EXIT)
    {
        route_record_note("a battle started on the way to the exit; fighting it");
        g_run.held = 0;
        enter_phase(PHASE_FIGHT, frame);
        return;
    }

    if (!player_has_control())
    {
        walk_without_control(step, frame);
        return;
    }

    if (g_run.interrupted)
    {
        g_run.interrupted = 0;
        g_run.hero_script_started = 0;
        set_next_target(z);
    }

    if (s_blocker_frames > 0)
    {
        g_run.held = 0;

        if (--s_blocker_frames == 0)
        {
            g_run.replans = 0;
            set_next_target(z);
            restart_progress(frame);
        }

        return;
    }

    to_x = s_path_index < s_path_length ? s_path[s_path_index].x >> POSITION_SHIFT : s_target_x;
    to_z = s_path_index < s_path_length ? s_path[s_path_index].z >> POSITION_SHIFT : s_target_z;
    distance = (to_x - x) * (to_x - x) + (to_z - z) * (to_z - z);

    if (distance <= ARRIVED_DISTANCE_SQUARED && arrive(step, frame, z))
    {
        return;
    }

    if (check_progress(step, frame, x, z, to_x, to_z, distance))
    {
        return;
    }

    if (s_sidestep_frames > 0)
    {
        unsigned short ahead = toward(x, z, to_x, to_z);

        g_run.held = (ahead & (BUTTON_LEFT | BUTTON_RIGHT)) ? (s_sidestep_side ? BUTTON_UP : BUTTON_DOWN) : (s_sidestep_side ? BUTTON_LEFT : BUTTON_RIGHT);

        if (--s_sidestep_frames == 0)
        {
            set_next_target(z);
        }

        return;
    }

    g_run.held = toward(x, z, to_x, to_z);
}
