#include "lom_native_draw.h"
#include "lom_native_field_widescreen.h"
#include "lom_native_widescreen.h"
#include "lom_native_draw_labels.h"
#include "overlays/field/internal/field_scene_internal.h"
#include "overlays/field/internal/field_actor_runtime.h"
#include "overlays/field/internal/field_calls.h"
#include "overlays/field/internal/field_effect_render_state.h"
#include "main/field_entry.h"
#include "lom_native_env.h"
#include <libgte.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum
{
    ORIGINAL_WIDTH = LOM_WIDESCREEN_ORIGINAL_WIDTH,
    WIDE_WIDTH = LOM_WIDESCREEN_WIDTH,
    MARGIN = LOM_WIDESCREEN_MARGIN,
    UNITS_PER_PIXEL = 256,
    SCENE_MASK = 0x7FFF,
    SKY_COLUMNS = ORIGINAL_WIDTH / FIELD_TILE_SIZE,
    SKY_MIN_ROWS = 14,
    DRAW_HEIGHT = 224,
    SKY_MAX_BOTTOM_ROWS = DRAW_HEIGHT / FIELD_TILE_SIZE,
    SKY_FORBIDDEN_FLAGS = 0xFC,
    WRAP_X_FLAG = 0x4,
    SCROLL_SCALE_ONE = 0x10,
    EDGE_SLACK = 8,
    SCROLL_SCALE_NEGATIVE = 0x80,
    SPRITE_GRID_KIND = 0,
    EMPTY_PART_KIND = 1,
    SKY_EDGE_COPIES = (MARGIN + FIELD_TILE_SIZE - 1) / FIELD_TILE_SIZE,
    SPRITE_WORDS = 3,
    SKY_FRAME_COPIES = (DRAW_HEIGHT / FIELD_TILE_SIZE + 1 + SKY_MAX_BOTTOM_ROWS) * 2 * SKY_EDGE_COPIES,
    COORDINATE_Y_SHIFT = 16,
};

static const char status_log_format[] = "[lom-native] FIELD widescreen: scene %d, map %d px: %s";
static const char rejected_layer_log_format[] = " (layer %d flags %X scroll %u)";
static const char layer_problem_log_format[] = "[lom-native] FIELD widescreen:   layer %d flags %X scroll %u, %d px wide: %s\n";
static const char* const status_names[LOM_FIELD_WIDESCREEN_STATUS_COUNT] = {"wide",      "off",  "no scene",          "narrow map",
                                                                            "movie map", "duel", "unsupported layer", "hidden edge"};

static const int hidden_edge_scenes[] = {135, 136, 780, 804, 581, 618};

static struct
{
    int scene;
    LomFieldWidescreenStatus status;
    int rejected;
    int valid;
} s_last_report;

typedef struct
{
    O_TAG;
    u32 code;
    u32 xy;
    u32 uv;
} SkyPrim;

_Static_assert(sizeof(SkyPrim) == sizeof(SPRT_16), "a sky copy is a SPRT_16");

static SkyPrim s_sky[2][SKY_FRAME_COPIES];
static unsigned s_sky_buffer;
static unsigned s_sky_used;

enum
{
    WRAP_BUFFER_BYTES = 512 * 1024,
    WRAP_CELL_BOUND = 128,
    WRAP_ALIGN = 16,
};

static _Alignas(16) unsigned char s_wrap[2][WRAP_BUFFER_BYTES];
static size_t s_wrap_used;
static unsigned char* s_wrap_cursor;
static unsigned char* s_wrap_start;
static size_t s_wrap_bound;

/**
 * @brief Recognize a fixed sky by its shape, never by the room: the first object's
 *        only part, a full 320-pixel-wide sprite grid that doesn't move.
 * @return That part, or NULL for any other layout.
 */
static const FieldPart* fixed_sky(void)
{
    if (!g_field_scene.scene || !g_field_scene.scene->objects)
    {
        return NULL;
    }

    const FieldObj* object = g_field_scene.scene->objects;
    const FieldObjDef* definition = object->def;
    const FieldPart* part = object->parts;

    if (!definition || !part || part->next || !part->def || (definition->flags.word & SKY_FORBIDDEN_FLAGS) ||
        (!(definition->flags.word & FIELD_OBJ_DEF_SCREEN_FIXED) && definition->motion.b.scroll_scale_x != 0) || definition->motion.b.drift_speed != 0 ||
        definition->motion.b.drift_angle != 0 || object->flags.b.drift_speed != 0 || object->drift_x != 0 || object->drift_y != 0 || object->x + part->x != 0 ||
        part->kind != SPRITE_GRID_KIND || part->node_count != 0 || FIELD_PART_SWEEP_MODE(part->def) != 0 || part->def->u.b.cols != SKY_COLUMNS ||
        part->def->u.b.rows < SKY_MIN_ROWS || part->instance_count != SKY_COLUMNS * part->def->u.b.rows || !part->def->tiles)
    {
        return NULL;
    }

    for (unsigned tile = 0; tile < part->instance_count; ++tile)
    {
        if (part->def->tiles[tile].texture_attrs & FIELD_TILE_SEMITRANS)
        {
            return NULL;
        }
    }

    return part;
}

/**
 * @brief How far across the room there's art to show. The room itself, unless
 *        it's drawn from art made for a wide view that reaches past it (the
 *        Remaster's): then every layer was painted for that view, and the wide
 *        view can show all of it.
 * @return 1 for such art, 0 for the room's own.
 */
static int art_extent(int* left, int* right)
{
    const FieldSceneHeader* room = g_field_scene.scene->header;

    if (lom_draw_field_room_extent(room->map_width, room->map_depth, left, right))
    {
        return 1;
    }

    *left = 0;
    *right = room->map_width;
    return 0;
}

typedef struct
{
    int valid;
    int left_hidden;
    int right_hidden;
    int width;
} LayerEdges;

/**
 * @brief Whether an object is a slow parallax layer: scrolling slower than the room, not wrapping, not fixed to the screen, not drifting.
 */
static int slow_layer(const FieldObj* object)
{
    const FieldObjDef* definition = object->def;

    const int sideways_drift = (rcos(object->flags.b.drift_angle * 16) * object->flags.b.drift_speed) / 256;

    return definition && !(definition->flags.word & (FIELD_OBJ_DEF_SCREEN_FIXED | WRAP_X_FLAG)) &&
           !(definition->motion.b.scroll_scale_x & SCROLL_SCALE_NEGATIVE) && definition->motion.b.scroll_scale_x < SCROLL_SCALE_ONE && sideways_drift == 0 &&
           object->drift_x == 0;
}

/**
 * @brief Where the scene emitter puts a slow layer's part in the original view.
 * @param scroll The game's horizontal scroll, in pixels.
 */
static int original_layer_x(const FieldObj* object, const FieldPart* part, int scroll)
{
    int camera_x = -scroll * UNITS_PER_PIXEL;
    int scroll_x = (camera_x * object->def->motion.b.scroll_scale_x) / SCROLL_SCALE_ONE;

    const FieldSceneHeader* room = g_field_scene.scene->header;

    return SHIFT_TOWARD_ZERO(scroll_x, 8) + lom_draw_field_layer_shift(object->def->motion.b.scroll_scale_x, -scroll, room->map_width, room->map_depth) +
           (object->x + part->x) / UNITS_PER_PIXEL;
}

/**
 * @brief Find the edges of a slow layer's part the original view hides at every camera.
 *        Placement is linear in the camera, so the two ends of the map decide it.
 */
static LayerEdges slow_layer_edges(const FieldObj* object, const FieldPart* part)
{
    LayerEdges edges = {0, 0, 0, 0};

    if (part->kind != SPRITE_GRID_KIND || !part->def || !g_field_scene.scene || !g_field_scene.scene->header)
    {
        return edges;
    }

    int last_scroll = g_field_scene.scene->header->map_width - ORIGINAL_WIDTH;
    int first = original_layer_x(object, part, 0);
    int last = original_layer_x(object, part, last_scroll > 0 ? last_scroll : 0);
    edges.valid = 1;
    edges.width = part->def->u.b.cols * FIELD_TILE_SIZE;
    edges.left_hidden = first <= EDGE_SLACK && last <= EDGE_SLACK;
    edges.right_hidden = first + edges.width >= ORIGINAL_WIDTH - EDGE_SLACK && last + edges.width >= ORIGINAL_WIDTH - EDGE_SLACK;
    return edges;
}

/**
 * @brief Whether a layer that scrolls faster than the room keeps hidden what
 *        the original hides: an edge the 4:3 view never shows, at any camera,
 *        mustn't show in the wide view at any draw camera either. Placement
 *        is linear in the camera, so the two ends of the map decide it.
 */
static int fast_layer_supported(const FieldObj* object)
{
    const int map_width = g_field_scene.scene->header->map_width;
    const int last = map_width - ORIGINAL_WIDTH;
    const int wide_first = MARGIN, wide_last = map_width - ORIGINAL_WIDTH - MARGIN;

    for (const FieldPart* part = object->parts; part; part = part->next)
    {
        if (part->kind == EMPTY_PART_KIND)
        {
            continue;
        }

        if (part->kind != SPRITE_GRID_KIND || !part->def)
        {
            return 0;
        }

        const int width = part->def->u.b.cols * FIELD_TILE_SIZE;
        const int first_x = original_layer_x(object, part, 0), last_x = original_layer_x(object, part, last);
        const int wide_first_x = original_layer_x(object, part, wide_first), wide_last_x = original_layer_x(object, part, wide_last);
        const int left_hidden = first_x <= EDGE_SLACK && last_x <= EDGE_SLACK;
        const int right_hidden = first_x + width >= ORIGINAL_WIDTH - EDGE_SLACK && last_x + width >= ORIGINAL_WIDTH - EDGE_SLACK;

        if (left_hidden && (wide_first_x > -MARGIN || wide_last_x > -MARGIN))
        {
            return 0;
        }

        if (right_hidden && (wide_first_x + width < ORIGINAL_WIDTH + MARGIN || wide_last_x + width < ORIGINAL_WIDTH + MARGIN))
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Why a layer keeps its room 4:3, or NULL if the wide view can draw it.
 */
static const char* layer_rule(const FieldObj* object)
{
    const FieldObjDef* definition = object->def;

    if (!definition || !object->parts)
    {
        return NULL;
    }

    const unsigned scale = definition->motion.b.scroll_scale_x;

    if (definition->flags.word & FIELD_OBJ_DEF_SCREEN_FIXED)
    {
        return "fixed to the screen";
    }

    if ((definition->flags.word & WRAP_X_FLAG) || scale == SCROLL_SCALE_ONE)
    {
        return NULL;
    }

    if (scale & SCROLL_SCALE_NEGATIVE)
    {
        return "scrolls backwards";
    }

    if (scale > SCROLL_SCALE_ONE)
    {
        return fast_layer_supported(object) ? NULL : "scrolls faster than the room, and would show an edge the original hides";
    }

    if (!slow_layer(object))
    {
        return "slow, and drifts";
    }

    for (const FieldPart* part = object->parts; part; part = part->next)
    {
        if (part->kind == EMPTY_PART_KIND)
        {
            continue;
        }

        LayerEdges edges = slow_layer_edges(object, part);

        if (!edges.valid)
        {
            return "slow, with a part that isn't a sprite grid";
        }

        if (edges.left_hidden && edges.right_hidden && edges.width < WIDE_WIDTH)
        {
            return "slow, and too narrow to cover the wide view";
        }
    }

    return NULL;
}

/**
 * @brief layer_rule, plus the layer's widest sprite grid in pixels for the log.
 */
static const char* layer_problem(const FieldObj* object, int* width)
{
    *width = 0;

    for (const FieldPart* part = object->parts; part; part = part->next)
    {
        if (part->kind == SPRITE_GRID_KIND && part->def && part->def->u.b.cols * FIELD_TILE_SIZE > *width)
        {
            *width = part->def->u.b.cols * FIELD_TILE_SIZE;
        }
    }

    return layer_rule(object);
}

LomFieldWidescreenStatus lom_native_field_widescreen_status(int* rejected_object)
{
    if (rejected_object)
    {
        *rejected_object = -1;
    }

    if (!lom_native_widescreen_enabled())
    {
        return LOM_FIELD_WIDESCREEN_DISABLED;
    }

    if (!g_field_scene.scene || !g_field_scene.scene->header)
    {
        return LOM_FIELD_WIDESCREEN_NO_SCENE;
    }

    int art_left, art_right;
    const int wide_art = art_extent(&art_left, &art_right);

    if (art_right - art_left < WIDE_WIDTH)
    {
        return LOM_FIELD_WIDESCREEN_NARROW;
    }

    if (g_field_scene.scene->vlc_table)
    {
        return LOM_FIELD_WIDESCREEN_MOVIE;
    }

    for (size_t i = 0; i < sizeof(hidden_edge_scenes) / sizeof(hidden_edge_scenes[0]); i++)
    {
        if ((g_field_entry_scene.word & SCENE_MASK) == hidden_edge_scenes[i])
        {
            return LOM_FIELD_WIDESCREEN_HIDDEN_EDGE;
        }
    }

    if (g_field_duel_mode)
    {
        return LOM_FIELD_WIDESCREEN_DUEL;
    }

    if (wide_art || fixed_sky())
    {
        return LOM_FIELD_WIDESCREEN_WIDE;
    }

    int index = 0;

    for (const FieldObj* object = g_field_scene.scene->objects; object; object = object->next, ++index)
    {
        const FieldObjDef* definition = object->def;

        if (!definition || !object->parts)
        {
            continue;
        }

        if (layer_rule(object))
        {
            if (rejected_object)
            {
                *rejected_object = index;
            }

            return LOM_FIELD_WIDESCREEN_UNSUPPORTED_LAYER;
        }
    }

    return LOM_FIELD_WIDESCREEN_WIDE;
}

/**
 * @brief Log every layer of a room and its sprite grids: how the layer scrolls,
 *        and where each grid sits at the ends of the original and wide views.
 * @param width The room's map width in pixels.
 */
static void log_layers(int width)
{
    const int last = width - ORIGINAL_WIDTH;
    int index = 0;

    for (const FieldObj* layer = g_field_scene.scene->objects; layer; layer = layer->next, ++index)
    {
        if (!layer->def)
        {
            continue;
        }

        fprintf(stderr, "[lom-native] FIELD widescreen layer %d: flags %X scale %u drift %d,%d\n", index, (unsigned)layer->def->flags.word,
                (unsigned)layer->def->motion.b.scroll_scale_x, (int)layer->flags.b.drift_speed, (int)layer->drift_x);

        for (const FieldPart* part = layer->parts; part; part = part->next)
        {
            if (part->kind != SPRITE_GRID_KIND || !part->def)
            {
                fprintf(stderr, "[lom-native] FIELD widescreen   part kind %d\n", (int)part->kind);
                continue;
            }

            fprintf(stderr, "[lom-native] FIELD widescreen   part %dx%d px, x at camera 0/%d: %d/%d, wide at %d/%d: %d/%d\n",
                    part->def->u.b.cols * FIELD_TILE_SIZE, part->def->u.b.rows * FIELD_TILE_SIZE, last, original_layer_x(layer, part, 0),
                    original_layer_x(layer, part, last), MARGIN, last - MARGIN, original_layer_x(layer, part, MARGIN),
                    original_layer_x(layer, part, last - MARGIN));
        }
    }
}

/**
 * @brief Log every layer that keeps a room 4:3, and why.
 */
static void log_layer_problems(void)
{
    int index = 0;

    for (const FieldObj* layer = g_field_scene.scene->objects; layer; layer = layer->next, ++index)
    {
        int layer_width;
        const char* problem = layer_problem(layer, &layer_width);

        if (problem)
        {
            fprintf(stderr, layer_problem_log_format, index, (unsigned)layer->def->flags.word, (unsigned)layer->def->motion.b.scroll_scale_x, layer_width,
                    problem);
        }
    }
}

/**
 * @brief Log the decision once per room and reason, while the setting is on, so a
 *        session log says why a room stayed 4:3.
 */
static void report(LomFieldWidescreenStatus status, int rejected)
{
    int scene = g_field_entry_scene.word & SCENE_MASK;

    if (status == LOM_FIELD_WIDESCREEN_DISABLED ||
        (s_last_report.valid && s_last_report.scene == scene && s_last_report.status == status && s_last_report.rejected == rejected))
    {
        return;
    }

    s_last_report.scene = scene;
    s_last_report.status = status;
    s_last_report.rejected = rejected;
    s_last_report.valid = 1;
    const int has_header = g_field_scene.scene && g_field_scene.scene->header;
    int width = has_header ? g_field_scene.scene->header->map_width : -1;
    fprintf(stderr, status_log_format, scene, width, status_names[status]);
    const FieldObj* object = g_field_scene.scene ? g_field_scene.scene->objects : NULL;

    for (int index = 0; object && index < rejected; ++index)
    {
        object = object->next;
    }

    if (rejected >= 0 && object && object->def)
    {
        fprintf(stderr, rejected_layer_log_format, rejected, (unsigned)object->def->flags.word, (unsigned)object->def->motion.b.scroll_scale_x);
    }

    fputc('\n', stderr);

    if (lom_native_env_on(LOM_ENV_WIDESCREEN_LAYERS) && has_header && width >= WIDE_WIDTH)
    {
        log_layers(width);
    }

    if (status == LOM_FIELD_WIDESCREEN_UNSUPPORTED_LAYER)
    {
        log_layer_problems();
    }
}

void lom_native_field_widescreen_begin_frame(void)
{
    int rejected = -1;
    LomFieldWidescreenStatus status = lom_native_field_widescreen_status(&rejected);
    report(status, rejected);
    s_sky_buffer ^= 1;
    s_sky_used = 0;
    s_wrap_used = 0;
    lom_native_widescreen_begin_frame(status == LOM_FIELD_WIDESCREEN_WIDE);
}

void lom_native_field_widescreen_end(void)
{
    lom_native_widescreen_reset();
}

int lom_native_field_widescreen_camera_x(int camera_x)
{
    int art_left, art_right;

    if (lom_native_widescreen_margin() != MARGIN || !g_field_scene.scene || !g_field_scene.scene->header)
    {
        return camera_x;
    }

    art_extent(&art_left, &art_right);

    if (art_right - art_left < WIDE_WIDTH)
    {
        return camera_x;
    }

    int left = -(art_left + MARGIN) * UNITS_PER_PIXEL;
    int right = -(art_right - ORIGINAL_WIDTH - MARGIN) * UNITS_PER_PIXEL;

    if (camera_x > left)
    {
        return left;
    }

    if (camera_x < right)
    {
        return right;
    }

    return camera_x;
}

int lom_native_field_widescreen_layer_x(const FieldObj* object, const FieldPart* part, int x, int scaled)
{
    int margin = lom_native_widescreen_margin();
    int art_left, art_right;

    if (!margin || !scaled || !object->def || object->def->motion.b.scroll_scale_x == SCROLL_SCALE_ONE || !slow_layer(object) || fixed_sky() ||
        (g_field_scene.scene && g_field_scene.scene->header && art_extent(&art_left, &art_right)))
    {
        return x;
    }

    LayerEdges edges = slow_layer_edges(object, part);

    if (!edges.valid)
    {
        return x;
    }

    if (edges.left_hidden && x > -margin)
    {
        x = -margin;
    }

    if (edges.right_hidden && x + edges.width < ORIGINAL_WIDTH + margin)
    {
        x = ORIGINAL_WIDTH + margin - edges.width;
    }

    return x;
}

/**
 * @brief Move one vertex word (x in the low half, y in the high) sideways.
 */
static void shift_vertex(u32* word, int shift)
{
    const u32 x = (u16)((s16)(*word & 0xFFFF) + shift);
    *word = (*word & 0xFFFF0000u) | x;
}

/**
 * @brief Check whether a word is the one that ends a polyline.
 */
static int polyline_end(u32 word)
{
    return (word & 0xF000F000u) == 0x50005000u;
}

/**
 * @brief Move a polygon command's three or four vertices sideways, past its
 *        colors and texture coordinates.
 * @param words The packet's GP0 words.
 * @param count How many there are.
 * @param at Where the command starts.
 * @param shift Pixels to add to every x.
 * @return Where the next command starts.
 */
static int shift_polygon(u32* words, int count, int at, int shift)
{
    const unsigned command = words[at] >> 24;
    const int gouraud = command & 0x10, vertices = (command & 0x08) ? 4 : 3, textured = command & 0x04;
    at++;

    for (int v = 0; v < vertices && at < count; v++)
    {
        at += (v > 0 && gouraud) ? 1 : 0;
        shift_vertex(&words[at++], shift);
        at += textured ? 1 : 0;
    }

    return at;
}

/**
 * @brief Move a line command's vertices sideways: two for a line, and for a
 *        polyline every one up to the word that ends it.
 * @param words The packet's GP0 words.
 * @param count How many there are.
 * @param at Where the command starts.
 * @param shift Pixels to add to every x.
 * @return Where the next command starts.
 */
static int shift_line(u32* words, int count, int at, int shift)
{
    const unsigned command = words[at] >> 24;
    const int gouraud = command & 0x10, polyline = command & 0x08;
    at++;

    for (int v = 0; at < count && (polyline || v < 2); v++)
    {
        if (v > 0 && gouraud)
        {
            if (polyline && polyline_end(words[at]))
            {
                return at + 1;
            }

            at++;
        }

        if (polyline && v > 0 && polyline_end(words[at]))
        {
            return at + 1;
        }

        shift_vertex(&words[at++], shift);
    }

    return at;
}

/**
 * @brief Move a rectangle command's corner sideways, past its texture
 *        coordinates and its size when it has them.
 * @param words The packet's GP0 words.
 * @param at Where the command starts.
 * @param shift Pixels to add to every x.
 * @return Where the next command starts.
 */
static int shift_rectangle(u32* words, int at, int shift)
{
    const unsigned command = words[at] >> 24;
    at++;
    shift_vertex(&words[at++], shift);
    at += (command & 0x04) ? 1 : 0;
    at += ((command >> 3) & 3) == 0 ? 1 : 0;
    return at;
}

/**
 * @brief Move the drawing commands in one packet sideways. Fills and copies
 *        are skipped, as are the commands that only set drawing state.
 * @param words The packet's GP0 words, after its header.
 * @param count How many there are.
 * @param shift Pixels to add to every x.
 * @return False if a command wasn't one it knows, and the rest were left alone.
 */
static int shift_commands(u32* words, int count, int shift)
{
    int i = 0;

    while (i < count)
    {
        const unsigned command = words[i] >> 24;

        if (command >= 0x20 && command < 0x40)
        {
            i = shift_polygon(words, count, i, shift);
        }
        else if (command >= 0x40 && command < 0x60)
        {
            i = shift_line(words, count, i, shift);
        }
        else if (command >= 0x60 && command < 0x80)
        {
            i = shift_rectangle(words, i, shift);
        }
        else if (command == 0x02)
        {
            i += 3;
        }
        else if (command >= 0x80 && command < 0xA0)
        {
            i += 4;
        }
        else if (command <= 0x01 || (command >= 0xE1 && command <= 0xE6))
        {
            i++;
        }
        else
        {
            return 0;
        }
    }

    return 1;
}

/**
 * @brief Check whether the game labelled the packet as part of its actor pass:
 *        an actor, an effect or a screen tint (lom_native_draw_labels.h).
 */
static int actor_pass_packet(const void* packet)
{
    const LomDrawLabel* label = lom_native_draw_label_find(packet);
    return label && (label->kind == LOM_LABEL_FIELD_ACTOR || label->kind == LOM_LABEL_FIELD_EFFECT || label->kind == LOM_LABEL_FIELD_TINT);
}

/**
 * @brief Move the packets the actor pass just labelled sideways, wherever they
 * were linked into the frame's ordering table.
 *
 * Each ordering table entry's list runs into the next entry (or the one
 * before, for a reversed table), so walking every entry's list until it gets
 * to another entry visits every packet once.
 */
static void shift_actor_packets(OT_TYPE* table, int entries, int shift)
{
    const OT_TYPE* table_end = table + entries;
    static int s_reported;

    for (int e = 0; e < entries; e++)
    {
        int steps = 0;

        for (P_TAG* packet = nextPrim(&table[e]); packet && (uintptr_t)packet != 0xFFFFFF; packet = nextPrim(packet))
        {
            if ((const OT_TYPE*)packet >= table && (const OT_TYPE*)packet < table_end)
            {
                break;
            }

            if (actor_pass_packet(packet) && !shift_commands((u32*)&packet->r0, (int)getlen(packet), shift) && !s_reported)
            {
                s_reported = 1;
                printf("[lom-native] FIELD widescreen: an actor packet had a command %02X it doesn't know, left where it was\n", (unsigned)packet->code);
            }

            if (++steps > 1 << 20)
            {
                break;
            }
        }
    }
}

void lom_native_field_widescreen_draw_actors(void* render, int alternate)
{
    FieldRenderContext* context = render;
    const int delta = lom_native_field_widescreen_camera_x(g_field_camera_x) - g_field_camera_x;
    const int shift = (g_field_view_offset_x + delta) / UNITS_PER_PIXEL - g_field_view_offset_x / UNITS_PER_PIXEL;

    field_prepare_actor_render_commands(context, alternate);
    field_render_actor_objects(context);

    if (shift)
    {
        shift_actor_packets((OT_TYPE*)context, FIELD_ORDERING_TABLE_SIZE, shift);
    }
}

void* lom_native_field_widescreen_extend_background(const FieldPart* part, void* last_primitive, void* next, int origin_y)
{
    if (lom_native_widescreen_margin() != MARGIN || part != fixed_sky())
    {
        return last_primitive;
    }

    SkyPrim* source = last_primitive;
    int x = (s16)(source->xy & 0xFFFF);

    if (x != 0 && x != ORIGINAL_WIDTH - FIELD_TILE_SIZE)
    {
        return last_primitive;
    }

    int y = (s16)(source->xy >> COORDINATE_Y_SHIFT);
    int last_sky_y = origin_y + (part->def->u.b.rows - 1) * FIELD_TILE_SIZE;
    unsigned rows = 1;

    if (y == last_sky_y)
    {
        while (y + (int)rows * FIELD_TILE_SIZE < DRAW_HEIGHT && rows <= SKY_MAX_BOTTOM_ROWS)
        {
            ++rows;
        }
    }

    if (s_sky_used + rows * SKY_EDGE_COPIES > SKY_FRAME_COPIES)
    {
        return last_primitive;
    }

    SkyPrim* tail = source;

    for (unsigned row = 0; row < rows; ++row)
    {
        int edge_y = y + (int)row * FIELD_TILE_SIZE;

        for (unsigned copy = 0; copy < SKY_EDGE_COPIES; ++copy)
        {
            int edge_x = x == 0 ? -(int)(copy + 1) * FIELD_TILE_SIZE : ORIGINAL_WIDTH + (int)copy * FIELD_TILE_SIZE;
            SkyPrim* clone = &s_sky[s_sky_buffer][s_sky_used++];
            clone->code = source->code;
            clone->xy = ((u32)(u16)edge_y << COORDINATE_Y_SHIFT) | (u16)edge_x;
            clone->uv = source->uv;
            setaddr(tail, clone);
            setlen(clone, SPRITE_WORDS);
            tail = clone;
        }
    }

    setaddr(tail, next);
    return tail;
}

unsigned char** lom_native_field_widescreen_wrap_cursor(unsigned cells)
{
    static int s_reported;
    const size_t bound = (size_t)cells * WRAP_CELL_BOUND;

    if (lom_native_widescreen_margin() != MARGIN || s_wrap_used + bound > WRAP_BUFFER_BYTES)
    {
        if (lom_native_widescreen_margin() == MARGIN && !s_reported)
        {
            s_reported = 1;
            printf("[lom-native] FIELD widescreen: no room for another copy of a wrapping layer (%u cells)\n", cells);
        }

        return NULL;
    }

    s_wrap_start = s_wrap_cursor = s_wrap[s_sky_buffer] + s_wrap_used;
    s_wrap_bound = bound;
    return &s_wrap_cursor;
}

void lom_native_field_widescreen_wrap_done(void)
{
    const size_t used = (size_t)(s_wrap_cursor - s_wrap_start);

    if (used > s_wrap_bound)
    {
        fprintf(stderr, "[lom-native] FIELD widescreen: a wrapping layer copy took %zu bytes, more than the %zu it was given\n", used, s_wrap_bound);
        abort();
    }

    s_wrap_used += (used + WRAP_ALIGN - 1) & ~(size_t)(WRAP_ALIGN - 1);
}
