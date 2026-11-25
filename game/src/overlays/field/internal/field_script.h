#ifndef FIELD_SCRIPT_H
#define FIELD_SCRIPT_H

#include "common.h"

#define FIELD_SCRIPT_RUNNING 0x80000000

#define FIELD_SCRIPT_OWNER 0xFF

#define FIELD_SCRIPT_BYTE_NONE 0xFF
#define FIELD_SCRIPT_OPTIONAL(value) (((value) == FIELD_SCRIPT_BYTE_NONE) ? -1 : (value))
#define FIELD_SCRIPT_BYTE_FLAG 0x80
#define FIELD_SCRIPT_BYTE_VALUE_MASK 0x7F

#define FIELD_SCRIPT_COND 0x1

#define OPERAND_TYPE_0(descriptor) ((descriptor) >> 6)
#define OPERAND_TYPE_1(descriptor) (((descriptor) >> 4) & 3)
#define OPERAND_TYPE_2(descriptor) (((descriptor) >> 2) & 3)
#define OPERAND_TYPE_3(descriptor) ((descriptor) & 3)

#define FIELD_SCRIPT_FRAME_COUNT 8

typedef struct FieldScriptFrame
{
    u8* pc;
    u32 flags;

    union
    {
        u32 word;

        struct
        {
            u32 resume : 1;
            u32 frames : 31;
        } bits;
    } wait;
} FieldScriptFrame;

typedef struct FieldScriptState
{
    union
    {
        u32 word;
        u8 owner_id;

        struct
        {
            u32 owner_id : 8;
            u32 unused_8 : 1;
            u32 local_base : 7;
            u32 unused_16 : 15;
            u32 running : 1;
        } bits;
    } status;

    s32 depth;
    FieldScriptFrame frames[FIELD_SCRIPT_FRAME_COUNT];
} FieldScriptState;

typedef struct
{
    s32 previous_flags;
    s32 previous_wait;
    u8* pc;
} FieldScriptRecord;

typedef struct
{
    s32 previous_flags;
    s32 previous_wait;
    u8* pc;
    u32 flags;

    union
    {
        u32 word;

        struct
        {
            u32 resume : 1;
            u32 frames : 31;
        } bits;
    } wait;
} FieldScriptRecordState;

#define FIELD_SCRIPT_VAR_LOCATION_MASK 0xFFF

#define FIELD_SCRIPT_VAR_SHIFT_MASK 0x1F

#define FIELD_SCRIPT_VAR_KIND(ref) (((ref) >> 12) & 7)
#define FIELD_SCRIPT_VAR_KIND_MASK 0x7000

#define FIELD_SCRIPT_VAR_LOCAL_KIND 3

#define FIELD_SCRIPT_VAR_OWNER_RELATIVE 0x8000
#define FIELD_SCRIPT_VAR_OWNER_RELATIVE_SHIFT 15

#define FIELD_SCRIPT_VAR_WORD_STEP 0x20

typedef struct
{
    u16 value;
} FieldScriptVariableRef;

extern FieldScriptState* g_field_script;

extern void (*g_field_script_ext_op_table[16])();

/**
 * @brief A call the route recorder wraps to see each opcode handler the
 * interpreter runs (tests/support/routes/route_record_cover.c). Nothing in the PS1
 * view.
 */
#if defined(LOM_NATIVE_POINTERS)
void lom_native_field_script_handler(void* handler);
#define FIELD_SCRIPT_TRACE(handler) lom_native_field_script_handler((void*)(handler))
#else
#define FIELD_SCRIPT_TRACE(handler) ((void)0)
#endif

#if defined(LOM_NATIVE_POINTERS)
#define FIELD_SCRIPT_FRAME_OFFSET(index) ((index) * sizeof(FieldScriptFrame))
_Static_assert(__builtin_offsetof(FieldScriptState, frames[1].pc) == FIELD_SCRIPT_FRAME_OFFSET(1) + __builtin_offsetof(FieldScriptRecord, pc) &&
                   __builtin_offsetof(FieldScriptState, frames[1].flags) ==
                       FIELD_SCRIPT_FRAME_OFFSET(2) + __builtin_offsetof(FieldScriptRecord, previous_flags) &&
                   __builtin_offsetof(FieldScriptState, frames[1].wait) == FIELD_SCRIPT_FRAME_OFFSET(1) + __builtin_offsetof(FieldScriptRecordState, wait),
               "FieldScriptRecord views a frame");
#else
#define FIELD_SCRIPT_FRAME_OFFSET(index) ((index) * 3 << 2)
#endif
#define FIELD_SCRIPT_RECORD(index) ((FieldScriptRecord*)((u8*)g_field_script + FIELD_SCRIPT_FRAME_OFFSET(index)))
#define FIELD_SCRIPT_RECORD_STATE(index) ((FieldScriptRecordState*)((u8*)g_field_script + FIELD_SCRIPT_FRAME_OFFSET(index)))
#define FIELD_SCRIPT_ACTIVE_RECORD() FIELD_SCRIPT_RECORD(g_field_script->depth)
#define FIELD_SCRIPT_ACTIVE_RECORD_STATE() FIELD_SCRIPT_RECORD_STATE(g_field_script->depth)

u8* field_script_read_operand(s32 type, u8* data, s32* value);
u8* field_script_read_operand_or_owner(s32 type, u8* data, s32* value);
u8* field_script_read_u16(u8* data, u16* value);

void field_script_branch();

s32 field_read_script_var();
void field_write_script_var();

s32 field_get_script_var(s32 owner_id, s32 variable);
void field_set_script_var(s32 owner_id, u32 variable, s32 value);

s32 field_read_bits(s32 width, void* base, s32 index, s32 shift, s32 bit_count);
void field_write_bits(s32 width, void* base, s32 index, s32 shift, s32 bit_count, s32 value);

#endif
