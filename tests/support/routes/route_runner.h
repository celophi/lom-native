#ifndef LOM_ROUTE_RUNNER_H
#define LOM_ROUTE_RUNNER_H

#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_records.h"
#include "overlays/field/internal/field_calls.h"
#include "overlays/field/internal/field_modal_runtime.h"
#include "overlays/field/internal/field_actor.h"
#include "overlays/field/internal/field_script.h"
#include "overlays/field/internal/field_actor_runtime.h"
#include "overlays/field/internal/field_scene_internal.h"
#include "overlays/field/internal/field_state_ops.h"
#include "overlays/field/field_overlay_handoff.h"
#include "main/game_state.h"
#include "main/field_entry.h"
#include "lom_native_ps1_memory.h"
#include "main/audio/akao_cmd.h"
#include "lom_native_field.h"
#include "lom_native_game.h"
#include "lom_native_session.h"
#include "route_record.h"
#include "lom_native_widescreen.h"
#include "lom_native_switches.h"
#include "lom_native_mods.h"
#include "lom_native_cd.h"
#include <SDL3/SDL_events.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void field_script_op_85(s32 scene_id, s32 object_id, s32 audio, s32 spawn_id);

extern s32 field_start_interaction(s32 actor_id, s32 script);

extern s32 g_field_scene_request_pending, g_field_pending_scene_id, g_pending_game_state;

#define g_field_scene_id (g_field_entry_scene.word)

extern s32 g_field_ring_menu_state, g_field_modal_state;

extern u8 g_title_selected_item;

extern s32 g_slot_slide_x, g_slot_slide_frames;

#define g_cload_choice_toggle (*(s32*)LOM_PS1_ADDRESS(0x8014A924u))
#define g_cload_selected_row (*(s32*)LOM_PS1_ADDRESS(0x80162354u))
#define lom_native_cload_g_card_entry_state (*(s32*)LOM_PS1_ADDRESS(0x80162350u))

extern FieldActorRecord* field_find_actor_record(s32 id);

#define g_field_player_command (g_field_actors[0].command)

extern int __llvm_profile_write_file(void) __attribute__((weak));

typedef struct
{
    s32 x;

    s32 y;

    s32 projection_scale;

    s32 unknown_0c;
} RouteWmapView;

typedef struct
{
    s32 cell_x;

    s32 cell_y;

    s16 next_cell_x;

    s16 next_cell_y;

    s16 position_x;

    s16 position_y;

    s16 target_x;

    s16 target_y;

    s32 moving;
} RouteWmapTraveler;

typedef struct
{
    s32 land_id;

    s16 placement_allowed;

    s16 travel_allowed;

    s32 spirit_sprites[8];
} RouteWmapCell;

extern RouteWmapView g_wmap_view;

extern RouteWmapCell g_wmap_cells[6][6];

extern s32 wmap_get_selected_artifact(void);

extern RouteWmapTraveler g_wmap_travelers[];

extern u8 g_wmap_land_lookup[];

extern s32 g_wmap_cursor_column, g_wmap_cursor_row, g_wmap_input_locked, g_wmap_view_scroll_mode, g_wmap_selection_phase, g_wmap_party_moving,
    g_wmap_sequence_count, g_wmap_exit_frame, g_wmap_frame_count, g_wmap_carousel_turn_frames, g_wmap_artifact_placement_active,
    g_wmap_artifact_placement_frame, g_wmap_selected_artifact, g_wmap_event_active, g_wmap_script_wait;

enum
{
    EXIT_DONE = 0,
    EXIT_STEP_FAILED = 1,
    EXIT_SETUP = 2,
};

enum
{
    STEP_CAPACITY = 512,
    STEP_ARGS = 10,
    NAME_BYTES = 64,
    VIA_CAPACITY = 8,
    DEFAULT_VBLANK_LIMIT = 60000,
};

enum
{
    ENTER_ARG_SCENE = 0,
    ENTER_ARG_OBJECT = 1,
    ENTER_ARG_AUDIO = 2,
    ENTER_ARG_SPAWN = 3,
    EXIT_ARG_LABEL = 0,
    EXIT_ARG_SCENE = 1,
    EXIT_ARG_CLEARANCE = 2,
    EXIT_ARG_EVENT = 3,
    WALK_ARG_X = 0,
    WALK_ARG_Z = 1,
    TALK_ARG_X = 0,
    TALK_ARG_Z = 1,
    TALK_ARG_EVENT = 2,
    TALK_ARG_LONG = 3,
    TALK_ARG_CLEARANCE = 4,
    TALK_ARG_RECORD = 5,
    TALK_ARG_REGIONS = 6,
    TALK_ARG_PARKED = 7,
    TALK_ARG_SCREEN = 8,
    TALK_ARG_FOLLOW = 9,
    PRESS_ARG_BUTTONS = 0,
    PRESS_ARG_FRAMES = 1,
    WAIT_ARG_FRAMES = 0,
    WAIT_SCENE_ARG_SCENE = 0,
    WAIT_SCENE_ARG_FRAMES = 1,
    WAIT_BATTLE_ARG_FRAMES = 0,
    EXPERIENCE_ARG_AMOUNT = 0,
    EXPERIENCE_ARG_CHARACTER = 1,
    GUEST_ARG_ID = 0,
    COMPANION_ARG_TEMPLATE = 0,
    FIGHT_ARG_FRAMES = 0,
    FIGHT_ARG_CONFIRM = 1,
    FIGHT_ARG_ROTATE = 2,
    FIGHT_ARG_FOR = 3,
    ANSWER_ARG_OPTION = 0,
    HIT_POINTS_ARG_VALUE = 0,
    HIT_POINTS_ARG_CHARACTER = 1,
    WATCH_ARG_FRAMES = 0,
    LAND_ARG_LAND = 0,
    LAND_ARG_SCENE = 1,
    PLACE_ARG_ARTIFACT = 0,
};

enum
{
    BUTTON_START = 0x0008,
    BUTTON_UP = 0x0010,
    BUTTON_RIGHT = 0x0020,
    BUTTON_DOWN = 0x0040,
    BUTTON_LEFT = 0x0080,
    BUTTON_TRIANGLE = 0x1000,
    BUTTON_CIRCLE = 0x2000,
    BUTTON_CROSS = 0x4000,
    BUTTON_SQUARE = 0x8000,
};

enum
{
    PAD_STATUS_OK = 0x00,
    PAD_DIGITAL = 0x41,
};

enum
{
    SCENE_MASK = 0x7FFF,
    WORLD_MAP_SCENE = 0x7FFF,
    ANY_SCENE = -1,
    POSITION_SHIFT = 8,
    FLOOR_SHIFT = 8,
    FLOOR_HEIGHT = 1 << FLOOR_SHIFT,
    INTERACTION_SCRIPT_NUMBER = FIELD_INTERACTION_SCRIPT - 1,
    SCRIPT_VAR_INTERACTIONS_BLOCKED = 0xFE1,
    RECORD_MONSTER = 0x40,
    FIELD_ACTION_ID_BASE = 3,
    NO_RECORD = -1,
    NO_ACTOR = -1,
    CONTACT_WIDTH = 8,
    CONTACT_HEIGHT = 16,
    CONTACT_DEPTH = 5,
    LEG_CAPACITY = 16,
    PATH_CAPACITY = 2 * LEG_CAPACITY + 1,
    TITLE_ITEM_CONTINUE = 1,
    LOAD_CHOICE_YES = 0,

    CARD_ENTRY_STATE_FIRST = 0x10,
};

enum
{
    CONTROL_FRAMES = 30,
    CONTROL_LIMIT = 3000,
    EVENT_TRY_PERIOD = 15,
    SCENE_LIMIT = 3000,
    TEXT_CONFIRM_PERIOD = 60,
    ANSWER_SPACING = 4,
    ANSWER_LIMIT = 4,
    LOAD_PRESS_PERIOD = 40,
    LOAD_TAP_VBLANKS = 2,
    MODAL_STALL_VBLANKS = 120,
    SCREEN_STALL_VBLANKS = 8,
    MODAL_TAP_PERIOD = 20,
    MODAL_TAP_CYCLE = 4,
    MODAL_TAP_CIRCLE = 0,
    MODAL_TAP_TRIANGLE = 2,
};

enum
{
    STUCK_FRAMES = 90,
    PLAN_ATTEMPTS = 4,
    ARRIVED_DISTANCE_SQUARED = 9,
    PROGRESS_DISTANCE_SQUARED = 4,
    NO_PROGRESS_YET = 1 << 30,
    INTERRUPTION_LIMIT = 2000,
    SIDESTEP_FRAMES = 20,
    BLOCKER_WAIT_FRAMES = 120,
    BLOCKER_WAITS = 2,
    DETOUR_OFFSETS = 3,
    DETOUR_STEP = 24,
    GOAL_CANDIDATES = 9,
    PATH_SAMPLE_STEP = 3,
    TRIGGER_REGION_LABEL = 0x10000,
};

enum
{
    TALK_OFFSET = 20,
    FACE_FRAMES = 8,
    TALK_PRESS_FRAME = 12,
    TALK_CONFIRM_PERIOD = 45,
    TALK_CONFIRM_BEAT = 20,
    TALK_BACK_OUT_AFTER = 400,
    TALK_BACK_OUT_PERIOD = 30,
    TALK_BACK_OUT_CIRCLE_BEAT = 10,
    TALK_BACK_OUT_TRIANGLE_BEAT = 25,
    TALK_DONE_FRAMES = 60,
    TALK_LIMIT = 1500,
    TALK_LONG_LIMIT = 8000,
    TALK_NOTHING_FRAMES = 120,
    TALK_SIDES = 4,
    TALK_TRIES = 2 * TALK_SIDES,
    EVENT_WATCH_FRAMES = 20000,
    SCRIPT_RUNS_LOGGED = 5,
};

enum
{
    FIGHT_PERIOD = 20,
    ATTACK_HOLD_FRAMES = 4,
    FIGHT_LIMIT = 20000,
    FIGHT_ROTATE_FRAMES = 600,
    FIGHT_ROTATE_LIMIT = 40000,
    WATCH_STALL_FRAMES = 3000,
};

enum
{
    WMAP_GRID_SIZE = 6,
    WMAP_CELL_SPACING = 48,
    WMAP_FRAME_COUNT_RESTARTED = 2,
    WMAP_INPUT_GRACE_FRAMES = 40,
    WORLD_MAP_TAP_PERIOD = 8,
    WORLD_MAP_LIMIT = 6000,
    WORLD_MAP_IDLE_VBLANKS = 60,
    CAROUSEL_TURN_LIMIT = 64,
    ARTIFACT_22 = 22,
    ARTIFACT_22_LAND = 16,
    WMAP_SELECTION_MAP = 0,
    WMAP_SELECTION_ARTIFACTS = 2,
    WMAP_NO_LAND = 0xFF,
    SAVED_LAND_ON_MAP = 0x02,
};

enum
{
    CAPTURE_PIXELS = 640 * 512,
    COLOR_BITS = 5,
    COLOR_MASK = 0x1F,
    COLOR_TO_8_BITS = 3,
};

typedef enum
{
    STEP_ENTER,
    STEP_EXIT,
    STEP_WALK,
    STEP_TALK,
    STEP_PRESS,
    STEP_WAIT,
    STEP_WAIT_SCENE,
    STEP_WAIT_BATTLE,
    STEP_CHECKPOINT,
    STEP_WATCH,
    STEP_EXPERIENCE,
    STEP_GUEST,
    STEP_COMPANION,
    STEP_ANSWER,
    STEP_FIGHT,
    STEP_LAND,
    STEP_PLACE,
    STEP_AUDIO,
    STEP_EVENT,
    STEP_WIDESCREEN,
    STEP_ALWAYS_RUN,
    STEP_SWITCH,
    STEP_HIT_POINTS,
} StepKind;

typedef struct
{
    StepKind kind;

    int args[STEP_ARGS];

    int via[VIA_CAPACITY][2];

    int via_count;

    char name[NAME_BYTES];
} Step;

typedef enum
{
    PHASE_LOADING,
    PHASE_START,
    PHASE_WAIT_CONTROL,
    PHASE_WALK,
    PHASE_WAIT_SCENE,
    PHASE_FACE,
    PHASE_TALK,
    PHASE_PRESS,
    PHASE_WAIT,
    PHASE_WAIT_BATTLE,
    PHASE_WATCH,
    PHASE_FIGHT,
    PHASE_FIGHT_WAIT,
    PHASE_WORLD_MAP,
    PHASE_EVENT,
    PHASE_DONE,
} Phase;

enum
{
    TITLE_BACK_NONE,
    TITLE_BACK_PENDING,
    TITLE_BACK_PRESSING,
};

enum
{
    TITLE_IDLE_NONE,
    TITLE_IDLE_WAITING,
    TITLE_IDLE_ON_TITLE,
};

enum
{
    CARD_BROWSE_NONE,
    CARD_BROWSE_DOWN,
    CARD_BROWSE_UP,
};

enum
{
    MOST_VBLANKS_PER_STEP = 8
};

typedef struct
{
    Step steps[STEP_CAPACITY];
    int step_count;
    int step;
    unsigned vblank_limit;
    unsigned title_item;
    int title_up;
    int title_back;
    int title_idle;
    int card_browse;
    int start_widescreen;
    unsigned vblanks;
    Phase phase;
    unsigned phase_start;
    unsigned phase_vblank;
    unsigned last_progress;
    int controllable;
    int best;
    int replans;
    int talk_busy;
    int wait_scene;
    unsigned short held;
    int answer;
    int via_index;
    int interrupted;
    int blocker_waits;
    int talk_side;
    int talk_tries;
    int talk_x;
    int talk_z;
    int world_map_frames_before;
    int world_map_ready;
    int exit_script_started;
    int hero_script_started;
    int person_started;
    int other_person;
} RouteRun;

extern RouteRun g_run;

extern const char* const route_step_names[];

int read_plan(const char* path);
void hero_position(int* x, int* z);
unsigned short confirm_buttons(unsigned elapsed, unsigned period, unsigned beat);
int player_has_control(void);
void describe_contact(char* out, size_t size);
unsigned short toward(int x, int z, int to_x, int to_z);
void restart_progress(unsigned frame);
void enter_phase(Phase phase, unsigned frame);
void request_quit(void);
void fail(unsigned frame, const char* reason);
void next_step(unsigned frame);
void start_walk(unsigned frame);
int set_next_target(int z);
void start_step(unsigned frame, int z);
int is_world_map_step(int index);
void continue_on_world_map(unsigned frame);
void leave_for_world_map(unsigned frame);
unsigned short world_map_buttons(void);
void arrive_from_world_map(unsigned frame, int scene);
void fight_frame(unsigned frame, int x, int z);
void walk_frame(unsigned frame, int x, int z);
void talk_frame(unsigned frame);
unsigned short loading_button(void);
void next_screen_step(void);
unsigned short screen_step(void);
void finish(void);

#endif
