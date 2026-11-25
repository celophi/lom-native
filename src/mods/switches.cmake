if(NOT TARGET lom_switches)
    add_library(lom_switches STATIC "${CMAKE_CURRENT_LIST_DIR}/switches.c" "${CMAKE_CURRENT_LIST_DIR}/mod_toggles.c")
    target_include_directories(lom_switches PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../include")
endif()
