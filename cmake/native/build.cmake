if(NOT EXISTS "${LOM_PSYZ_DIR}/psyz/include/psyz.h")
    message(FATAL_ERROR
        "PSY-Z was not found at ${LOM_PSYZ_DIR}. "
        "Initialize the external/psyz submodule recursively.")
endif()

include(cmake/psyz.cmake)
include(cmake/lua.cmake)
include(launcher/core/common.cmake)

add_library(lom_native_memory STATIC src/core/native_memory.c)
target_include_directories(lom_native_memory PUBLIC "${LOM_DECOMP_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")

include(cmake/native/game.cmake)

include(cmake/native/port.cmake)
include(cmake/native/mods.cmake)
include(cmake/native/checks.cmake)

foreach(suffix IN ITEMS "" _jp)
    add_library(lom_native_stubs${suffix} STATIC "${LOM_NATIVE_STUBS${suffix}}")
    add_dependencies(lom_native_stubs${suffix} lom_native_unit_link${suffix})

    set(runtime_libraries lom_native_game${suffix} lom_mods lom_mod_manifest lom_native_port${suffix} lom_native_stubs${suffix} lom_native_memory psyz
        lom_lua lom_common)
    set(route_runtime_libraries lom_native_game_routes${suffix} lom_mods lom_mod_manifest lom_native_port${suffix} lom_native_stubs${suffix}
        lom_native_memory psyz lom_lua lom_common)
    if(NOT APPLE)
        list(JOIN runtime_libraries "," runtime_libraries)
        list(JOIN route_runtime_libraries "," route_runtime_libraries)
        set(runtime_libraries "$<LINK_GROUP:RESCAN,${runtime_libraries}>")
        set(route_runtime_libraries "$<LINK_GROUP:RESCAN,${route_runtime_libraries}>")
    endif()
    add_library(lom_native_runtime${suffix} INTERFACE)
    target_link_libraries(lom_native_runtime${suffix} INTERFACE ${runtime_libraries} m)
    target_link_options(lom_native_runtime${suffix} INTERFACE ${LOM_LINK_GC_SECTIONS})

    add_library(lom_native_route_runtime${suffix} INTERFACE)
    target_link_libraries(lom_native_route_runtime${suffix} INTERFACE ${route_runtime_libraries} m)
    target_link_options(lom_native_route_runtime${suffix} INTERFACE ${LOM_LINK_GC_SECTIONS})
endforeach()

include(cmake/native/app.cmake)

option(LOM_BUILD_TESTS "Build the tests" ON)
if(LOM_BUILD_TESTS)
    include(cmake/tests/shared.cmake)
    include(cmake/tests/psyz.cmake)
    include(cmake/native/tests.cmake)
    include(cmake/tests/registry.cmake)
endif()

option(LOM_BUILD_LAUNCHER "Build the desktop launcher" ON)
if(LOM_BUILD_LAUNCHER AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LOM_LAUNCHER_BUILD_TESTS ${LOM_BUILD_TESTS})
    add_subdirectory(launcher)
endif()

if(WIN32)
    include(cmake/native/windows.cmake)
endif()
include(cmake/native/install.cmake)
