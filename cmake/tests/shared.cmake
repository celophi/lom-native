add_library(lom_psyz_bootstrap STATIC
    ${LOM_PSYZ_DIR}/psyz/src/psyz/libgte.c
    ${LOM_PSYZ_DIR}/psyz/src/psyz/libgte_sw_sqrt.c
    ${LOM_PSYZ_DIR}/decomp/src/libgte/sqrtbl.c
    ${LOM_PSYZ_DIR}/decomp/src/libgte/msc02.c
    tests/support/platform/psyz_log_stub.c
)
target_compile_definitions(lom_psyz_bootstrap PUBLIC __psyz)
target_include_directories(lom_psyz_bootstrap PUBLIC
    ${LOM_PSYZ_DIR}/psyz/include
)
target_link_libraries(lom_psyz_bootstrap PUBLIC m)

add_executable(lom-psyz-smoke
    tests/smoke/platform/psyz_smoke.c
)
target_link_libraries(lom-psyz-smoke PRIVATE lom_psyz_bootstrap)

if(TARGET psyz)
    target_compile_definitions(lom-psyz-smoke PRIVATE LOM_NATIVE_GTE_LEADING_BITS=1)
    get_target_property(gte_sources lom_psyz_bootstrap SOURCES)
    list(FILTER gte_sources EXCLUDE REGEX "src/psyz/libgte\\.c$")
    set_property(TARGET lom_psyz_bootstrap PROPERTY SOURCES "${gte_sources}")
    target_sources(lom_psyz_bootstrap PRIVATE "${LOM_PATCHED_PSYZ_DIR}/libgte.c")
    target_include_directories(lom_psyz_bootstrap PRIVATE "${LOM_PSYZ_DIR}/psyz/src/psyz")
endif()
add_library(lom_decomp_resident OBJECT ${LOM_DECOMP_DIR}/src/main/card_callbacks.c)
target_include_directories(lom_decomp_resident PRIVATE ${LOM_DECOMP_DIR}/include)
add_executable(lom-upstream-smoke tests/smoke/platform/upstream_smoke.c $<TARGET_OBJECTS:lom_decomp_resident>)
target_include_directories(lom-upstream-smoke PRIVATE ${LOM_DECOMP_DIR}/include)

add_executable(lom-random-smoke tests/smoke/platform/random_smoke.c src/core/random_native.c)
target_include_directories(lom-random-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(UNIX OR WIN32)
    add_executable(lom-movie-decoder-smoke
        tests/smoke/media/movie_decoder_smoke.c
        src/movie/movie_decoder.c
        ${LOM_MOVIE_PROCESS_SOURCE})
    target_include_directories(lom-movie-decoder-smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/movie)
    if(WIN32)
        target_sources(lom-movie-decoder-smoke PRIVATE tests/support/media/movie_test_win32.c)
    endif()

    add_executable(lom-portable-smoke
        tests/smoke/platform/portable_smoke.c
        src/core/portable.c)
    target_include_directories(lom-portable-smoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_executable(lom-movie-audio-output-sdl-smoke
        tests/smoke/media/movie_audio_output_sdl_smoke.c
        src/movie/movie_audio.c
        src/movie/movie_audio_fifo.c
        src/movie/movie_audio_output.c)
    target_include_directories(lom-movie-audio-output-sdl-smoke PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/movie)
    target_link_libraries(lom-movie-audio-output-sdl-smoke PRIVATE SDL3::SDL3)
    target_link_options(lom-movie-audio-output-sdl-smoke PRIVATE
        -Wl,--wrap=SDL_SetAudioStreamGetCallback -Wl,--wrap=SDL_PutAudioStreamData
        -Wl,--wrap=SDL_DestroyAudioStream)
    set_property(TARGET lom-movie-audio-output-sdl-smoke PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
