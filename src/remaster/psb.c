#include "remaster_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Read a little-endian number.
 * @param p Its first byte.
 * @param n How many bytes it takes, up to 8.
 * @return The number.
 */
static uint64_t le(const unsigned char* p, unsigned n)
{
    uint64_t v = 0;

    for (unsigned i = 0; i < n; ++i)
    {
        v |= (uint64_t)p[i] << (8 * i);
    }

    return v;
}

unsigned char* lom_remaster_load(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");

    if (!file)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* data = length > 0 ? malloc((size_t)length) : NULL;

    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length)
    {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);

    if (length < 8 || memcmp(data, "mdf", 4) != 0)
    {
        *size = (size_t)length;
        return data;
    }

    size_t unpacked = (size_t)le(data + 4, 4);
    unsigned char* out = malloc(unpacked ? unpacked : 1);
    size_t written = 0;

    if (!out || lom_remaster_inflate(data + 8, (size_t)length - 8, out, unpacked, &written) || written != unpacked)
    {
        free(out);
        free(data);
        return NULL;
    }

    free(data);
    *size = unpacked;
    return out;
}

typedef struct
{
    size_t count, item_size, items, end;
} IntArray;

/**
 * @brief Read the header of a PSB int array: a type byte that says how many
 *        bytes its count takes, the count, then a type byte that says how many
 *        bytes each item takes.
 * @param psb The reader.
 * @param pos The array's offset.
 * @param array Set to where its items are and how big they are.
 * @return 0, or -1 when it isn't an int array or runs past the end.
 */
static int int_array(const LomPsb* psb, size_t pos, IntArray* array)
{
    if (pos >= psb->size)
    {
        return -1;
    }

    unsigned type = psb->data[pos];

    if (type < 0x0D || type > 0x14 || pos + 1 + (type - 0x0C) >= psb->size)
    {
        return -1;
    }

    unsigned count_size = type - 0x0C;
    array->count = (size_t)le(psb->data + pos + 1, count_size);
    unsigned size_type = psb->data[pos + 1 + count_size];

    if (size_type < 0x0D || size_type > 0x14)
    {
        return -1;
    }

    array->item_size = size_type - 0x0C;
    array->items = pos + 2 + count_size;
    array->end = array->items + array->count * array->item_size;
    return array->end <= psb->size ? 0 : -1;
}

/**
 * @brief Read an item of an int array.
 * @param psb The reader.
 * @param array The array, from int_array().
 * @param index Which item, from 0. It isn't checked.
 * @return The item.
 */
static uint64_t array_at(const LomPsb* psb, const IntArray* array, size_t index)
{
    return le(psb->data + array->items + index * array->item_size, (unsigned)array->item_size);
}

/**
 * @brief Read the PSB's names, which are stored as a tree of characters, each
 *        name a path from a leaf back to the root.
 * @param psb The reader. Its names are set.
 * @param pos Where the tree starts.
 * @return 0, or -1 when the tree is bad or memory runs out.
 */
static int read_names(LomPsb* psb, size_t pos)
{
    IntArray charset, tree, indexes;

    if (int_array(psb, pos, &charset) || int_array(psb, charset.end, &tree) || int_array(psb, tree.end, &indexes))
    {
        return -1;
    }

    psb->name_count = indexes.count;
    psb->names = calloc(indexes.count ? indexes.count : 1, sizeof(char*));

    if (!psb->names)
    {
        return -1;
    }

    for (size_t i = 0; i < indexes.count; ++i)
    {
        unsigned char reversed[256];
        size_t length = 0;
        uint64_t node = array_at(psb, &tree, (size_t)array_at(psb, &indexes, i));

        while (node != 0 && length < sizeof(reversed))
        {
            if (node >= tree.count)
            {
                return -1;
            }

            uint64_t parent = array_at(psb, &tree, (size_t)node);

            if (parent >= charset.count)
            {
                return -1;
            }

            reversed[length++] = (unsigned char)(node - array_at(psb, &charset, (size_t)parent));
            node = parent;
        }

        char* name = malloc(length + 1);

        if (!name)
        {
            return -1;
        }

        for (size_t j = 0; j < length; ++j)
        {
            name[j] = (char)reversed[length - 1 - j];
        }

        name[length] = 0;
        psb->names[i] = name;
    }

    return 0;
}

int lom_psb_open(LomPsb* psb, const unsigned char* data, size_t size)
{
    memset(psb, 0, sizeof(*psb));
    psb->data = data;
    psb->size = size;

    if (size < 40 || memcmp(data, "PSB", 4) != 0 || le(data + 6, 2) != 0)
    {
        return -1;
    }

    psb->strings = (size_t)le(data + 16, 4);
    psb->strings_data = (size_t)le(data + 20, 4);
    psb->chunk_offsets = (size_t)le(data + 24, 4);
    psb->chunk_lengths = (size_t)le(data + 28, 4);
    psb->chunk_data = (size_t)le(data + 32, 4);
    psb->root = (size_t)le(data + 36, 4);

    if (read_names(psb, (size_t)le(data + 12, 4)) || psb->root >= size)
    {
        lom_psb_close(psb);
        return -1;
    }

    return 0;
}

void lom_psb_close(LomPsb* psb)
{
    for (size_t i = 0; psb->names && i < psb->name_count; ++i)
    {
        free(psb->names[i]);
    }

    free(psb->names);
    psb->names = NULL;
    psb->name_count = 0;
}

int lom_psb_kind(const LomPsb* psb, size_t value)
{
    if (value >= psb->size)
    {
        return LOM_PSB_BAD;
    }

    unsigned type = psb->data[value];

    if (type <= 1)
    {
        return LOM_PSB_NONE;
    }
    if (type <= 3)
    {
        return LOM_PSB_BOOL;
    }
    if (type <= 0x0C)
    {
        return LOM_PSB_INT;
    }
    if (type <= 0x14)
    {
        return LOM_PSB_INT_ARRAY;
    }
    if (type <= 0x18)
    {
        return LOM_PSB_STRING;
    }
    if (type <= 0x1C)
    {
        return LOM_PSB_CHUNK;
    }
    if (type <= 0x1F)
    {
        return LOM_PSB_FLOAT;
    }
    if (type == 0x20)
    {
        return LOM_PSB_LIST;
    }
    if (type == 0x21)
    {
        return LOM_PSB_OBJECT;
    }
    return LOM_PSB_BAD;
}

int64_t lom_psb_int(const LomPsb* psb, size_t value)
{
    if (value >= psb->size)
    {
        return 0;
    }

    unsigned type = psb->data[value];

    if (type == 3)
    {
        return 1;
    }

    if (type == 0x1E && value + 5 <= psb->size)
    {
        float f;
        memcpy(&f, psb->data + value + 1, sizeof(f));
        return (int64_t)f;
    }

    if (type == 0x1F && value + 9 <= psb->size)
    {
        double d;
        memcpy(&d, psb->data + value + 1, sizeof(d));
        return (int64_t)d;
    }

    if (type < 5 || type > 0x0C)
    {
        return 0;
    }

    unsigned n = type - 4;
    uint64_t v = le(psb->data + value + 1, n);

    if (n < 8 && (v >> (8 * n - 1)) & 1)
    {
        v |= ~UINT64_C(0) << (8 * n);
    }

    return (int64_t)v;
}

/**
 * @brief Read the index that follows a string's or chunk's type byte, whose
 *        size the type gives.
 * @param psb The reader.
 * @param value The string or chunk.
 * @param first_type The type that means a one-byte index.
 * @return The index.
 */
static uint64_t index_after(const LomPsb* psb, size_t value, unsigned first_type)
{
    unsigned n = psb->data[value] - first_type + 1;
    return le(psb->data + value + 1, n);
}

const char* lom_psb_string(const LomPsb* psb, size_t value)
{
    IntArray offsets;

    if (lom_psb_kind(psb, value) != LOM_PSB_STRING || int_array(psb, psb->strings, &offsets))
    {
        return NULL;
    }

    uint64_t index = index_after(psb, value, 0x15);

    if (index >= offsets.count)
    {
        return NULL;
    }

    size_t start = psb->strings_data + (size_t)array_at(psb, &offsets, (size_t)index);
    return start < psb->size && memchr(psb->data + start, 0, psb->size - start) ? (const char*)psb->data + start : NULL;
}

int lom_psb_chunk(const LomPsb* psb, size_t value, const unsigned char** data, size_t* size)
{
    IntArray offsets, lengths;

    if (lom_psb_kind(psb, value) != LOM_PSB_CHUNK || int_array(psb, psb->chunk_offsets, &offsets) || int_array(psb, psb->chunk_lengths, &lengths))
    {
        return -1;
    }

    uint64_t index = index_after(psb, value, 0x19);

    if (index >= offsets.count || index >= lengths.count)
    {
        return -1;
    }

    size_t start = psb->chunk_data + (size_t)array_at(psb, &offsets, (size_t)index);
    size_t length = (size_t)array_at(psb, &lengths, (size_t)index);

    if (start > psb->size || length > psb->size - start)
    {
        return -1;
    }

    *data = psb->data + start;
    *size = length;
    return 0;
}

int64_t lom_psb_array_item(const LomPsb* psb, size_t array, size_t index)
{
    IntArray a;
    return int_array(psb, array, &a) || index >= a.count ? 0 : (int64_t)array_at(psb, &a, index);
}

size_t lom_psb_count(const LomPsb* psb, size_t list)
{
    IntArray offsets;
    return lom_psb_kind(psb, list) == LOM_PSB_LIST && !int_array(psb, list + 1, &offsets) ? offsets.count : 0;
}

int lom_psb_item(const LomPsb* psb, size_t list, size_t index, size_t* item)
{
    IntArray offsets;

    if (lom_psb_kind(psb, list) != LOM_PSB_LIST || int_array(psb, list + 1, &offsets) || index >= offsets.count)
    {
        return -1;
    }

    *item = offsets.end + (size_t)array_at(psb, &offsets, index);
    return *item < psb->size ? 0 : -1;
}

int lom_psb_member(const LomPsb* psb, size_t object, const char* name, size_t* member)
{
    IntArray names, offsets;

    if (lom_psb_kind(psb, object) != LOM_PSB_OBJECT || int_array(psb, object + 1, &names) || int_array(psb, names.end, &offsets) ||
        names.count != offsets.count)
    {
        return -1;
    }

    for (size_t i = 0; i < names.count; ++i)
    {
        uint64_t n = array_at(psb, &names, i);

        if (n < psb->name_count && strcmp(psb->names[n], name) == 0)
        {
            *member = offsets.end + (size_t)array_at(psb, &offsets, i);
            return *member < psb->size ? 0 : -1;
        }
    }

    return -1;
}

size_t lom_psb_members(const LomPsb* psb, size_t object)
{
    IntArray names, offsets;

    if (lom_psb_kind(psb, object) != LOM_PSB_OBJECT || int_array(psb, object + 1, &names) || int_array(psb, names.end, &offsets) ||
        names.count != offsets.count)
    {
        return 0;
    }

    return names.count;
}

int lom_psb_member_at(const LomPsb* psb, size_t object, size_t index, const char** name, size_t* member)
{
    IntArray names, offsets;

    if (lom_psb_kind(psb, object) != LOM_PSB_OBJECT || int_array(psb, object + 1, &names) || int_array(psb, names.end, &offsets) ||
        names.count != offsets.count || index >= names.count)
    {
        return -1;
    }

    const uint64_t n = array_at(psb, &names, index);

    if (n >= psb->name_count)
    {
        return -1;
    }

    *name = psb->names[n];
    *member = offsets.end + (size_t)array_at(psb, &offsets, index);
    return *member < psb->size ? 0 : -1;
}

int lom_psb_path(const LomPsb* psb, size_t from, const char* path, size_t* value)
{
    char part[128];
    size_t at = from;

    while (*path)
    {
        size_t length = strcspn(path, "/");

        if (length >= sizeof(part))
        {
            return -1;
        }

        memcpy(part, path, length);
        part[length] = 0;
        path += length + (path[length] == '/');

        if (lom_psb_kind(psb, at) == LOM_PSB_LIST)
        {
            char* end;
            unsigned long index = strtoul(part, &end, 10);

            if (*end || lom_psb_item(psb, at, index, &at))
            {
                return -1;
            }
        }
        else if (lom_psb_member(psb, at, part, &at))
        {
            return -1;
        }
    }

    *value = at;
    return 0;
}
