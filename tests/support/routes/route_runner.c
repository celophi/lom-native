#include "route_runner.h"

static const char SCRIPT_TRACE_VARIABLE[] = "LOM_ROUTE_SCRIPT_TRACE";

static const char FAILURE_CAPTURE_VARIABLE[] = "LOM_ROUTE_FAILURE_CAPTURE";

static const char WIDESCREEN_VARIABLE[] = "LOM_ROUTE_WIDESCREEN";

static const char RESOLUTION_VARIABLE[] = "LOM_ROUTE_RESOLUTION";

static const char MUSIC_ONLY_VARIABLE[] = "LOM_ROUTE_MUSIC_ONLY";

void lom_sound_set_player_volumes(int music, int effects);

int Psyz_VideoSetInternalResolution(unsigned multiplier);

RouteRun g_run = {
    .vblank_limit = DEFAULT_VBLANK_LIMIT,
    .title_item = TITLE_ITEM_CONTINUE,
    .title_back = TITLE_BACK_NONE,
    .title_idle = TITLE_IDLE_NONE,
    .card_browse = CARD_BROWSE_NONE,
    .phase = PHASE_LOADING,
    .other_person = NO_ACTOR,
};

extern s32 __real_field_start_interaction(s32 actor_id, s32 script);

extern void __real_field_script_run(FieldScriptState* context);

extern s32 __real_cdrom_queue_seek(s32 resource);

static int s_failed;

static unsigned s_game_steps;

static unsigned s_most_vblanks_per_step;

static int s_quitting;

static unsigned s_last_game_state = ~0u;

static unsigned s_last_hook_vblank;

static int s_wait_control_scene;

static int s_watch_scene;

static unsigned s_watch_scene_since;

static unsigned s_watch_frames;

static int s_script_runs_to_log;

/**
 * @brief Log every CD resource the game seeks to, to the runner's log, with the scene change it serves.
 * @param resource CD resource number.
 * @return What cdrom_queue_seek returns.
 */
s32 __wrap_cdrom_queue_seek(s32 resource)
{
    fprintf(stderr, "route-runner: cd seek resource %#x at frame %u (scene %d, transition requested %d to scene %#x, pending scene %#x)\n", (unsigned)resource,
            lom_native_field_draw_count(), (int)(g_field_scene_id & SCENE_MASK), (int)g_field_runtime->transition.bits.requested,
            (unsigned)g_field_runtime->transition.fields.scene_id, (unsigned)g_field_pending_scene_id);
    return __real_cdrom_queue_seek(resource);
}

/**
 * @brief Log every interaction the game tries to start (talking, and the scripts of markers walked into), and
 *        why one didn't start, to the runner's log. Not the record: it's for working out failures.
 * @param actor_id Actor interacted with.
 * @param script Script or message id.
 * @return What field_start_interaction returns.
 */
s32 __wrap_field_start_interaction(s32 actor_id, s32 script)
{
    s32 started = __real_field_start_interaction(actor_id, script);

    fprintf(stderr, "route-runner: interaction actor %d script %#x at frame %u: %s (script running %d, interactions blocked %d, transition %d)\n",
            (int)actor_id, (unsigned)script, lom_native_field_draw_count(), started ? "started" : "refused",
            g_field_runtime->script.frames[g_field_runtime->script.depth].pc != NULL, (int)field_get_script_var(0, SCRIPT_VAR_INTERACTIONS_BLOCKED),
            (int)g_field_runtime->transition.bits.requested);
    s_script_runs_to_log = started ? SCRIPT_RUNS_LOGGED : 0;

    if (started && (script & FIELD_INTERACTION_SCRIPT) && g_run.steps[g_run.step].kind == STEP_EXIT &&
        (script & INTERACTION_SCRIPT_NUMBER) == g_run.steps[g_run.step].args[EXIT_ARG_LABEL])
    {
        g_run.exit_script_started = 1;
    }

    if (started && (script & FIELD_INTERACTION_SCRIPT) && actor_id == 0)
    {
        g_run.hero_script_started = 1;
    }

    if (started && actor_id != 0 && g_run.steps[g_run.step].kind == STEP_TALK && g_run.steps[g_run.step].args[TALK_ARG_RECORD] != NO_RECORD)
    {
        if (actor_id == g_run.steps[g_run.step].args[TALK_ARG_RECORD] + FIELD_ACTION_ID_BASE)
        {
            g_run.person_started = 1;
        }
        else
        {
            g_run.other_person = actor_id;
        }
    }

    if (started && (script & FIELD_INTERACTION_SCRIPT))
    {
        const u16* table = g_field_event_scripts;
        int index = script & INTERACTION_SCRIPT_NUMBER;

        fprintf(stderr, "route-runner: event scripts at %p, table[%d..%d] = %#x %#x %#x, script %d starts with %#x %#x %#x\n", (const void*)table, index,
                index + 2, table[index], table[index + 1], table[index + 2], index, ((const u8*)table + table[index])[0], ((const u8*)table + table[index])[1],
                ((const u8*)table + table[index])[2]);
    }

    return started;
}

/**
 * @brief Log the main script's first steps after an interaction starts, to the runner's log.
 * @param context Script context to run.
 */
void __wrap_field_script_run(FieldScriptState* context)
{
    FieldScriptFrame* record = &context->frames[context->depth];
    int log = context == &g_field_runtime->script && s_script_runs_to_log > 0;
    const u8* pc = record->pc;
    static int trace = -1;

    if (trace < 0)
    {
        trace = getenv(SCRIPT_TRACE_VARIABLE) != NULL;
    }

    if (trace && pc)
    {
        __real_field_script_run(context);
        fprintf(stderr, "route-runner: script %p from %#x status %#x stack", (void*)context, (unsigned)(pc - (const u8*)g_field_event_scripts),
                (unsigned)context->status.word);

        for (int i = 0; i <= context->depth && i < FIELD_SCRIPT_FRAME_COUNT; i++)
        {
            const u8* frame_pc = context->frames[i].pc;

            fprintf(stderr, " %#x", frame_pc ? (unsigned)(frame_pc - (const u8*)g_field_event_scripts) : 0u);
        }

        fprintf(stderr, "\n");
        return;
    }

    if (log)
    {
        fprintf(stderr, "route-runner: main script before: depth %d pc %p opcode %#x wait %d status %#x\n", (int)context->depth, (const void*)pc,
                pc ? (unsigned)*pc : 0u, (int)record->wait.bits.frames, (unsigned)context->status.word);
    }

    __real_field_script_run(context);

    if (log)
    {
        record = &context->frames[context->depth];
        fprintf(stderr, "route-runner: main script after: depth %d pc %p status %#x transition %d\n", (int)context->depth, (const void*)record->pc,
                (unsigned)context->status.word, (int)g_field_runtime->transition.bits.requested);
        s_script_runs_to_log--;
    }
}

/**
 * @brief Start tracking a walk's progress afresh: nothing reached yet, and no time stuck.
 * @param frame Current FIELD frame.
 */
void restart_progress(unsigned frame)
{
    g_run.best = NO_PROGRESS_YET;
    g_run.last_progress = frame;
}

/**
 * @brief Switch phase, starting its frame count and progress tracking.
 * @param phase New phase.
 * @param frame Current FIELD frame.
 */
void enter_phase(Phase phase, unsigned frame)
{
    g_run.phase = phase;
    g_run.phase_start = g_run.last_progress = frame;
    g_run.phase_vblank = g_run.vblanks;
    g_run.best = NO_PROGRESS_YET;
}

/**
 * @brief Ask the game to quit, which ends the run.
 */
void request_quit(void)
{
    SDL_Event quit = {.type = SDL_EVENT_QUIT};

    if (!s_quitting)
    {
        s_quitting = 1;
        SDL_PushEvent(&quit);
    }
}

/**
 * @brief Save the displayed frame as a PPM, to see what went wrong.
 * @param path Output file.
 */
static void capture_screen(const char* path)
{
    static unsigned short pixels[CAPTURE_PIXELS];
    DISPENV display;
    FILE* file;

    GetDispEnv(&display);
    DrawSync(0);

    if (display.disp.w * display.disp.h > CAPTURE_PIXELS || StoreImage(&display.disp, (u_long*)pixels) < 0 || !(file = fopen(path, "wb")))
    {
        return;
    }

    fprintf(file, "P6 %d %d 255\n", display.disp.w, display.disp.h);

    for (int i = 0; i < display.disp.w * display.disp.h; i++)
    {
        unsigned short pixel = pixels[i];
        unsigned char rgb[3] = {(unsigned char)((pixel & COLOR_MASK) << COLOR_TO_8_BITS),
                                (unsigned char)(((pixel >> COLOR_BITS) & COLOR_MASK) << COLOR_TO_8_BITS),
                                (unsigned char)(((pixel >> (2 * COLOR_BITS)) & COLOR_MASK) << COLOR_TO_8_BITS)};

        fwrite(rgb, 1, sizeof(rgb), file);
    }

    fclose(file);
}

/**
 * @brief Stop the route because a step failed.
 * @param frame Current FIELD frame.
 * @param reason What went wrong.
 */
void fail(unsigned frame, const char* reason)
{
    char note[480], contact[280] = "";

    if (g_run.steps[g_run.step].kind == STEP_EXIT && route_field_running())
    {
        describe_contact(contact, sizeof(contact));
    }

    snprintf(note, sizeof(note), "failed step %d (%s) at frame %u: %s%s%s%s%s", g_run.step, route_step_names[g_run.steps[g_run.step].kind], frame, reason,
             g_game_state == GAME_STATE_FIELD && g_field_runtime && g_field_runtime->state.bits.battle_active ? " (in a battle)" : "",
             g_game_state == GAME_STATE_FIELD && g_field_return_to_title_prompt_state ? " (hero defeated)" : "", contact[0] ? "; " : "", contact);
    fprintf(stderr, "route: %s\n", note);
    route_record_note(note);

    if (getenv(FAILURE_CAPTURE_VARIABLE))
    {
        capture_screen(getenv(FAILURE_CAPTURE_VARIABLE));
    }

    s_failed = 1;
    g_run.held = 0;
    enter_phase(PHASE_DONE, frame);
    request_quit();
}

/**
 * @brief Finish the current step and move on.
 * @param frame Current FIELD frame.
 */
void next_step(unsigned frame)
{
    g_run.held = 0;
    g_run.step++;
    g_run.replans = 0;

    if (g_run.step >= g_run.step_count)
    {
        enter_phase(PHASE_DONE, frame);
        request_quit();
        return;
    }

    enter_phase(PHASE_START, frame);
}

/**
 * @brief Make one of the game's audio calls by name, as an audio step does: the AKAO calls the port provides
 *        (src/audio/akao_native.c), since those are the ones the game makes, and FIELD's effect-table calls (a
 *        scene's or a monster's effect set, a weapon's table), which its scene loads and actor scripts make.
 * @param name The call's name in akao_cmd.h without the akao_ prefix.
 * @param a Its arguments, in order; ones it doesn't take are ignored.
 * @return 0 when there's no call by that name.
 */
static int audio_call(const char* name, const int a[4])
{
    if (!strcmp(name, "stop_song"))
    {
        akao_stop_song(a[0]);
    }
    else if (!strcmp(name, "stop_all_songs"))
    {
        akao_stop_all_songs();
    }
    else if (!strcmp(name, "set_song_volume"))
    {
        akao_set_song_volume(a[0], a[1]);
    }
    else if (!strcmp(name, "fade_song_volume"))
    {
        akao_fade_song_volume(a[0], a[1], a[2]);
    }
    else if (!strcmp(name, "fade_song_volume_from"))
    {
        akao_fade_song_volume_from(a[0], a[1], a[2], a[3]);
    }
    else if (!strcmp(name, "fade_xa_volume"))
    {
        akao_fade_xa_volume(a[0], a[1]);
    }
    else if (!strcmp(name, "pause_audio"))
    {
        akao_pause_audio((u32)a[0]);
    }
    else if (!strcmp(name, "resume_audio"))
    {
        akao_resume_audio((u32)a[0]);
    }
    else if (!strcmp(name, "set_mono_output"))
    {
        akao_set_mono_output(a[0]);
    }
    else if (!strcmp(name, "set_master_volume"))
    {
        akao_set_master_volume(a[0]);
    }
    else if (!strcmp(name, "set_master_tempo_modifier"))
    {
        akao_set_master_tempo_modifier(a[0]);
    }
    else if (!strcmp(name, "play_sound"))
    {
        akao_play_sound(a[0]);
    }
    else if (!strcmp(name, "play_sfx"))
    {
        akao_play_sfx(a[0], a[1], a[2], a[3]);
    }
    else if (!strcmp(name, "stop_sfx"))
    {
        akao_stop_sfx(a[0], a[1]);
    }
    else if (!strcmp(name, "fade_sfx_volume"))
    {
        akao_fade_sfx_volume(a[0], a[1], a[2], a[3]);
    }
    else if (!strcmp(name, "fade_sfx_pan"))
    {
        akao_fade_sfx_pan(a[0], a[1], a[2], a[3]);
    }
    else if (!strcmp(name, "set_all_sfx_volume"))
    {
        akao_set_all_sfx_volume(a[0]);
    }
    else if (!strcmp(name, "fade_all_sfx_volume"))
    {
        akao_fade_all_sfx_volume(a[0], a[1]);
    }
    else if (!strcmp(name, "release_all_sfx"))
    {
        akao_release_all_sfx();
    }
    else if (!strcmp(name, "load_sfx_tables"))
    {
        field_load_sfx_tables(a[0]);
    }
    else if (!strcmp(name, "play_set_sfx"))
    {
        field_play_set_sfx(a[0], a[1], 0, a[2]);
    }
    else if (!strcmp(name, "load_weapon_sfx_table"))
    {
        field_load_weapon_sfx_table(a[0], a[1]);
    }
    else if (!strcmp(name, "play_weapon_sfx"))
    {
        field_play_weapon_sfx(a[0], a[1], a[2]);
    }
    else
    {
        return 0;
    }

    return 1;
}

/**
 * @brief Start walking: to an exit, to a point, or to someone to talk to, finding where a parked actor is first.
 * @param frame Current FIELD frame.
 * @param z Hero z, for walks that keep it.
 */
static void start_walk_step(unsigned frame, int z)
{
    const Step* step = &g_run.steps[g_run.step];

    g_run.via_index = 0;
    g_run.talk_side = 0;
    g_run.talk_tries = 0;
    g_run.talk_x = step->args[TALK_ARG_X];
    g_run.talk_z = step->args[TALK_ARG_Z];

    g_run.other_person = NO_ACTOR;

    if (step->kind == STEP_TALK && step->args[TALK_ARG_RECORD] != NO_RECORD && step->args[TALK_ARG_PARKED])
    {
        Vec3i position;

        if (field_get_actor_position(step->args[TALK_ARG_RECORD] + FIELD_ACTION_ID_BASE, &position) >= 0)
        {
            char note[80];

            g_run.talk_x = position.x >> POSITION_SHIFT;
            g_run.talk_z = position.z >> POSITION_SHIFT;
            snprintf(note, sizeof(note), "layout record %d's actor is at (%d, %d)", step->args[TALK_ARG_RECORD], g_run.talk_x, g_run.talk_z);
            route_record_note(note);
        }
    }

    g_run.interrupted = 0;
    g_run.blocker_waits = 0;

    if (set_next_target(z))
    {
        fail(frame, "this scene has no exit marker with that label");
        return;
    }

    start_walk(frame);
}

/**
 * @brief Give the hero or a party member experience, and level them up.
 * @param frame Current FIELD frame.
 */
static void add_experience(unsigned frame)
{
    const Step* step = &g_run.steps[g_run.step];
    int index = step->args[EXPERIENCE_ARG_CHARACTER];
    FieldCharacterRecord* hero = &g_field_game_state->characters[index];
    unsigned experience = hero->progress.bits.experience + (unsigned)step->args[EXPERIENCE_ARG_AMOUNT];
    char note[96];

    hero->progress.bits.experience = experience > FIELD_EXPERIENCE_MAX ? FIELD_EXPERIENCE_MAX : experience;
    field_apply_character_level_ups(index, 0);
    snprintf(note, sizeof(note), "%s at level %d with %d hp", index ? "party member" : "hero", (int)hero->progress.level, (int)hero->hp);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Set the hero's hit points, or another party member's, from 1 to
 *        their maximum, for routes that need the party to fall.
 * @param frame Current FIELD frame.
 */
static void set_hit_points(unsigned frame)
{
    const int index = g_run.steps[g_run.step].args[HIT_POINTS_ARG_CHARACTER];
    FieldObjectState* hero = &g_field_object_states[index];
    unsigned hp = (unsigned)g_run.steps[g_run.step].args[HIT_POINTS_ARG_VALUE];
    char note[64];

    if (index < 0 || index > 2 || hero->maximum_hp == 0)
    {
        fail(frame, "there's no party member in that slot");
        return;
    }

    hp = hp < 1 ? 1 : hp > hero->maximum_hp ? hero->maximum_hp : hp;
    hero->current_hp.word = hp;
    hero->hp_display.bits.value = hp;
    snprintf(note, sizeof(note), "%s at %u of %u hp", index ? "party member" : "hero", hp, (unsigned)hero->maximum_hp);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Have a guest join the party.
 * @param frame Current FIELD frame.
 */
static void add_guest(unsigned frame)
{
    const Step* step = &g_run.steps[g_run.step];
    char note[64];

    if (!field_join_guest(step->args[GUEST_ARG_ID]))
    {
        fail(frame, "the game has no template for that guest");
        return;
    }

    field_set_script_var(0, FIELD_VARIABLE_GUEST_VARIANT, step->args[GUEST_ARG_ID]);
    snprintf(note, sizeof(note), "guest %d joined", step->args[GUEST_ARG_ID]);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Pick which option the route answers choices with from now on.
 * @param frame Current FIELD frame.
 */
static void choose_answer(unsigned frame)
{
    const Step* step = &g_run.steps[g_run.step];
    char note[64];

    if (step->args[ANSWER_ARG_OPTION] < 0 || step->args[ANSWER_ARG_OPTION] > ANSWER_LIMIT)
    {
        fail(frame, "an answer has to be 0 to 4");
        return;
    }

    g_run.answer = step->args[ANSWER_ARG_OPTION];
    snprintf(note, sizeof(note), "answering choices with option %d", g_run.answer);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Add a companion to the pet slots and have it join.
 * @param frame Current FIELD frame.
 */
static void add_companion(unsigned frame)
{
    const Step* step = &g_run.steps[g_run.step];
    char note[64];
    s32 slot;
    s32 variant;

    slot = field_add_stored_companion(step->args[COMPANION_ARG_TEMPLATE]);

    if (slot >= PET_RECORD_COUNT)
    {
        fail(frame, "the game couldn't add that companion (no templates here, or every pet slot is in use)");
        return;
    }

    g_gosub_result_count = 1;
    g_gosub_result_values[0] = slot;
    variant = field_join_companion();

    if (variant == FIELD_NO_VARIANT)
    {
        fail(frame, "the companion didn't join");
        return;
    }

    field_set_script_var(0, FIELD_VARIABLE_COMPANION_VARIANT, variant);
    snprintf(note, sizeof(note), "companion %d joined as pet %d, species %d", step->args[COMPANION_ARG_TEMPLATE], slot, variant);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Make one of the sound driver calls the route asks for.
 * @param frame Current FIELD frame.
 */
static void make_audio_call(unsigned frame)
{
    const Step* step = &g_run.steps[g_run.step];
    char note[128];

    if (!audio_call(step->name, step->args))
    {
        fail(frame, "no such audio call");
        return;
    }

    snprintf(note, sizeof(note), "audio %s %d %d %d %d", step->name, step->args[0], step->args[1], step->args[2], step->args[3]);
    route_record_note(note);
    next_step(frame);
}

/**
 * @brief Begin the current step.
 * @param frame Current FIELD frame.
 * @param z Hero z, for walks that keep it.
 */
void start_step(unsigned frame, int z)
{
    const Step* step = &g_run.steps[g_run.step];

    switch (step->kind)
    {
    case STEP_ENTER:
        field_script_op_85(step->args[ENTER_ARG_SCENE], step->args[ENTER_ARG_OBJECT], step->args[ENTER_ARG_AUDIO], step->args[ENTER_ARG_SPAWN]);
        g_run.wait_scene = step->args[ENTER_ARG_SCENE] & SCENE_MASK;
        enter_phase(PHASE_WAIT_SCENE, frame);
        break;
    case STEP_EXIT:
    case STEP_WALK:
    case STEP_TALK:
        start_walk_step(frame, z);
        break;
    case STEP_PRESS:
        g_run.held = (unsigned short)step->args[PRESS_ARG_BUTTONS];
        enter_phase(PHASE_PRESS, frame);
        break;
    case STEP_WAIT:
        enter_phase(PHASE_WAIT, frame);
        break;
    case STEP_WAIT_SCENE:
        g_run.wait_scene = step->args[WAIT_SCENE_ARG_SCENE] & SCENE_MASK;
        enter_phase(PHASE_WAIT_SCENE, frame);
        break;
    case STEP_WAIT_BATTLE:
        enter_phase(PHASE_WAIT_BATTLE, frame);
        break;
    case STEP_CHECKPOINT:
        route_record_checkpoint(step->name, frame, g_field_scene_id & SCENE_MASK);
        next_step(frame);
        break;
    case STEP_WIDESCREEN:
        lom_native_widescreen_set_enabled(step->args[0]);
        next_step(frame);
        break;
    case STEP_ALWAYS_RUN:
        lom_native_switch_set_setting(LOM_SWITCH_ALWAYS_RUN, step->args[0]);
        next_step(frame);
        break;
    case STEP_SWITCH:
        if (lom_native_switch_find(step->name) < 0)
        {
            fail(frame, "the plan names a switch this build doesn't have");
            break;
        }

        lom_native_switch_set_setting((LomSwitch)lom_native_switch_find(step->name), step->args[0]);
        next_step(frame);
        break;
    case STEP_EXPERIENCE:
        add_experience(frame);
        break;
    case STEP_HIT_POINTS:
        set_hit_points(frame);
        break;
    case STEP_GUEST:
        add_guest(frame);
        break;
    case STEP_ANSWER:
        choose_answer(frame);
        break;
    case STEP_COMPANION:
        add_companion(frame);
        break;
    case STEP_WATCH:
        g_run.controllable = 0;
        s_watch_scene = g_field_scene_id & SCENE_MASK;
        s_watch_scene_since = frame;
        s_watch_frames = (unsigned)step->args[WATCH_ARG_FRAMES];
        enter_phase(PHASE_WATCH, frame);
        break;
    case STEP_FIGHT:
        enter_phase(PHASE_FIGHT_WAIT, frame);
        break;
    case STEP_LAND:
    case STEP_PLACE:
        fail(frame, "a world map step needs the steps before it to leave for the world map");
        break;
    case STEP_EVENT:
        enter_phase(PHASE_EVENT, frame);
        break;
    case STEP_AUDIO:
        make_audio_call(frame);
        break;
    }
}

/**
 * @brief Record what this frame covers: the scene, an exit being taken, monsters in a battle.
 * @param scene Current scene.
 */
static void observe_coverage(int scene)
{
    static int s_exit_noted;

    route_record_cover(ROUTE_COVER_SCENE, (unsigned)scene, 0, 0);

    if (!g_field_scene_request_pending)
    {
        s_exit_noted = 0;
    }
    else if (!s_exit_noted && g_run.phase == PHASE_WALK)
    {
        FieldCollisionQuery probe = {0};
        int label;

        probe.x = g_field_actors[0].x;
        probe.y = g_field_actors[0].y;
        probe.z = g_field_actors[0].z;
        probe.width = CONTACT_WIDTH;
        probe.height_tolerance = CONTACT_HEIGHT;
        probe.depth = CONTACT_DEPTH;
        label = field_collision_hit_markers(&probe);
        s_exit_noted = 1;

        if (g_run.steps[g_run.step].kind == STEP_EXIT && g_run.exit_script_started)
        {
            label = g_run.steps[g_run.step].args[EXIT_ARG_LABEL];
        }

        if (label >= 0)
        {
            route_record_cover(ROUTE_COVER_EXIT, (unsigned)scene, (unsigned)label, (unsigned)g_field_pending_scene_id & SCENE_MASK);
        }
    }

    if (g_field_battle != NULL && g_field_runtime && g_field_runtime->state.bits.battle_active)
    {
        for (int i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
        {
            FieldStatusRecord* record = &g_field_battle->records[i];

            if (record->side == FIELD_RECORD_MONSTER_SIDE && record->template != NULL)
            {
                route_record_cover(ROUTE_COVER_MONSTER, (unsigned)scene, record->template->id, 0);
            }
        }
    }
}

/**
 * @brief Log the FIELD state the route runs into whenever it changes: the
 *        scene, its event scripts, and any transition or state change pending.
 * @param frame The FIELD frame.
 * @param scene The scene.
 */
static void log_field_state(unsigned frame, int scene)
{
    if (g_run.phase != PHASE_LOADING && route_field_running())
    {
        static int s_logged_scene = -1, s_logged_transition = -1, s_logged_pending_state = -1;
        static const void* s_logged_scripts;
        static unsigned s_logged_first;
        const u16* scripts = g_field_event_scripts;
        unsigned first = scripts ? scripts[0] : 0;

        if (scene != s_logged_scene || (const void*)scripts != s_logged_scripts || first != s_logged_first ||
            (int)g_field_runtime->transition.bits.requested != s_logged_transition || (int)g_pending_game_state != s_logged_pending_state)
        {
            fprintf(stderr,
                    "route-runner: frame %u scene %d, event scripts %p first entry %#x, transition %d to %#x, scene request %d pending %#x, pending game state "
                    "%d\n",
                    frame, scene, (const void*)scripts, first, (int)g_field_runtime->transition.bits.requested,
                    (unsigned)g_field_runtime->transition.fields.scene_id, (int)g_field_scene_request_pending, (unsigned)g_field_pending_scene_id,
                    (int)g_pending_game_state);
            s_logged_scene = scene;
            s_logged_scripts = scripts;
            s_logged_first = first;
            s_logged_transition = g_field_runtime->transition.bits.requested;
            s_logged_pending_state = g_pending_game_state;
        }
    }
}

/**
 * @brief Start the step: walking, talking and the like wait for the player to have control first; the rest start at once.
 * @param frame The FIELD frame.
 * @param scene The scene.
 * @param z The hero's z.
 */
static void start_frame(unsigned frame, int scene, int z)
{
    StepKind kind = g_run.steps[g_run.step].kind;

    if (kind == STEP_ENTER || kind == STEP_EXIT || kind == STEP_WALK || kind == STEP_TALK || kind == STEP_EXPERIENCE || kind == STEP_HIT_POINTS ||
        kind == STEP_GUEST || kind == STEP_COMPANION)
    {
        g_run.controllable = 0;
        s_wait_control_scene = scene;
        enter_phase(PHASE_WAIT_CONTROL, frame);
    }
    else
    {
        start_step(frame, z);
    }
}

/**
 * @brief Wait for the player to have control, pressing through any text, then start the step.
 * @param frame The FIELD frame.
 * @param scene The scene.
 * @param z The hero's z.
 */
static void wait_for_control_frame(unsigned frame, int scene, int z)
{
    g_run.controllable = player_has_control() ? g_run.controllable + 1 : 0;
    g_run.held = !g_run.controllable ? confirm_buttons(frame - g_run.phase_start, TEXT_CONFIRM_PERIOD, TEXT_CONFIRM_PERIOD / 2) : 0;

    if (g_run.controllable >= CONTROL_FRAMES && g_run.steps[g_run.step].kind == STEP_EXIT && scene != s_wait_control_scene)
    {
        char reason[96];

        snprintf(reason, sizeof(reason), "an event moved the hero from scene %d to scene %d before the walk", s_wait_control_scene, scene);
        fail(frame, reason);
    }
    else if (g_run.controllable >= CONTROL_FRAMES)
    {
        g_run.held = 0;
        start_step(frame, z);
    }
    else if (frame - g_run.phase_start > CONTROL_LIMIT)
    {
        char reason[128];

        snprintf(reason, sizeof(reason), "the player never got control (interaction %d, text %d, control mode %d, scene change %d)",
                 (int)g_field_interaction_active, (int)g_field_text_session_active, (int)(g_field_actors[0].control.word & FIELD_CONTROL_MODE_MASK),
                 (int)g_field_scene_request_pending);
        fail(frame, reason);
    }
}

/**
 * @brief Try to start the step's event script every few frames, pressing through text, until it starts.
 * @param frame The FIELD frame.
 */
static void event_frame(unsigned frame)
{
    g_run.held = confirm_buttons(frame - g_run.phase_start, TEXT_CONFIRM_PERIOD, TEXT_CONFIRM_PERIOD / 2);

    if ((frame - g_run.phase_start) % EVENT_TRY_PERIOD == 0 &&
        field_start_interaction(g_run.steps[g_run.step].args[0] + FIELD_ACTION_ID_BASE, g_run.steps[g_run.step].args[1]))
    {
        char note[96];

        snprintf(note, sizeof(note), "started layout record %d's script %#x", g_run.steps[g_run.step].args[0], (unsigned)g_run.steps[g_run.step].args[1]);
        route_record_note(note);
        g_run.held = 0;
        next_step(frame);
    }
    else if (frame - g_run.phase_start > CONTROL_LIMIT)
    {
        fail(frame, "the game never started that event (no such actor, or another script kept running)");
    }
}

/**
 * @brief Wait for the step's battle to start, pressing through text when the step asks.
 * @param frame The FIELD frame.
 */
static void fight_wait_frame(unsigned frame)
{
    g_run.held = 0;

    if (g_run.steps[g_run.step].args[FIGHT_ARG_CONFIRM])
    {
        g_run.controllable = player_has_control() ? g_run.controllable + 1 : 0;
        g_run.held = !g_run.controllable ? confirm_buttons(frame - g_run.phase_start, TEXT_CONFIRM_PERIOD, TEXT_CONFIRM_PERIOD / 2) : 0;
    }

    if (g_field_runtime->state.bits.battle_active)
    {
        route_record_note("a battle is on; fighting it");
        enter_phase(PHASE_FIGHT, frame);
    }
    else if (frame - g_run.phase_start > (unsigned)g_run.steps[g_run.step].args[FIGHT_ARG_FRAMES])
    {
        fail(frame, "no battle started");
    }
}

/**
 * @brief Wait for the scene the step is going to.
 * @param frame The FIELD frame.
 * @param scene The scene.
 */
static void wait_scene_frame(unsigned frame, int scene)
{
    g_run.held = 0;

    if (scene == g_run.wait_scene && !g_field_scene_request_pending)
    {
        next_step(frame);
    }
    else if (frame - g_run.phase_start >
             (unsigned)(g_run.steps[g_run.step].kind == STEP_WAIT_SCENE ? g_run.steps[g_run.step].args[WAIT_SCENE_ARG_FRAMES] : SCENE_LIMIT))
    {
        fail(frame, "the scene was never entered");
    }
}

/**
 * @brief Turn the hero toward who they're about to talk to, then press to talk.
 * @param frame The FIELD frame.
 * @param x The hero's x.
 * @param z The hero's z.
 */
static void face_frame(unsigned frame, int x, int z)
{
    g_run.held = frame - g_run.phase_start < FACE_FRAMES ? toward(x, z, g_run.talk_x, g_run.talk_z) : 0;

    if (frame - g_run.phase_start == TALK_PRESS_FRAME)
    {
        g_run.held = BUTTON_CROSS;
        g_run.talk_busy = 0;
        g_run.controllable = 0;
        enter_phase(PHASE_TALK, frame);
    }
}

/**
 * @brief Hold the step's buttons for its frames.
 * @param frame The FIELD frame.
 */
static void press_frame(unsigned frame)
{
    if (frame - g_run.phase_start >= (unsigned)(g_run.steps[g_run.step].args[PRESS_ARG_FRAMES] > 0 ? g_run.steps[g_run.step].args[PRESS_ARG_FRAMES] : 1))
    {
        next_step(frame);
    }
}

/**
 * @brief Wait the step's frames.
 * @param frame The FIELD frame.
 */
static void wait_frame(unsigned frame)
{
    g_run.held = 0;

    if (frame - g_run.phase_start >= (unsigned)g_run.steps[g_run.step].args[WAIT_ARG_FRAMES])
    {
        next_step(frame);
    }
}

/**
 * @brief Wait for a battle to start.
 * @param frame The FIELD frame.
 */
static void wait_battle_frame(unsigned frame)
{
    g_run.held = 0;

    if (g_field_runtime->state.bits.battle_active)
    {
        next_step(frame);
    }
    else if (frame - g_run.phase_start > (unsigned)g_run.steps[g_run.step].args[WAIT_BATTLE_ARG_FRAMES])
    {
        fail(frame, "no battle started");
    }
}

/**
 * @brief Watch an event play, pressing through text, until the player has control, the scene stalls, or the step's frames run out.
 * @param frame The FIELD frame.
 * @param scene The scene.
 */
static void watch_frame(unsigned frame, int scene)
{
    g_run.controllable = player_has_control() ? g_run.controllable + 1 : 0;
    g_run.held = !g_run.controllable ? confirm_buttons(frame - g_run.phase_start, TEXT_CONFIRM_PERIOD, TEXT_CONFIRM_PERIOD / 2) : 0;

    if (scene != s_watch_scene)
    {
        s_watch_scene = scene;
        s_watch_scene_since = frame;
    }

    if (!g_run.controllable && frame - s_watch_scene_since >= WATCH_STALL_FRAMES)
    {
        char note[96];

        snprintf(note, sizeof(note), "watched %u frames; no control and no scene change for %u", frame - g_run.phase_start, (unsigned)WATCH_STALL_FRAMES);
        route_record_note(note);
        g_run.held = 0;
        next_step(frame);
        return;
    }

    if (g_run.controllable >= CONTROL_FRAMES || frame - g_run.phase_start >= s_watch_frames)
    {
        char note[96];

        snprintf(note, sizeof(note),
                 g_run.controllable >= CONTROL_FRAMES ? "watched until the player got control, %u frames" : "watched %u frames, still no control",
                 frame - g_run.phase_start);
        route_record_note(note);
        g_run.held = 0;
        next_step(frame);
    }
}

/**
 * @brief Runs before every FIELD frame: record it, then advance the current step.
 */
static void frame_hook(void)
{
    unsigned frame = lom_native_field_draw_count();
    int scene = g_field_scene_id & SCENE_MASK;
    int x, z;

    log_field_state(frame, scene);
    s_last_hook_vblank = g_run.vblanks;
    hero_position(&x, &z);
    route_record_frame(frame, scene, x, z);
    observe_coverage(scene);

    switch (g_run.phase)
    {
    case PHASE_WORLD_MAP:
        if (route_field_running())
        {
            arrive_from_world_map(frame, scene);
        }

        break;
    case PHASE_LOADING:
        g_run.held = 0;
        g_run.step = 0;
        enter_phase(g_run.step_count ? PHASE_START : PHASE_DONE, frame);

        if (!g_run.step_count)
        {
            request_quit();
        }

        break;
    case PHASE_START:
        start_frame(frame, scene, z);
        break;
    case PHASE_WAIT_CONTROL:
        wait_for_control_frame(frame, scene, z);
        break;
    case PHASE_EVENT:
        event_frame(frame);
        break;
    case PHASE_WALK:
        walk_frame(frame, x, z);
        break;
    case PHASE_FIGHT:
        fight_frame(frame, x, z);
        break;
    case PHASE_FIGHT_WAIT:
        fight_wait_frame(frame);
        break;
    case PHASE_WAIT_SCENE:
        wait_scene_frame(frame, scene);
        break;
    case PHASE_FACE:
        face_frame(frame, x, z);
        break;
    case PHASE_TALK:
        talk_frame(frame);
        break;
    case PHASE_PRESS:
        press_frame(frame);
        break;
    case PHASE_WAIT:
        wait_frame(frame);
        break;
    case PHASE_WAIT_BATTLE:
        wait_battle_frame(frame);
        break;
    case PHASE_WATCH:
        watch_frame(frame, scene);
        break;
    case PHASE_DONE:
        g_run.held = 0;
        break;
    }
}

/**
 * @brief Note each change of game state, and follow the ones the route was
 *        waiting for: the world map an exit leads to, or a watched event
 *        leaving the field, which ends the route.
 */
static void follow_game_state(void)
{
    if (g_game_state == s_last_game_state)
    {
        return;
    }

    char note[96];

    snprintf(note, sizeof(note), "game state %u at vblank %u", (unsigned)g_game_state, g_run.vblanks);
    route_record_note(note);
    s_last_game_state = g_game_state;

    if (g_game_state == GAME_STATE_WORLD_MAP && g_run.phase == PHASE_WALK && g_run.steps[g_run.step].kind == STEP_EXIT &&
        (g_run.steps[g_run.step].args[EXIT_ARG_SCENE] == WORLD_MAP_SCENE || g_run.steps[g_run.step].args[EXIT_ARG_SCENE] == ANY_SCENE))
    {
        leave_for_world_map(lom_native_field_draw_count());
    }

    if (g_game_state != GAME_STATE_FIELD && g_run.phase == PHASE_WATCH)
    {
        snprintf(note, sizeof(note), "watched until the game left the field for state %u; the route ends here", (unsigned)g_game_state);
        route_record_note(note);
        g_run.held = 0;
        g_run.step = g_run.step_count - 1;
        enter_phase(PHASE_DONE, lom_native_field_draw_count());
        request_quit();
    }
}

/**
 * @brief Count the vblank, save the screen when FIELD has stopped drawing
 *        for a while, and fail the run when it reaches its frame limit.
 */
static void watch_run(void)
{
    if (g_run.vblanks + 1 - s_last_hook_vblank > SCREEN_STALL_VBLANKS)
    {
        route_record_screen(g_run.vblanks + 1);
    }

    if (++g_run.vblanks >= g_run.vblank_limit && !s_quitting)
    {
        route_record_note("failed: the run's frame limit was reached");
        fprintf(stderr, "route: the run's frame limit was reached (last FIELD frame %u vblanks ago, game state %u)\n", g_run.vblanks - s_last_hook_vblank,
                (unsigned)g_game_state);

        if (getenv(FAILURE_CAPTURE_VARIABLE))
        {
            capture_screen(getenv(FAILURE_CAPTURE_VARIABLE));
        }

        s_failed = 1;
        request_quit();
    }
}

/**
 * @brief The buttons to hold this vblank. Usually the current step's; while
 *        loading, on the world map, or while FIELD has stopped drawing because
 *        a screen of its own or an event is up, what that needs instead.
 * @param held The current step's buttons.
 * @return The buttons to hold.
 */
static unsigned short choose_buttons(unsigned short held)
{
    if (g_run.phase == PHASE_LOADING)
    {
        held = g_run.vblanks % LOAD_PRESS_PERIOD < LOAD_TAP_VBLANKS ? loading_button() : 0;
    }
    else if (g_run.phase == PHASE_WORLD_MAP)
    {
        held = world_map_buttons();
    }
    else if ((g_run.phase == PHASE_PRESS || g_run.phase == PHASE_WAIT) && g_run.vblanks - s_last_hook_vblank > SCREEN_STALL_VBLANKS && !s_failed)
    {
        held = screen_step();
    }
    else if (g_run.phase == PHASE_START && (g_run.steps[g_run.step].kind == STEP_PRESS || g_run.steps[g_run.step].kind == STEP_WAIT) &&
             g_run.vblanks - s_last_hook_vblank > MODAL_STALL_VBLANKS && !s_failed)
    {
        unsigned frame = lom_native_field_draw_count();

        while (g_run.phase == PHASE_START &&
               (g_run.steps[g_run.step].kind == STEP_PRESS || g_run.steps[g_run.step].kind == STEP_WAIT || g_run.steps[g_run.step].kind == STEP_CHECKPOINT))
        {
            start_step(frame, 0);
        }
        held = g_run.held;
    }
    else if (g_run.phase == PHASE_TALK && g_run.steps[g_run.step].kind == STEP_TALK && g_run.steps[g_run.step].args[TALK_ARG_SCREEN] &&
             (g_run.vblanks - s_last_hook_vblank > SCREEN_STALL_VBLANKS || g_field_ring_menu_state != 0 || g_field_modal_state != 0) && !s_failed)
    {
        route_record_note(g_field_ring_menu_state != 0 ? "the talk opened the ring menu; the steps after it play there"
                                                       : "the talk opened a screen of its own; the steps after it play there");
        next_screen_step();
        held = g_run.held;
    }
    else if ((g_run.phase == PHASE_WALK || g_run.phase == PHASE_TALK) && g_run.steps[g_run.step].kind == STEP_TALK &&
             g_run.steps[g_run.step].args[TALK_ARG_EVENT] && g_run.vblanks - s_last_hook_vblank > MODAL_STALL_VBLANKS && !s_failed)
    {
        unsigned frame = lom_native_field_draw_count();

        route_record_note("an event left the field for a screen of its own; watching it");
        g_run.controllable = 0;
        s_watch_scene = g_field_scene_id & SCENE_MASK;
        s_watch_scene_since = frame;
        s_watch_frames = EVENT_WATCH_FRAMES;
        enter_phase(PHASE_WATCH, frame);
    }
    else if (g_run.phase == PHASE_EVENT && g_run.vblanks - s_last_hook_vblank > SCREEN_STALL_VBLANKS && !s_failed)
    {
        held = g_run.vblanks % TEXT_CONFIRM_PERIOD == TEXT_CONFIRM_PERIOD / 2 ? BUTTON_CROSS : 0;
    }
    else if ((g_run.phase == PHASE_WAIT_CONTROL || g_run.phase == PHASE_WALK) && g_run.vblanks - s_last_hook_vblank > MODAL_STALL_VBLANKS && !s_failed)
    {
        fail(lom_native_field_draw_count(), "an event left the field for a screen of its own");
    }
    else if (g_run.phase == PHASE_WATCH && g_run.vblanks - s_last_hook_vblank > MODAL_STALL_VBLANKS)
    {
        held = g_run.vblanks % LOAD_PRESS_PERIOD < LOAD_TAP_VBLANKS ? BUTTON_CROSS : 0;
    }
    else if (g_run.phase == PHASE_TALK && g_run.vblanks - s_last_hook_vblank > MODAL_STALL_VBLANKS)
    {
        unsigned tap = (g_run.vblanks / MODAL_TAP_PERIOD) % MODAL_TAP_CYCLE;

        held = g_run.vblanks % MODAL_TAP_PERIOD < LOAD_TAP_VBLANKS ? (tap == MODAL_TAP_CIRCLE     ? BUTTON_CIRCLE
                                                                      : tap == MODAL_TAP_TRIANGLE ? BUTTON_TRIANGLE
                                                                                                  : 0)
                                                                   : 0;
    }

    return held;
}

/**
 * @brief Write a digital pad's receive buffer holding some buttons.
 * @param pad The buffer.
 * @param held The buttons, as a US player presses them. The Japanese game
 *        confirms with Circle and cancels with Cross (the US one swaps them
 *        when it reads the pad), so for it they're swapped here, and plans
 *        mean the same on both.
 */
static void write_pad(unsigned char* pad, unsigned short held)
{
#if defined(VERSION_JP)
    held = (unsigned short)((held & ~(BUTTON_CIRCLE | BUTTON_CROSS)) | (held & BUTTON_CIRCLE ? BUTTON_CROSS : 0) | (held & BUTTON_CROSS ? BUTTON_CIRCLE : 0));
#endif
    unsigned short raw = (unsigned short)~held;

    pad[0] = PAD_STATUS_OK;
    pad[1] = PAD_DIGITAL;
    pad[2] = (unsigned char)(raw & 0xFF);
    pad[3] = (unsigned char)(raw >> 8);
}

/**
 * @brief The session's pad script: port 1 is a digital pad holding g_run.held. Also the run's watchdog.
 * @param vblank Unused.
 * @param port_buffers The ports' raw receive buffers.
 */
static void pad_script(unsigned long long vblank, unsigned char* port_buffers[2])
{
    unsigned short held = g_run.held;

    (void)vblank;

    follow_game_state();
    watch_run();

    if (g_run.phase == PHASE_LOADING && g_game_state == GAME_STATE_WORLD_MAP && is_world_map_step(0))
    {
        g_run.world_map_frames_before = g_wmap_frame_count;
        g_run.world_map_ready = 0;
        g_run.step = 0;
        continue_on_world_map(lom_native_field_draw_count());
    }

    held = choose_buttons(held);

    if (port_buffers[0])
    {
        write_pad(port_buffers[0], held);
    }
}

/**
 * @brief Fail the run when a route's mod was refused or its scripts stopped,
 *        so a test mod's own checks, which stop it when they fail, fail the
 *        route too.
 */
static void check_mods(void)
{
    for (int i = 0; i < lom_native_mods_refused_count() && !s_failed; ++i)
    {
        char note[256];

        snprintf(note, sizeof(note), "failed: a mod was refused (%s)", lom_native_mods_refused(i));
        route_record_note(note);
        s_failed = 1;
    }

    for (int i = 0; i < lom_native_mods_count() && !s_failed; ++i)
    {
        if (!lom_native_mods_running(i))
        {
            char note[160];

            snprintf(note, sizeof(note), "failed: the mod %s stopped (its error is in runner.log)", lom_native_mods_id(i));
            route_record_note(note);
            s_failed = 1;
        }
    }
}

/**
 * @brief Close the record and exit with the route's result. Runs however the
 *        game ends: outside FIELD, the game exits the process itself on a quit.
 *        A refused or stopped mod fails the run.
 */
void finish(void)
{
    static int s_finished;

    if (s_finished)
    {
        return;
    }

    s_finished = 1;

    if (g_run.phase != PHASE_DONE && !s_failed)
    {
        char note[96];

        snprintf(note, sizeof(note), "failed: the game stopped during step %d", g_run.step);
        route_record_note(note);
        s_failed = 1;
    }

    check_mods();

    route_record_close(g_run.phase == PHASE_DONE && !s_failed ? g_run.step_count : g_run.step, g_run.step_count, s_failed);

    if (__llvm_profile_write_file)
    {
        __llvm_profile_write_file();
    }

    fflush(NULL);
    _exit(s_failed ? EXIT_STEP_FAILED : EXIT_DONE);
}

/**
 * @brief Boot the game, pick Continue, and run the plan.
 * @param argc Argument count.
 * @param argv Disc root, plan file and record folder.
 * @return EXIT_DONE, EXIT_STEP_FAILED or EXIT_SETUP.
 */
int main(int argc, char** argv)
{
    LomNativeGameStatus status;

    setvbuf(stdout, NULL, _IOLBF, 0);

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <disc root> <plan file> <record folder>\n", argv[0]);
        return EXIT_SETUP;
    }

    if (read_plan(argv[2]) || route_record_open(argv[3]))
    {
        return EXIT_SETUP;
    }

    if (getenv(WIDESCREEN_VARIABLE))
    {
        lom_native_widescreen_set_enabled(1);
    }

    lom_native_mods_start(LOM_NATIVE_DISC_SERIAL);
    status = lom_native_game_initialize(argv[1]);

    if (status != LOM_NATIVE_GAME_RUNNING)
    {
        fprintf(stderr, "route: the game didn't start (%d)\n", (int)status);
        return EXIT_SETUP;
    }

    if (getenv(MUSIC_ONLY_VARIABLE))
    {
        lom_sound_set_player_volumes(100, 0);
    }

    if (getenv(RESOLUTION_VARIABLE))
    {
        Psyz_VideoSetInternalResolution((unsigned)atoi(getenv(RESOLUTION_VARIABLE)));
    }

    atexit(finish);
    lom_native_session_set_pad_script(pad_script);
    lom_native_field_set_frame_hook(frame_hook);

    while (status == LOM_NATIVE_GAME_RUNNING)
    {
        unsigned before = g_run.vblanks;
        status = lom_native_game_step();
        ++s_game_steps;

        if (g_run.vblanks - before > s_most_vblanks_per_step)
        {
            s_most_vblanks_per_step = g_run.vblanks - before;
        }
    }

    fprintf(stderr, "route: %u steps, at most %u vblanks in one\n", s_game_steps, s_most_vblanks_per_step);

    if (s_most_vblanks_per_step > MOST_VBLANKS_PER_STEP && !s_failed)
    {
        char note[128];
        snprintf(note, sizeof(note), "failed: one step ran the game for %u vblanks without handing control back to the port", s_most_vblanks_per_step);
        route_record_note(note);
        s_failed = 1;
    }

    lom_native_game_shutdown();
    finish();
    return s_failed ? EXIT_STEP_FAILED : EXIT_DONE;
}
