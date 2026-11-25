#ifndef LOM_NATIVE_UNIT_DECODE_H
#define LOM_NATIVE_UNIT_DECODE_H

#include <stddef.h>

#define LOM_UNIT_STRINGIFY_(text) #text
#define LOM_UNIT_STRINGIFY(text) LOM_UNIT_STRINGIFY_(text)
#define LOM_UNIT_SYMBOL(name) LOM_UNIT_STRINGIFY(__USER_LABEL_PREFIX__) name
#ifdef __APPLE__
#define LOM_UNIT_WEAK(symbol) ".globl " symbol "\n\t.weak_definition " symbol
#else
#define LOM_UNIT_WEAK(symbol) ".weak " symbol
#endif

typedef struct LomUnitDecodeContext LomUnitDecodeContext;

typedef struct
{
    const char* name;
    unsigned int address;
    unsigned int size;
    void* host;
    size_t host_size;
    int plain;
    unsigned int ps1_stride;
    size_t host_stride;
    void (*decode)(const unsigned char* source, LomUnitDecodeContext* context);
    void (*encode)(unsigned char* destination);
} LomUnitGlobal;

typedef struct
{
    unsigned int address;
    const void* code;
    const char* name;
} LomUnitFunction;

typedef struct
{
    const char* name;
    unsigned int vram;
    unsigned int image_size;
    const LomUnitGlobal* globals;
    size_t global_count;
    const LomUnitFunction* functions;
    size_t function_count;
} LomUnit;

extern const LomUnit lom_unit_slus;

extern const LomUnit lom_unit_field;

extern const LomUnit lom_unit_wmap;

extern const LomUnit lom_unit_title;

extern const LomUnit lom_unit_gname;

extern const LomUnit lom_unit_menu;

extern const LomUnit lom_unit_shop;

extern const LomUnit lom_unit_zukan;

extern const LomUnit lom_unit_golem;

extern const LomUnit lom_unit_gover;

extern const LomUnit lom_unit_carda;

extern const LomUnit lom_unit_gosub;

extern const LomUnit lom_unit_wsel;

extern const LomUnit lom_unit_cload;

extern const LomUnit lom_unit_niki;

extern const LomUnit lom_unit_addhero;

/**
 * @brief Load an overlay's globals from its file, just read into its window,
 *        as the PS1 gets them by loading it (src/core/overlay_units.c).
 * @param resource The file's CD resource index. Files that aren't units are
 *        ignored.
 * @param destination Where the file was read.
 * @param size Its size in bytes.
 */
void lom_native_unit_streamed(int resource, void* destination, int size);

/**
 * @brief Decode a unit's image into its globals, and make it the loaded unit
 * at its addresses.
 * @param unit The unit's table, lom_unit_<name>.
 * @param image The unit's image as the PS1 loads it, at unit->vram.
 * @param window Where those bytes stay in host memory while the unit is
 * loaded. Pointers into image data no global covers (a TIM, say) point here.
 *
 * A unit loaded earlier at overlapping addresses stops being loaded. Anything
 * that can't be decoded stops the program, naming the global and the address.
 */
void lom_unit_load(const LomUnit* unit, const void* image, void* window);

/** @brief The host address of a PS1 address, in the units loaded now. NULL for 0. */
void* lom_unit_host_address(unsigned int address, const char* where);

/** @brief The host function at a PS1 function address, in the units loaded now. */
void* lom_unit_host_code(unsigned int address, const char* where);

/** @brief The PS1 address of a host address in a loaded unit, or 0 if it isn't in one. */
unsigned int lom_unit_ps1_address(const void* host);

/**
 * @brief Find how many bytes of a loaded unit's global are left from a host
 *        address.
 * @param host An address inside a global.
 * @return The bytes from @p host to the global's end, or 0 when no loaded
 *        global holds it.
 */
size_t lom_unit_host_bytes_left(const void* host);

/**
 * @brief Check whether a unit is loaded.
 * @param name Its name, like "field".
 * @return 1 if it is, otherwise 0.
 */
int lom_unit_is_loaded(const char* name);

/** @brief The PS1 address of a host pointer, for encoding. 0 for NULL; stops the program if it can't. */
unsigned int lom_unit_ps1_pointer(const void* host, const char* where);

/** @brief The PS1 address of a host function, for encoding. 0 for NULL. */
unsigned int lom_unit_ps1_code_address(const void* code, const char* where);

/**
 * @brief Read PS1 bytes as the PS1 would see them at an address, from the
 *        loaded units' globals.
 * @param address PS1 address.
 * @param out Receives size bytes; bytes no readable global holds are 0.
 * @param size Byte count.
 * @return 1 if every byte came from a global, 0 if some are zeros for want of
 *        one.
 *  A global whose layout differs natively is encoded back to PS1 bytes whole to
 * read it; pointers in it that can't be placed come out as 0. For the game's
 * reads past the end of an array, which on the PS1 land in whatever follows.
 */
int lom_unit_read_ps1(unsigned int address, void* out, unsigned int size);

/**
 * @brief Write PS1 bytes at an address into the loaded units' globals, as the
 *        PS1's write there would land.
 * @return 1 if every byte landed in a global, 0 if some had nowhere to go.
 *  A global whose layout differs natively is encoded, patched and decoded again
 * (leniently). For the game's writes past the end of an array.
 */
int lom_unit_write_ps1(unsigned int address, const void* in, unsigned int size);

/** @brief Run an element encoder, writing pointers it can't place as 0 instead of stopping. */
void lom_unit_encode_lenient(void (*encode)(const void*, unsigned char*), const void* source, unsigned char* destination);

/** @brief Run an element decoder from lom_unit_read_ps1's bytes, placing pointers it can't as NULL instead of stopping. */
void lom_unit_decode_lenient(void (*decode)(const unsigned char*, void*), const unsigned char* source, void* destination);

/**
 * @brief Check that a global the game builds while it runs is all zeros in the
 *        unit's file, and stop the program if it isn't, since zeroing it would
 *        lose data.
 * @param bytes The global's bytes in the file.
 * @param size How many.
 * @param name The global's name, for the error.
 */
void lom_unit_expect_zero(const unsigned char* bytes, unsigned int size, const char* name);

/**
 * @brief Turn a PS1 address in a unit's data into the host address of what it
 *        points at, for a generated decoder. Stops the program for one that
 *        can't be placed.
 * @param context The decoder's context.
 * @param address The PS1 address; 0 gives NULL.
 * @param where What's being decoded, for the error.
 * @return The host address.
 */
void* lom_unit_decode_pointer(LomUnitDecodeContext* context, unsigned int address, const char* where);

/**
 * @brief Turn a PS1 function address in a unit's data into the native function,
 *        for a generated decoder.
 * @param context The decoder's context.
 * @param address The PS1 address; 0 gives NULL.
 * @param where What's being decoded, for the error.
 * @return The function, or NULL when it isn't built natively.
 */
void* lom_unit_decode_code(LomUnitDecodeContext* context, unsigned int address, const char* where);

/**
 * @brief Read a little-endian 32-bit word from PS1 bytes.
 * @param bytes The word's first byte.
 * @return The word.
 */
static inline unsigned int lom_unit_read_u32(const unsigned char* bytes)
{
    return (unsigned int)bytes[0] | (unsigned int)bytes[1] << 8 | (unsigned int)bytes[2] << 16 | (unsigned int)bytes[3] << 24;
}

/**
 * @brief Write a little-endian 32-bit word into PS1 bytes.
 * @param bytes The word's first byte.
 * @param value The word.
 */
static inline void lom_unit_write_u32(unsigned char* bytes, unsigned int value)
{
    bytes[0] = (unsigned char)value;
    bytes[1] = (unsigned char)(value >> 8);
    bytes[2] = (unsigned char)(value >> 16);
    bytes[3] = (unsigned char)(value >> 24);
}

/**
 * @brief Read a bitfield from PS1 bytes, the way the PS1's compiler lays them
 *        out.
 * @param bytes The start of the record.
 * @param bit The field's first bit, counted from @p bytes.
 * @param width Its width in bits, up to 56.
 * @return The field's value.
 */
static inline unsigned long long lom_unit_read_bits(const unsigned char* bytes, unsigned int bit, unsigned int width)
{
    unsigned long long word = 0;
    for (unsigned int i = 0; i < 8; i++)
    {
        word |= (unsigned long long)bytes[bit / 8 + i] << (8 * i);
    }
    return (word >> (bit % 8)) & ((1ull << width) - 1);
}

/**
 * @brief Write a bitfield into PS1 bytes, the way the PS1's compiler lays them
 *        out, leaving the bits around it alone.
 * @param bytes The start of the record.
 * @param bit The field's first bit, counted from @p bytes.
 * @param width Its width in bits.
 * @param value The value; bits past @p width are dropped.
 */
static inline void lom_unit_write_bits(unsigned char* bytes, unsigned int bit, unsigned int width, unsigned long long value)
{
    value &= (1ull << width) - 1;
    for (unsigned int i = 0; i < width; i++)
    {
        unsigned int at = bit + i;
        bytes[at / 8] = (unsigned char)((bytes[at / 8] & ~(1u << (at % 8))) | (((value >> i) & 1u) << (at % 8)));
    }
}

/**
 * @brief Read a little-endian signed 32-bit word from PS1 bytes.
 * @param bytes The word's first byte.
 * @return The word.
 */
static inline int lom_unit_read_s32(const unsigned char* bytes)
{
    return (int)lom_unit_read_u32(bytes);
}

#endif
