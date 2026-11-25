#include "common.h"
#include <abs.h>
#include <rand.h>

#include "../internal/field_calls.h"
#include "../internal/field_records.h"
#include "common/golem_shape.h"
#include "common/saved_game.h"
#include "common/vector.h"
#include "main/main.h"

#define DISTANCE_BUCKET_COUNT_MIN 4
#define DISTANCE_BUCKET_COUNT_MAX 6
#define DISTANCE_STEP_COUNT 150
#define DISTANCE_STEP_SIZE 256
#define DISTANCE_BUCKET_WEIGHT 1024

#define CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

/**
 * @brief Pick a grid column from the distance between the companion and an actor.
 *
 * The x distance, in DISTANCE_STEP_SIZE steps up to DISTANCE_STEP_COUNT, is
 * split into grid_bound buckets (the joined golem group's grid_bound, clamped
 * to 4-6). The bucket the distance falls into weighs DISTANCE_BUCKET_WEIGHT
 * and every bucket further away a third of its neighbour; a random draw over
 * the total weight selects the column.
 *
 * @param actor_id Actor whose distance to the companion is measured.
 * @return Selected bucket, or the distance's own bucket when the draw selects none.
 * @note With no golem group joined the bucket count is read uninitialized, and
 *       the weight total is never cleared before it is summed (original bugs).
 */
s32 field_select_distance_bucket(s32 actor_id)
{
    Vec3i companion_position = {0};
    Vec3i actor_position = {0};
    s32 weights[DISTANCE_BUCKET_COUNT_MAX];
    s32 draw;
    s32 index;
    s32 radius;
    s32 bucket;
    s32 divisor;
    s32 cumulative_weight;
    s32 center;
    s32 total_weight = 0;
    s32 selected;
    s32 distance;
    s32 bucket_count = DISTANCE_BUCKET_COUNT_MIN;

    selected = -1;

    if ((u32)g_saved_game.layout.joined_golem < GOLEM_RECORD_COUNT)
    {
        bucket_count = g_saved_game.layout.golem_records[g_saved_game.layout.joined_golem].logic_layout.bits.grid_bound;
    }

    bucket_count = CLAMP(bucket_count, DISTANCE_BUCKET_COUNT_MIN, DISTANCE_BUCKET_COUNT_MAX);
    field_get_actor_position(FIELD_PARTY_COMPANION, &companion_position);
    field_get_actor_position(actor_id, &actor_position);
    distance = abs(companion_position.x - actor_position.x);
    center = distance / DISTANCE_STEP_SIZE;
    center = CLAMP(center, 0, DISTANCE_STEP_COUNT - 1);
    divisor = 1;
    center /= DISTANCE_STEP_COUNT / bucket_count;
    center = CLAMP(center, 0, bucket_count - 1);

    for (radius = 0; radius < bucket_count; radius++)
    {
        index = center + radius;

        if (index < bucket_count)
        {
            weights[index] = DISTANCE_BUCKET_WEIGHT / divisor;
        }

        index = center - radius;

        if (index >= 0)
        {
            weights[index] = DISTANCE_BUCKET_WEIGHT / divisor;
        }

        divisor *= 3;
    }

    for (radius = 0; radius < bucket_count; radius++)
    {
        total_weight += weights[radius];
    }

    draw = (rand() * total_weight) / RAND_MAX;
    cumulative_weight = 0;

    for (bucket = 0; bucket < bucket_count; bucket++)
    {
        if (draw >= cumulative_weight && draw < cumulative_weight + weights[bucket])
        {
            selected = bucket;
        }

        cumulative_weight += weights[bucket];
    }

    if (selected == -1)
    {
        selected = center;
    }

    return selected;
}
