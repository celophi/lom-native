#ifndef FIELD_CONTACT_GEOMETRY_H
#define FIELD_CONTACT_GEOMETRY_H

#include "field_effect_types.h"
#include "common/vector.h"

struct FieldActor;

typedef union
{
    s32 packed;
    Vec2s coord;
} FieldContactPoint;

typedef struct
{
    s32 actor;
    FieldContactPoint point;
} FieldContactResult;

void field_load_actor_sequence_data(void);
s32 field_test_quad_actor_contacts(FieldContactPoint* quad, FieldMotionRecord* effect, FieldContactResult* contact);
s32 field_resolve_actor_movement(struct FieldActor* actor, s32* position, s32 mode);
#define FIELD_CONTACT_RESULT_PRESENT 0x8000
#define FIELD_CONTACT_RESULT_ACTOR_MASK 0x7FFF
s32 field_find_actor_overlap(struct FieldActor* actor, s32* position, s32 filter_group);
void field_start_actor_contact_interaction(struct FieldActor* actor, s32 object_index);
void field_probe_actor_interaction(struct FieldActor* actor);
s32 field_find_actor_in_range(s32* reference_position, s32 distance_limit, FieldActorSlot* source_actor, s32 opposing_group);
void field_collect_attack_sphere_hits(FieldActorSlot* actor, FieldObjectPart* part);
s32 field_get_position_distance(VECTOR* a, VECTOR* b);

#endif
