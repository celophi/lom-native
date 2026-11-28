#include "overlays/field/internal/field_actor_tables.h"
#include "overlays/field/internal/field_actor_behavior.h"
#include "lom_native_random.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

extern int __real_lom_native_rand(void);

static unsigned observed_draws;

static int observe_draws;

/**
 * @brief Count the random numbers drawn while the case watches.
 * @return What the real one returns.
 */
int __wrap_lom_native_rand(void)
{
    int result = __real_lom_native_rand();

    if (observe_draws)
    {
        ++observed_draws;
    }

    return result;
}

#define IDLE_CHECK(t)                                                                                                                                          \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        if (!(t))                                                                                                                                              \
        {                                                                                                                                                      \
            fprintf(stderr, "FIELD idle oracle case %s, facing %u, alias %u, line %d: %s\n", row->name, facing, alias, __LINE__, #t);                          \
            return 1;                                                                                                                                          \
        }                                                                                                                                                      \
    } while (0)

/**
 * @brief Check an actor's idle animation picks the PS1's animation and delay
 *        for each seed, drawing the same number of random numbers.
 * @param actor The actor.
 * @param data Its data.
 * @return 0. A failed check stops the test.
 */
int lom_test_field_idle_oracle(FieldActor* actor, u8* data)
{
    const struct IdleCase
    {
        const char* name;
        unsigned seed;
        u8 animation, delay;
        u16 state;
        u32 flags;
        unsigned draws;
        u8 result_animation, result_delay;
        int next;
    } rows[] = {{"threshold6000", 0xDC77A683u, 0, 0, 0, 1, 1, 0, 0, 24576},
                {"threshold6001", 0xC7DCA683u, 0, 0, 0, 1, 1, 1, 0, 11885},
                {"walking-delay2", 1, 2, 2, 9, 1, 0, 2, 1, 16838},
                {"walking-delay1", 1, 2, 1, 9, 1, 1, 0, 0, 5758},
                {"walking-delay0", 1, 2, 0, 9, 1, 1, 0, 0, 5758},
                {"idle-complete", 1, 0, 0, 0, 1, 1, 0, 0, 5758},
                {"idle-active", 1, 1, 0, 1, 1, 0, 1, 0, 16838},
                {"no-actions-idle", 1, 0, 0, 0, 0, 0, 0, 0, 16838}};

    FieldActor before_actor = *actor;
    FieldResourceEntry before_resource = g_field_resource_entries[3];
    u8 before_mode = data[5];
    data[5] = 0;
    unsigned checked = 0;

    const unsigned aliases = 1;

    for (unsigned alias = 0; alias < aliases; ++alias)
    {
        for (unsigned facing = 0; facing < 2; ++facing)
        {
            for (unsigned i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
            {
                const struct IdleCase* row = &rows[i];
                g_field_resource_entries[3].start = data;
                g_field_resource_entries[3].flags = row->flags;
                *actor = (FieldActor){.resource_index = 3,
                                      .object_index = 3,
                                      .animation = (u8)(row->animation | (facing ? 0x80 : 0)),
                                      .animation_state = row->state,
                                      .stop_delay = row->delay,
                                      .animation_frame = 7,
                                      .animation_active = 0,
                                      .frame_ticks = 11,
                                      .control.word = FIELD_CONTROL_PLAY_ONCE};
                FieldActor expected_no_restart = *actor;
                expected_no_restart.stop_delay = row->result_delay;
                lom_native_srand(row->seed);
                observed_draws = 0;
                observe_draws = 1;
                field_update_actor_movement_animation(actor, 0, 0);
                observe_draws = 0;
                IDLE_CHECK(observed_draws == row->draws);
                IDLE_CHECK(__real_lom_native_rand() == row->next);
                IDLE_CHECK(actor->animation == (row->result_animation | (facing ? 0x80 : 0)));
                IDLE_CHECK(actor->stop_delay == row->result_delay);

                if (row->draws)
                {
                    IDLE_CHECK(actor->animation_frame == 0 && actor->animation_state == 1);
                    IDLE_CHECK(actor->animation_active == 1 && actor->frame_ticks == 0);
                    IDLE_CHECK(!(actor->control.word & FIELD_CONTROL_PLAY_ONCE));
                    IDLE_CHECK(actor->frame_timer == 2 && actor->frame_length == 2);
                    IDLE_CHECK((uintptr_t)actor->frame_data == (uintptr_t)(data + 0x60));
                    IDLE_CHECK(actor->height == (row->result_animation ? 6 : 0));
                    IDLE_CHECK(actor->next_height == (row->result_animation ? 9 : 0));
                    IDLE_CHECK(actor->speed_accumulator == (row->result_animation ? 4 : 0));
                }
                else
                {
                    IDLE_CHECK(!memcmp(actor, &expected_no_restart, sizeof(*actor)));
                }

                ++checked;
            }
        }
    }

    *actor = before_actor;
    g_field_resource_entries[3] = before_resource;
    data[5] = before_mode;
    printf("  FIELD idle oracle: %u original caller cases, the 6000/6001 threshold, eligibility, draw count, facing and both RAM views\n", checked);
    return 0;
}
