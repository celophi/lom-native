include(src/mods/manifest.cmake)

add_library(lom_mods STATIC src/mods/mods.cpp src/mods/mod_scripts.cpp)
target_include_directories(lom_mods PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
include(src/ui/notices.cmake)
target_link_libraries(lom_mods PUBLIC lom_mod_manifest lom_notices lom_lua lom_common)
