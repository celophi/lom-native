include("${CMAKE_CURRENT_LIST_DIR}/switches.cmake")

if(NOT TARGET lom_mod_manifest)
    add_library(lom_mod_manifest STATIC "${CMAKE_CURRENT_LIST_DIR}/mod_manifest.cpp" "${CMAKE_CURRENT_LIST_DIR}/mod_files_provider.cpp")
    target_compile_features(lom_mod_manifest PUBLIC cxx_std_17)
    target_include_directories(lom_mod_manifest PUBLIC "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../include")
    target_link_libraries(lom_mod_manifest PUBLIC lom_common lom_switches)
endif()
