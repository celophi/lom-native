#include "lom_native_unit_decode.h"

#include "main/cd_resources.h"

#include <stddef.h>

/**
 * @brief Find the decoded unit of an overlay, by its file.
 * @param resource The overlay's file.
 * @return The unit, or NULL for a file that isn't an overlay.
 */
static const LomUnit* unit_of(int resource)
{
    switch (resource)
    {
    case CD_RES_FIELD_BIN:
        return &lom_unit_field;
    case CD_RES_WMAP_BIN:
        return &lom_unit_wmap;
    case CD_RES_TITLE_BIN:
        return &lom_unit_title;
    case CD_RES_GNAME_BIN:
        return &lom_unit_gname;
    case CD_RES_MENU_BIN:
        return &lom_unit_menu;
    case CD_RES_SHOP_BIN:
        return &lom_unit_shop;
    case CD_RES_ZUKAN_BIN:
        return &lom_unit_zukan;
    case CD_RES_GOLEM_BIN:
        return &lom_unit_golem;
    case CD_RES_GOVER_BIN:
        return &lom_unit_gover;
    case CD_RES_CARDA_BIN:
        return &lom_unit_carda;
    case CD_RES_GOSUB_BIN:
        return &lom_unit_gosub;
    case CD_RES_WSEL_BIN:
        return &lom_unit_wsel;
    case CD_RES_CLOAD_BIN:
        return &lom_unit_cload;
    case CD_RES_NIKI_BIN:
        return &lom_unit_niki;
    case CD_RES_ADDHERO_BIN:
        return &lom_unit_addhero;
    default:
        return NULL;
    }
}

void lom_native_unit_streamed(int resource, void* destination, int size)
{
    const LomUnit* unit = unit_of(resource);
    if (unit != NULL && size > 0)
    {
        lom_unit_load(unit, destination, destination);
    }
}
