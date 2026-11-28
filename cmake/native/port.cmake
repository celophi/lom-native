if(WIN32)
    set(LOM_MOVIE_PROCESS_SOURCE src/movie/movie_process_windows.c)
    set(LOM_CARD_SLOT_SOURCE src/card/card_slot_windows.c)
    set(LOM_GAME_CONTEXT_SOURCE src/core/game_context_windows.c)
else()
    set(LOM_MOVIE_PROCESS_SOURCE src/movie/movie_process_posix.c)
    set(LOM_CARD_SLOT_SOURCE src/card/card_slot_posix.c)
    set(LOM_GAME_CONTEXT_SOURCE src/core/game_context_posix.c)
endif()

set(LOM_NATIVE_PORT_SOURCES
    src/audio/akao_host.c
    src/audio/akao_stream_native.c
    src/audio/sound_boundary.c
    src/audio/spu_shadow.c
    src/card/card_font.c
    src/card/card_libapi.c
    src/card/card_native.c
    ${LOM_CARD_SLOT_SOURCE}
    src/card/save_native.c
    src/cd/cd_audio_native.c
    src/cd/cdrom_native.c
    src/core/boundary.c
    src/core/diagnostics.c
    src/core/game_loop.c
    ${LOM_GAME_CONTEXT_SOURCE}
    src/core/lom_game.c
    src/core/overlay_native.c
    src/core/overlay_units.c
    src/core/portable.c
    src/core/random_native.c
    src/core/resident_native.c
    src/core/session.c
    src/core/session_folder.c
    src/core/time_boundary.c
    src/core/unit_decode.c
    src/field/field_gameplay.c
    src/field/field_japanese_balance.c
    src/field/field_quick_rooms.c
    src/field/field_map_decode.c
    src/field/field_music_stream_native.c
    src/field/field_native.c
    src/field/field_native_services.c
    src/field/field_script_deferred.c
    src/field/field_widescreen.c
    src/gpu/draw_calls.c
    src/gpu/draw_labels.c
    src/gpu/gpu_init.c
    src/gpu/graphics_boundary.c
    src/input/controller_psyq.c
    src/input/controller_rumble.c
    src/input/input_boundary.c
    src/movie/field_movie_native.c
    src/movie/movie_audio.c
    src/movie/movie_audio_fifo.c
    src/movie/movie_audio_output.c
    src/movie/movie_decoder.c
    ${LOM_MOVIE_PROCESS_SOURCE}
    src/movie/movie_native.c
    src/movie/movie_source.c
    src/overlays/gname_entry.c
    src/overlays/gname_native.c
    src/overlays/gover_native.c
    src/overlays/niki_native.c
    src/overlays/shop_native.c
    src/overlays/title_entry.c
    src/overlays/title_hooks.c
    src/overlays/title_native.c
    src/overlays/wmap_native.c
    src/overlays/wsel_native.c
    src/remaster/inflate.c
    src/remaster/psb.c
    src/remaster/remaster.c
    src/remaster/remaster_effects.c
    src/remaster/remaster_music.c
    src/remaster/remaster_menu.c
    src/remaster/remaster_cards.c
    src/remaster/remaster_movies.c
    src/remaster/remaster_screens.c
    src/remaster/remaster_text.c
    src/remaster/remaster_text_encyclopedia.c
    src/remaster/remaster_text_glyphs.c
    src/remaster/remaster_text_items.c
    src/remaster/remaster_text_markup.c
    src/remaster/remaster_text_scenes.c
    src/remaster/remaster_text_tables.c
    src/remaster/remaster_ui.c)
include(cmake/native/card_fonts.cmake)
if(WIN32)
    list(APPEND LOM_NATIVE_PORT_SOURCES src/core/win32_libc.c src/core/diagnostics_windows.c)
    find_package(Threads REQUIRED)
else()
    list(APPEND LOM_NATIVE_PORT_SOURCES src/core/diagnostics_posix.c)
endif()

foreach(version IN ITEMS us jp)
    string(TOUPPER "${version}" version_define)
    set(target lom_native_port)
    if(NOT version STREQUAL "us")
        set(target lom_native_port_${version})
    endif()
    add_library(${target} STATIC ${LOM_NATIVE_PORT_SOURCES} $<TARGET_OBJECTS:lom_native_card_fonts>)
    if(WIN32)
        target_link_libraries(${target} PUBLIC Threads::Threads dbghelp)
    else()
        target_link_libraries(${target} PUBLIC ${CMAKE_DL_LIBS})
    endif()
    target_compile_definitions(${target} PRIVATE __psyz VERSION_${version_define} LOM_NATIVE=1 _GNU_SOURCE)
    target_compile_options(${target} PRIVATE -fms-extensions "SHELL:-idirafter ${LOM_DECOMP_DIR}/include/sdk"
        -Werror=implicit-function-declaration -Werror=int-conversion -Werror=incompatible-pointer-types)
    target_include_directories(${target} BEFORE PUBLIC ${LOM_NATIVE_GAME_INCLUDES})
    target_link_libraries(${target} PUBLIC lom_native_memory psyz)
endforeach()
