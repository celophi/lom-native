#ifndef FIELD_ACTOR_SEQUENCE_RUNTIME_H
#define FIELD_ACTOR_SEQUENCE_RUNTIME_H

#include "field_effect_types.h"
#include "field_actor_binding.h"

#define FIELD_SEQUENCE_BANK_COUNT 11
#define FIELD_SEQUENCE_ROW_COUNT 24
#define FIELD_SEQUENCE_ROW_SIZE 32
#define FIELD_SEQUENCE_BANK_SIZE (FIELD_SEQUENCE_ROW_COUNT * FIELD_SEQUENCE_ROW_SIZE)

extern u8 g_field_actor_sequence_data[FIELD_SEQUENCE_BANK_COUNT * FIELD_SEQUENCE_BANK_SIZE];

struct FieldActor;

void field_apply_sequence_displacement(struct FieldActor* actor, s32 direction_x, s32 vertical_step, s32 direction_z);
void field_update_sequence_actor_binding(struct FieldActor* actor, s32 release_actor);
s32 field_execute_actor_sequence(struct FieldActor* actor, s32 script_index);
s32 field_allocate_sequence_actor(s32 index, s32 flags);
void field_restart_sequence_animation(struct FieldActor* actor);
void field_update_object_tints(void);

#endif
