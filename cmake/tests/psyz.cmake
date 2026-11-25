if(TARGET psyz)
    add_executable(lom-gpu-subtract-smoke tests/smoke/gpu/gpu_subtract_smoke.c)
    target_link_libraries(lom-gpu-subtract-smoke PRIVATE psyz)
endif()

if(TARGET lom_native_presentation)
    add_executable(lom-xbr-smoke tests/smoke/gpu/xbr_smoke.c)
    target_include_directories(lom-xbr-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu")
    target_link_libraries(lom-xbr-smoke PRIVATE lom_native_presentation)
endif()

if(TARGET psyz)
    add_executable(lom-gpu-polygon-limits-smoke tests/smoke/gpu/gpu_polygon_limits_smoke.c)
    target_link_libraries(lom-gpu-polygon-limits-smoke PRIVATE psyz)

    add_executable(lom-gpu-tpage-smoke tests/smoke/gpu/gpu_tpage_smoke.c)
    target_link_libraries(lom-gpu-tpage-smoke PRIVATE psyz)
endif()

if(TARGET psyz)
    add_executable(lom-gte-register-transfer-smoke tests/smoke/gpu/gte_register_transfer_smoke.c)
    target_compile_features(lom-gte-register-transfer-smoke PRIVATE c_std_11)
    target_link_libraries(lom-gte-register-transfer-smoke PRIVATE psyz)
    add_executable(lom-gte-screen-offset-smoke tests/smoke/gpu/gte_screen_offset_smoke.c)
    target_compile_features(lom-gte-screen-offset-smoke PRIVATE c_std_11)
    target_link_libraries(lom-gte-screen-offset-smoke PRIVATE psyz)
endif()

if(TARGET psyz)
    add_executable(lom-gte-color-alias-smoke tests/smoke/gpu/gte_color_alias_smoke.c)
    target_compile_features(lom-gte-color-alias-smoke PRIVATE c_std_11)
    target_link_libraries(lom-gte-color-alias-smoke PRIVATE psyz)
    if(UNIX OR WIN32)
        add_executable(lom-root-counter-smoke tests/smoke/audio/root_counter_smoke.c)
        target_link_libraries(lom-root-counter-smoke PRIVATE psyz)
        target_link_options(lom-root-counter-smoke PRIVATE ${LOM_LINK_GC_SECTIONS})

        add_executable(lom-audio-clock-smoke
            tests/smoke/audio/audio_clock_smoke.c
        )
        target_compile_definitions(lom-audio-clock-smoke PRIVATE _POSIX_C_SOURCE=200809L)
        target_include_directories(lom-audio-clock-smoke PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-audio-clock-smoke PRIVATE psyz)
        if(WIN32)
            target_link_libraries(lom-audio-clock-smoke PRIVATE Threads::Threads)
        endif()

        add_executable(lom-audio-categories-smoke tests/smoke/audio/audio_categories_smoke.c)
        target_include_directories(lom-audio-categories-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" ${LOM_PSYZ_DIR}/psyz/include)
        target_link_libraries(lom-audio-categories-smoke PRIVATE psyz)
        target_link_options(lom-audio-categories-smoke PRIVATE -Wl,--wrap=Psyz_CdPullSamples)

        add_executable(lom-spu-live-adsr-smoke
            tests/smoke/audio/spu_live_adsr_smoke.c
        )
        target_include_directories(lom-spu-live-adsr-smoke PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-live-adsr-smoke PRIVATE psyz)

        add_executable(lom-spu-voice-end-smoke
            tests/smoke/audio/spu_voice_end_smoke.c
        )
        target_include_directories(lom-spu-voice-end-smoke PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-voice-end-smoke PRIVATE psyz)

        add_executable(lom-spu-noise-pmon-smoke
            tests/smoke/audio/spu_noise_pmon_smoke.c
        )
        target_include_directories(lom-spu-noise-pmon-smoke PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-noise-pmon-smoke PRIVATE psyz)

        add_executable(lom-spu-counter-interleave-smoke
            tests/smoke/audio/spu_counter_interleave_smoke.c
        )
        target_include_directories(lom-spu-counter-interleave-smoke PRIVATE
            ${LOM_PSYZ_DIR}/psyz/include
        )
        target_link_libraries(lom-spu-counter-interleave-smoke PRIVATE psyz)
    endif()
endif()

if(TARGET psyz AND PSYZ_RENDERER STREQUAL "sdl3_gpu" AND (UNIX OR WIN32))
    add_executable(lom-settings-store-smoke tests/smoke/platform/settings_store_smoke.cpp)
    target_include_directories(lom-settings-store-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-settings-store-smoke PRIVATE psyz)
    target_link_options(lom-settings-store-smoke PRIVATE -Wl,--wrap=SDL_RenamePath)

    add_executable(lom-settings-runtime-smoke tests/smoke/platform/settings_runtime_smoke.cpp)
    target_include_directories(lom-settings-runtime-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-settings-runtime-smoke PRIVATE psyz)
    target_link_options(lom-settings-runtime-smoke PRIVATE -Wl,--wrap=SDL_GetKeyboardState)

    add_executable(lom-menu-model-smoke tests/smoke/platform/menu_model_smoke.cpp)
    target_link_libraries(lom-menu-model-smoke PRIVATE psyz)

    add_executable(lom-menu-text-smoke tests/smoke/platform/menu_text_smoke.cpp)
    target_include_directories(lom-menu-text-smoke PRIVATE "${LOM_IMGUI_DIR}")
    target_link_libraries(lom-menu-text-smoke PRIVATE psyz)

    add_executable(lom-menu-glyphs-smoke tests/smoke/platform/menu_glyphs_smoke.cpp)
    target_link_libraries(lom-menu-glyphs-smoke PRIVATE psyz)

    add_executable(lom-menu-quit-smoke tests/smoke/platform/menu_quit_smoke.c)
    target_include_directories(lom-menu-quit-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(lom-menu-quit-smoke PRIVATE psyz)

    add_executable(lom-menu-render-smoke tests/smoke/platform/menu_render_smoke.cpp)
    target_include_directories(lom-menu-render-smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu")
    target_link_libraries(lom-menu-render-smoke PRIVATE psyz lom_native_presentation)
endif()
