set(LOM_LUA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/lua" CACHE PATH "Path to Lua's source")
if(NOT EXISTS "${LOM_LUA_DIR}/lua.h")
    message(FATAL_ERROR "Lua was not found at ${LOM_LUA_DIR}. Initialize the external/lua submodule (tools/ci/checkout-submodules.sh).")
endif()

set(LOM_LUA_SOURCES
    lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio
    lauxlib lbaselib lcorolib lmathlib lstrlib ltablib lutf8lib)
list(TRANSFORM LOM_LUA_SOURCES PREPEND "${LOM_LUA_DIR}/")
list(TRANSFORM LOM_LUA_SOURCES APPEND ".c")

add_library(lom_lua STATIC ${LOM_LUA_SOURCES})
set_target_properties(lom_lua PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF)
target_include_directories(lom_lua SYSTEM PUBLIC "${LOM_LUA_DIR}")
target_compile_definitions(lom_lua PRIVATE "luai_makeseed(L)=((unsigned int)0x4C4F4D4Du)")
