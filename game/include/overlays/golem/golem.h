#ifndef OVERLAYS_GOLEM_H
#define OVERLAYS_GOLEM_H

#include "common.h"

struct GolemRenderContext;

/**
 * @brief Run the golem workshop until the player leaves it.
 * @param render_buffers Render context GOLEM draws into.
 * @param restore_slot_on_cancel Nonzero restores the selected golem slot when the player cancels.
 */
void golem_run(struct GolemRenderContext* render_buffers, s32 restore_slot_on_cancel);

#endif
