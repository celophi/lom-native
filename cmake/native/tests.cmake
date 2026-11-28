add_executable(lom-unit-decode-integration tests/integration/unit_decode_integration.c)
target_compile_definitions(lom-unit-decode-integration PRIVATE __psyz VERSION_US)
target_link_libraries(lom-unit-decode-integration PRIVATE lom_native_runtime lom_native_port)

add_library(lom_native_psyz_gte_contract OBJECT tests/support/sdk/psyz_gte_contract.c)
target_link_libraries(lom_native_psyz_gte_contract PRIVATE psyz)
add_executable(lom-sdk-abi-test tests/unit/sdk_abi_test.c
    $<TARGET_OBJECTS:lom_native_psyz_gte_contract>)
target_compile_definitions(lom-sdk-abi-test PRIVATE __psyz VERSION_US)
target_compile_options(lom-sdk-abi-test PRIVATE -fno-builtin)
target_include_directories(lom-sdk-abi-test BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES})
target_link_libraries(lom-sdk-abi-test PRIVATE lom_native_port)

function(lom_native_runtime_test target)
    cmake_parse_arguments(PARSE_ARGV 1 test "FIELD" "" "WRAP")
    add_executable(${target} ${test_UNPARSED_ARGUMENTS})
    target_compile_definitions(${target} PRIVATE __psyz VERSION_US LOM_NATIVE=1)
    target_link_libraries(${target} PRIVATE lom_native_runtime lom_native_port)
    if(test_FIELD)
        target_include_directories(${target} BEFORE PRIVATE "${LOM_DECOMP_DIR}/src/overlays/field")
    endif()
    if(test_WRAP)
        list(TRANSFORM test_WRAP PREPEND "-Wl,--wrap=")
        target_link_options(${target} PRIVATE ${test_WRAP})
    endif()
endfunction()

lom_native_runtime_test(lom-cd-stream-integration tests/integration/cd_stream_integration.c)
lom_native_runtime_test(lom-session-prune-integration tests/integration/session_prune_integration.c)
lom_native_runtime_test(lom-session-switches-integration tests/integration/session_switches_integration.c)
lom_native_runtime_test(lom-song-render tests/support/tools/song_render.c)
lom_native_runtime_test(lom-remaster-files-test tests/unit/remaster_files_test.c)
lom_native_runtime_test(lom-remaster-files-provider-contract tests/providers/remaster_files_provider_contract.c)
lom_native_runtime_test(lom-remaster-screens-test tests/unit/remaster_screens_test.c)
lom_native_runtime_test(lom-remaster-movies-integration tests/integration/remaster_movies_integration.c)
lom_native_runtime_test(lom-crash-report-integration tests/integration/crash_report_integration.c)
lom_native_runtime_test(lom-field-cd-wait-component tests/component/field_cd_wait_component.c)
target_compile_definitions(lom-field-cd-wait-component PRIVATE lom_native_field_fail=__wrap_lom_native_field_fail)
target_link_options(lom-field-cd-wait-component PRIVATE -Wl,--wrap=lom_native_field_fail)
lom_native_runtime_test(lom-cd-resource-integration tests/integration/cd_resource_integration.c)
lom_native_runtime_test(lom-cd-callback-integration tests/integration/cd_callback_integration.c)
lom_native_runtime_test(lom-cd-audio-matrix-specification tests/behavior/cd_audio_matrix_specification.c WRAP CdMix CD_vol)

foreach(test IN ITEMS completion audio-output-edges)
    string(REPLACE "-" "_" file "${test}")
    set(target lom-movie-${test}-component)
    add_executable(${target} tests/component/movie_${file}_component.c src/core/game_loop.c ${LOM_GAME_CONTEXT_SOURCE}
        src/gpu/draw_labels.c)
    target_compile_definitions(${target} PRIVATE __psyz VERSION_US LOM_NATIVE=1 _POSIX_C_SOURCE=200809L)
    target_compile_options(${target} PRIVATE -ffunction-sections -fdata-sections)
    target_include_directories(${target} BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES}
        "${CMAKE_CURRENT_SOURCE_DIR}/src/movie" $<TARGET_PROPERTY:SDL3::SDL3,INTERFACE_INCLUDE_DIRECTORIES>)
    target_link_options(${target} PRIVATE ${LOM_LINK_GC_SECTIONS})
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endforeach()
target_sources(lom-movie-completion-component PRIVATE src/movie/movie_decoder.c ${LOM_MOVIE_PROCESS_SOURCE})
target_link_libraries(lom-movie-completion-component PRIVATE lom_switches)
if(WIN32)
    target_sources(lom-movie-completion-component PRIVATE tests/support/media/movie_test_win32.c)
endif()
target_link_options(lom-movie-completion-component PRIVATE
    -Wl,--wrap=lom_movie_decoder_close -Wl,--wrap=lom_movie_decoder_read_frame)

add_library(lom_movie_audio_output_edge_owner OBJECT src/movie/movie_audio_output.c)
target_compile_definitions(lom_movie_audio_output_edge_owner PRIVATE
    malloc=movie_test_malloc calloc=movie_test_calloc free=movie_test_free)
target_compile_options(lom_movie_audio_output_edge_owner PRIVATE -ffunction-sections -fdata-sections -Wno-unknown-attributes)
target_include_directories(lom_movie_audio_output_edge_owner PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/movie"
    $<TARGET_PROPERTY:SDL3::SDL3,INTERFACE_INCLUDE_DIRECTORIES>)
set_property(TARGET lom_movie_audio_output_edge_owner PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
target_sources(lom-movie-audio-output-edges-component PRIVATE
    src/movie/movie_audio.c src/movie/movie_audio_fifo.c $<TARGET_OBJECTS:lom_movie_audio_output_edge_owner>)
target_link_options(lom-movie-audio-output-edges-component PRIVATE
    -Wl,--wrap=lom_movie_audio_fifo_put -Wl,--wrap=lom_movie_audio_fifo_pull)

add_library(lom_movie_audio_voice_oracle OBJECT
    tests/support/oracles/movie_audio_original_voice_oracle.c
    "${LOM_DECOMP_DIR}/src/main/audio/akao_xa_stream.c")
target_compile_definitions(lom_movie_audio_voice_oracle PRIVATE ${LOM_NATIVE_GAME_DEFINITIONS})
target_compile_options(lom_movie_audio_voice_oracle PRIVATE ${LOM_NATIVE_GAME_OPTIONS})
target_include_directories(lom_movie_audio_voice_oracle BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES})
add_executable(lom-movie-audio-rematrix-test
    tests/unit/movie_audio_rematrix_test.c
    src/movie/movie_audio.c
    src/movie/movie_audio_fifo.c
    $<TARGET_OBJECTS:lom_movie_audio_voice_oracle>)
target_include_directories(lom-movie-audio-rematrix-test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/movie")
target_link_options(lom-movie-audio-rematrix-test PRIVATE ${LOM_LINK_GC_SECTIONS})
foreach(target IN ITEMS lom_movie_audio_voice_oracle lom-movie-audio-rematrix-test)
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endforeach()
if(WIN32)
    foreach(target IN ITEMS lom-movie-completion-component lom-movie-audio-output-edges-component lom-movie-audio-rematrix-test)
        target_link_options(${target} PRIVATE "LINKER:-Xlink=-force:unresolved")
    endforeach()
endif()

find_package(Threads REQUIRED)
lom_native_runtime_test(lom-spu-completion-component tests/component/spu_completion_component.c tests/support/sdk/spu_completion_fixture.c)
target_compile_definitions(lom-spu-completion-component PRIVATE _POSIX_C_SOURCE=200809L)
target_include_directories(lom-spu-completion-component BEFORE PRIVATE "${LOM_PSYZ_DIR}/psyz/include")
target_link_libraries(lom-spu-completion-component PRIVATE Threads::Threads)
target_link_options(lom-spu-completion-component PRIVATE
    -Wl,--wrap=SpuSetTransferCallback -Wl,--wrap=psyz_log -Wl,--wrap=ResetCallback
    -Wl,--wrap=DMACallback -Wl,--wrap=WaitEvent)

lom_native_runtime_test(lom-akao-mmio-retrigger-test tests/unit/akao_mmio_retrigger_test.c)
target_compile_definitions(lom-akao-mmio-retrigger-test PRIVATE _GNU_SOURCE)
target_include_directories(lom-akao-mmio-retrigger-test BEFORE PRIVATE "${LOM_PSYZ_DIR}/psyz/include")

lom_native_runtime_test(lom-akao-noise-opcodes-component tests/component/akao_noise_opcodes_component.c)
target_compile_definitions(lom-akao-noise-opcodes-component PRIVATE _POSIX_C_SOURCE=200809L)

lom_native_runtime_test(lom-akao-irq-component tests/component/akao_irq_component.c tests/support/audio/akao_sfx_volume_probe.c tests/support/audio/akao_category_probe.c)
target_compile_definitions(lom-akao-irq-component PRIVATE _POSIX_C_SOURCE=200809L)

lom_native_runtime_test(lom-akao-stream-component tests/component/akao_stream_component.c WRAP lom_native_akao_stream_fail)

lom_native_runtime_test(lom-card-integration tests/integration/card_integration.c)
target_compile_definitions(lom-card-integration PRIVATE _POSIX_C_SOURCE=200809L)

lom_native_runtime_test(lom-keyboard-controls-component tests/component/keyboard_controls_component.c tests/support/platform/lom_native_display.c WRAP SDL_HasKeyboard SDL_GetKeyboardState)
lom_native_runtime_test(lom-controller-multitap-slot-a-component tests/component/controller_multitap_slot_a_component.c)
lom_native_runtime_test(lom-controller-rumble-component tests/component/controller_rumble_component.c tests/support/platform/lom_native_display.c)
target_include_directories(lom-controller-rumble-component PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/platform")
target_link_options(lom-controller-rumble-component PRIVATE -Wl,--wrap=SDL_HasKeyboard -Wl,--wrap=lom_native_PadSetAct)
add_library(lom-controller-callback-fixture SHARED tests/support/platform/controller_callback_fixture.c)
add_library(lom_native_controller_only OBJECT "${LOM_DECOMP_DIR}/src/main/controller.c")
target_compile_definitions(lom_native_controller_only PRIVATE ${LOM_NATIVE_GAME_DEFINITIONS})
target_compile_options(lom_native_controller_only PRIVATE ${LOM_NATIVE_GAME_OPTIONS})
target_include_directories(lom_native_controller_only BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES})
add_executable(lom-controller-callback-component tests/component/controller_callback_component.c
    src/input/controller_psyq.c src/input/controller_rumble.c src/input/input_boundary.c src/core/boundary.c
    src/core/time_boundary.c src/core/game_loop.c ${LOM_GAME_CONTEXT_SOURCE} src/gpu/draw_labels.c
    $<TARGET_OBJECTS:lom_native_controller_only>)
target_compile_definitions(lom-controller-callback-component PRIVATE __psyz VERSION_US LOM_NATIVE=1)
target_compile_options(lom-controller-callback-component PRIVATE -ffunction-sections -fdata-sections)
target_include_directories(lom-controller-callback-component BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES})
target_link_libraries(lom-controller-callback-component PRIVATE lom_native_memory ${CMAKE_DL_LIBS})
target_link_options(lom-controller-callback-component PRIVATE ${LOM_LINK_GC_SECTIONS})
foreach(boundary IN ITEMS VSyncCallback PadInitDirect PadStartCom PadStopCom PadChkVsync PadGetState)
    target_link_options(lom-controller-callback-component PRIVATE "-Wl,--wrap=lom_native_${boundary}")
endforeach()
add_dependencies(lom-controller-callback-component lom-controller-callback-fixture)

lom_native_runtime_test(lom-field-services-component tests/component/field_services_component.c WRAP akao_upload_bank)

lom_native_runtime_test(lom-gte-tables-integration tests/integration/gte_tables_integration.c)
add_executable(lom-japanese-balance-component tests/component/japanese_balance_component.c)
target_compile_definitions(lom-japanese-balance-component PRIVATE VERSION_US)
target_link_libraries(lom-japanese-balance-component PRIVATE lom_switches)
add_executable(lom-gte-tables-jp-integration tests/integration/gte_tables_integration.c)
target_compile_definitions(lom-gte-tables-jp-integration PRIVATE __psyz VERSION_JP LOM_NATIVE=1)
target_link_libraries(lom-gte-tables-jp-integration PRIVATE lom_native_runtime_jp lom_native_port_jp)

lom_native_runtime_test(lom-field-lands-component tests/component/field_lands_component.c FIELD)

lom_native_runtime_test(lom-field-heap-component tests/component/field_heap_component.c FIELD WRAP cdrom_queue_read)

lom_native_runtime_test(lom-wmap-callback-tables-integration tests/integration/wmap_callback_tables_integration.c)

lom_native_runtime_test(lom-overlay-integration tests/integration/overlay_integration.c)

lom_native_runtime_test(lom-title-input-component tests/component/title_input_component.c)
target_include_directories(lom-title-input-component BEFORE PRIVATE "${LOM_DECOMP_DIR}/src")
target_link_options(lom-title-input-component PRIVATE -Wl,--wrap=akao_play_sfx)

lom_native_runtime_test(lom-field-golem-party-component tests/component/field_golem_party_component.c FIELD)

lom_native_runtime_test(lom-field-music-stream-component tests/component/field_music_stream_component.c)
target_compile_definitions(lom-field-music-stream-component PRIVATE _GNU_SOURCE)
target_link_options(lom-field-music-stream-component PRIVATE
    -Wl,--wrap=akao_play_song -Wl,--wrap=akao_spu_wait -Wl,--wrap=lom_native_field_fail)

lom_native_runtime_test(lom-field-actions-component tests/component/field_actions_component.c FIELD WRAP cdrom_queue_read cdrom_wait_queue_empty)

lom_native_runtime_test(lom-field-slots-component tests/component/field_slots_component.c FIELD)

lom_native_runtime_test(lom-field-collision-component tests/component/field_collision_component.c FIELD)

lom_native_runtime_test(lom-field-resources-component tests/component/field_resources_component.c FIELD WRAP LoadImage DrawSync)

lom_native_runtime_test(lom-field-party-component tests/component/field_party_component.c FIELD WRAP LoadImage cdrom_queue_read cdrom_wait_queue_empty)

lom_native_runtime_test(lom-field-actor-component tests/component/field_actor_component.c tests/support/oracles/field_idle_oracle.c FIELD WRAP lom_native_rand)

add_executable(lom-title-entry-integration
    tests/integration/title_entry_integration.c
    tests/support/platform/lom_native_gp0_hash.c
)
target_compile_definitions(lom-title-entry-integration PRIVATE __psyz VERSION_US)
target_include_directories(lom-title-entry-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/platform")
target_link_libraries(lom-title-entry-integration PRIVATE lom_native_runtime lom_native_port)

add_executable(lom-field-map-decode-integration tests/integration/field_map_decode_integration.c)
target_compile_definitions(lom-field-map-decode-integration PRIVATE __psyz VERSION_US)
target_link_libraries(lom-field-map-decode-integration PRIVATE lom_native_runtime lom_native_port)

foreach(version IN ITEMS us jp)
    string(TOUPPER "${version}" version_define)
    if(version STREQUAL "us")
        set(suffix "")
        set(tool_suffix "")
    else()
        set(suffix "_${version}")
        set(tool_suffix "-${version}")
    endif()

    add_executable(lom-save-layout${tool_suffix} tests/support/routes/save_layout.c)
    target_compile_definitions(lom-save-layout${tool_suffix} PRIVATE __psyz VERSION_${version_define})
    target_link_libraries(lom-save-layout${tool_suffix} PRIVATE lom_native_port${suffix})

    add_executable(lom-route-runner${tool_suffix}
        tests/support/routes/route_fight.c
        tests/support/routes/route_plan.c
        tests/support/routes/route_record.c
        tests/support/routes/route_record_cover.c
        tests/support/routes/route_record_gpu.c
        tests/support/routes/route_runner.c
        tests/support/routes/route_screens.c
        tests/support/routes/route_talk.c
        tests/support/routes/route_walk.c
        tests/support/routes/route_world_map.c
    )
    target_compile_definitions(lom-route-runner${tool_suffix} PRIVATE __psyz VERSION_${version_define})
    target_compile_options(lom-route-runner${tool_suffix} PRIVATE -fno-builtin -fno-strict-aliasing)
    target_include_directories(lom-route-runner${tool_suffix} BEFORE PRIVATE
        "${LOM_DECOMP_DIR}/src" "${LOM_DECOMP_DIR}/src/overlays/field")
    target_link_libraries(lom-route-runner${tool_suffix} PRIVATE lom_native_route_runtime${suffix} lom_native_port${suffix})
    target_link_options(lom-route-runner${tool_suffix} PRIVATE
        "-Wl,--wrap=lom_native_field_script_handler,--wrap=akao_play_song,--wrap=akao_switch_song"
        "-Wl,--wrap=Draw_PushPrim,--wrap=Draw_LoadImage,--wrap=Draw_ClearImage,--wrap=Draw_MoveImage,--wrap=Draw_StoreImage"
        "-Wl,--wrap=Draw_SetTexpageMode,--wrap=Draw_SetTextureWindow,--wrap=Draw_SetAreaStart"
        "-Wl,--wrap=Draw_SetAreaEnd,--wrap=Draw_SetOffset,--wrap=Draw_SetMask,--wrap=Psyz_SpuPullSamples"
        "-Wl,--wrap=field_start_interaction,--wrap=field_script_run,--wrap=cdrom_queue_seek,--wrap=lom_native_akao_start_song_channels")
    target_sources(lom-route-runner${tool_suffix} PRIVATE $<TARGET_OBJECTS:lom_native_test_clock>)
endforeach()

include(cmake/tests/routes.cmake)

add_executable(lom-mods-specification tests/behavior/mods_specification.cpp src/core/random_native.c src/core/portable.c)
target_include_directories(lom-mods-specification PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(lom-mods-specification PRIVATE lom_mods)
