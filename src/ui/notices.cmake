if(NOT TARGET lom_notices)
    add_library(lom_notices STATIC "${CMAKE_CURRENT_LIST_DIR}/notices.c")
    target_include_directories(lom_notices PUBLIC "${CMAKE_CURRENT_LIST_DIR}/../../include")
endif()
