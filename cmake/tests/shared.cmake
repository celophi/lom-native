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

add_executable(lom-psyz-test
    tests/unit/psyz_test.c
)
target_link_libraries(lom-psyz-test PRIVATE lom_psyz_bootstrap)

if(TARGET psyz)
    target_compile_definitions(lom-psyz-test PRIVATE LOM_NATIVE_GTE_LEADING_BITS=1)
    get_target_property(gte_sources lom_psyz_bootstrap SOURCES)
    list(FILTER gte_sources EXCLUDE REGEX "src/psyz/libgte\\.c$")
    set_property(TARGET lom_psyz_bootstrap PROPERTY SOURCES "${gte_sources}")
    target_sources(lom_psyz_bootstrap PRIVATE "${LOM_PATCHED_PSYZ_DIR}/libgte.c")
    target_include_directories(lom_psyz_bootstrap PRIVATE "${LOM_PSYZ_DIR}/psyz/src/psyz")
endif()
add_library(lom_decomp_resident OBJECT ${LOM_DECOMP_DIR}/src/main/card_callbacks.c)
target_include_directories(lom_decomp_resident PRIVATE ${LOM_DECOMP_DIR}/include)
add_executable(lom-upstream-test tests/unit/upstream_test.c $<TARGET_OBJECTS:lom_decomp_resident>)
target_include_directories(lom-upstream-test PRIVATE ${LOM_DECOMP_DIR}/include)

add_executable(lom-random-test tests/unit/random_test.c src/core/random_native.c)
target_include_directories(lom-random-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(UNIX OR WIN32)
    add_executable(lom-movie-decoder-integration
        tests/integration/movie_decoder_integration.c
        src/movie/movie_decoder.c
        ${LOM_MOVIE_PROCESS_SOURCE})
    target_include_directories(lom-movie-decoder-integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/movie)
    if(WIN32)
        target_sources(lom-movie-decoder-integration PRIVATE tests/support/media/movie_test_win32.c)
    endif()

    add_executable(lom-portable-integration
        tests/integration/portable_integration.c
        src/core/portable.c)
    target_include_directories(lom-portable-integration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_executable(lom-movie-audio-output-sdl-specification
        tests/behavior/movie_audio_output_sdl_specification.c
        src/movie/movie_audio.c
        src/movie/movie_audio_fifo.c
        src/movie/movie_audio_output.c)
    target_include_directories(lom-movie-audio-output-sdl-specification PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src/movie)
    target_link_libraries(lom-movie-audio-output-sdl-specification PRIVATE SDL3::SDL3)
    target_link_options(lom-movie-audio-output-sdl-specification PRIVATE
        -Wl,--wrap=SDL_SetAudioStreamGetCallback -Wl,--wrap=SDL_PutAudioStreamData
        -Wl,--wrap=SDL_DestroyAudioStream)
    set_property(TARGET lom-movie-audio-output-sdl-specification PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
