#include "remaster_internal.h"

#include <stdint.h>
#include <string.h>

enum
{
    MAX_BITS = 15,
    FAST_BITS = 10,
    LITERAL_CODES = 288,
    DISTANCE_CODES = 30
};

typedef struct
{
    const unsigned char* in;
    size_t in_size, in_pos;
    uint64_t bit_buffer;
    unsigned bit_count;
    unsigned char* out;
    size_t out_size, out_pos;
    int failed;
} Inflater;

typedef struct
{
    unsigned short counts[MAX_BITS + 1];
    unsigned short symbols[LITERAL_CODES];
    unsigned short fast[1 << FAST_BITS];
} Huffman;

/**
 * @brief Fill the bit buffer from the stream, a byte at a time, as far as it
 *        holds whole bytes.
 * @param s The inflater.
 */
static void refill(Inflater* s)
{
    while (s->bit_count <= 56 && s->in_pos < s->in_size)
    {
        s->bit_buffer |= (uint64_t)s->in[s->in_pos++] << s->bit_count;
        s->bit_count += 8;
    }
}

/**
 * @brief Read bits from the stream, lowest first.
 * @param s The inflater. It fails when the stream runs out.
 * @param need How many bits, up to 16.
 * @return The bits.
 */
static unsigned bits(Inflater* s, unsigned need)
{
    if (s->bit_count < need)
    {
        refill(s);

        if (s->bit_count < need)
        {
            s->failed = 1;
            return 0;
        }
    }

    unsigned value = (unsigned)(s->bit_buffer & ((1u << need) - 1u));
    s->bit_buffer >>= need;
    s->bit_count -= need;
    return value;
}

/**
 * @brief Reverse the order of a code's bits, since deflate sends Huffman
 *        codes highest bit first inside a stream read lowest bit first.
 * @param code The code.
 * @param length How many bits it has.
 * @return The code with its bits reversed.
 */
static unsigned reverse(unsigned code, int length)
{
    unsigned reversed = 0;

    for (int i = 0; i < length; ++i)
    {
        reversed = (reversed << 1) | ((code >> i) & 1u);
    }

    return reversed;
}

/**
 * @brief Build a canonical Huffman code from each symbol's code length, with
 *        a table that decodes the codes of up to FAST_BITS bits in one look.
 * @param h Set to the code.
 * @param lengths Each symbol's code length, 0 for a symbol that isn't used.
 * @param count How many symbols.
 * @return 0.
 */
static int build(Huffman* h, const unsigned char* lengths, int count)
{
    unsigned short offsets[MAX_BITS + 2];
    memset(h->counts, 0, sizeof(h->counts));

    for (int i = 0; i < count; ++i)
    {
        h->counts[lengths[i]]++;
    }

    h->counts[0] = 0;
    offsets[1] = 0;

    for (int len = 1; len <= MAX_BITS; ++len)
    {
        offsets[len + 1] = (unsigned short)(offsets[len] + h->counts[len]);
    }

    for (int i = 0; i < count; ++i)
    {
        if (lengths[i])
        {
            h->symbols[offsets[lengths[i]]++] = (unsigned short)i;
        }
    }

    memset(h->fast, 0, sizeof(h->fast));
    unsigned code = 0;
    int index = 0;

    for (int len = 1; len <= MAX_BITS; ++len)
    {
        for (int n = 0; n < h->counts[len]; ++n, ++code)
        {
            const unsigned short symbol = h->symbols[index++];

            if (len <= FAST_BITS)
            {
                for (unsigned slot = reverse(code, len); slot < (1u << FAST_BITS); slot += 1u << len)
                {
                    h->fast[slot] = (unsigned short)(symbol << 4 | (unsigned)len);
                }
            }
        }

        code <<= 1;
    }

    return 0;
}

/**
 * @brief Read one symbol of a Huffman code: from the table when its code is
 *        short, otherwise a bit at a time.
 * @param s The inflater.
 * @param h The code.
 * @return The symbol, or -1 when the stream runs out or has a code that isn't
 *        in it.
 */
static int decode(Inflater* s, const Huffman* h)
{
    if (s->bit_count < FAST_BITS)
    {
        refill(s);
    }

    const unsigned entry = h->fast[s->bit_buffer & ((1u << FAST_BITS) - 1u)];

    if (entry && (entry & 15u) <= s->bit_count)
    {
        s->bit_buffer >>= entry & 15u;
        s->bit_count -= entry & 15u;
        return (int)(entry >> 4);
    }

    int code = 0, first = 0, index = 0;

    for (int len = 1; len <= MAX_BITS; ++len)
    {
        code |= (int)bits(s, 1);

        if (s->failed)
        {
            return -1;
        }

        int count = h->counts[len];

        if (code - count < first)
        {
            return h->symbols[index + (code - first)];
        }

        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }

    s->failed = 1;
    return -1;
}

static const unsigned short length_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                               31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char length_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short distance_base[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                                 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char distance_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/**
 * @brief Write one byte of the output.
 * @param s The inflater. It fails when the output is full.
 * @param byte The byte.
 * @return 0, or -1 when there's no room.
 */
static int put(Inflater* s, unsigned char byte)
{
    if (s->out_pos >= s->out_size)
    {
        s->failed = 1;
        return -1;
    }

    s->out[s->out_pos++] = byte;
    return 0;
}

/**
 * @brief Inflate a compressed block's literals and copies, up to its end code.
 *        A copy that overlaps itself is done in chunks that double, since what
 *        it has written already repeats the pattern.
 * @param s The inflater.
 * @param literals The literal and length code.
 * @param distances The distance code.
 * @return 0, or -1 when the block is bad or doesn't fit.
 */
static int codes(Inflater* s, const Huffman* literals, const Huffman* distances)
{
    for (;;)
    {
        int symbol = decode(s, literals);

        if (symbol < 0)
        {
            return -1;
        }

        if (symbol < 256)
        {
            if (put(s, (unsigned char)symbol))
            {
                return -1;
            }

            continue;
        }

        if (symbol == 256)
        {
            return 0;
        }

        symbol -= 257;

        if (symbol >= 29)
        {
            return -1;
        }

        size_t length = length_base[symbol] + bits(s, length_extra[symbol]);
        int dsym = decode(s, distances);

        if (dsym < 0 || dsym >= 30)
        {
            return -1;
        }

        size_t distance = distance_base[dsym] + bits(s, distance_extra[dsym]);

        if (s->failed || distance > s->out_pos || length > s->out_size - s->out_pos)
        {
            return -1;
        }

        unsigned char* to = s->out + s->out_pos;
        const unsigned char* from = to - distance;

        for (size_t left = length; left;)
        {
            size_t chunk = (size_t)(to - from);
            chunk = chunk < left ? chunk : left;
            memcpy(to, from, chunk);
            to += chunk;
            left -= chunk;
        }

        s->out_pos += length;
    }
}

/**
 * @brief Inflate a block compressed with deflate's fixed codes.
 * @param s The inflater.
 * @return 0, or -1 when the block is bad or doesn't fit.
 */
static int fixed_block(Inflater* s)
{
    unsigned char lengths[LITERAL_CODES + DISTANCE_CODES];
    Huffman literals, distances;
    int i = 0;

    for (; i < 144; ++i)
    {
        lengths[i] = 8;
    }
    for (; i < 256; ++i)
    {
        lengths[i] = 9;
    }
    for (; i < 280; ++i)
    {
        lengths[i] = 7;
    }
    for (; i < LITERAL_CODES; ++i)
    {
        lengths[i] = 8;
    }

    build(&literals, lengths, LITERAL_CODES);
    memset(lengths, 5, DISTANCE_CODES);
    build(&distances, lengths, DISTANCE_CODES);
    return codes(s, &literals, &distances);
}

/**
 * @brief Inflate a block that brings its own codes, after reading them.
 * @param s The inflater.
 * @return 0, or -1 when the block is bad or doesn't fit.
 */
static int dynamic_block(Inflater* s)
{
    static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    unsigned char lengths[LITERAL_CODES + DISTANCE_CODES];
    Huffman lencode, literals, distances;
    unsigned nlen = bits(s, 5) + 257, ndist = bits(s, 5) + 1, ncode = bits(s, 4) + 4;

    if (s->failed || nlen > LITERAL_CODES || ndist > DISTANCE_CODES)
    {
        return -1;
    }

    memset(lengths, 0, 19);

    for (unsigned i = 0; i < ncode; ++i)
    {
        lengths[order[i]] = (unsigned char)bits(s, 3);
    }

    build(&lencode, lengths, 19);
    unsigned index = 0;

    while (index < nlen + ndist)
    {
        int symbol = decode(s, &lencode);

        if (symbol < 0)
        {
            return -1;
        }

        if (symbol < 16)
        {
            lengths[index++] = (unsigned char)symbol;
            continue;
        }

        unsigned char value = 0;
        unsigned repeat;

        if (symbol == 16)
        {
            if (index == 0)
            {
                return -1;
            }

            value = lengths[index - 1];
            repeat = 3 + bits(s, 2);
        }
        else if (symbol == 17)
        {
            repeat = 3 + bits(s, 3);
        }
        else
        {
            repeat = 11 + bits(s, 7);
        }

        if (index + repeat > nlen + ndist)
        {
            return -1;
        }

        while (repeat--)
        {
            lengths[index++] = value;
        }
    }

    build(&literals, lengths, (int)nlen);
    build(&distances, lengths + nlen, (int)ndist);
    return codes(s, &literals, &distances);
}

/**
 * @brief Copy a block that isn't compressed, from the next whole byte. The
 *        whole bytes the bit buffer read ahead are given back first.
 * @param s The inflater.
 * @return 0, or -1 when the block is bad or doesn't fit.
 */
static int stored_block(Inflater* s)
{
    s->in_pos -= (s->bit_count / 8);
    s->bit_buffer = 0;
    s->bit_count = 0;

    if (s->in_pos + 4 > s->in_size)
    {
        return -1;
    }

    unsigned len = s->in[s->in_pos] | (unsigned)s->in[s->in_pos + 1] << 8;
    unsigned nlen = s->in[s->in_pos + 2] | (unsigned)s->in[s->in_pos + 3] << 8;
    s->in_pos += 4;

    if ((len ^ 0xFFFFu) != nlen || s->in_pos + len > s->in_size || len > s->out_size - s->out_pos)
    {
        return -1;
    }

    memcpy(s->out + s->out_pos, s->in + s->in_pos, len);
    s->in_pos += len;
    s->out_pos += len;
    return 0;
}

int lom_remaster_inflate(const unsigned char* in, size_t in_size, unsigned char* out, size_t out_size, size_t* written)
{
    Inflater s = {in, in_size, 0, 0, 0, out, out_size, 0, 0};

    if (in_size < 2 || (in[0] & 0x0F) != 8 || ((in[0] << 8) | in[1]) % 31 != 0 || (in[1] & 0x20))
    {
        return -1;
    }

    s.in_pos = 2;
    int last;

    do
    {
        last = (int)bits(&s, 1);
        unsigned type = bits(&s, 2);

        if (s.failed)
        {
            return -1;
        }

        int result = type == 0 ? stored_block(&s) : type == 1 ? fixed_block(&s) : type == 2 ? dynamic_block(&s) : -1;

        if (result || s.failed)
        {
            return -1;
        }
    } while (!last);

    *written = s.out_pos;
    return 0;
}
