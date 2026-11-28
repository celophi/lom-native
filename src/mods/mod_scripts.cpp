#include "mod_scripts.h"
#include "mod_files_provider.h"
#include "mod_manifest.h"
#include "lom_native_mod_disc.h"
#include "lom_native_mod_game.h"
#include "lom_native_mod_seed.h"
#include "lom_native_mods.h"
#include "lom_native_notices.h"
#include "lom_native_random.h"
#include "lom_native_switches.h"

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lom::mods
{

namespace
{

constexpr std::size_t memory_limit = 32 * 1024 * 1024;

constexpr int instructions_per_check = 1000;

constexpr long checks_per_call = 10000;

constexpr long checks_per_game_start = 1000000;

constexpr int random_bits = 15;

constexpr int random_mask = (1 << random_bits) - 1;

constexpr char scripts_folder[] = "scripts";

constexpr char hooks_key[] = "lom-native hooks";

constexpr char modules_key[] = "lom-native modules";

constexpr const char* hook_names[LOM_MOD_HOOK_COUNT] = {"scene_enter", "battle_start", "battle_end", "frame", "game_start", "file_load", "artifact_award"};

constexpr char bytes_type[] = "lom.bytes";

constexpr char random_type[] = "lom.random";

constexpr std::uint64_t pcg_multiplier = 6364136223846793005ULL;

constexpr std::uint64_t pcg_stream = 0x6C6F6D2D6E617469ULL;

constexpr std::size_t max_disc_path = 255;

constexpr const char* removed_globals[] = {"dofile", "loadfile", "load"};

constexpr int party_slots = 3;

/**
 * @brief Find the ModScripts a Lua state belongs to, kept in its extra space.
 * @param state The state, or one of its coroutines.
 * @return The ModScripts.
 */
ModScripts* owner(lua_State* state)
{
    return *static_cast<ModScripts**>(lua_getextraspace(state));
}

/**
 * @brief Read a whole file.
 * @param path The file.
 * @param text Set to its bytes.
 * @return True when it could be read.
 */
bool read_file(const std::filesystem::path& path, std::string& text)
{
    return lom::mods::mod_files_provider().read(path, text);
}

/**
 * @brief Turn a module name for require into a path in the mod's scripts
 *        folder: letters, digits and underscores, with dots between folders.
 * @param name The module name.
 * @param path Set to "scripts/a/b.lua" for "a.b".
 * @return True when the name is allowed.
 */
bool module_path(const char* name, std::string& path)
{
    const std::size_t length = std::strlen(name);

    if (length == 0 || name[0] == '.' || name[length - 1] == '.' || std::strstr(name, "..") != nullptr)
    {
        return false;
    }

    path = scripts_folder;
    path += '/';

    for (const char* c = name; *c; ++c)
    {
        if (*c == '.')
        {
            path += '/';
        }
        else if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_')
        {
            path += *c;
        }
        else
        {
            return false;
        }
    }

    path += ".lua";
    return true;
}

/**
 * @brief Draw 30 bits from the game's random number generator, two draws of 15.
 * @return The number, from 0 to 2^30 - 1.
 */
lua_Integer game_random_bits()
{
    const lua_Integer high = lom_native_rand() & random_mask;
    const lua_Integer low = lom_native_rand() & random_mask;
    return (high << random_bits) | low;
}

struct Bytes
{
    std::size_t size;

    unsigned char data[1];
};

struct Generator
{
    std::uint64_t state;

    std::uint64_t increment;
};

/**
 * @brief Draw PCG32's next number.
 * @param generator The generator.
 * @return 32 random bits.
 */
std::uint32_t next(Generator& generator)
{
    const std::uint64_t old = generator.state;
    generator.state = old * pcg_multiplier + generator.increment;
    const std::uint32_t shifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
    const std::uint32_t rotation = static_cast<std::uint32_t>(old >> 59u);
    return (shifted >> rotation) | (shifted << ((32u - rotation) & 31u));
}

/**
 * @brief Seed a generator the way PCG32's pcg32_srandom does, on lom-native's
 *        own stream.
 * @param generator The generator.
 * @param seed The seed.
 */
void seed_generator(Generator& generator, std::uint64_t seed)
{
    generator.state = 0;
    generator.increment = (pcg_stream << 1u) | 1u;
    next(generator);
    generator.state += seed;
    next(generator);
}

/**
 * @brief Draw a whole number from 0 to span - 1, every one as likely.
 * @param generator The generator.
 * @param span How many numbers, from 1 to 2^32.
 * @return The number.
 */
std::uint64_t below(Generator& generator, std::uint64_t span)
{
    if (span == (std::uint64_t{1} << 32))
    {
        return next(generator);
    }

    const std::uint32_t bound = static_cast<std::uint32_t>(span);
    const std::uint32_t threshold = static_cast<std::uint32_t>(-bound) % bound;

    for (;;)
    {
        const std::uint32_t drawn = next(generator);

        if (drawn >= threshold)
        {
            return drawn % bound;
        }
    }
}

/**
 * @brief Check a path on the disc a script gave: letters, digits, _, -, . and
 *        forward slashes between parts, and no part that's empty, "." or "..".
 * @param path The path.
 * @param empty_allowed Whether "" (the whole disc) is allowed.
 * @return True when it's allowed.
 */
bool disc_path_allowed(const char* path, bool empty_allowed)
{
    const std::size_t length = std::strlen(path);

    if (length == 0)
    {
        return empty_allowed;
    }

    if (length > max_disc_path || path[0] == '/' || path[length - 1] == '/')
    {
        return false;
    }

    const char* part = path;

    for (const char* c = path;; ++c)
    {
        if (*c == '/' || *c == 0)
        {
            const std::size_t part_length = static_cast<std::size_t>(c - part);

            if (part_length == 0 || (part_length == 1 && part[0] == '.') || (part_length == 2 && part[0] == '.' && part[1] == '.'))
            {
                return false;
            }

            if (*c == 0)
            {
                return true;
            }

            part = c + 1;
        }
        else if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-' || *c == '.'))
        {
            return false;
        }
    }
}

/**
 * @brief Collect a disc listing's paths, for lom_native_mod_disc_list.
 * @param path A file's path.
 * @param context The std::vector<std::string> to add it to.
 */
void collect_path(const char* path, void* context)
{
    static_cast<std::vector<std::string>*>(context)->push_back(path);
}

/**
 * @brief Push bytes held in C memory as a Lua string, under lua_pcall, so
 *        running out of the mod's memory can't skip freeing them.
 * @param state The state, with a light userdata to the bytes and their size.
 * @return 1: the string.
 */
int push_block(lua_State* state)
{
    const unsigned char* data = static_cast<const unsigned char*>(lua_touserdata(state, 1));
    const lua_Integer size = lua_tointeger(state, 2);
    lua_pushlstring(state, reinterpret_cast<const char*>(data), static_cast<std::size_t>(size));
    return 1;
}

/**
 * @brief Push a listing's paths as a Lua list, under lua_pcall, so running out
 *        of the mod's memory can't skip the vector's destructor.
 * @param state The state, with a light userdata to the std::vector<std::string>.
 * @return 1: the list.
 */
int push_paths(lua_State* state)
{
    const std::vector<std::string>* paths = static_cast<const std::vector<std::string>*>(lua_touserdata(state, 1));
    lua_createtable(state, static_cast<int>(paths->size()), 0);

    for (std::size_t i = 0; i < paths->size(); ++i)
    {
        lua_pushlstring(state, (*paths)[i].data(), (*paths)[i].size());
        lua_rawseti(state, -2, static_cast<lua_Integer>(i) + 1);
    }

    return 1;
}

/**
 * @brief Push C memory as a Lua string and free it, raising the push's error
 *        afterwards if it failed.
 * @param state The state.
 * @param data The bytes, from malloc.
 * @param size How many.
 */
void push_and_free(lua_State* state, unsigned char* data, std::size_t size)
{
    lua_pushcfunction(state, push_block);
    lua_pushlightuserdata(state, data);
    lua_pushinteger(state, static_cast<lua_Integer>(size));
    const int status = lua_pcall(state, 2, 1, 0);
    std::free(data);

    if (status != LUA_OK)
    {
        lua_error(state);
    }
}

/**
 * @brief Run a file_load hook with the file as a string: pushes the path and
 *        the bytes inside the protected call, so running out of memory stops
 *        the mod instead of the game.
 * @param state The state: the hook function, a light userdata to the path,
 *        one to the bytes, and their size.
 * @return 1: what the hook returned.
 */
int call_file_hook(lua_State* state)
{
    const char* path = static_cast<const char*>(lua_touserdata(state, 2));
    const char* data = static_cast<const char*>(lua_touserdata(state, 3));
    const lua_Integer size = lua_tointeger(state, 4);
    lua_settop(state, 1);
    lua_pushstring(state, path);
    lua_pushlstring(state, data, static_cast<std::size_t>(size));
    lua_call(state, 2, 1);
    return 1;
}
}

struct ScriptApi
{
    /**
     * @brief Lua's message handler: add a traceback to an error.
     * @param state The state.
     * @return 1: the message with its traceback.
     */
    static int traceback(lua_State* state)
    {
        const char* message = lua_tostring(state, 1);
        luaL_traceback(state, state, message ? message : luaL_tolstring(state, 1, nullptr), 1);
        return 1;
    }

    /**
     * @brief The count hook: charge the call's instruction budget, and stop a
     *        script that runs past it.
     * @param state The state.
     * @param debug Unused.
     */
    static void count(lua_State* state, lua_Debug* debug)
    {
        (void)debug;

        if (--owner(state)->budget_ < 0)
        {
            luaL_error(state, "it ran for more than %d million instructions at once", static_cast<int>(checks_per_call * instructions_per_check / 1000000));
        }
    }

    /**
     * @brief lom.log and print: write the arguments to the game's log, as text
     *        with spaces between, after the mod's id.
     * @param state The state.
     * @return 0.
     */
    static int log(lua_State* state)
    {
        const int count = lua_gettop(state);
        luaL_Buffer line;
        luaL_buffinit(state, &line);

        for (int i = 1; i <= count; ++i)
        {
            if (i > 1)
            {
                luaL_addchar(&line, ' ');
            }

            luaL_tolstring(state, i, nullptr);
            luaL_addvalue(&line);
        }

        luaL_pushresult(&line);
        std::fprintf(stderr, "[lom-native] mod %s: %s\n", owner(state)->mod_id_.c_str(), lua_tostring(state, -1));
        return 0;
    }

    /**
     * @brief lom.on(hook, function): run the function at a hook.
     * @param state The state.
     * @return 0.
     */
    static int on(lua_State* state)
    {
        const char* name = luaL_checkstring(state, 1);
        luaL_checktype(state, 2, LUA_TFUNCTION);
        int hook = 0;

        while (hook < LOM_MOD_HOOK_COUNT && std::strcmp(hook_names[hook], name) != 0)
        {
            ++hook;
        }

        if (hook == LOM_MOD_HOOK_COUNT)
        {
            return luaL_error(state, "there's no hook called %s", name);
        }

        lua_getfield(state, LUA_REGISTRYINDEX, hooks_key);
        lua_rawgeti(state, -1, hook + 1);
        lua_pushvalue(state, 2);
        lua_rawseti(state, -2, static_cast<lua_Integer>(lua_rawlen(state, -2)) + 1);
        lua_pop(state, 2);
        return 0;
    }

    /**
     * @brief lom.switch(name): whether a switch is on.
     * @param state The state.
     * @return 1: true or false.
     */
    static int switch_on(lua_State* state)
    {
        const char* name = luaL_checkstring(state, 1);
        const int id = lom_native_switch_find(name);

        if (id < 0)
        {
            return luaL_error(state, "there's no switch called %s", name);
        }

        lua_pushboolean(state, lom_native_switch(static_cast<LomSwitch>(id)));
        return 1;
    }

    /**
     * @brief Stop a game-state call made outside a hook, when FIELD's state
     *        isn't there to read.
     * @param state The state.
     * @param name The call, for the error.
     */
    static void need_hook(lua_State* state, const char* name)
    {
        if (!owner(state)->in_hook_)
        {
            luaL_error(state, "lom.%s only works inside a hook, while FIELD is running", name);
        }
    }

    /**
     * @brief lom.money(): the party's lucre.
     * @param state The state.
     * @return 1: the lucre.
     */
    static int money(lua_State* state)
    {
        need_hook(state, "money");
        lua_pushinteger(state, field_mod_money());
        return 1;
    }

    /**
     * @brief lom.add_money(amount): give the party lucre, up to the game's
     *        maximum.
     * @param state The state.
     * @return 1: whether any was added.
     */
    static int add_money(lua_State* state)
    {
        need_hook(state, "add_money");
        const lua_Integer amount = luaL_checkinteger(state, 1);
        lua_pushboolean(state, amount > 0 && amount <= INT32_MAX && field_mod_add_money(static_cast<int>(amount)));
        return 1;
    }

    /**
     * @brief lom.spend_money(amount): take lucre, when the party has more than
     *        the amount, as the game's shops do.
     * @param state The state.
     * @return 1: whether it was spent.
     */
    static int spend_money(lua_State* state)
    {
        need_hook(state, "spend_money");
        const lua_Integer amount = luaL_checkinteger(state, 1);
        lua_pushboolean(state, amount > 0 && amount <= INT32_MAX && field_mod_spend_money(static_cast<int>(amount)));
        return 1;
    }

    /**
     * @brief lom.party(): the party members present, each a table with slot
     *        (0 to 2), hp and max_hp.
     * @param state The state.
     * @return 1: the list.
     */
    static int party(lua_State* state)
    {
        need_hook(state, "party");
        lua_newtable(state);
        lua_Integer count = 0;

        for (int slot = 0; slot < party_slots; ++slot)
        {
            int hp = 0;
            int maximum_hp = 0;

            if (!field_mod_party_member(slot, &hp, &maximum_hp))
            {
                continue;
            }

            lua_createtable(state, 0, 3);
            lua_pushinteger(state, slot);
            lua_setfield(state, -2, "slot");
            lua_pushinteger(state, hp);
            lua_setfield(state, -2, "hp");
            lua_pushinteger(state, maximum_hp);
            lua_setfield(state, -2, "max_hp");
            lua_rawseti(state, -2, ++count);
        }

        return 1;
    }

    /**
     * @brief lom.heal(slot, amount): restore a party member's hit points, up to
     *        their maximum. Someone at 0 stays down.
     * @param state The state.
     * @return 1: whether they were healed.
     */
    static int heal(lua_State* state)
    {
        need_hook(state, "heal");
        const lua_Integer slot = luaL_checkinteger(state, 1);
        const lua_Integer amount = luaL_checkinteger(state, 2);
        const bool healed =
            slot >= 0 && slot < party_slots && amount > 0 && amount <= INT32_MAX && field_mod_heal(static_cast<int>(slot), static_cast<int>(amount));
        lua_pushboolean(state, healed);
        return 1;
    }

    /**
     * @brief lom.scene(): the scene FIELD last entered, or nil before the first.
     * @param state The state.
     * @return 1: the scene's number, or nil.
     */
    static int scene(lua_State* state)
    {
        const int scene = owner(state)->scene_;

        if (scene < 0)
        {
            lua_pushnil(state);
        }
        else
        {
            lua_pushinteger(state, scene);
        }

        return 1;
    }

    /**
     * @brief math.random, from the game's random number generator: no
     *        arguments for a number from 0 up to 1, (n) for 1 to n, (m, n) for
     *        m to n.
     * @param state The state.
     * @return 1: the number.
     */
    static int random(lua_State* state)
    {
        const lua_Integer bits = game_random_bits();
        lua_Integer low = 1;
        lua_Integer high;

        switch (lua_gettop(state))
        {
        case 0:
            lua_pushnumber(state, static_cast<lua_Number>(bits) / static_cast<lua_Number>(lua_Integer{1} << (2 * random_bits)));
            return 1;
        case 1:
            high = luaL_checkinteger(state, 1);
            break;
        case 2:
            low = luaL_checkinteger(state, 1);
            high = luaL_checkinteger(state, 2);
            break;
        default:
            return luaL_error(state, "math.random takes no more than two numbers");
        }

        luaL_argcheck(state, low <= high, lua_gettop(state), "the range is empty");
        const lua_Unsigned span = static_cast<lua_Unsigned>(high) - static_cast<lua_Unsigned>(low) + 1;
        const lua_Unsigned offset = span == 0 ? static_cast<lua_Unsigned>(bits) : static_cast<lua_Unsigned>(bits) % span;
        lua_pushinteger(state, static_cast<lua_Integer>(static_cast<lua_Unsigned>(low) + offset));
        return 1;
    }

    /**
     * @brief math.randomseed, which mods can't use: their random numbers are
     *        the game's, so a recording replays them the same.
     * @param state The state.
     * @return Never returns.
     */
    static int random_seed(lua_State* state)
    {
        return luaL_error(state, "math.randomseed isn't available; mods use the game's random numbers");
    }

    /**
     * @brief Load a module from the mod's scripts folder as text, without
     *        running it. All its C++ work ends here, so a Lua error raised after
     *        it can't skip a destructor.
     * @param state The state.
     * @param name The module's name.
     * @return True with the loaded chunk on the stack, false with the error.
     */
    static bool load_module(lua_State* state, const char* name)
    {
        std::string path;
        std::string text;

        if (!module_path(name, path))
        {
            lua_pushfstring(state, "require can only load the mod's own scripts, by names like \"helpers\" or \"ui.panel\", not %s", name);
            return false;
        }

        if (!read_file(owner(state)->folder_ / path, text))
        {
            lua_pushfstring(state, "the mod has no %s for require \"%s\"", path.c_str(), name);
            return false;
        }

        const std::string chunk_name = "@" + path;
        return luaL_loadbufferx(state, text.data(), text.size(), chunk_name.c_str(), "t") == LUA_OK;
    }

    /**
     * @brief require(name): run a module from the mod's own scripts folder,
     *        once, as text, and return what it returns.
     * @param state The state.
     * @return 1: the module's value, or true when it returns nothing.
     */
    static int require(lua_State* state)
    {
        const char* name = luaL_checkstring(state, 1);
        lua_getfield(state, LUA_REGISTRYINDEX, modules_key);

        if (lua_getfield(state, -1, name) != LUA_TNIL)
        {
            return 1;
        }

        lua_pop(state, 1);

        if (!load_module(state, name))
        {
            return lua_error(state);
        }

        lua_call(state, 0, 1);

        if (lua_isnil(state, -1))
        {
            lua_pop(state, 1);
            lua_pushboolean(state, 1);
        }

        lua_pushvalue(state, -1);
        lua_setfield(state, -3, name);
        return 1;
    }

    /**
     * @brief lom.game(): the running game's disc serial.
     * @param state The state.
     * @return 1: "SLUS-01013" or "SLPS-02170".
     */
    static int game(lua_State* state)
    {
        lua_pushstring(state, owner(state)->game_.c_str());
        return 1;
    }

    /**
     * @brief lom.seed(): the seed of the game being played, for a mod whose
     *        mod.json asks for one.
     * @param state The state.
     * @return 1: the seed, or nil before a game starts or for a mod that
     *         doesn't ask for one.
     */
    static int seed(lua_State* state)
    {
        std::uint32_t seed = 0;

        if (owner(state)->seeded_ && lom_native_mod_seed_current(&seed))
        {
            lua_pushinteger(state, static_cast<lua_Integer>(seed));
        }
        else
        {
            lua_pushnil(state);
        }

        return 1;
    }

    /**
     * @brief lom.disc_file(path): a file from the player's disc, as it is on
     *        the disc.
     * @param state The state.
     * @return 1: its bytes as a string, or nil when there's no such file.
     */
    static int disc_file(lua_State* state)
    {
        const char* path = luaL_checkstring(state, 1);

        if (!disc_path_allowed(path, false))
        {
            return luaL_error(state, "lom.disc_file needs a path on the disc like \"ANA/MAPINFO/BON_1F00.IMG\", not %s", path);
        }

        std::size_t size = 0;
        unsigned char* data = lom_native_mod_disc_read(path, &size);

        if (!data)
        {
            lua_pushnil(state);
            return 1;
        }

        push_and_free(state, data, size);
        return 1;
    }

    /**
     * @brief lom.disc_files(folder): the paths of the files in a folder on the
     *        disc and the folders under it, sorted.
     * @param state The state.
     * @return 1: the list, or nil when there's no such folder.
     */
    static int disc_files(lua_State* state)
    {
        const char* folder = luaL_optstring(state, 1, "");

        if (!disc_path_allowed(folder, true))
        {
            return luaL_error(state, "lom.disc_files needs a folder on the disc like \"ANA/MAPINFO\", not %s", folder);
        }

        int status;
        {
            std::vector<std::string> paths;

            if (lom_native_mod_disc_list(folder, collect_path, &paths) != 0)
            {
                lua_pushnil(state);
                return 1;
            }

            std::sort(paths.begin(), paths.end());
            lua_pushcfunction(state, push_paths);
            lua_pushlightuserdata(state, &paths);
            status = lua_pcall(state, 1, 1, 0);
        }

        if (status != LUA_OK)
        {
            return lua_error(state);
        }

        return 1;
    }

    /**
     * @brief lom.unpack(bytes): unpack a file the game keeps compressed on the
     *        disc, like the overlays in BIN, the way the game does.
     * @param state The state.
     * @return 1: the unpacked bytes as a string.
     */
    static int unpack(lua_State* state)
    {
        std::size_t size = 0;
        const char* packed = luaL_checklstring(state, 1, &size);
        std::size_t unpacked_size = 0;
        unsigned char* unpacked = lom_native_mod_disc_unpack(reinterpret_cast<const unsigned char*>(packed), size, &unpacked_size);

        if (!unpacked)
        {
            return luaL_error(state, "lom.unpack got bytes that aren't a compressed file from the disc");
        }

        push_and_free(state, unpacked, unpacked_size);
        return 1;
    }

    /**
     * @brief lom.bytes(text): a buffer of bytes that can be changed in place,
     *        holding a copy of the text.
     * @param state The state.
     * @return 1: the buffer.
     */
    static int bytes(lua_State* state)
    {
        std::size_t size = 0;
        const char* text = luaL_checklstring(state, 1, &size);
        Bytes* buffer = static_cast<Bytes*>(lua_newuserdatauv(state, offsetof(Bytes, data) + (size ? size : 1), 0));
        buffer->size = size;
        std::memcpy(buffer->data, text, size);
        luaL_setmetatable(state, bytes_type);
        return 1;
    }

    /**
     * @brief A lom.bytes method to read or write a number: buffer:u16(offset)
     *        reads, buffer:u16(offset, value) writes. Little-endian, like the
     *        PS1; offsets from 0. Its width and whether it's signed are the
     *        closure's upvalues.
     * @param state The state.
     * @return 1 when reading: the number; 0 when writing.
     */
    static int bytes_number(lua_State* state)
    {
        Bytes* buffer = static_cast<Bytes*>(luaL_checkudata(state, 1, bytes_type));
        const int width = static_cast<int>(lua_tointeger(state, lua_upvalueindex(1)));
        const bool is_signed = lua_toboolean(state, lua_upvalueindex(2));
        const lua_Integer offset = luaL_checkinteger(state, 2);

        if (offset < 0 || static_cast<lua_Unsigned>(offset) + static_cast<lua_Unsigned>(width) > buffer->size)
        {
            return luaL_error(state, "offset %d is outside the %d bytes (offsets start at 0)", static_cast<int>(offset), static_cast<int>(buffer->size));
        }

        unsigned char* at = buffer->data + offset;

        if (lua_gettop(state) < 3)
        {
            std::uint32_t number = 0;

            for (int i = width - 1; i >= 0; --i)
            {
                number = (number << 8) | at[i];
            }

            lua_Integer result = number;

            if (is_signed && (number >> (width * 8 - 1)) & 1u)
            {
                result -= lua_Integer{1} << (width * 8);
            }

            lua_pushinteger(state, result);
            return 1;
        }

        const lua_Integer value = luaL_checkinteger(state, 3);
        const lua_Integer lowest = -(lua_Integer{1} << (width * 8 - 1));
        const lua_Integer highest = (lua_Integer{1} << (width * 8)) - 1;

        if (value < lowest || value > highest)
        {
            return luaL_error(state, "%d doesn't fit in %d byte%s", static_cast<int>(value), width, width == 1 ? "" : "s");
        }

        std::uint32_t number = static_cast<std::uint32_t>(value);

        for (int i = 0; i < width; ++i)
        {
            at[i] = static_cast<unsigned char>(number & 0xFFu);
            number >>= 8;
        }

        return 0;
    }

    /**
     * @brief buffer:string(): the buffer's bytes as a string.
     * @param state The state.
     * @return 1: the string.
     */
    static int bytes_string(lua_State* state)
    {
        const Bytes* buffer = static_cast<const Bytes*>(luaL_checkudata(state, 1, bytes_type));
        lua_pushlstring(state, reinterpret_cast<const char*>(buffer->data), buffer->size);
        return 1;
    }

    /**
     * @brief #buffer: how many bytes it has.
     * @param state The state.
     * @return 1: the length.
     */
    static int bytes_length(lua_State* state)
    {
        const Bytes* buffer = static_cast<const Bytes*>(luaL_checkudata(state, 1, bytes_type));
        lua_pushinteger(state, static_cast<lua_Integer>(buffer->size));
        return 1;
    }

    /**
     * @brief lom.random(seed): a random number generator of its own, PCG32
     *        seeded with the number, separate from the game's.
     * @param state The state.
     * @return 1: the generator.
     */
    static int random_generator(lua_State* state)
    {
        const lua_Integer seed = luaL_checkinteger(state, 1);
        Generator* generator = static_cast<Generator*>(lua_newuserdatauv(state, sizeof(Generator), 0));
        seed_generator(*generator, static_cast<std::uint64_t>(seed));
        luaL_setmetatable(state, random_type);
        return 1;
    }

    /**
     * @brief Draw a whole number from low to high for a generator method,
     *        raising an error when the range is empty or wider than 2^32.
     * @param state The state.
     * @param generator The generator.
     * @param low The lowest.
     * @param high The highest.
     * @return The number.
     */
    static lua_Integer draw(lua_State* state, Generator& generator, lua_Integer low, lua_Integer high)
    {
        if (low > high)
        {
            luaL_error(state, "the range %d to %d is empty", static_cast<int>(low), static_cast<int>(high));
        }

        const std::uint64_t span = static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low) + 1u;

        if (span == 0 || span > (std::uint64_t{1} << 32))
        {
            luaL_error(state, "a range can hold at most 2^32 numbers");
        }

        return static_cast<lua_Integer>(static_cast<std::uint64_t>(low) + below(generator, span));
    }

    /**
     * @brief generator:int(n) for 1 to n, or generator:int(m, n) for m to n.
     * @param state The state.
     * @return 1: the number.
     */
    static int random_int(lua_State* state)
    {
        Generator* generator = static_cast<Generator*>(luaL_checkudata(state, 1, random_type));
        const bool both = lua_gettop(state) >= 3;
        const lua_Integer low = both ? luaL_checkinteger(state, 2) : 1;
        const lua_Integer high = luaL_checkinteger(state, both ? 3 : 2);
        lua_pushinteger(state, draw(state, *generator, low, high));
        return 1;
    }

    /**
     * @brief generator:float(): a number from 0 up to 1, not including 1.
     * @param state The state.
     * @return 1: the number.
     */
    static int random_float(lua_State* state)
    {
        Generator* generator = static_cast<Generator*>(luaL_checkudata(state, 1, random_type));
        const std::uint64_t high = next(*generator) >> 5u;
        const std::uint64_t low = next(*generator) >> 6u;
        lua_pushnumber(state, static_cast<lua_Number>(high * 67108864u + low) / 9007199254740992.0);
        return 1;
    }

    /**
     * @brief generator:shuffle(list): shuffle a list in place, Fisher-Yates
     *        from the end.
     * @param state The state.
     * @return 1: the list.
     */
    static int random_shuffle(lua_State* state)
    {
        Generator* generator = static_cast<Generator*>(luaL_checkudata(state, 1, random_type));
        luaL_checktype(state, 2, LUA_TTABLE);
        const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, 2));

        for (lua_Integer i = count; i > 1; --i)
        {
            const lua_Integer j = draw(state, *generator, 1, i);
            lua_rawgeti(state, 2, i);
            lua_rawgeti(state, 2, j);
            lua_rawseti(state, 2, i);
            lua_rawseti(state, 2, j);
        }

        lua_settop(state, 2);
        return 1;
    }

    /**
     * @brief generator:pick(list): one of a list's items, every one as likely.
     * @param state The state.
     * @return 1: the item.
     */
    static int random_pick(lua_State* state)
    {
        Generator* generator = static_cast<Generator*>(luaL_checkudata(state, 1, random_type));
        luaL_checktype(state, 2, LUA_TTABLE);
        const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state, 2));

        if (count == 0)
        {
            return luaL_error(state, "generator:pick got an empty list");
        }

        lua_rawgeti(state, 2, draw(state, *generator, 1, count));
        return 1;
    }
};

namespace
{

const luaL_Reg api_functions[] = {
    {"on", ScriptApi::on},
    {"log", ScriptApi::log},
    {"switch", ScriptApi::switch_on},
    {"money", ScriptApi::money},
    {"add_money", ScriptApi::add_money},
    {"spend_money", ScriptApi::spend_money},
    {"party", ScriptApi::party},
    {"heal", ScriptApi::heal},
    {"scene", ScriptApi::scene},
    {"game", ScriptApi::game},
    {"seed", ScriptApi::seed},
    {"disc_file", ScriptApi::disc_file},
    {"disc_files", ScriptApi::disc_files},
    {"unpack", ScriptApi::unpack},
    {"bytes", ScriptApi::bytes},
    {"random", ScriptApi::random_generator},
    {nullptr, nullptr},
};

const luaL_Reg random_methods[] = {
    {"int", ScriptApi::random_int},
    {"float", ScriptApi::random_float},
    {"shuffle", ScriptApi::random_shuffle},
    {"pick", ScriptApi::random_pick},
    {nullptr, nullptr},
};

struct BytesNumber
{
    const char* name;

    int width;

    bool is_signed;
};

const BytesNumber bytes_numbers[] = {
    {"u8", 1, false}, {"u16", 2, false}, {"u32", 4, false}, {"s8", 1, true}, {"s16", 2, true}, {"s32", 4, true},
};
}

std::vector<std::string> ModScripts::api_surface()
{
    std::vector<std::string> names{"lom.api"};

    for (const luaL_Reg& function : api_functions)
    {
        if (function.name)
        {
            names.push_back(std::string("lom.") + function.name);
        }
    }

    for (const char* hook : hook_names)
    {
        names.push_back(std::string("hook ") + hook);
    }

    return names;
}

ModScripts::ModScripts(std::string mod_id, std::filesystem::path folder) : mod_id_(std::move(mod_id)), folder_(std::move(folder))
{
}

ModScripts::~ModScripts()
{
    if (state_)
    {
        lua_close(state_);
    }
}

void* ModScripts::allocate(void* user, void* block, std::size_t old_size, std::size_t new_size)
{
    ModScripts* self = static_cast<ModScripts*>(user);
    const std::size_t was = block ? old_size : 0;

    if (new_size == 0)
    {
        std::free(block);
        self->memory_used_ -= was;
        return nullptr;
    }

    if (new_size > was && self->memory_used_ - was + new_size > memory_limit)
    {
        return nullptr;
    }

    void* grown = std::realloc(block, new_size);

    if (grown)
    {
        self->memory_used_ = self->memory_used_ - was + new_size;
    }

    return grown;
}

bool ModScripts::open_sandbox()
{
    static const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base},       {LUA_STRLIBNAME, luaopen_string}, {LUA_TABLIBNAME, luaopen_table},
        {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8},  {LUA_COLIBNAME, luaopen_coroutine},
    };

    for (const luaL_Reg& library : libraries)
    {
        luaL_requiref(state_, library.name, library.func, 1);
        lua_pop(state_, 1);
    }

    for (const char* name : removed_globals)
    {
        lua_pushnil(state_);
        lua_setglobal(state_, name);
    }

    lua_getglobal(state_, LUA_STRLIBNAME);
    lua_pushnil(state_);
    lua_setfield(state_, -2, "dump");
    lua_pop(state_, 1);

    lua_getglobal(state_, LUA_MATHLIBNAME);
    lua_pushcfunction(state_, ScriptApi::random);
    lua_setfield(state_, -2, "random");
    lua_pushcfunction(state_, ScriptApi::random_seed);
    lua_setfield(state_, -2, "randomseed");
    lua_pop(state_, 1);

    lua_pushcfunction(state_, ScriptApi::log);
    lua_setglobal(state_, "print");
    lua_pushcfunction(state_, ScriptApi::require);
    lua_setglobal(state_, "require");

    lua_createtable(state_, LOM_MOD_HOOK_COUNT, 0);

    for (int hook = 0; hook < LOM_MOD_HOOK_COUNT; ++hook)
    {
        lua_newtable(state_);
        lua_rawseti(state_, -2, hook + 1);
    }

    lua_setfield(state_, LUA_REGISTRYINDEX, hooks_key);
    lua_newtable(state_);
    lua_setfield(state_, LUA_REGISTRYINDEX, modules_key);

    luaL_newmetatable(state_, bytes_type);
    lua_newtable(state_);

    for (const BytesNumber& number : bytes_numbers)
    {
        lua_pushinteger(state_, number.width);
        lua_pushboolean(state_, number.is_signed);
        lua_pushcclosure(state_, ScriptApi::bytes_number, 2);
        lua_setfield(state_, -2, number.name);
    }

    lua_pushcfunction(state_, ScriptApi::bytes_string);
    lua_setfield(state_, -2, "string");
    lua_setfield(state_, -2, "__index");
    lua_pushcfunction(state_, ScriptApi::bytes_length);
    lua_setfield(state_, -2, "__len");
    lua_pop(state_, 1);

    luaL_newmetatable(state_, random_type);
    luaL_newlib(state_, random_methods);
    lua_setfield(state_, -2, "__index");
    lua_pop(state_, 1);

    luaL_newlib(state_, api_functions);
    lua_pushinteger(state_, mod_api);
    lua_setfield(state_, -2, "api");
    lua_setglobal(state_, "lom");
    lua_sethook(state_, ScriptApi::count, LUA_MASKCOUNT, instructions_per_check);
    return true;
}

bool ModScripts::call(int arguments, int results, std::string& error, long budget)
{
    const int base = lua_gettop(state_) - arguments;
    lua_pushcfunction(state_, ScriptApi::traceback);
    lua_insert(state_, base);
    budget_ = budget;
    const int status = lua_pcall(state_, arguments, results, base);
    lua_remove(state_, base);

    if (status == LUA_OK)
    {
        return true;
    }

    if (status == LUA_ERRMEM)
    {
        error = "it used more than its " + std::to_string(memory_limit / (1024 * 1024)) + " MB of memory";
    }
    else
    {
        const char* message = lua_tostring(state_, -1);
        error = message ? message : "it raised an error that isn't text";
    }

    lua_pop(state_, 1);
    return false;
}

bool ModScripts::run_script(const std::string& path, std::string& error)
{
    std::string text;

    if (!read_file(folder_ / path, text))
    {
        error = "its script " + path + " can't be read";
        return false;
    }

    const std::string chunk_name = "@" + path;

    if (luaL_loadbufferx(state_, text.data(), text.size(), chunk_name.c_str(), "t") != LUA_OK)
    {
        const char* message = lua_tostring(state_, -1);
        error = message ? message : "its script " + path + " doesn't load";
        lua_pop(state_, 1);
        return false;
    }

    return call(0, 0, error, checks_per_call);
}

void ModScripts::stop(const std::string& error)
{
    stopped_ = true;
    std::fprintf(stderr, "[lom-native] mods: %s stopped: %s\n", mod_id_.c_str(), error.c_str());
    lom_native_notice_post(LOM_NOTICE_MOD_STOPPED, mod_id_.c_str(), error.c_str());
}

std::unique_ptr<ModScripts> ModScripts::start(const std::string& mod_id, const std::filesystem::path& folder, const std::vector<std::string>& scripts,
                                              const std::string& game, bool seeded, std::string& error)
{
    std::unique_ptr<ModScripts> host(new ModScripts(mod_id, folder));
    host->game_ = game;
    host->seeded_ = seeded;
    host->state_ = lua_newstate(allocate, host.get());

    if (!host->state_)
    {
        error = "Lua couldn't start";
        return nullptr;
    }

    *static_cast<ModScripts**>(lua_getextraspace(host->state_)) = host.get();

    if (!host->open_sandbox())
    {
        error = "its sandbox couldn't be set up";
        return nullptr;
    }

    for (const std::string& script : scripts)
    {
        if (!host->run_script(script, error))
        {
            return nullptr;
        }
    }

    return host;
}

void ModScripts::each(LomModHook hook, bool game_state, long budget, int results, const std::function<int()>& push,
                      const std::function<bool(std::string&)>& take)
{
    if (stopped_)
    {
        return;
    }

    in_hook_ = game_state;
    lua_getfield(state_, LUA_REGISTRYINDEX, hooks_key);
    lua_rawgeti(state_, -1, hook + 1);
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state_, -1));

    for (lua_Integer i = 1; i <= count && !stopped_; ++i)
    {
        lua_rawgeti(state_, -1, i);
        const int arguments = push();
        std::string error;

        if (!call(arguments, results, error, budget))
        {
            stop(std::string(hook_names[hook]) + ": " + error);
            break;
        }

        const bool kept = take(error);
        lua_pop(state_, results);

        if (!kept)
        {
            stop(std::string(hook_names[hook]) + ": " + error);
        }
    }

    lua_pop(state_, 2);
    in_hook_ = false;
}

void ModScripts::hook(LomModHook hook, int argument)
{
    if (hook < 0 || hook > LOM_MOD_HOOK_FRAME)
    {
        return;
    }

    if (hook == LOM_MOD_HOOK_SCENE_ENTER)
    {
        scene_ = argument;
    }

    each(
        hook, true, checks_per_call, 0,
        [&]()
        {
            if (hook != LOM_MOD_HOOK_SCENE_ENTER)
            {
                return 0;
            }

            lua_pushinteger(state_, argument);
            return 1;
        },
        [](std::string&) { return true; });
}

void ModScripts::game_start(const std::uint32_t* seed)
{
    each(
        LOM_MOD_HOOK_GAME_START, false, checks_per_game_start, 0,
        [&]()
        {
            if (seed && seeded_)
            {
                lua_pushinteger(state_, static_cast<lua_Integer>(*seed));
            }
            else
            {
                lua_pushnil(state_);
            }

            return 1;
        },
        [](std::string&) { return true; });
}

bool ModScripts::file_load(const char* path, unsigned char* data, int size)
{
    bool changed = false;

    each(
        LOM_MOD_HOOK_FILE_LOAD, false, checks_per_call, 1,
        [&]()
        {
            lua_pushcfunction(state_, call_file_hook);
            lua_insert(state_, -2);
            lua_pushlightuserdata(state_, const_cast<char*>(path));
            lua_pushlightuserdata(state_, data);
            lua_pushinteger(state_, size);
            return 4;
        },
        [&](std::string& error)
        {
            std::size_t length = 0;
            const unsigned char* bytes = nullptr;

            if (lua_isnil(state_, -1))
            {
                return true;
            }

            if (lua_type(state_, -1) == LUA_TSTRING)
            {
                bytes = reinterpret_cast<const unsigned char*>(lua_tolstring(state_, -1, &length));
            }
            else if (const Bytes* buffer = static_cast<const Bytes*>(luaL_testudata(state_, -1, bytes_type)))
            {
                bytes = buffer->data;
                length = buffer->size;
            }
            else
            {
                error = std::string("it returned a ") + luaL_typename(state_, -1) + " for " + path + ", not bytes or nil";
                return false;
            }

            if (length != static_cast<std::size_t>(size))
            {
                error =
                    "it returned " + std::to_string(length) + " bytes for " + path + ", which has " + std::to_string(size) + "; a file has to keep its length";
                return false;
            }

            if (std::memcmp(data, bytes, length) != 0)
            {
                std::memcpy(data, bytes, length);
                changed = true;
            }

            return true;
        });

    return changed;
}

int ModScripts::value(LomModHook hook, int value, int lowest, int highest)
{
    each(
        hook, true, checks_per_call, 1,
        [&]()
        {
            lua_pushinteger(state_, value);
            return 1;
        },
        [&](std::string& error)
        {
            if (lua_isnil(state_, -1))
            {
                return true;
            }

            if (!lua_isinteger(state_, -1) || lua_tointeger(state_, -1) < lowest || lua_tointeger(state_, -1) > highest)
            {
                const std::string given = lua_isinteger(state_, -1) ? std::to_string(lua_tointeger(state_, -1)) : std::string("a ") + luaL_typename(state_, -1);
                error = "it returned " + given + ", and the answer has to be a whole number from " + std::to_string(lowest) + " to " + std::to_string(highest) +
                        ", or nil";
                return false;
            }

            value = static_cast<int>(lua_tointeger(state_, -1));
            return true;
        });

    return value;
}

bool ModScripts::wants(LomModHook hook) const
{
    if (stopped_ || hook < 0 || hook >= LOM_MOD_HOOK_COUNT)
    {
        return false;
    }

    lua_getfield(state_, LUA_REGISTRYINDEX, hooks_key);
    lua_rawgeti(state_, -1, hook + 1);
    const bool any = lua_rawlen(state_, -1) > 0;
    lua_pop(state_, 2);
    return any;
}

bool ModScripts::stopped() const
{
    return stopped_;
}

}
