#include "lom_native_card.h"
#include "lom_native_boundary.h"

/**
 * @brief Report a memory card call to lom_native_boundary.h.
 * @param call Which call.
 * @param a Its argument.
 * @param result What it returned.
 * @return @p result.
 */
static long report(LomBoundarySaveCall call, long a, long result)
{
    lom_native_boundary_event(LOM_BOUNDARY_SAVES, call, (unsigned)a, (unsigned)result);
    return result;
}

long lom_native_card_open_libapi(char* path, unsigned long mode)
{
    return report(LOM_BOUNDARY_SAVE_OPEN, (long)mode, lom_native_card_open(path, (int)mode));
}

long lom_native_card_close_libapi(long handle)
{
    return report(LOM_BOUNDARY_SAVE_CLOSE, 0, lom_native_card_close((int)handle));
}

long lom_native_card_read_libapi(long handle, void* data, long size)
{
    return report(LOM_BOUNDARY_SAVE_READ, size, lom_native_card_read((int)handle, data, (int)size));
}

long lom_native_card_write_libapi(long handle, void* data, long size)
{
    return report(LOM_BOUNDARY_SAVE_WRITE, size, lom_native_card_write((int)handle, data, (int)size));
}

long lom_native_card_erase_libapi(char* path)
{
    return report(LOM_BOUNDARY_SAVE_ERASE, 0, lom_native_card_erase(path));
}

long lom_native_card_rename_libapi(char* from, char* to)
{
    return report(LOM_BOUNDARY_SAVE_RENAME, 0, lom_native_card_rename(from, to));
}
