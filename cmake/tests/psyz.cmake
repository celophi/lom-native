if(TARGET psyz)
    add_executable(lom-gpu-subtract-integration tests/integration/gpu_subtract_integration.c)
    target_link_libraries(lom-gpu-subtract-integration PRIVATE psyz)
endif()

if(TARGET lom_native_presentation)
    add_executable(lom-xbr-integration tests/integration/xbr_integration.c)
    target_include_directories(lom-xbr-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu")
    target_link_libraries(lom-xbr-integration PRIVATE lom_native_presentation)
endif()

if(TARGET psyz)
    add_executable(lom-gpu-polygon-limits-integration tests/integration/gpu_polygon_limits_integration.c)
    target_link_libraries(lom-gpu-polygon-limits-integration PRIVATE psyz)

    add_executable(lom-gpu-tpage-integration tests/integration/gpu_tpage_integration.c)
    target_link_libraries(lom-gpu-tpage-integration PRIVATE psyz)
endif()

if(TARGET psyz)
    add_executable(lom-gte-register-transfer-test tests/unit/gte_register_transfer_test.c)
    target_compile_features(lom-gte-register-transfer-test PRIVATE c_std_11)
    target_link_libraries(lom-gte-register-transfer-test PRIVATE psyz)
    add_executable(lom-gte-screen-offset-test tests/unit/gte_screen_offset_test.c)
    target_compile_features(lom-gte-screen-offset-test PRIVATE c_std_11)
    target_link_libraries(lom-gte-screen-offset-test PRIVATE psyz)
endif()

if(TARGET psyz)
    add_executable(lom-gte-color-alias-test tests/unit/gte_color_alias_test.c)
    target_compile_features(lom-gte-color-alias-test PRIVATE c_std_11)
    target_link_libraries(lom-gte-color-alias-test PRIVATE psyz)
    if(UNIX OR WIN32)
        add_executable(lom-root-counter-test tests/unit/root_counter_test.c)
        target_link_libraries(lom-root-counter-test PRIVATE psyz)
        target_link_options(lom-root-counter-test PRIVATE ${LOM_LINK_GC_SECTIONS})

        add_executable(lom-audio-clock-integration
            tests/integration/audio_clock_integration.c
        )
        target_compile_definitions(lom-audio-clock-integration PRIVATE _POSIX_C_SOURCE=200809L)
        target_include_directories(lom-audio-clock-integration PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-audio-clock-integration PRIVATE psyz)
        if(WIN32)
            target_link_libraries(lom-audio-clock-integration PRIVATE Threads::Threads)
        endif()

        add_executable(lom-audio-categories-specification tests/behavior/audio_categories_specification.c)
        target_include_directories(lom-audio-categories-specification PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" ${LOM_PSYZ_DIR}/psyz/include)
        target_link_libraries(lom-audio-categories-specification PRIVATE psyz)
        target_link_options(lom-audio-categories-specification PRIVATE -Wl,--wrap=Psyz_CdPullSamples)

        add_executable(lom-spu-live-adsr-test
            tests/unit/spu_live_adsr_test.c
        )
        target_include_directories(lom-spu-live-adsr-test PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-live-adsr-test PRIVATE psyz)

        add_executable(lom-spu-voice-end-test
            tests/unit/spu_voice_end_test.c
        )
        target_include_directories(lom-spu-voice-end-test PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-voice-end-test PRIVATE psyz)

        add_executable(lom-spu-noise-pmon-test
            tests/unit/spu_noise_pmon_test.c
        )
        target_include_directories(lom-spu-noise-pmon-test PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-noise-pmon-test PRIVATE psyz)

        add_executable(lom-spu-counter-interleave-test
            tests/unit/spu_counter_interleave_test.c
        )
        target_include_directories(lom-spu-counter-interleave-test PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-counter-interleave-test PRIVATE psyz)
    endif()
endif()

if(TARGET psyz AND PSYZ_RENDERER STREQUAL "sdl3_gpu" AND (UNIX OR WIN32))
    add_executable(lom-settings-store-specification tests/behavior/settings_store_specification.cpp)
    target_include_directories(lom-settings-store-specification PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-settings-store-specification PRIVATE psyz)
    target_link_options(lom-settings-store-specification PRIVATE -Wl,--wrap=SDL_RenamePath)

    add_executable(lom-settings-runtime-specification tests/behavior/settings_runtime_specification.cpp)
    target_include_directories(lom-settings-runtime-specification PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-settings-runtime-specification PRIVATE psyz)
    target_link_options(lom-settings-runtime-specification PRIVATE -Wl,--wrap=SDL_GetKeyboardState)

    add_executable(lom-menu-model-specification tests/behavior/menu_model_specification.cpp)
    target_link_libraries(lom-menu-model-specification PRIVATE psyz)

    add_executable(lom-menu-text-test tests/unit/menu_text_test.cpp)
    target_include_directories(lom-menu-text-test PRIVATE "${LOM_IMGUI_DIR}")
    target_link_libraries(lom-menu-text-test PRIVATE psyz)

    add_executable(lom-menu-glyphs-test tests/unit/menu_glyphs_test.cpp)
    target_link_libraries(lom-menu-glyphs-test PRIVATE psyz)

    add_executable(lom-menu-quit-integration tests/integration/menu_quit_integration.c)
    target_include_directories(lom-menu-quit-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-menu-quit-integration PRIVATE psyz)

    add_executable(lom-menu-render-integration tests/integration/menu_render_integration.cpp)
    target_include_directories(lom-menu-render-integration PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu")
    target_link_libraries(lom-menu-render-integration PRIVATE psyz lom_native_presentation)
endif()
