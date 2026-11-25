#include "mod_scripts.h"
#include "mod_manifest.h"
#include "lom_native_mod_game.h"
#include "lom_native_notices.h"
#include "lom_native_random.h"
#include "lom_native_switches.h"

extern "C"
{
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace lom::mods
{

namespace
{

constexpr std::size_t memory_limit = 32 * 1024 * 1024;

constexpr int instructions_per_check = 1000;

constexpr long checks_per_call = 10000;

constexpr int random_bits = 15;

constexpr int random_mask = (1 << random_bits) - 1;

constexpr char scripts_folder[] = "scripts";

constexpr char hooks_key[] = "lom-native hooks";

constexpr char modules_key[] = "lom-native modules";

constexpr const char* hook_names[LOM_MOD_HOOK_COUNT] = {"scene_enter", "battle_start", "battle_end", "frame"};

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
    std::ifstream input(path, std::ios::binary);

    if (!input)
    {
        return false;
    }

    std::ostringstream bytes;
    bytes << input.rdbuf();
    text = bytes.str();
    return true;
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
    {nullptr, nullptr},
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

    luaL_newlib(state_, api_functions);
    lua_pushinteger(state_, mod_api);
    lua_setfield(state_, -2, "api");
    lua_setglobal(state_, "lom");
    lua_sethook(state_, ScriptApi::count, LUA_MASKCOUNT, instructions_per_check);
    return true;
}

bool ModScripts::call(int arguments, int results, std::string& error)
{
    const int base = lua_gettop(state_) - arguments;
    lua_pushcfunction(state_, ScriptApi::traceback);
    lua_insert(state_, base);
    budget_ = checks_per_call;
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

    return call(0, 0, error);
}

void ModScripts::stop(const std::string& error)
{
    stopped_ = true;
    std::fprintf(stderr, "[lom-native] mods: %s stopped: %s\n", mod_id_.c_str(), error.c_str());
    lom_native_notice_post(LOM_NOTICE_MOD_STOPPED, mod_id_.c_str(), error.c_str());
}

std::unique_ptr<ModScripts> ModScripts::start(const std::string& mod_id, const std::filesystem::path& folder, const std::vector<std::string>& scripts,
                                              std::string& error)
{
    std::unique_ptr<ModScripts> host(new ModScripts(mod_id, folder));
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

void ModScripts::hook(LomModHook hook, int argument)
{
    if (stopped_ || hook < 0 || hook >= LOM_MOD_HOOK_COUNT)
    {
        return;
    }

    if (hook == LOM_MOD_HOOK_SCENE_ENTER)
    {
        scene_ = argument;
    }

    in_hook_ = true;
    lua_getfield(state_, LUA_REGISTRYINDEX, hooks_key);
    lua_rawgeti(state_, -1, hook + 1);
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(state_, -1));

    for (lua_Integer i = 1; i <= count && !stopped_; ++i)
    {
        lua_rawgeti(state_, -1, i);
        int arguments = 0;

        if (hook == LOM_MOD_HOOK_SCENE_ENTER)
        {
            lua_pushinteger(state_, argument);
            arguments = 1;
        }

        std::string error;

        if (!call(arguments, 0, error))
        {
            stop(std::string(hook_names[hook]) + ": " + error);
        }
    }

    lua_pop(state_, 2);
    in_hook_ = false;
}

bool ModScripts::stopped() const
{
    return stopped_;
}

}
