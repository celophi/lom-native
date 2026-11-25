set(LOM_IMGUI_DIR "${LOM_PSYZ_DIR}/external/cimgui/imgui")

target_compile_definitions(psyz PRIVATE LOM_NATIVE_MENU=1)
target_include_directories(psyz PRIVATE "${LOM_IMGUI_DIR}" "${LOM_IMGUI_DIR}/backends")
target_sources(psyz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_backend.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_icons.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_input.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_runtime.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_model.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_glyphs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/menu_text.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/settings/preference_store.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/settings/service.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/host_controls.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/binding_policy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/input/binding_capture.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/host_audio.cpp"
    "${LOM_IMGUI_DIR}/imgui.cpp"
    "${LOM_IMGUI_DIR}/imgui_draw.cpp"
    "${LOM_IMGUI_DIR}/imgui_tables.cpp"
    "${LOM_IMGUI_DIR}/imgui_widgets.cpp"
    "${LOM_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${LOM_IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp")

file(GLOB LOM_MENU_TRANSLATIONS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/translations/*.json")
set(LOM_MENU_TRANSLATIONS_CPP "${CMAKE_CURRENT_BINARY_DIR}/generated/menu_translations.cpp")
execute_process(COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/embed_menu_translations.py"
    "${LOM_MENU_TRANSLATIONS_CPP}" ${LOM_MENU_TRANSLATIONS} RESULT_VARIABLE LOM_MENU_TRANSLATIONS_RESULT)
if(NOT LOM_MENU_TRANSLATIONS_RESULT EQUAL 0)
    message(FATAL_ERROR "Couldn't compile the in-game menu's translations. The error is above.")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/embed_menu_translations.py" ${LOM_MENU_TRANSLATIONS})
target_sources(psyz PRIVATE "${LOM_MENU_TRANSLATIONS_CPP}")
set_source_files_properties("${LOM_MENU_TRANSLATIONS_CPP}" TARGET_DIRECTORY psyz PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/ui")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake")
set(LOM_MENU_FONTS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/menu_fonts")
file(MAKE_DIRECTORY "${LOM_MENU_FONTS_DIR}")
file(GLOB LOM_MENU_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/fonts/*.ttf" "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/fonts/*.otf")
foreach(font IN LISTS LOM_MENU_FONTS)
    get_filename_component(name "${font}" NAME_WE)
    string(TOLOWER "${name}" symbol)
    string(REPLACE "-" "_" symbol "lom_font_${symbol}")
    lom_embed_file("${font}" "${LOM_MENU_FONTS_DIR}/${name}.c" "${symbol}")
    target_sources(psyz PRIVATE "${LOM_MENU_FONTS_DIR}/${name}.c")
endforeach()
