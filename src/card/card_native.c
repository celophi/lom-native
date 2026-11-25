#include "lom_native_card.h"
#include "card_slot.h"
#include "lom_native_boundary.h"
#include "lom_native_fs.h"
#include "lom_native_portable.h"
#include "lom_native_psyz_sdk.h"
#include "lom_native_env.h"
#include "lom_native_mods.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

_Static_assert(sizeof(LomNativeCardEntry) == 40, "PS1 directory entry size");

_Static_assert(offsetof(LomNativeCardEntry, next) == 28, "PS1 directory next offset");
#define BLOCK_BYTES 8192
#define BLOCK_COUNT 15
#define HANDLE_COUNT 8
static char card_root[PATH_MAX];

static int configured;

static struct
{
    int active, fd, writable;
} handles[HANDLE_COUNT];

static struct
{
    int active, enabled, group, pending;
    long spec;
} events[8];

static LomNativeCardEntry listing[BLOCK_COUNT];

static unsigned listing_count, listing_cursor;

/**
 * @brief Report a card result to the open events of its group: each one is
 *        marked as happened when the result is the one it waits for, and
 *        cleared otherwise.
 * @param group 0 for the software events, 1 for the hardware ones.
 * @param spec The result: 4 (I/O done), 0x8000 (error), 0x100 (new card) or
 *        0x2000 (timeout).
 */
static void signal_event(int group, long spec)
{
    for (unsigned i = 0; i < 8; ++i)
    {
        if (events[i].active && events[i].group == group)
        {
            events[i].pending = events[i].spec == spec;
        }
    }
}

int lom_native_card_set_root(const char* path)
{
    for (unsigned i = 0; i < HANDLE_COUNT; ++i)
    {
        if (handles[i].active)
        {
            return -1;
        }
    }

    if (path && (!*path || strlen(path) >= sizeof(card_root)))
    {
        return -1;
    }

    configured = path != NULL;
    card_root[0] = 0;

    if (path)
    {
        strcpy(card_root, path);
    }

    listing_count = listing_cursor = 0;
    return 0;
}

/**
 * @brief Keep the cards in a folder of their own inside a card root, for mods
 *        that change gameplay (lom_native_mods_cards_name), and make sure its
 *        slot0/ exists.
 * @param base The card root the game would use without those mods.
 * @return 0, or -1 when the folder can't be made.
 */
static int use_mods_folder(const char* base)
{
    char root[sizeof(card_root)];
    char slot[PATH_MAX];
    int n = snprintf(root, sizeof(root), "%s/%s", base, lom_native_mods_cards_name());

    if (n < 0 || n >= (int)sizeof(root) || lom_native_make_folders(root))
    {
        return -1;
    }

    n = snprintf(slot, sizeof(slot), "%s/slot0", root);

    if (n < 0 || n >= (int)sizeof(slot) || (lom_native_make_folder(slot) && errno != EEXIST))
    {
        return -1;
    }

    return lom_native_card_set_root(root);
}

/**
 * @brief Find the card root the first time it's needed (lom_native_card_root)
 *        and make sure slot0/ exists. With mods that change gameplay on, the
 *        cards go in a folder of their own inside it, so those saves never mix
 *        with the normal game's.
 * @return 0, or -1 when there's nowhere to keep the cards.
 */
static int setup(void)
{
    if (configured)
    {
        return 0;
    }

    const char* explicit_root = lom_native_env_value(LOM_ENV_CARD_ROOT);

    if (explicit_root)
    {
        return lom_native_mods_cards_name() ? use_mods_folder(explicit_root) : lom_native_card_set_root(explicit_root);
    }

    if (lom_native_user_folder(LOM_NATIVE_USER_DATA, "cards", card_root, sizeof(card_root)) || lom_native_make_folders(card_root))
    {
        return -1;
    }

    if (lom_native_mods_cards_name())
    {
        char base[sizeof(card_root)];
        strcpy(base, card_root);
        return use_mods_folder(base);
    }

    char slot[PATH_MAX];
    int n = snprintf(slot, sizeof(slot), "%s/slot0", card_root);

    if (n < 0 || n >= (int)sizeof(slot) || (lom_native_make_folder(slot) && errno != EEXIST))
    {
        return -1;
    }

    configured = 1;
    return 0;
}

const char* lom_native_card_root(void)
{
    return setup() ? NULL : card_root;
}

/**
 * @brief Open a slot's folder, which is the card.
 * @param slot 0 or 1.
 * @return A handle to the folder (on Windows, just the slot again), or -1 with
 *        errno set: ENOENT means no card.
 */
static int open_slot(int slot)
{
    if (slot < 0 || slot > 1 || setup())
    {
        errno = EINVAL;
        return -1;
    }

    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/slot%d", card_root, slot);

    if (n < 0 || n >= (int)sizeof(path))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    return card_slot_open(path, slot);
}

/**
 * @brief Split a card path into its slot and file name, refusing names a card
 *        can't have.
 * @param path "bu00:NAME" or "bu10:NAME".
 * @param name Set to the name.
 * @return The slot, 0 or 1, or -1.
 */
static int split_path(const char* path, const char** name)
{
    if (!path || strncmp(path, "bu", 2) || (path[2] != '0' && path[2] != '1') || path[3] != '0' || path[4] != ':')
    {
        return -1;
    }

    *name = path + 5;
    size_t n = strlen(*name);

    if (!n || n > 20 || **name == '.')
    {
        return -1;
    }

    for (size_t i = 0; i < n; ++i)
    {
        if ((unsigned char)(*name)[i] < 0x20 || (*name)[i] == '/' || (*name)[i] == '\\' || (*name)[i] == ':' || (*name)[i] == '*' || (*name)[i] == '?')
        {
            return -1;
        }
    }

    return path[2] - '0';
}

/**
 * @brief Check that a card file's size is whole blocks, and no more than a
 *        card holds.
 */
static int card_file_size_ok(off_t size)
{
    return size > 0 && size % BLOCK_BYTES == 0 && size <= BLOCK_BYTES * BLOCK_COUNT;
}

/**
 * @brief Sort the listing by name, as a real card's directory comes back.
 */
static void sort_listing(void)
{
    for (unsigned i = 1; i < listing_count; ++i)
    {
        for (unsigned j = i; j && memcmp(listing[j - 1].name, listing[j].name, 20) > 0; --j)
        {
            LomNativeCardEntry tmp = listing[j];
            listing[j] = listing[j - 1];
            listing[j - 1] = tmp;
        }
    }
}

/**
 * @brief Give each listed file the sector its blocks would start at on a real
 *        card, one after another from the first block after the directory.
 */
static void place_listing(void)
{
    int sector = 64;

    for (unsigned i = 0; i < listing_count; ++i)
    {
        listing[i].head = sector;
        sector += listing[i].size / 128;
    }
}

/**
 * @brief List a card's files, sorted by name, with each one's size and where
 *        its blocks would start on a real card.
 * @param slot 0 or 1.
 * @return How many blocks are used, or -1 when the card can't be read.
 */
static int read_listing(int slot)
{
    listing_count = listing_cursor = 0;
    int fd = open_slot(slot);

    if (fd < 0)
    {
        return -1;
    }

    DIR* dir = card_slot_list(fd);

    if (!dir)
    {
        card_slot_close(fd);
        return -1;
    }

    struct dirent* ent;
    int used = 0, failed = 0;
    errno = 0;

    while ((ent = readdir(dir)))
    {
        if (ent->d_name[0] == '.')
        {
            continue;
        }

        struct stat st;
        size_t n = strlen(ent->d_name);

        if (!n || n > 20 || card_slot_stat(fd, ent->d_name, &st) || !S_ISREG(st.st_mode) || !card_file_size_ok(st.st_size) || listing_count >= BLOCK_COUNT)
        {
            failed = 1;
            break;
        }

        used += (int)(st.st_size / BLOCK_BYTES);

        if (used > BLOCK_COUNT)
        {
            failed = 1;
            break;
        }

        LomNativeCardEntry* e = &listing[listing_count++];
        memset(e, 0, sizeof(*e));
        memcpy(e->name, ent->d_name, n);
        e->size = (int32_t)st.st_size;
        e->attr = 0x51;
        errno = 0;
    }

    if (errno)
    {
        failed = 1;
    }

    closedir(dir);

    if (failed)
    {
        listing_count = 0;
        return -1;
    }

    sort_listing();
    place_listing();
    return used;
}

long lom_native_card_open_event(unsigned int desc, int spec, int mode, unsigned int callback)
{
    if ((uint32_t)desc != 0xF4000001u && (uint32_t)desc != 0xF0000011u)
    {
        return -1;
    }

    if (mode != 0x2000 || callback || (spec != 4 && spec != 0x8000 && spec != 0x100 && spec != 0x2000))
    {
        return -1;
    }

    for (unsigned i = 0; i < 8; ++i)
    {
        if (!events[i].active)
        {
            events[i].active = 1;
            events[i].enabled = events[i].pending = 0;
            events[i].group = (uint32_t)desc == 0xF0000011u;
            events[i].spec = spec;
            return (long)i + 1;
        }
    }

    return -1;
}

long lom_native_card_open_event_libapi(unsigned long desc, long spec, long mode, long (*callback)())
{
    if (callback)
    {
        return -1;
    }

    return lom_native_card_open_event((unsigned int)desc, (int)spec, (int)mode, 0);
}

long lom_native_card_close_event(int event)
{
    if (event < 1 || event > 8 || !events[event - 1].active)
    {
        return 0;
    }

    memset(&events[event - 1], 0, sizeof(events[0]));
    return 1;
}

long lom_native_card_enable_event(int event)
{
    if (event < 1 || event > 8 || !events[event - 1].active)
    {
        return 0;
    }

    events[event - 1].enabled = 1;
    return 1;
}

long lom_native_card_test_event(int event)
{
    if (event < 1 || event > 8 || !events[event - 1].active || !events[event - 1].enabled)
    {
        return 0;
    }

    int result = events[event - 1].pending;
    events[event - 1].pending = 0;
    return result;
}

/**
 * @brief Check a card slot and signal the result to an event group: I/O done
 *        for a card, new card for an empty slot, or error.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @param group The events to signal.
 * @return 1.
 */
static long card_request(int channel, int group)
{
    int fd = -1;
    errno = EINVAL;

    if (channel == 0 || channel == 16)
    {
        fd = open_slot((int)channel / 16);
    }

    long result = fd >= 0 ? 4 : (errno == ENOENT ? 0x100 : 0x8000);

    if (fd >= 0)
    {
        card_slot_close(fd);
    }

    signal_event(group, result);
    return 1;
}

/**
 * @brief Report a memory card call to lom_native_boundary.h.
 * @param call Which call.
 * @param channel The card's channel.
 * @param result What it returned.
 * @return @p result.
 */
static long report(LomBoundarySaveCall call, int channel, long result)
{
    lom_native_boundary_event(LOM_BOUNDARY_SAVES, call, (unsigned)channel, (unsigned)result);
    return result;
}

long lom_native_card_info(int channel)
{
    return report(LOM_BOUNDARY_SAVE_CARD_INFO, channel, card_request(channel, 0));
}

long lom_native_card_load(int channel)
{
    return report(LOM_BOUNDARY_SAVE_CARD_LOAD, channel, card_request(channel, 0));
}

long lom_native_card_clear(int channel)
{
    return report(LOM_BOUNDARY_SAVE_CARD_CLEAR, channel, card_request(channel, 1));
}

long lom_native_card_wait(int slot)
{
    return slot == 0 || slot == 1;
}

void* lom_native_card_first(const char* pattern, LomNativeCardEntry* entry)
{
    const char* name = NULL;
    int wildcard = pattern && (!strcmp(pattern, "bu00:*") || !strcmp(pattern, "bu10:*"));
    int slot = wildcard ? pattern[2] - '0' : split_path(pattern, &name);

    if (slot < 0 || !entry || read_listing(slot) < 0)
    {
        signal_event(0, 0x8000);
        return NULL;
    }

    if (!wildcard)
    {
        for (unsigned i = 0; i < listing_count; ++i)
        {
            if (strlen(name) == strnlen(listing[i].name, 20) && !memcmp(name, listing[i].name, strlen(name)))
            {
                *entry = listing[i];
                listing_cursor = listing_count;
                return entry;
            }
        }

        listing_cursor = listing_count;
        return NULL;
    }

    return lom_native_card_next(entry);
}

void* lom_native_card_next(LomNativeCardEntry* entry)
{
    if (!entry || listing_cursor >= listing_count)
    {
        return NULL;
    }

    *entry = listing[listing_cursor++];
    return entry;
}

/**
 * @brief Find a handle that isn't in use.
 * @return Its index, or -1 when they all are.
 */
static int free_handle(void)
{
    for (unsigned i = 0; i < HANDLE_COUNT; ++i)
    {
        if (!handles[i].active)
        {
            return (int)i;
        }
    }

    return -1;
}

/**
 * @brief Check that a new file of some blocks fits on a card, with what's on
 *        it already.
 * @param slot 0 or 1.
 * @param blocks The new file's size in blocks.
 * @return 1 when it does, otherwise 0.
 */
static int new_file_fits(int slot, unsigned blocks)
{
    if (!blocks || blocks > BLOCK_COUNT)
    {
        return 0;
    }

    int used = read_listing(slot);
    return used >= 0 && used + (int)blocks <= BLOCK_COUNT;
}

int lom_native_card_open(const char* path, int flags)
{
    const char* name;
    int slot = split_path(path, &name);

    if (slot < 0)
    {
        return -1;
    }

    int i = free_handle();

    if (i < 0)
    {
        return -1;
    }

    int create = (flags & 0x200) != 0, writable = (flags & 2) != 0 || create;
    unsigned blocks = (unsigned)flags >> 16;

    if (create && !new_file_fits(slot, blocks))
    {
        return -1;
    }

    int dir = open_slot(slot);

    if (dir < 0)
    {
        return -1;
    }

    int fd = card_slot_open_file(dir, name, (writable ? O_RDWR : O_RDONLY) | (create ? O_CREAT | O_EXCL : 0));

    if (create && fd >= 0 && ftruncate(fd, (off_t)blocks * BLOCK_BYTES))
    {
        close(fd);
        fd = -1;
        card_slot_unlink(dir, name);
    }

    struct stat st;

    if (fd >= 0 && (fstat(fd, &st) || !S_ISREG(st.st_mode) || !card_file_size_ok(st.st_size)))
    {
        close(fd);
        fd = -1;
    }

    card_slot_close(dir);

    if (fd < 0)
    {
        return -1;
    }

    handles[i].active = 1;
    handles[i].fd = fd;
    handles[i].writable = writable;
    return i + 1;
}

int lom_native_card_close(int handle)
{
    if (handle < 1 || handle > HANDLE_COUNT || !handles[handle - 1].active)
    {
        return -1;
    }

    int result = close(handles[handle - 1].fd);
    handles[handle - 1].active = 0;
    return result;
}

/**
 * @brief Read or write a card file from where the last transfer ended, and
 *        signal the software events with the result.
 * @param handle The file's handle.
 * @param data The bytes.
 * @param size How many; the file has to have that many left.
 * @param writing Nonzero to write and flush.
 * @return @p size, or -1.
 */
static int transfer(int handle, void* data, int size, int writing)
{
    if (handle < 1 || handle > HANDLE_COUNT || !handles[handle - 1].active || !data || size < 0 || (writing && !handles[handle - 1].writable))
    {
        signal_event(0, 0x8000);
        return -1;
    }

    int fd = handles[handle - 1].fd;
    struct stat st;
    off_t pos = lseek(fd, 0, SEEK_CUR);

    if (pos < 0 || fstat(fd, &st) || (off_t)size > st.st_size - pos)
    {
        signal_event(0, 0x8000);
        return -1;
    }

    int done = 0;

    while (done < size)
    {
        ssize_t n = writing ? write(fd, (char*)data + done, (size_t)(size - done)) : read(fd, (char*)data + done, (size_t)(size - done));

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n <= 0)
        {
            signal_event(0, 0x8000);
            return -1;
        }

        done += (int)n;
    }

    if (writing && card_file_sync(fd))
    {
        signal_event(0, 0x8000);
        return -1;
    }

    signal_event(0, 4);
    return size;
}

int lom_native_card_read(int handle, void* data, int size)
{
    return transfer(handle, data, size, 0);
}

int lom_native_card_write(int handle, const void* data, int size)
{
    return transfer(handle, (void*)data, size, 1);
}

/**
 * @brief Read or write one 128-byte sector of a card, in whichever file
 *        read_listing places there, and signal the hardware events with the
 *        result.
 * @param channel 0 for slot 1, 16 for slot 2.
 * @param sector 64 to 1023.
 * @param data The 128 bytes.
 * @param writing Nonzero to write.
 * @return 1, or 0.
 */
static long sector_transfer(int channel, int sector, void* data, int writing)
{
    int slot = channel == 0 ? 0 : channel == 16 ? 1 : -1;

    if (slot < 0 || !data || sector < 64 || sector >= 1024 || read_listing(slot) < 0)
    {
        signal_event(1, 0x8000);
        return 0;
    }

    for (unsigned i = 0; i < listing_count; ++i)
    {
        LomNativeCardEntry* e = &listing[i];

        if (sector < e->head || sector >= e->head + e->size / 128)
        {
            continue;
        }

        int dir = open_slot(slot);

        if (dir < 0)
        {
            break;
        }

        char name[21];
        memcpy(name, e->name, 20);
        name[20] = 0;
        int fd = card_slot_open_file(dir, name, writing ? O_RDWR : O_RDONLY);
        card_slot_close(dir);

        if (fd < 0)
        {
            break;
        }

        off_t offset = (sector - e->head) * 128;
        ssize_t n;

        do
        {
            n = card_file_transfer(fd, data, 128, offset, writing);
        } while (n < 0 && errno == EINTR);

        int ok = n == 128 && (!writing || !card_file_sync(fd));
        close(fd);
        signal_event(1, ok ? 4 : 0x8000);
        return ok;
    }

    signal_event(1, 0x8000);
    return 0;
}

long lom_native_card_read_sector(int channel, int sector, void* data)
{
    return report(LOM_BOUNDARY_SAVE_READ_SECTOR, channel, sector_transfer(channel, sector, data, 0));
}

long lom_native_card_write_sector(int channel, int sector, const void* data)
{
    return report(LOM_BOUNDARY_SAVE_WRITE_SECTOR, channel, sector_transfer(channel, sector, (void*)data, 1));
}

long lom_native_card_format(int channel)
{
    signal_event(0, 0x8000);
    return report(LOM_BOUNDARY_SAVE_FORMAT, channel, 0);
}

int lom_native_card_erase(const char* path)
{
    const char* name;
    int slot = split_path(path, &name);

    if (slot < 0)
    {
        return 0;
    }

    int dir = open_slot(slot);

    if (dir < 0)
    {
        return 0;
    }

    int result = card_slot_unlink(dir, name) == 0;

    if (result && card_slot_sync(dir))
    {
        result = 0;
    }

    card_slot_close(dir);
    return result;
}

int lom_native_card_rename(const char* from, const char* to)
{
    const char *src, *dst;
    int slot = split_path(from, &src);

    if (slot < 0 || split_path(to, &dst) != slot)
    {
        return 0;
    }

    int dir = open_slot(slot);

    if (dir < 0)
    {
        return 0;
    }

    int result = card_slot_rename(dir, src, dst) == 0;

    if (result && card_slot_sync(dir))
    {
        result = 0;
    }

    card_slot_close(dir);
    return result;
}

void lom_native_card_lock(void)
{
    PS1_EnterCriticalSection();
}

void lom_native_card_unlock(void)
{
    PS1_ExitCriticalSection();
}

void lom_native_card_new_card(void)
{
    _new_card();
}
