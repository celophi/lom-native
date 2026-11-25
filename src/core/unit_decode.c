#include "lom_native_unit_decode.h"
#include "lom_native_ps1_memory.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LOADED_UNITS 4

typedef struct
{
    const LomUnit* unit;
    unsigned char* window;
} LoadedUnit;

struct LomUnitDecodeContext
{
    const LomUnitGlobal* global;
};

static LoadedUnit s_loaded[MAX_LOADED_UNITS];

static int s_lenient;

/**
 * @brief Stop because an address can't be decoded.
 * @param where Who asked.
 * @param what What went wrong.
 * @param address The PS1 address.
 */
static void fail(const char* where, const char* what, unsigned int address)
{
    fprintf(stderr, "unit decode: %s: %s 0x%08X\n", where, what, address);
    abort();
}

/**
 * @brief Check whether two units' PS1 images overlap.
 * @param a One.
 * @param b The other.
 * @return Nonzero when they do.
 */
static int overlaps(const LomUnit* a, const LomUnit* b)
{
    return a->vram < b->vram + b->image_size && b->vram < a->vram + a->image_size;
}

/**
 * @brief Find where a PS1 address is in the port's memory: in a loaded unit's
 *        globals, its image, or a fixed address.
 * @param address The PS1 address, or 0.
 * @param where Who asked, for the error.
 * @param fail_if_missing Nonzero to stop when it can't be found, unless
 *        decoding is lenient.
 * @return Where it is, or NULL.
 */
static void* host_address(unsigned int address, const char* where, int fail_if_missing)
{
    if (address == 0)
    {
        return NULL;
    }
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        const LomUnit* unit = s_loaded[slot].unit;
        if (unit == NULL)
        {
            continue;
        }
        for (size_t i = 0; i < unit->global_count; i++)
        {
            const LomUnitGlobal* global = &unit->globals[i];
            if (address < global->address || address >= global->address + global->size)
            {
                continue;
            }
            unsigned int offset = address - global->address;
            if (global->plain)
            {
                return (unsigned char*)global->host + offset;
            }
            if (offset == 0)
            {
                return global->host;
            }
            if (global->ps1_stride != 0 && offset % global->ps1_stride == 0)
            {
                return (unsigned char*)global->host + offset / global->ps1_stride * global->host_stride;
            }
            if (fail_if_missing && !s_lenient)
            {
                fprintf(stderr, "unit decode: %s: points %u bytes into %s, inside an element\n", where, offset, global->name);
                fail(where, "can't place", address);
            }
            return NULL;
        }
        if (address >= unit->vram && address < unit->vram + unit->image_size && s_loaded[slot].window != NULL)
        {
            return s_loaded[slot].window + (address - unit->vram);
        }
    }
    void* fixed = lom_native_fixed_address(address);
    if (fixed != NULL)
    {
        return fixed;
    }
    if (fail_if_missing && !s_lenient)
    {
        fail(where, "no loaded unit has", address);
    }
    return NULL;
}

void* lom_unit_host_address(unsigned int address, const char* where)
{
    return host_address(address, where, 1);
}

void* lom_unit_host_code(unsigned int address, const char* where)
{
    if (address == 0)
    {
        return NULL;
    }
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        const LomUnit* unit = s_loaded[slot].unit;
        if (unit == NULL)
        {
            continue;
        }
        for (size_t i = 0; i < unit->function_count; i++)
        {
            if (unit->functions[i].address == address)
            {
                if (unit->functions[i].code == NULL && !s_lenient)
                {
                    fprintf(stderr, "unit decode: %s: %s isn't built natively\n", where, unit->functions[i].name);
                    fail(where, "no code for", address);
                }
                return (void*)unit->functions[i].code;
            }
        }
    }
    if (!s_lenient)
    {
        fail(where, "no loaded unit has a function at", address);
    }
    return NULL;
}

unsigned int lom_unit_ps1_address(const void* host)
{
    const unsigned char* bytes = host;
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        const LomUnit* unit = s_loaded[slot].unit;
        if (unit == NULL)
        {
            continue;
        }
        for (size_t i = 0; i < unit->global_count; i++)
        {
            const LomUnitGlobal* global = &unit->globals[i];
            const unsigned char* start = global->host;
            if (bytes < start || bytes >= start + global->host_size)
            {
                continue;
            }
            size_t offset = (size_t)(bytes - start);
            if (global->plain)
            {
                return global->address + (unsigned int)offset;
            }
            if (offset == 0)
            {
                return global->address;
            }
            if (global->host_stride != 0 && offset % global->host_stride == 0)
            {
                return global->address + (unsigned int)(offset / global->host_stride * global->ps1_stride);
            }
            return 0;
        }
        unsigned char* window = s_loaded[slot].window;
        if (window != NULL && bytes >= window && bytes < window + unit->image_size)
        {
            return unit->vram + (unsigned int)(bytes - window);
        }
    }
    return 0;
}

unsigned int lom_unit_ps1_pointer(const void* host, const char* where)
{
    if (host == NULL)
    {
        return 0;
    }
    unsigned int address = lom_unit_ps1_address(host);
    if (address == 0)
    {
        address = lom_native_fixed_ps1_address(host);
    }
    if (address == 0 && !s_lenient)
    {
        fprintf(stderr, "unit decode: %s: %p isn't in a loaded unit\n", where, host);
        abort();
    }
    return address;
}

unsigned int lom_unit_ps1_code_address(const void* code, const char* where)
{
    if (code == NULL)
    {
        return 0;
    }
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        const LomUnit* unit = s_loaded[slot].unit;
        for (size_t i = 0; unit != NULL && i < unit->function_count; i++)
        {
            if (unit->functions[i].code == code)
            {
                return unit->functions[i].address;
            }
        }
    }
    if (s_lenient)
    {
        return 0;
    }
    fprintf(stderr, "unit decode: %s: %p isn't a function of a loaded unit\n", where, code);
    abort();
}

static unsigned char* s_encoded;
static size_t s_encoded_size;

int lom_unit_read_ps1(unsigned int address, void* out, unsigned int size)
{
    unsigned char* bytes = out;
    int all = 1;

    memset(out, 0, size);

    for (unsigned int done = 0; done < size;)
    {
        unsigned int at = address + done;
        const LomUnitGlobal* found = NULL;

        for (int slot = 0; slot < MAX_LOADED_UNITS && found == NULL; slot++)
        {
            const LomUnit* unit = s_loaded[slot].unit;

            for (size_t i = 0; unit != NULL && i < unit->global_count; i++)
            {
                const LomUnitGlobal* global = &unit->globals[i];

                if (at >= global->address && at < global->address + global->size && (global->plain || global->encode != NULL))
                {
                    found = global;
                    break;
                }
            }
        }

        if (found == NULL)
        {
            all = 0;
            done++;
            continue;
        }

        unsigned int offset = at - found->address;
        unsigned int count = found->size - offset < size - done ? found->size - offset : size - done;

        if (found->plain)
        {
            memcpy(bytes + done, (const unsigned char*)found->host + offset, count);
        }
        else
        {
            if (s_encoded_size < found->size)
            {
                unsigned char* grown = realloc(s_encoded, found->size);

                if (grown == NULL)
                {
                    fail("lom_unit_read_ps1", "out of memory encoding the global at", found->address);
                }

                s_encoded = grown;
                s_encoded_size = found->size;
            }

            s_lenient = 1;
            found->encode(s_encoded);
            s_lenient = 0;
            memcpy(bytes + done, s_encoded + offset, count);
        }

        done += count;
    }

    return all;
}

int lom_unit_write_ps1(unsigned int address, const void* in, unsigned int size)
{
    const unsigned char* bytes = in;
    int all = 1;

    for (unsigned int done = 0; done < size;)
    {
        unsigned int at = address + done;
        const LomUnitGlobal* found = NULL;

        for (int slot = 0; slot < MAX_LOADED_UNITS && found == NULL; slot++)
        {
            const LomUnit* unit = s_loaded[slot].unit;

            for (size_t i = 0; unit != NULL && i < unit->global_count; i++)
            {
                const LomUnitGlobal* global = &unit->globals[i];

                if (at >= global->address && at < global->address + global->size && (global->plain || (global->encode != NULL && global->decode != NULL)))
                {
                    found = global;
                    break;
                }
            }
        }

        if (found == NULL)
        {
            all = 0;
            done++;
            continue;
        }

        unsigned int offset = at - found->address;
        unsigned int count = found->size - offset < size - done ? found->size - offset : size - done;

        if (found->plain)
        {
            memcpy((unsigned char*)found->host + offset, bytes + done, count);
        }
        else
        {
            if (s_encoded_size < found->size)
            {
                unsigned char* grown = realloc(s_encoded, found->size);

                if (grown == NULL)
                {
                    fail("lom_unit_write_ps1", "out of memory encoding the global at", found->address);
                }

                s_encoded = grown;
                s_encoded_size = found->size;
            }

            s_lenient = 1;
            found->encode(s_encoded);
            memcpy(s_encoded + offset, bytes + done, count);
            found->decode(s_encoded, NULL);
            s_lenient = 0;
        }

        done += count;
    }

    return all;
}

void lom_unit_decode_lenient(void (*decode)(const unsigned char*, void*), const unsigned char* source, void* destination)
{
    s_lenient = 1;
    decode(source, destination);
    s_lenient = 0;
}

void lom_unit_expect_zero(const unsigned char* bytes, unsigned int size, const char* name)
{
    for (unsigned int i = 0; i < size; i++)
    {
        if (bytes[i] != 0)
        {
            fprintf(stderr, "unit decode: %s is listed as run-time state, but its byte %u in the file is 0x%02X\n", name, i, bytes[i]);
            abort();
        }
    }
}

void* lom_unit_decode_pointer(LomUnitDecodeContext* context, unsigned int address, const char* where)
{
    (void)context;
    return host_address(address, where, 1);
}

void* lom_unit_decode_code(LomUnitDecodeContext* context, unsigned int address, const char* where)
{
    (void)context;
    return lom_unit_host_code(address, where);
}

int lom_unit_is_loaded(const char* name)
{
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        if (s_loaded[slot].unit != NULL && strcmp(s_loaded[slot].unit->name, name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

size_t lom_unit_host_bytes_left(const void* host)
{
    uintptr_t address = (uintptr_t)host;

    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        const LomUnit* unit = s_loaded[slot].unit;

        for (size_t i = 0; unit != NULL && i < unit->global_count; i++)
        {
            uintptr_t start = (uintptr_t)unit->globals[i].host;

            if (address >= start && address < start + unit->globals[i].host_size)
            {
                return start + unit->globals[i].host_size - address;
            }
        }
    }

    return 0;
}

void lom_unit_load(const LomUnit* unit, const void* image, void* window)
{
    int free_slot = -1;
    for (int slot = 0; slot < MAX_LOADED_UNITS; slot++)
    {
        if (s_loaded[slot].unit != NULL && (s_loaded[slot].unit == unit || overlaps(s_loaded[slot].unit, unit)))
        {
            s_loaded[slot].unit = NULL;
        }
        if (s_loaded[slot].unit == NULL && free_slot < 0)
        {
            free_slot = slot;
        }
    }
    if (free_slot < 0)
    {
        fail(unit->name, "too many units loaded at once, loading at", unit->vram);
    }
    s_loaded[free_slot].unit = unit;
    s_loaded[free_slot].window = window;

    const unsigned char* bytes = image;
    for (size_t i = 0; i < unit->global_count; i++)
    {
        const LomUnitGlobal* global = &unit->globals[i];
        LomUnitDecodeContext context = {global};
        if (global->address + global->size <= unit->vram + unit->image_size)
        {
            global->decode(bytes + (global->address - unit->vram), &context);
        }
        else if (global->address >= unit->vram + unit->image_size)
        {
            memset(global->host, 0, global->host_size);
        }
        else
        {
            size_t in_file = unit->vram + unit->image_size - global->address;
            unsigned char* padded = calloc(1, global->size);
            if (padded == NULL)
            {
                fail(global->name, "out of memory decoding the global at", global->address);
            }
            memcpy(padded, bytes + (global->address - unit->vram), in_file);
            global->decode(padded, &context);
            free(padded);
        }
    }
}

void lom_unit_encode_lenient(void (*encode)(const void*, unsigned char*), const void* source, unsigned char* destination)
{
    s_lenient = 1;
    encode(source, destination);
    s_lenient = 0;
}
