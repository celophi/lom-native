if(NOT EXISTS "${LOM_PSYZ_DIR}/external/SDL/CMakeLists.txt")
    message(STATUS
        "PSY-Z SDL submodule is not populated; graphical target 'lom' disabled. "
        "Run: git submodule update --init --recursive")
    return()
endif()

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${LOM_PSYZ_DIR}/psyz ${CMAKE_BINARY_DIR}/psyz)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Git REQUIRED)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/psyz_patches.cmake")

if(TARGET SDL3-static AND WIN32)
    target_compile_options(SDL3-static PRIVATE $<$<COMPILE_LANGUAGE:C>:-mms-bitfields>)
endif()

set(LOM_PATCHED_PSYZ_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/psyz_native")
set(LOM_NATIVE_PSYZ_PLATFORM_DIR "${LOM_PATCHED_PSYZ_DIR}/platform")
file(MAKE_DIRECTORY "${LOM_NATIVE_PSYZ_PLATFORM_DIR}")
foreach(platform_header IN ITEMS lom_vsync_clock.h lom_vsync_failure.h lom_native_psyz.h)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/include/${platform_header}"
        "${LOM_NATIVE_PSYZ_PLATFORM_DIR}/${platform_header}" COPYONLY)
endforeach()
if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
    foreach(shader IN ITEMS lom_psx_vert_spv.h lom_psx_frag_spv.h lom_psx_vert_msl.h lom_psx_frag_msl.h
            lom_psx_vert_dxil.h lom_psx_frag_dxil.h)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/shaders/${shader}")
            configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/shaders/${shader}"
                "${LOM_NATIVE_PSYZ_PLATFORM_DIR}/${shader}" COPYONLY)
        endif()
    endforeach()
endif()

lom_patch_psyz_sources("${CMAKE_CURRENT_SOURCE_DIR}/patches/psyz/series.json")

target_compile_definitions(psyz PRIVATE _GNU_SOURCE)
target_include_directories(psyz BEFORE PUBLIC "${LOM_PATCHED_PSYZ_DIR}/include")
target_include_directories(psyz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${LOM_PSYZ_DIR}/decomp/src/libspu"
    "${LOM_PSYZ_DIR}/psyz/src/psyz")
include(src/mods/switches.cmake)
include(src/ui/notices.cmake)
target_link_libraries(psyz PUBLIC lom_switches lom_notices)
target_sources(psyz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/widescreen.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/remaster/remaster_settings.c")

if(PSYZ_RENDERER STREQUAL "sdl3_gpu")
    add_library(lom_native_presentation STATIC src/gpu/presentation.c src/gpu/xbr.c src/gpu/crossfade.c src/gpu/hd_movie_layout.c)
    target_include_directories(lom_native_presentation PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom_native_presentation PUBLIC lom_switches)
    target_link_libraries(lom_native_presentation PUBLIC SDL3-static)
    target_link_libraries(psyz PRIVATE lom_native_presentation)
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/widescreen.c" TARGET_DIRECTORY psyz
        PROPERTIES COMPILE_DEFINITIONS LOM_WIDESCREEN_CANVAS=1)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/psyz_menu.cmake")
endif()

target_include_directories(psyz PRIVATE
    "${LOM_PSYZ_DIR}/psyz/src/platform"
    "${LOM_PSYZ_DIR}/psyz/src"
    "${LOM_PSYZ_DIR}/decomp/src/libgpu")
