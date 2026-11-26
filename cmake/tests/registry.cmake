include_guard(GLOBAL)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(LOM_DISC_ROOT "" CACHE PATH "Your extracted Legend of Mana disc, for tests that need it")
set(LOM_DISC_ROOT_JP "" CACHE PATH "Your extracted Japanese Legend of Mana disc, for the Japanese game's tests")
set(LOM_TEST_RESULTS "${CMAKE_BINARY_DIR}/test-results" CACHE PATH "Where test folders go, unless LOM_TEST_RUN_DIR is set")
set(LOM_TEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/run_test.py")

function(lom_add_test)
    cmake_parse_arguments(T "GPU;NO_HEADLESS;PACED_ONLY" "NAME;TIER;TIMEOUT;EXPECT_EXIT;CLOCK;CAPTURE_ENV;DISC" "COMMAND;LABELS;ENV" ${ARGN})
    if(NOT T_NAME OR T_TIER STREQUAL "" OR NOT T_COMMAND)
        message(FATAL_ERROR "lom_add_test needs NAME, TIER and COMMAND")
    endif()

    list(POP_FRONT T_COMMAND program)
    if(TARGET ${program})
        set(program "$<TARGET_FILE:${program}>")
    elseif(program MATCHES "^lom-")
        return()
    endif()

    set(runner ${Python3_EXECUTABLE} "${LOM_TEST_RUNNER}" --name ${T_NAME} --run-dir "${LOM_TEST_RESULTS}")
    if(T_DISC)
        list(APPEND runner --disc "${T_DISC}")
    elseif(LOM_DISC_ROOT)
        list(APPEND runner --disc "${LOM_DISC_ROOT}")
    endif()
    if(T_NO_HEADLESS)
        list(APPEND runner --no-headless)
    endif()
    if(DEFINED T_EXPECT_EXIT)
        list(APPEND runner --expect-exit ${T_EXPECT_EXIT})
    endif()
    if(NOT T_CLOCK)
        set(T_CLOCK virtual)
    endif()
    list(APPEND runner --clock ${T_CLOCK})
    if(T_PACED_ONLY)
        list(APPEND runner --paced-only)
    endif()
    foreach(item IN LISTS T_ENV)
        list(APPEND runner --env ${item})
    endforeach()
    if(T_CAPTURE_ENV)
        list(APPEND runner --capture-env ${T_CAPTURE_ENV})
    endif()

    add_test(NAME ${T_NAME} COMMAND ${runner} -- ${program} ${T_COMMAND})

    string(REGEX MATCH "^[a-z0-9]+" subsystem "${T_NAME}")
    set(labels t${T_TIER} ${subsystem} ${T_LABELS})
    if(T_GPU)
        list(APPEND labels gpu)
    endif()
    string(JOIN ";" all_args ${T_COMMAND})
    if(all_args MATCHES "@DISC@")
        list(APPEND labels disc)
    endif()

    if(NOT T_TIMEOUT)
        set(T_TIMEOUT 120)
    endif()
    set_tests_properties(${T_NAME} PROPERTIES LABELS "${labels}" TIMEOUT ${T_TIMEOUT})
    if(T_PACED_ONLY)
        set_property(TEST ${T_NAME} PROPERTY RUN_SERIAL TRUE)
    endif()
endfunction()

add_library(lom_native_test_clock OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/session/test_clock.c")
get_property(lom_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS lom_test_targets)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "EXECUTABLE" AND target MATCHES "^lom-.*-smoke$")
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:lom_native_test_clock>)
    endif()
endforeach()

if(TARGET lom-gte-register-transfer-smoke)
    lom_add_test(NAME gte-register-transfer TIER 0 NO_HEADLESS TIMEOUT 30 LABELS sdk gte
        COMMAND lom-gte-register-transfer-smoke)
endif()
if(TARGET lom-gte-screen-offset-smoke)
    lom_add_test(NAME gte-screen-offset TIER 0 NO_HEADLESS TIMEOUT 30 LABELS sdk gte
        COMMAND lom-gte-screen-offset-smoke)
endif()
if(TARGET lom-gte-color-alias-smoke)
    lom_add_test(NAME gte-color-alias TIER 0 NO_HEADLESS TIMEOUT 60 LABELS sdk gte
        COMMAND lom-gte-color-alias-smoke)
endif()

lom_add_test(NAME movie-decoder TIER 0 NO_HEADLESS COMMAND lom-movie-decoder-smoke)
lom_add_test(NAME portable TIER 0 NO_HEADLESS LABELS platform COMMAND lom-portable-smoke)
lom_add_test(NAME separation-check TIER 0 NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/separation_check.py")
lom_add_test(NAME comment-check TIER 0 NO_HEADLESS LABELS lint
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/lint/check_comments.py")
lom_add_test(NAME native-stubs TIER 0 NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/native_stubs_check.py" "${LOM_NATIVE_STUBS}")
lom_add_test(NAME native-stubs-jp TIER 0 NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/native_stubs_check.py" "${LOM_NATIVE_STUBS_jp}")
lom_add_test(NAME crash-report TIER 0 NO_HEADLESS LABELS platform COMMAND lom-crash-report-smoke)
lom_add_test(NAME remaster-files TIER 0 NO_HEADLESS LABELS platform COMMAND lom-remaster-files-smoke)
foreach(case IN ITEMS size newest count)
    lom_add_test(NAME session-prune-${case} TIER 0 NO_HEADLESS LABELS platform session
        COMMAND lom-session-prune-smoke ${case})
endforeach()
foreach(case IN ITEMS record replay v4 old)
    lom_add_test(NAME session-switches-${case} TIER 0 NO_HEADLESS LABELS platform session
        COMMAND lom-session-switches-smoke ${case})
endforeach()
lom_add_test(NAME session-switches-unknown TIER 0 NO_HEADLESS EXPECT_EXIT 2 LABELS platform session
    COMMAND lom-session-switches-smoke unknown)
foreach(case IN ITEMS switches refused scripts dice api examples notices surface cards games relations toggles options)
    lom_add_test(NAME mods-${case} TIER 0 NO_HEADLESS LABELS platform mods
        COMMAND lom-mods-smoke ${case} "${CMAKE_CURRENT_SOURCE_DIR}/tests/mods" "${CMAKE_CURRENT_SOURCE_DIR}/launcher/data/mods")
endforeach()
lom_add_test(NAME movie-completion TIER 0 NO_HEADLESS TIMEOUT 30 LABELS movie decoder
    COMMAND lom-movie-completion-smoke)
lom_add_test(NAME movie-audio-output-sdl TIER 0 TIMEOUT 30
    COMMAND lom-movie-audio-output-sdl-smoke)
lom_add_test(NAME movie-audio-rematrix TIER 0 NO_HEADLESS TIMEOUT 30
    COMMAND lom-movie-audio-rematrix-smoke)
lom_add_test(NAME movie-audio-output-edges TIER 0 NO_HEADLESS TIMEOUT 30
    COMMAND lom-movie-audio-output-edges-smoke)

if(TARGET lom-cd-audio-matrix-smoke)
    lom_add_test(NAME cd-audio-matrix TIER 0 NO_HEADLESS TIMEOUT 30 LABELS cd sdk
        COMMAND lom-cd-audio-matrix-smoke)
endif()
if(TARGET lom-controller-callback-smoke AND TARGET lom-controller-callback-fixture)
    lom_add_test(NAME controller-callback TIER 0 NO_HEADLESS TIMEOUT 60
        COMMAND lom-controller-callback-smoke $<TARGET_FILE:lom-controller-callback-fixture>)
endif()

lom_add_test(NAME spu-completion TIER 0 NO_HEADLESS TIMEOUT 60 LABELS spu sdk
    COMMAND lom-spu-completion-smoke)
lom_add_test(NAME root-counter TIER 0 NO_HEADLESS TIMEOUT 60 LABELS sdk audio
    COMMAND lom-root-counter-smoke)

foreach(name IN ITEMS sdk-abi upstream psyz random card field-collision
        field-resources field-actor field-party field-golem-party field-actions field-slots field-heap
        field-services akao-mmio-retrigger
        spu-voice-end spu-noise-pmon spu-counter-interleave spu-live-adsr audio-categories title-input)
    lom_add_test(NAME ${name} TIER 0 COMMAND lom-${name}-smoke)
endforeach()

lom_add_test(NAME akao-irq TIER 0 CLOCK paced PACED_ONLY COMMAND lom-akao-irq-smoke @DISC@)
lom_add_test(NAME audio-clock TIER 0 CLOCK paced PACED_ONLY COMMAND lom-audio-clock-smoke)

if(TARGET lom-settings-store-smoke)
    lom_add_test(NAME settings-store TIER 0 TIMEOUT 30 LABELS input
        COMMAND lom-settings-store-smoke @DIR@)
    lom_add_test(NAME settings-runtime TIER 0 TIMEOUT 30 LABELS input
        COMMAND lom-settings-runtime-smoke @DIR@)
    lom_add_test(NAME menu-model TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-model-smoke)
    lom_add_test(NAME menu-glyphs TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-glyphs-smoke)
    lom_add_test(NAME menu-text TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-text-smoke)
endif()

lom_add_test(NAME cd-callback TIER 0 NO_HEADLESS COMMAND lom-cd-callback-smoke @DISC@)
foreach(name IN ITEMS cd cd-stream overlay wmap-callback-tables akao-noise-opcodes akao-stream field-cd-wait
        field-music-stream field-lands unit-decode field-map-decode gte-tables japanese-balance)
    lom_add_test(NAME ${name} TIER 0 COMMAND lom-${name}-smoke @DISC@)
endforeach()
if(LOM_DISC_ROOT_JP)
    lom_add_test(NAME gte-tables-jp TIER 0 DISC "${LOM_DISC_ROOT_JP}" COMMAND lom-gte-tables-jp-smoke @DISC@)
    lom_add_test(NAME japanese-balance-data TIER 0 NO_HEADLESS LABELS mods
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/make_japanese_balance.py" @DISC@ "${LOM_DISC_ROOT_JP}" --check)
endif()

if(TARGET lom-controller-multitap-slot-a-smoke)
    lom_add_test(NAME controller-multitap-slot-a TIER 0 GPU CLOCK virtual TIMEOUT 60 LABELS input sdk
        COMMAND lom-controller-multitap-slot-a-smoke)
endif()
foreach(name IN ITEMS gpu-tpage gpu-subtract gpu-polygon-limits xbr)
    lom_add_test(NAME ${name} TIER 1 GPU COMMAND lom-${name}-smoke)
endforeach()
if(TARGET lom-menu-render-smoke)
    lom_add_test(NAME menu-render TIER 1 GPU LABELS input COMMAND lom-menu-render-smoke @OUT@)
    lom_add_test(NAME menu-quit TIER 1 GPU CLOCK paced TIMEOUT 60 LABELS input COMMAND lom-menu-quit-smoke)
endif()
lom_add_test(NAME title-entry TIER 1 GPU TIMEOUT 300 COMMAND lom-title-entry-smoke @DISC@)
lom_add_test(NAME keyboard-controls TIER 1 GPU COMMAND lom-keyboard-controls-smoke @OUT@)
lom_add_test(NAME controller-rumble TIER 1 GPU TIMEOUT 120 LABELS input
    COMMAND lom-controller-rumble-smoke)

if(COMMAND lom_add_route_tests)
    lom_add_route_tests()
endif()
