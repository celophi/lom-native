#include "common.h"
#include "overlays/field/internal/field_calls.h"
#include "overlays/field/internal/field_state_ops.h"
typedef struct FieldMenuElement FieldMenuElement;
#include "lom_native_overlay_entry.h"
#include <stdio.h>

/**
 * @brief Stop FIELD because its script reached a service the port hasn't
 *        ported.
 * @param name The service.
 */
static _Noreturn void unsupported(const char* name)
{
    fprintf(stderr, "[lom-native] FIELD script reached unported service %s\n", name);
    lom_native_field_fail(name);
}

int lom_native_card_mcx_sync(int mode, long* command, long* result)
{
    unsupported("PocketStation sync");
}

int lom_native_card_mcx_get_time(int port, unsigned char* time)
{
    unsupported("PocketStation clock read");
}
