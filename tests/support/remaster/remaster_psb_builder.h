#ifndef LOM_REMASTER_PSB_BUILDER_H
#define LOM_REMASTER_PSB_BUILDER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define REMASTER_PSB_NODES 4096
#define REMASTER_PSB_NAMES 256
#define REMASTER_PSB_STRINGS 255
#define REMASTER_PSB_CHUNKS 255
#define REMASTER_PSB_TRIE 65536

enum
{
    REMASTER_PSB_NUMBER,
    REMASTER_PSB_TEXT,
    REMASTER_PSB_CHUNK,
    REMASTER_PSB_LIST,
    REMASTER_PSB_OBJECT
};

typedef struct RemasterPsbNode
{
    int kind;
    int64_t number;
    const char* text;
    const unsigned char* bytes;
    size_t size;
    const char* key;
    struct RemasterPsbNode* first;
    struct RemasterPsbNode* last;
    struct RemasterPsbNode* next;
} RemasterPsbNode;

typedef struct
{
    unsigned char* bytes;
    size_t size;
    size_t room;
} RemasterPsbBytes;

typedef struct
{
    RemasterPsbNode nodes[REMASTER_PSB_NODES];
    unsigned node_count;
    const char* names[REMASTER_PSB_NAMES];
    unsigned name_count;
    const char* strings[REMASTER_PSB_STRINGS];
    unsigned string_count;
    const RemasterPsbNode* chunks[REMASTER_PSB_CHUNKS];
    unsigned chunk_count;
} RemasterPsbBuilder;

typedef struct
{
    const char* label;
    int width;
    int height;
    uint32_t colour;
    int stripes;
    const uint32_t* stripe_colours;
    int bands;
    const uint32_t* band_colours;
} RemasterPicture;

/**
 * @brief Add bytes to the end of a buffer.
 * @param out The buffer.
 * @param bytes The bytes.
 * @param size How many.
 */
static inline void remaster_psb_put(RemasterPsbBytes* out, const void* bytes, size_t size)
{
    if (out->size + size > out->room)
    {
        out->room = (out->size + size) * 2 + 64;
        out->bytes = realloc(out->bytes, out->room);
    }

    memcpy(out->bytes + out->size, bytes, size);
    out->size += size;
}

/**
 * @brief Add one byte to the end of a buffer.
 * @param out The buffer.
 * @param byte The byte.
 */
static inline void remaster_psb_byte(RemasterPsbBytes* out, unsigned byte)
{
    const unsigned char b = (unsigned char)byte;
    remaster_psb_put(out, &b, 1);
}

/**
 * @brief Add a number, little-endian, in a number of bytes.
 * @param out The buffer.
 * @param value The number.
 * @param size How many bytes.
 */
static inline void remaster_psb_little(RemasterPsbBytes* out, uint64_t value, unsigned size)
{
    for (unsigned i = 0; i < size; ++i)
    {
        remaster_psb_byte(out, (unsigned)(value >> (8 * i)) & 0xFF);
    }
}

/**
 * @brief How many bytes a number needs, unsigned, at least one.
 * @param value The number.
 * @return The bytes.
 */
static inline unsigned remaster_psb_width(uint64_t value)
{
    unsigned size = 1;

    while (size < 8 && value >> (8 * size))
    {
        ++size;
    }

    return size;
}

/**
 * @brief Add a PSB array of numbers: its count's size and count, its items' size, then the items.
 * @param out The buffer.
 * @param values The numbers.
 * @param count How many.
 */
static inline void remaster_psb_array(RemasterPsbBytes* out, const uint64_t* values, size_t count)
{
    unsigned size = 1;

    for (size_t i = 0; i < count; ++i)
    {
        const unsigned width = remaster_psb_width(values[i]);
        size = width > size ? width : size;
    }

    const unsigned count_size = remaster_psb_width(count);
    remaster_psb_byte(out, 0x0C + count_size);
    remaster_psb_little(out, count, count_size);
    remaster_psb_byte(out, 0x0C + size);

    for (size_t i = 0; i < count; ++i)
    {
        remaster_psb_little(out, values[i], size);
    }
}

/**
 * @brief Start building a PSB.
 * @param builder The builder.
 */
static inline void remaster_psb_start(RemasterPsbBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
}

/**
 * @brief Make a node.
 * @param builder The builder.
 * @param kind Its kind.
 * @return The node.
 */
static inline RemasterPsbNode* remaster_psb_node(RemasterPsbBuilder* builder, int kind)
{
    RemasterPsbNode* node = &builder->nodes[builder->node_count++];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    return node;
}

/**
 * @brief Make an object, which members go in with remaster_psb_add.
 * @param builder The builder.
 * @return The object.
 */
static inline RemasterPsbNode* remaster_psb_object(RemasterPsbBuilder* builder)
{
    return remaster_psb_node(builder, REMASTER_PSB_OBJECT);
}

/**
 * @brief Make a list, which items go in with remaster_psb_add.
 * @param builder The builder.
 * @return The list.
 */
static inline RemasterPsbNode* remaster_psb_list(RemasterPsbBuilder* builder)
{
    return remaster_psb_node(builder, REMASTER_PSB_LIST);
}

/**
 * @brief Make a number.
 * @param builder The builder.
 * @param value Its value.
 * @return The number.
 */
static inline RemasterPsbNode* remaster_psb_number(RemasterPsbBuilder* builder, int64_t value)
{
    RemasterPsbNode* node = remaster_psb_node(builder, REMASTER_PSB_NUMBER);
    node->number = value;
    return node;
}

/**
 * @brief Make a string.
 * @param builder The builder.
 * @param text Its text, which has to stay valid until the PSB is built.
 * @return The string.
 */
static inline RemasterPsbNode* remaster_psb_text(RemasterPsbBuilder* builder, const char* text)
{
    RemasterPsbNode* node = remaster_psb_node(builder, REMASTER_PSB_TEXT);
    node->text = text;
    return node;
}

/**
 * @brief Make a chunk of bytes, like a picture's pixels.
 * @param builder The builder.
 * @param bytes The bytes, which have to stay valid until the PSB is built.
 * @param size How many.
 * @return The chunk.
 */
static inline RemasterPsbNode* remaster_psb_chunk(RemasterPsbBuilder* builder, const unsigned char* bytes, size_t size)
{
    RemasterPsbNode* node = remaster_psb_node(builder, REMASTER_PSB_CHUNK);
    node->bytes = bytes;
    node->size = size;
    return node;
}

/**
 * @brief Put a member in an object, or an item at the end of a list.
 * @param parent The object or list.
 * @param key The member's name, or NULL for a list's item.
 * @param child The member or item.
 * @return The child.
 */
static inline RemasterPsbNode* remaster_psb_add(RemasterPsbNode* parent, const char* key, RemasterPsbNode* child)
{
    child->key = key;

    if (parent->last)
    {
        parent->last->next = child;
    }
    else
    {
        parent->first = child;
    }

    parent->last = child;
    return child;
}

/**
 * @brief The number of a key's name, adding it the first time.
 * @param builder The builder.
 * @param name The name.
 * @return Its number.
 */
static inline unsigned remaster_psb_name(RemasterPsbBuilder* builder, const char* name)
{
    for (unsigned i = 0; i < builder->name_count; ++i)
    {
        if (strcmp(builder->names[i], name) == 0)
        {
            return i;
        }
    }

    builder->names[builder->name_count] = name;
    return builder->name_count++;
}

/**
 * @brief Number every key in a tree, depth first, in the order they're met.
 * @param builder The builder.
 * @param node The tree.
 */
static inline void remaster_psb_names(RemasterPsbBuilder* builder, const RemasterPsbNode* node)
{
    for (const RemasterPsbNode* child = node->first; child; child = child->next)
    {
        if (node->kind == REMASTER_PSB_OBJECT)
        {
            remaster_psb_name(builder, child->key);
        }

        remaster_psb_names(builder, child);
    }
}

static inline void remaster_psb_value(RemasterPsbBuilder* builder, const RemasterPsbNode* node, RemasterPsbBytes* out);

/**
 * @brief Write a list's or object's items, after their offsets.
 * @param builder The builder.
 * @param items The items, in order.
 * @param count How many.
 * @param out The buffer.
 */
static inline void remaster_psb_items(RemasterPsbBuilder* builder, const RemasterPsbNode* const* items, size_t count, RemasterPsbBytes* out)
{
    RemasterPsbBytes body = {0};
    uint64_t* offsets = calloc(count ? count : 1, sizeof(uint64_t));

    for (size_t i = 0; i < count; ++i)
    {
        offsets[i] = body.size;
        remaster_psb_value(builder, items[i], &body);
    }

    remaster_psb_array(out, offsets, count);
    remaster_psb_put(out, body.bytes, body.size);
    free(offsets);
    free(body.bytes);
}

/**
 * @brief Write a value.
 * @param builder The builder.
 * @param node The value.
 * @param out The buffer.
 */
static inline void remaster_psb_value(RemasterPsbBuilder* builder, const RemasterPsbNode* node, RemasterPsbBytes* out)
{
    const RemasterPsbNode* items[REMASTER_PSB_NODES];
    size_t count = 0;

    switch (node->kind)
    {
    case REMASTER_PSB_NUMBER:
    {
        const uint64_t magnitude = node->number < 0 ? (uint64_t)(-(node->number + 1)) + 1 : (uint64_t)node->number;
        unsigned bits = 0;

        while (bits < 64 && magnitude >> bits)
        {
            ++bits;
        }

        const unsigned size = (bits + 8) / 8 > 1 ? (bits + 8) / 8 : 1;
        remaster_psb_byte(out, 4 + size);
        remaster_psb_little(out, (uint64_t)node->number, size);
        break;
    }

    case REMASTER_PSB_TEXT:
    {
        unsigned index = 0;

        while (index < builder->string_count && strcmp(builder->strings[index], node->text) != 0)
        {
            ++index;
        }

        if (index == builder->string_count)
        {
            builder->strings[builder->string_count++] = node->text;
        }

        remaster_psb_byte(out, 0x15);
        remaster_psb_byte(out, index);
        break;
    }

    case REMASTER_PSB_CHUNK:
        builder->chunks[builder->chunk_count] = node;
        remaster_psb_byte(out, 0x19);
        remaster_psb_byte(out, builder->chunk_count++);
        break;

    case REMASTER_PSB_LIST:
        for (const RemasterPsbNode* child = node->first; child; child = child->next)
        {
            items[count++] = child;
        }

        remaster_psb_byte(out, 0x20);
        remaster_psb_items(builder, items, count, out);
        break;

    case REMASTER_PSB_OBJECT:
    {
        uint64_t names[REMASTER_PSB_NAMES];

        for (unsigned name = 0; name < builder->name_count; ++name)
        {
            for (const RemasterPsbNode* child = node->first; child; child = child->next)
            {
                if (strcmp(child->key, builder->names[name]) == 0)
                {
                    names[count] = name;
                    items[count++] = child;
                }
            }
        }

        remaster_psb_byte(out, 0x21);
        remaster_psb_array(out, names, count);
        remaster_psb_items(builder, items, count, out);
        break;
    }

    default:
        break;
    }
}

/**
 * @brief Write the key names as the PSB's trie: each node's base, each node's parent, and the node that ends each
 *        name. A node's id is its parent's base plus its character, a name's end being character 0.
 * @param builder The builder, with its names numbered.
 * @param out The buffer.
 */
static inline void remaster_psb_trie(const RemasterPsbBuilder* builder, RemasterPsbBytes* out)
{
    static int child[REMASTER_PSB_NODES][256];
    static int parent[REMASTER_PSB_NODES], id[REMASTER_PSB_NODES], base[REMASTER_PSB_NODES], order[REMASTER_PSB_NODES];
    static unsigned char used[REMASTER_PSB_TRIE];
    static uint64_t charset[REMASTER_PSB_TRIE], tree[REMASTER_PSB_TRIE];
    uint64_t ends[REMASTER_PSB_NAMES];
    int nodes = 1, end_nodes[REMASTER_PSB_NAMES];

    memset(child, 0, sizeof(child));
    memset(used, 0, sizeof(used));
    memset(base, 0, sizeof(base));

    for (unsigned n = 0; n < builder->name_count; ++n)
    {
        int node = 0;
        const unsigned char* name = (const unsigned char*)builder->names[n];

        for (size_t i = 0;; ++i)
        {
            const unsigned ch = name[i];

            if (!child[node][ch])
            {
                child[node][ch] = nodes;
                parent[nodes] = node;
                nodes++;
            }

            node = child[node][ch];

            if (!ch)
            {
                break;
            }
        }

        end_nodes[n] = node;
    }

    int head = 0, tail = 0, size = 1;
    order[tail++] = 0;
    id[0] = 0;
    used[0] = 1;

    while (head < tail)
    {
        const int node = order[head++];
        int b = 1, any = 0;

        for (unsigned ch = 0; ch < 256; ++ch)
        {
            any |= child[node][ch] != 0;
        }

        if (!any)
        {
            continue;
        }

        for (;; ++b)
        {
            int clash = 0;

            for (unsigned ch = 0; ch < 256 && !clash; ++ch)
            {
                clash = child[node][ch] && used[b + ch];
            }

            if (!clash)
            {
                break;
            }
        }

        base[node] = b;

        for (unsigned ch = 0; ch < 256; ++ch)
        {
            if (child[node][ch])
            {
                const int kid = child[node][ch];
                id[kid] = b + (int)ch;
                used[b + ch] = 1;
                size = b + (int)ch + 1 > size ? b + (int)ch + 1 : size;
                order[tail++] = kid;
            }
        }
    }

    memset(charset, 0, sizeof(uint64_t) * (size_t)size);
    memset(tree, 0, sizeof(uint64_t) * (size_t)size);

    for (int node = 0; node < nodes; ++node)
    {
        charset[id[node]] = (uint64_t)base[node];

        if (node)
        {
            tree[id[node]] = (uint64_t)id[parent[node]];
        }
    }

    for (unsigned n = 0; n < builder->name_count; ++n)
    {
        ends[n] = (uint64_t)id[end_nodes[n]];
    }

    remaster_psb_array(out, charset, (size_t)size);
    remaster_psb_array(out, tree, (size_t)size);
    remaster_psb_array(out, ends, builder->name_count);
}

/**
 * @brief Build a whole PSB from a tree of values.
 * @param builder The builder the tree was made in.
 * @param root The root object.
 * @param size Set to the PSB's size.
 * @return The PSB's bytes, which the caller frees.
 */
static inline unsigned char* remaster_psb_build(RemasterPsbBuilder* builder, const RemasterPsbNode* root, size_t* size)
{
    RemasterPsbBytes names = {0}, body = {0}, strings = {0}, string_offsets = {0}, chunk_offsets = {0}, chunk_lengths = {0}, out = {0};
    uint64_t offsets[REMASTER_PSB_STRINGS > REMASTER_PSB_CHUNKS ? REMASTER_PSB_STRINGS : REMASTER_PSB_CHUNKS];
    uint64_t lengths[REMASTER_PSB_CHUNKS];

    remaster_psb_names(builder, root);
    remaster_psb_value(builder, root, &body);
    remaster_psb_trie(builder, &names);

    for (unsigned i = 0; i < builder->string_count; ++i)
    {
        offsets[i] = strings.size;
        remaster_psb_put(&strings, builder->strings[i], strlen(builder->strings[i]) + 1);
    }

    remaster_psb_array(&string_offsets, offsets, builder->string_count);
    uint64_t at = 0;

    for (unsigned i = 0; i < builder->chunk_count; ++i)
    {
        offsets[i] = at;
        lengths[i] = builder->chunks[i]->size;
        at += builder->chunks[i]->size;
    }

    remaster_psb_array(&chunk_offsets, offsets, builder->chunk_count);
    remaster_psb_array(&chunk_lengths, lengths, builder->chunk_count);

    const uint32_t names_at = 40;
    const uint32_t strings_at = names_at + (uint32_t)names.size;
    const uint32_t strings_data_at = strings_at + (uint32_t)string_offsets.size;
    const uint32_t root_at = strings_data_at + (uint32_t)strings.size;
    const uint32_t chunk_offsets_at = root_at + (uint32_t)body.size;
    const uint32_t chunk_lengths_at = chunk_offsets_at + (uint32_t)chunk_offsets.size;
    const uint32_t chunk_data_at = chunk_lengths_at + (uint32_t)chunk_lengths.size;
    const uint32_t header[7] = {names_at, strings_at, strings_data_at, chunk_offsets_at, chunk_lengths_at, chunk_data_at, root_at};

    remaster_psb_put(&out, "PSB", 4);
    remaster_psb_little(&out, 2, 2);
    remaster_psb_little(&out, 0, 2);
    remaster_psb_little(&out, 40, 4);

    for (unsigned i = 0; i < 7; ++i)
    {
        remaster_psb_little(&out, header[i], 4);
    }

    remaster_psb_put(&out, names.bytes, names.size);
    remaster_psb_put(&out, string_offsets.bytes, string_offsets.size);
    remaster_psb_put(&out, strings.bytes, strings.size);
    remaster_psb_put(&out, body.bytes, body.size);
    remaster_psb_put(&out, chunk_offsets.bytes, chunk_offsets.size);
    remaster_psb_put(&out, chunk_lengths.bytes, chunk_lengths.size);

    for (unsigned i = 0; i < builder->chunk_count; ++i)
    {
        remaster_psb_put(&out, builder->chunks[i]->bytes, builder->chunks[i]->size);
    }

    free(names.bytes);
    free(body.bytes);
    free(strings.bytes);
    free(string_offsets.bytes);
    free(chunk_offsets.bytes);
    free(chunk_lengths.bytes);
    *size = out.size;
    return out.bytes;
}

/**
 * @brief Build one of the Remaster's picture files, as its screens' art comes: a list of pictures, each one RGBA8
 *        texture filled with one colour, or with stripes across it or bands down it of a colour each, so a test can
 *        tell from a pixel which picture, or which frame or row of one, was drawn.
 * @param pictures The pictures: each one's label, size and colour, its four bytes from the top byte down, as
 *        they're stored (the Remaster stores blue first), or how many stripes of equal width or bands of equal height
 *        and their colours.
 * @param count How many.
 * @param size Set to the file's size.
 * @return The file's bytes, which the caller frees.
 */
static inline unsigned char* remaster_psb_picture_file(const RemasterPicture* pictures, int count, size_t* size)
{
    RemasterPsbBuilder* builder = malloc(sizeof(*builder));
    unsigned char* pixels[REMASTER_PSB_CHUNKS];
    remaster_psb_start(builder);
    RemasterPsbNode* root = remaster_psb_object(builder);
    RemasterPsbNode* list = remaster_psb_add(root, "imageList", remaster_psb_list(builder));

    for (int i = 0; i < count; ++i)
    {
        const RemasterPicture* picture = &pictures[i];
        const size_t bytes = (size_t)picture->width * (size_t)picture->height * 4;
        pixels[i] = malloc(bytes);

        for (size_t p = 0; p < bytes; p += 4)
        {
            const int column = (int)(p / 4 % (size_t)picture->width), row = (int)(p / 4 / (size_t)picture->width);
            const uint32_t colour = picture->stripes > 0 ? picture->stripe_colours[column * picture->stripes / picture->width]
                                    : picture->bands > 0 ? picture->band_colours[row * picture->bands / picture->height]
                                                         : picture->colour;
            pixels[i][p] = (unsigned char)(colour >> 24);
            pixels[i][p + 1] = (unsigned char)(colour >> 16);
            pixels[i][p + 2] = (unsigned char)(colour >> 8);
            pixels[i][p + 3] = (unsigned char)colour;
        }

        RemasterPsbNode* item = remaster_psb_add(list, NULL, remaster_psb_object(builder));
        remaster_psb_add(item, "label", remaster_psb_text(builder, picture->label));
        remaster_psb_add(item, "width", remaster_psb_number(builder, picture->width));
        remaster_psb_add(item, "height", remaster_psb_number(builder, picture->height));
        RemasterPsbNode* texture = remaster_psb_add(remaster_psb_add(item, "texture", remaster_psb_list(builder)), NULL, remaster_psb_object(builder));
        remaster_psb_add(texture, "left", remaster_psb_number(builder, 0));
        remaster_psb_add(texture, "top", remaster_psb_number(builder, 0));
        remaster_psb_add(texture, "width", remaster_psb_number(builder, picture->width));
        remaster_psb_add(texture, "height", remaster_psb_number(builder, picture->height));
        RemasterPsbNode* image = remaster_psb_add(texture, "image", remaster_psb_object(builder));
        remaster_psb_add(image, "type", remaster_psb_text(builder, "RGBA8"));
        remaster_psb_add(image, "width", remaster_psb_number(builder, picture->width));
        remaster_psb_add(image, "height", remaster_psb_number(builder, picture->height));
        remaster_psb_add(image, "pixel", remaster_psb_chunk(builder, pixels[i], bytes));
    }

    unsigned char* file = remaster_psb_build(builder, root, size);

    for (int i = 0; i < count; ++i)
    {
        free(pixels[i]);
    }

    free(builder);
    return file;
}

/**
 * @brief Build one of the Remaster's fonts with some letters in it, each a solid block as tall as the font's line and
 *        as wide as the letter goes along, sitting on the baseline, so a line of them is easy to measure.
 * @param letters The letters, in UTF-8, one after another, each ended by a 0, the list by another.
 * @param advance How far along each letter goes, in the font's pixels.
 * @param size Set to the file's size.
 * @return The file's bytes, which the caller frees.
 */
static inline unsigned char* remaster_psb_font_file(const char* letters, int advance, size_t* size)
{
    enum
    {
        LINE = 52,
        BASELINE = 42
    };

    RemasterPsbBuilder* builder = malloc(sizeof(*builder));
    int count = 0;

    for (const char* letter = letters; *letter; letter += strlen(letter) + 1)
    {
        ++count;
    }

    const int width = count * advance;
    unsigned char* atlas = malloc((size_t)width * LINE);
    memset(atlas, 255, (size_t)width * LINE);
    remaster_psb_start(builder);
    RemasterPsbNode* root = remaster_psb_object(builder);
    RemasterPsbNode* code = remaster_psb_add(root, "code", remaster_psb_object(builder));
    RemasterPsbNode* source = remaster_psb_add(root, "source", remaster_psb_list(builder));
    RemasterPsbNode* image = remaster_psb_add(source, NULL, remaster_psb_object(builder));
    remaster_psb_add(image, "type", remaster_psb_text(builder, "A8"));
    remaster_psb_add(image, "width", remaster_psb_number(builder, width));
    remaster_psb_add(image, "height", remaster_psb_number(builder, LINE));
    remaster_psb_add(image, "pixel", remaster_psb_chunk(builder, atlas, (size_t)width * LINE));
    int x = 0;

    for (const char* letter = letters; *letter; letter += strlen(letter) + 1)
    {
        RemasterPsbNode* entry = remaster_psb_add(code, letter, remaster_psb_object(builder));
        remaster_psb_add(entry, "id", remaster_psb_number(builder, 0));
        remaster_psb_add(entry, "x", remaster_psb_number(builder, x));
        remaster_psb_add(entry, "y", remaster_psb_number(builder, 0));
        remaster_psb_add(entry, "w", remaster_psb_number(builder, advance));
        remaster_psb_add(entry, "h", remaster_psb_number(builder, LINE));
        remaster_psb_add(entry, "width", remaster_psb_number(builder, advance));
        remaster_psb_add(entry, "b", remaster_psb_number(builder, BASELINE));
        x += advance;
    }

    unsigned char* file = remaster_psb_build(builder, root, size);
    free(atlas);
    free(builder);
    return file;
}

#endif
