#ifndef _FIELD_ENTRY_H
#define _FIELD_ENTRY_H

#include "common.h"

typedef union FieldSceneId
{
    s32 word;
    u16 id;
} FieldSceneId;

extern u32 g_field_spawn_id;
extern FieldSceneId g_field_entry_scene;
extern s32 g_field_sound_bank_id;
extern s32 g_field_music_id;
extern s32 g_field_object_id;
extern s32 g_field_secondary_music_id;
extern s32 g_field_scene_request_pending;

#endif
