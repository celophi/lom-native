if(NOT TARGET lom_common)
    add_library(lom_common STATIC
        "${CMAKE_CURRENT_LIST_DIR}/src/common/hex_text.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/common/json.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/common/sha256.cpp")
    target_compile_features(lom_common PUBLIC cxx_std_17)
    target_include_directories(lom_common
        PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include"
        PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src")
endif()
