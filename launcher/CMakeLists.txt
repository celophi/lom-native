cmake_minimum_required(VERSION 3.20)
project(lom_launcher LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOM_LAUNCHER_BUILD_APP "Build the Qt desktop launcher application" ON)
option(LOM_LAUNCHER_BUILD_TESTS "Build launcher tests" ON)
set(LOM_APP_VERSION "" CACHE STRING
    "Version the app reports; leave empty to get it from git (release builds pass the tag's)")
if(LOM_APP_VERSION STREQUAL "0.4.0-dev")
    set(LOM_APP_VERSION "" CACHE STRING
        "Version the app reports; leave empty to get it from git (release builds pass the tag's)" FORCE)
endif()

if(LOM_APP_VERSION)
    set(lom_version "${LOM_APP_VERSION}")
else()
    set(lom_version "0.0.0")
    set(lom_describe "")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND "${GIT_EXECUTABLE}" describe --tags --long --dirty --match "v[0-9]*"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE lom_describe OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET RESULT_VARIABLE lom_describe_result)
        if(NOT lom_describe_result EQUAL 0)
            set(lom_describe "")
        endif()
    endif()
    set(lom_archival "${CMAKE_CURRENT_SOURCE_DIR}/../.git_archival.txt")
    if(NOT lom_describe AND EXISTS "${lom_archival}")
        file(STRINGS "${lom_archival}" lom_describe REGEX "^describe: " LIMIT_COUNT 1)
        string(REGEX REPLACE "^describe: " "" lom_describe "${lom_describe}")
    endif()
    if(lom_describe MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)(-([0-9]+)-g([0-9a-f]+))?(-dirty)?$")
        set(lom_release "${CMAKE_MATCH_1}")
        set(lom_ahead "${CMAKE_MATCH_3}")
        set(lom_commit "${CMAKE_MATCH_4}")
        set(lom_dirty "${CMAKE_MATCH_5}")
        set(lom_version "${lom_release}")
        if((lom_ahead AND NOT lom_ahead EQUAL 0) OR lom_dirty)
            if(NOT lom_ahead)
                set(lom_ahead 0)
            endif()
            string(APPEND lom_version "+${lom_ahead}.g${lom_commit}")
            if(lom_dirty)
                string(APPEND lom_version ".dirty")
            endif()
        endif()
    endif()
endif()
set(LOM_VERSION "${lom_version}" CACHE INTERNAL "The version the app reports")
message(STATUS "lom-native version: ${LOM_VERSION}")
set(LOM_UPDATE_REPOSITORY "celophi/lom-native" CACHE STRING
    "GitHub owner/name checked for releases and their notes; empty turns both off")
set(LOM_APP_ID "io.github.celophi.lom_native" CACHE STRING "Desktop application ID")

set(LOM_LAUNCHER_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(LOM_LAUNCHER_WARNINGS -Wall -Wextra -Wpedantic)
elseif(MSVC)
    set(LOM_LAUNCHER_WARNINGS /W4 /utf-8)
endif()

include(core/common.cmake)
include("${CMAKE_CURRENT_SOURCE_DIR}/../src/mods/manifest.cmake")

add_library(lom_launcher_core STATIC
    core/src/disc/cue_sheet.cpp
    core/src/disc/disc_image.cpp
    core/src/disc/disc_importer.cpp
    core/src/disc/file_digest.cpp
    core/src/disc/file_manifest.cpp
    core/src/disc/file_snapshot.cpp
    core/src/disc/folder_verifier.cpp
    core/src/disc/installation.cpp
    core/src/disc/iso9660_reader.cpp
    core/src/disc/release_catalog.cpp
    core/src/saves/lom_save.cpp
    core/src/saves/memory_card.cpp
    core/src/saves/save_folder.cpp
    core/src/saves/save_importer.cpp
    core/src/common/atomic_file.cpp
    core/src/common/clock_provider.cpp
    core/src/common/files_provider.cpp
    core/src/common/launcher_settings.cpp
    core/src/common/progress.cpp
    core/src/common/random_provider.cpp
    core/src/common/remaster_install.cpp
    core/src/common/semantic_version.cpp
    core/src/mods/mod_library.cpp
    core/src/common/sha1.cpp
    core/src/common/text_case.cpp
    core/src/common/update_package.cpp
    core/src/common/zip_writer.cpp)
target_compile_features(lom_launcher_core PUBLIC cxx_std_17)
target_include_directories(lom_launcher_core
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/core/src")
target_compile_options(lom_launcher_core PRIVATE ${LOM_LAUNCHER_WARNINGS})
target_link_libraries(lom_launcher_core PUBLIC lom_common lom_mod_manifest)

add_executable(lom-disc-tool cli/lom_disc_tool.cpp)
target_link_libraries(lom-disc-tool PRIVATE lom_launcher_core)
target_compile_options(lom-disc-tool PRIVATE ${LOM_LAUNCHER_WARNINGS})

if(LOM_LAUNCHER_BUILD_APP)
    find_package(Qt6 6.5 QUIET COMPONENTS Widgets Network)
    if(Qt6_FOUND)
        add_subdirectory(app)
    else()
        message(STATUS "Qt 6.5 or newer not found: skipping the desktop launcher application")
    endif()
endif()

if(LOM_LAUNCHER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
