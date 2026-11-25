#ifndef FIELD_ACTOR_BINDING_H
#define FIELD_ACTOR_BINDING_H

#include "common.h"

#define FIELD_ACTOR_BINDING_COUNT 3
#define FIELD_SHARED_BINDING (FIELD_ACTOR_BINDING_COUNT - 1)

#define FIELD_BINDING_IDLE 0
#define FIELD_BINDING_LOADING 1
#define FIELD_BINDING_READY 2

typedef struct
{
    s32 state;
    s32 load_id;
    s32 resource_id;
    s32 owner;
    s32 unused_10;
    s32 unused_14;
    s32 slot;
} FieldActorBinding;

extern FieldActorBinding g_field_actor_bindings[];

void field_clear_pending_binding_restarts(void);
void field_restart_pending_bindings(void);

/**
 * @brief Binding used by object @p object_index (objects 2 and up share the last binding).
 * @param object_index Object index; only its low byte selects the binding.
 * @return The object's animation actor binding.
 */
static inline FieldActorBinding* field_object_binding(s32 object_index)
{
    FieldActorBinding* bindings;
    s32 offset;

    bindings = g_field_actor_bindings;
    if ((u8)object_index < FIELD_SHARED_BINDING)
    {
        offset = object_index * sizeof(FieldActorBinding);
    }
    else
    {
        offset = FIELD_SHARED_BINDING * sizeof(FieldActorBinding);
    }
    return (FieldActorBinding*)((u8*)bindings + offset);
}

#endif
