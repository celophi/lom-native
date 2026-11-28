include_guard(GLOBAL)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(LOM_DISC_ROOT "" CACHE PATH "Your extracted Legend of Mana disc, for tests that need it")
set(LOM_DISC_ROOT_JP "" CACHE PATH "Your extracted Japanese Legend of Mana disc, for the Japanese game's tests")
set(LOM_TEST_RESULTS "${CMAKE_BINARY_DIR}/test-results" CACHE PATH "Where test folders go, unless LOM_TEST_RUN_DIR is set")
set(LOM_TEST_RUNNER "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/run_test.py")
set(LOM_TEST_KINDS unit contract behavior integration component static e2e)
set(LOM_TEST_TARGET_KINDS test unit contract contract specification behavior integration integration component component)
set(LOM_TEST_TARGET_PATTERN "^lom-.+-(test|contract|specification|integration|component)$")

function(lom_test_kind_of target out)
    set(kind "")
    if(target MATCHES "${LOM_TEST_TARGET_PATTERN}")
        list(FIND LOM_TEST_TARGET_KINDS "${CMAKE_MATCH_1}" at)
        math(EXPR at "${at} + 1")
        list(GET LOM_TEST_TARGET_KINDS ${at} kind)
    endif()
    set(${out} "${kind}" PARENT_SCOPE)
endfunction()

function(lom_add_test)
    cmake_parse_arguments(T "GPU;NO_HEADLESS;PACED_ONLY" "NAME;TIER;KIND;TIMEOUT;EXPECT_EXIT;CLOCK;CAPTURE_ENV;DISC" "COMMAND;LABELS;ENV" ${ARGN})
    if(NOT T_NAME OR T_TIER STREQUAL "" OR NOT T_COMMAND)
        message(FATAL_ERROR "lom_add_test needs NAME, TIER and COMMAND")
    endif()

    list(POP_FRONT T_COMMAND program)
    lom_test_kind_of("${program}" kind)
    if(NOT T_KIND)
        set(T_KIND "${kind}")
    endif()
    if(NOT T_KIND IN_LIST LOM_TEST_KINDS)
        message(FATAL_ERROR "${T_NAME}: say what kind of test it is, with KIND (one of ${LOM_TEST_KINDS}), or build it as a target named for its kind, like lom-${T_NAME}-test")
    endif()
    set(target "")
    if(TARGET ${program})
        set(target ${program})
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
    if(subsystem IN_LIST LOM_TEST_KINDS)
        set(subsystem "")
    endif()
    set(labels t${T_TIER} ${T_KIND} ${subsystem} ${T_LABELS})
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
    if(target)
        list(JOIN labels "," joined)
        set_property(GLOBAL APPEND PROPERTY LOM_TEST_RUNS "${target}|${T_NAME}|${joined}")
    endif()
endfunction()

function(lom_add_specifications)
    cmake_parse_arguments(S "" "NAME;TIER;SOURCE;TARGET" "LABELS" ${ARGN})
    if(NOT TARGET ${S_TARGET})
        return()
    endif()
    set(source "${CMAKE_CURRENT_SOURCE_DIR}/${S_SOURCE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
    file(STRINGS "${source}" specifications REGEX "^SPECIFICATION\\(")
    list(LENGTH specifications count)
    foreach(number RANGE 1 ${count})
        lom_add_test(NAME ${S_NAME}-${number} TIER ${S_TIER} NO_HEADLESS TIMEOUT 60 LABELS ${S_LABELS} COMMAND ${S_TARGET} ${number})
    endforeach()
endfunction()

add_library(lom_native_test_clock OBJECT "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/session/test_clock.c")
get_property(lom_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS lom_test_targets)
    get_target_property(type ${target} TYPE)
    if(type STREQUAL "EXECUTABLE" AND target MATCHES "${LOM_TEST_TARGET_PATTERN}")
        target_sources(${target} PRIVATE $<TARGET_OBJECTS:lom_native_test_clock>)
    endif()
endforeach()

if(TARGET lom-gte-register-transfer-test)
    lom_add_test(NAME gte-register-transfer TIER 0 NO_HEADLESS TIMEOUT 30 LABELS sdk gte
        COMMAND lom-gte-register-transfer-test)
endif()
if(TARGET lom-gte-screen-offset-test)
    lom_add_test(NAME gte-screen-offset TIER 0 NO_HEADLESS TIMEOUT 30 LABELS sdk gte
        COMMAND lom-gte-screen-offset-test)
endif()
if(TARGET lom-gte-color-alias-test)
    lom_add_test(NAME gte-color-alias TIER 0 NO_HEADLESS TIMEOUT 60 LABELS sdk gte
        COMMAND lom-gte-color-alias-test)
endif()

lom_add_test(NAME movie-decoder TIER 0 NO_HEADLESS COMMAND lom-movie-decoder-integration)
lom_add_test(NAME portable TIER 0 NO_HEADLESS LABELS platform COMMAND lom-portable-integration)
lom_add_test(NAME separation-check TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/separation_check.py")
lom_add_test(NAME provider-check TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/provider_check.py")
lom_add_test(NAME spec-check TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/spec_check.py")
lom_add_test(NAME test-check TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/test_check.py" "${CMAKE_BINARY_DIR}/test-targets.json")
lom_add_test(NAME comment-check TIER 0 KIND static NO_HEADLESS LABELS lint
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/lint/check_comments.py")
lom_add_test(NAME native-stubs TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/native_stubs_check.py" "${LOM_NATIVE_STUBS}")
lom_add_test(NAME native-stubs-jp TIER 0 KIND static NO_HEADLESS LABELS platform
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/native_stubs_check.py" "${LOM_NATIVE_STUBS_jp}")
lom_add_test(NAME crash-report TIER 0 NO_HEADLESS LABELS platform COMMAND lom-crash-report-integration)
lom_add_test(NAME remaster-files TIER 0 NO_HEADLESS LABELS platform COMMAND lom-remaster-files-test)
lom_add_test(NAME remaster-files-provider TIER 0 NO_HEADLESS LABELS platform COMMAND lom-remaster-files-provider-contract)
lom_add_test(NAME disc-files-provider TIER 0 NO_HEADLESS LABELS platform COMMAND lom-disc-files-provider-contract)
lom_add_test(NAME remaster-screens TIER 0 NO_HEADLESS LABELS platform COMMAND lom-remaster-screens-test)
foreach(specification IN ITEMS encyclopedia_picture item_name ring_menu_background ring_menu_icon ring_menu_key ring_menu_open
        save_screen_background save_screen_open save_screen_picture save_screen_text scene_text screen_fade start_the_game naming_screen_background naming_screen_glyph naming_screen_open
        title_backdrop title_menu world_select_loaded world_select_picture world_select_zoom
        world_map_label world_map_panel world_map_spirit world_map_status)
    string(REPLACE "_" "-" name "${specification}")
    lom_add_specifications(NAME remaster-${name} TIER 0 LABELS remaster TARGET lom-remaster-${name}-specification
        SOURCE tests/behavior/remaster/${specification}_specification.c)
endforeach()
lom_add_test(NAME remaster-movies TIER 0 NO_HEADLESS LABELS platform COMMAND lom-remaster-movies-integration)
foreach(case IN ITEMS size newest count)
    lom_add_test(NAME session-prune-${case} TIER 0 NO_HEADLESS LABELS platform session
        COMMAND lom-session-prune-integration ${case})
endforeach()
foreach(case IN ITEMS record replay v4 old)
    lom_add_test(NAME session-switches-${case} TIER 0 NO_HEADLESS LABELS platform session
        COMMAND lom-session-switches-integration ${case})
endforeach()
lom_add_test(NAME session-switches-unknown TIER 0 NO_HEADLESS EXPECT_EXIT 2 LABELS platform session
    COMMAND lom-session-switches-integration unknown)
foreach(case IN ITEMS switches refused scripts dice api examples notices surface cards games relations toggles options api5 seeds files artifacts)
    lom_add_test(NAME mods-${case} TIER 0 NO_HEADLESS LABELS platform mods
        COMMAND lom-mods-specification ${case} "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/mods" "${CMAKE_CURRENT_SOURCE_DIR}/launcher/data/mods")
endforeach()
lom_add_test(NAME movie-completion TIER 0 NO_HEADLESS TIMEOUT 30 LABELS movie decoder
    COMMAND lom-movie-completion-component)
lom_add_test(NAME movie-audio-output-sdl TIER 0 TIMEOUT 30
    COMMAND lom-movie-audio-output-sdl-specification)
lom_add_test(NAME movie-audio-rematrix TIER 0 NO_HEADLESS TIMEOUT 30
    COMMAND lom-movie-audio-rematrix-test)
lom_add_test(NAME movie-audio-output-edges TIER 0 NO_HEADLESS TIMEOUT 30
    COMMAND lom-movie-audio-output-edges-component)

if(TARGET lom-cd-audio-matrix-specification)
    lom_add_test(NAME cd-audio-matrix TIER 0 NO_HEADLESS TIMEOUT 30 LABELS cd sdk
        COMMAND lom-cd-audio-matrix-specification)
endif()
if(TARGET lom-controller-callback-component AND TARGET lom-controller-callback-fixture)
    lom_add_test(NAME controller-callback TIER 0 NO_HEADLESS TIMEOUT 60
        COMMAND lom-controller-callback-component $<TARGET_FILE:lom-controller-callback-fixture>)
endif()

lom_add_test(NAME spu-completion TIER 0 NO_HEADLESS TIMEOUT 60 LABELS spu sdk
    COMMAND lom-spu-completion-component)
lom_add_test(NAME root-counter TIER 0 NO_HEADLESS TIMEOUT 60 LABELS sdk audio
    COMMAND lom-root-counter-test)

foreach(target IN ITEMS lom-sdk-abi-test lom-upstream-test lom-psyz-test lom-random-test lom-card-integration
        lom-field-collision-component lom-field-resources-component lom-field-actor-component
        lom-field-party-component lom-field-golem-party-component lom-field-actions-component
        lom-field-slots-component lom-field-heap-component lom-field-services-component lom-akao-mmio-retrigger-test
        lom-spu-voice-end-test lom-spu-noise-pmon-test lom-spu-counter-interleave-test lom-spu-live-adsr-test
        lom-audio-categories-specification lom-title-input-component)
    string(REGEX REPLACE "^lom-(.*)-[a-z]+$" "\\1" name "${target}")
    lom_add_test(NAME ${name} TIER 0 COMMAND ${target})
endforeach()

lom_add_test(NAME akao-irq TIER 0 CLOCK paced PACED_ONLY COMMAND lom-akao-irq-component @DISC@)
lom_add_test(NAME audio-clock TIER 0 CLOCK paced PACED_ONLY COMMAND lom-audio-clock-integration)

if(TARGET lom-settings-store-specification)
    lom_add_test(NAME settings-store TIER 0 TIMEOUT 30 LABELS input
        COMMAND lom-settings-store-specification @DIR@)
    lom_add_test(NAME settings-runtime TIER 0 TIMEOUT 30 LABELS input
        COMMAND lom-settings-runtime-specification @DIR@)
    lom_add_test(NAME menu-model TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-model-specification)
    lom_add_test(NAME menu-glyphs TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-glyphs-test)
    lom_add_test(NAME menu-text TIER 0 NO_HEADLESS TIMEOUT 30 LABELS input
        COMMAND lom-menu-text-test)
endif()

lom_add_test(NAME cd-callback TIER 0 NO_HEADLESS COMMAND lom-cd-callback-integration @DISC@)
lom_add_test(NAME cd TIER 0 COMMAND lom-cd-resource-integration @DISC@)
foreach(target IN ITEMS lom-cd-stream-integration lom-overlay-integration lom-wmap-callback-tables-integration
        lom-akao-noise-opcodes-component lom-akao-stream-component lom-field-cd-wait-component
        lom-field-music-stream-component lom-field-lands-component lom-unit-decode-integration
        lom-field-map-decode-integration lom-gte-tables-integration lom-japanese-balance-component)
    string(REGEX REPLACE "^lom-(.*)-[a-z]+$" "\\1" name "${target}")
    lom_add_test(NAME ${name} TIER 0 COMMAND ${target} @DISC@)
endforeach()
if(LOM_DISC_ROOT_JP)
    lom_add_test(NAME gte-tables-jp TIER 0 DISC "${LOM_DISC_ROOT_JP}" COMMAND lom-gte-tables-jp-integration @DISC@)
    lom_add_test(NAME japanese-balance-data TIER 0 KIND static NO_HEADLESS LABELS mods
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/make_japanese_balance.py" @DISC@ "${LOM_DISC_ROOT_JP}" --check)
endif()

if(TARGET lom-controller-multitap-slot-a-component)
    lom_add_test(NAME controller-multitap-slot-a TIER 0 GPU CLOCK virtual TIMEOUT 60 LABELS input sdk
        COMMAND lom-controller-multitap-slot-a-component)
endif()
foreach(target IN ITEMS lom-gpu-tpage-integration lom-gpu-subtract-integration lom-gpu-polygon-limits-integration
        lom-xbr-integration)
    string(REGEX REPLACE "^lom-(.*)-[a-z]+$" "\\1" name "${target}")
    lom_add_test(NAME ${name} TIER 1 GPU COMMAND ${target})
endforeach()
if(TARGET lom-menu-render-integration)
    lom_add_test(NAME menu-render TIER 1 GPU LABELS input COMMAND lom-menu-render-integration @OUT@)
    lom_add_test(NAME menu-quit TIER 1 GPU CLOCK paced TIMEOUT 60 LABELS input COMMAND lom-menu-quit-integration)
endif()
lom_add_test(NAME title-entry TIER 1 GPU TIMEOUT 300 COMMAND lom-title-entry-integration @DISC@)
lom_add_test(NAME hd-renderer-provider TIER 1 GPU LABELS platform COMMAND lom-hd-renderer-provider-contract)
lom_add_test(NAME keyboard-controls TIER 1 GPU COMMAND lom-keyboard-controls-component @OUT@)
lom_add_test(NAME controller-rumble TIER 1 GPU TIMEOUT 120 LABELS input
    COMMAND lom-controller-rumble-component)

if(COMMAND lom_add_route_tests)
    lom_add_route_tests()
endif()

set(manifest "{}")
get_property(runs GLOBAL PROPERTY LOM_TEST_RUNS)
get_property(targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(target IN LISTS targets)
    get_target_property(type ${target} TYPE)
    if(NOT type STREQUAL "EXECUTABLE" OR NOT target MATCHES "${LOM_TEST_TARGET_PATTERN}")
        continue()
    endif()
    set(entry "{}")
    foreach(property IN ITEMS SOURCES LINK_OPTIONS COMPILE_DEFINITIONS)
        get_target_property(values ${target} ${property})
        set(array "[]")
        if(values)
            foreach(value IN LISTS values)
                string(JSON length LENGTH "${array}")
                string(REPLACE "\\" "\\\\" value "${value}")
                string(REPLACE "\"" "\\\"" value "${value}")
                string(JSON array SET "${array}" ${length} "\"${value}\"")
            endforeach()
        endif()
        string(JSON entry SET "${entry}" "${property}" "${array}")
    endforeach()
    set(array "[]")
    foreach(run IN LISTS runs)
        string(REPLACE "|" ";" run "${run}")
        list(GET run 0 run_target)
        if(run_target STREQUAL target)
            list(GET run 1 run_name)
            list(GET run 2 run_labels)
            string(JSON length LENGTH "${array}")
            string(JSON array SET "${array}" ${length} "{\"name\": \"${run_name}\", \"labels\": \"${run_labels}\"}")
        endif()
    endforeach()
    string(JSON entry SET "${entry}" "TESTS" "${array}")
    string(JSON manifest SET "${manifest}" "${target}" "${entry}")
endforeach()
file(WRITE "${CMAKE_BINARY_DIR}/test-targets.json" "${manifest}\n")
