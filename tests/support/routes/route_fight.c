#include "route_runner.h"

static int s_fight_target;

static int s_fight_target_hp = -1;

static unsigned s_fight_target_since;

/**
 * @brief The nearest monster still standing in the battle.
 * @param x Hero x.
 * @param z Hero z.
 * @param out_x Receives its x.
 * @param out_z Receives its z.
 * @return Non-zero when there is one.
 */
static int nearest_monster(int x, int z, int* out_x, int* out_z)
{
    int best = -1;

    if (g_field_battle == NULL)
    {
        return 0;
    }

    for (int i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
    {
        const FieldStatusRecord* record = &g_field_battle->records[i];
        Vec3i position;

        if (!(record->side & RECORD_MONSTER) || record->state == NULL || record->state->current_hp.word == 0 ||
            field_get_actor_position(record->meta.bytes.id, &position) < 0)
        {
            continue;
        }

        int mx = position.x >> POSITION_SHIFT, mz = position.z >> POSITION_SHIFT;
        int distance = (mx - x) * (mx - x) + (mz - z) * (mz - z);

        if (best < 0 || distance < best)
        {
            best = distance;
            *out_x = mx;
            *out_z = mz;
        }
    }

    return best >= 0;
}

/**
 * @brief The monster a target-rotating fight step is after: the same one while its HP keeps dropping, the next standing
 *        one (in record order) once it hasn't for FIGHT_ROTATE_FRAMES (a part that takes no damage, say).
 * @param frame Current FIELD frame.
 * @param out_x Receives its x.
 * @param out_z Receives its z.
 * @return Non-zero when a monster is standing.
 */
static int rotating_monster(unsigned frame, int* out_x, int* out_z)
{
    int standing[FIELD_BATTLE_RECORD_COUNT], count = 0;

    if (g_field_battle == NULL)
    {
        return 0;
    }

    for (int i = 0; i < FIELD_BATTLE_RECORD_COUNT; i++)
    {
        const FieldStatusRecord* record = &g_field_battle->records[i];
        Vec3i position;

        if ((record->side & RECORD_MONSTER) && record->state != NULL && record->state->current_hp.word != 0 &&
            field_get_actor_position(record->meta.bytes.id, &position) >= 0)
        {
            standing[count++] = i;
        }
    }

    if (count == 0)
    {
        return 0;
    }

    const FieldStatusRecord* target = &g_field_battle->records[standing[s_fight_target % count]];
    int hp = target->state->current_hp.word;

    if (hp != s_fight_target_hp)
    {
        s_fight_target_hp = hp;
        s_fight_target_since = frame;
    }
    else if (frame - s_fight_target_since > FIGHT_ROTATE_FRAMES)
    {
        s_fight_target++;
        s_fight_target_hp = -1;
        s_fight_target_since = frame;
        target = &g_field_battle->records[standing[s_fight_target % count]];
    }

    Vec3i position;

    field_get_actor_position(target->meta.bytes.id, &position);
    *out_x = position.x >> POSITION_SHIFT;
    *out_z = position.z >> POSITION_SHIFT;
    return 1;
}

/**
 * @brief One frame of a battle that locks the exits: close in on the nearest monster and attack.
 * @param frame Current FIELD frame.
 * @param x Hero x.
 * @param z Hero z.
 */
void fight_frame(unsigned frame, int x, int z)
{
    unsigned elapsed = frame - g_run.phase_start;
    int mx, mz;

    if (!g_field_runtime->state.bits.battle_active)
    {
        char note[64];

        snprintf(note, sizeof(note), "the battle ended after %u frames", elapsed);
        route_record_note(note);
        g_run.held = 0;

        if (g_run.steps[g_run.step].kind == STEP_FIGHT)
        {
            next_step(frame);
            return;
        }

        g_run.replans = 0;
        g_run.best = NO_PROGRESS_YET;
        g_run.interrupted = 0;
        set_next_target(z);
        enter_phase(PHASE_WALK, frame);
        return;
    }

    unsigned limit = g_run.steps[g_run.step].kind == STEP_FIGHT && g_run.steps[g_run.step].args[FIGHT_ARG_ROTATE] ? FIGHT_ROTATE_LIMIT : FIGHT_LIMIT;

    if (g_run.steps[g_run.step].kind == STEP_FIGHT && g_run.steps[g_run.step].args[FIGHT_ARG_FOR] > 0 &&
        elapsed >= (unsigned)g_run.steps[g_run.step].args[FIGHT_ARG_FOR])
    {
        char note[64];

        snprintf(note, sizeof(note), "fought %u frames; the battle goes on", elapsed);
        route_record_note(note);
        g_run.held = 0;
        next_step(frame);
        return;
    }

    if (g_field_return_to_title_prompt_state || elapsed > limit)
    {
        fail(frame, g_field_return_to_title_prompt_state ? "lost the battle" : "the battle never ended");
        return;
    }

    if (!(g_run.steps[g_run.step].kind == STEP_FIGHT && g_run.steps[g_run.step].args[FIGHT_ARG_ROTATE] ? rotating_monster(frame, &mx, &mz)
                                                                                                       : nearest_monster(x, z, &mx, &mz)))
    {
        g_run.held = elapsed % TEXT_CONFIRM_PERIOD == TEXT_CONFIRM_PERIOD / 2 ? BUTTON_CROSS : 0;
        return;
    }

    g_run.held = toward(x, z, mx, mz) | (elapsed % FIGHT_PERIOD < ATTACK_HOLD_FRAMES ? BUTTON_SQUARE : 0);

    if (g_run.steps[g_run.step].kind == STEP_FIGHT && g_run.steps[g_run.step].args[FIGHT_ARG_ROTATE] &&
        elapsed % TEXT_CONFIRM_PERIOD == TEXT_CONFIRM_PERIOD / 2)
    {
        g_run.held = BUTTON_CROSS;
    }
}
