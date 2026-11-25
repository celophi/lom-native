foreach(suffix IN ITEMS "" _jp)
    set(units_dir "${LOM_NATIVE_UNITS_DIR${suffix}}")
    set(unit_objects ${LOM_NATIVE_UNIT_OBJECTS${suffix}})
    set(LOM_NATIVE_STUBS${suffix} "${units_dir}/native_stubs.c")
    list(LENGTH unit_objects unit_count)
    add_custom_command(OUTPUT "${LOM_NATIVE_STUBS${suffix}}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/native_stubs.py" "${LOM_NATIVE_STUBS${suffix}}"
            ${unit_objects} $<TARGET_FILE:lom_native_port${suffix}> $<TARGET_FILE:lom_mods> $<TARGET_FILE:lom_switches> $<TARGET_FILE:lom_native_memory> $<TARGET_FILE:psyz>
            --units ${unit_count} --nm "${CMAKE_NM}" --data "${units_dir}/data_names.txt"
        DEPENDS ${unit_objects} lom_native_port${suffix} lom_mods lom_switches lom_native_memory psyz "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/native_stubs.py"
        COMMENT "Listing what isn't native yet${suffix}"
        VERBATIM)
endforeach()
file(GLOB_RECURSE LOM_SEPARATION_INPUTS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/game/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/game/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/game/include/*.h")
set(LOM_SEPARATION_STAMP "${CMAKE_CURRENT_BINARY_DIR}/separation_check.stamp")
add_custom_command(OUTPUT "${LOM_SEPARATION_STAMP}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/separation_check.py"
    COMMAND ${CMAKE_COMMAND} -E touch "${LOM_SEPARATION_STAMP}"
    DEPENDS ${LOM_SEPARATION_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/separation_check.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/separation_allow.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/unit_globals.json"
    COMMENT "Checking the game reaches the platform only through its boundaries"
    VERBATIM)
add_custom_target(lom_separation_check ALL DEPENDS "${LOM_SEPARATION_STAMP}")
file(GLOB LOM_SHADER_SOURCE_INPUTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/gpu/shaders/*")
set(LOM_SHADER_SOURCES_STAMP "${CMAKE_CURRENT_BINARY_DIR}/shader_sources_check.stamp")
add_custom_command(OUTPUT "${LOM_SHADER_SOURCES_STAMP}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/check_shader_sources.py"
    COMMAND ${CMAKE_COMMAND} -E touch "${LOM_SHADER_SOURCES_STAMP}"
    DEPENDS ${LOM_SHADER_SOURCE_INPUTS} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/check_shader_sources.py"
    COMMENT "Checking the shader headers were built from the shaders as they are"
    VERBATIM)
add_custom_target(lom_shader_sources_check ALL DEPENDS "${LOM_SHADER_SOURCES_STAMP}")
foreach(version IN ITEMS us jp)
    if(version STREQUAL "us")
        set(suffix "")
        set(checked_in "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/unit_globals_layout.txt")
    else()
        set(suffix "_${version}")
        set(checked_in "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/unit_globals_layout_${version}.txt")
    endif()
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/unit_layout_check${suffix}.stamp")
    add_custom_command(OUTPUT "${stamp}"
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/check_unit_layout.py" "${LOM_NATIVE_UNITS_DIR${suffix}}/layout.txt"
            --version ${version}
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS ${LOM_NATIVE_UNIT_GLOBALS${suffix}} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/check_unit_layout.py" "${checked_in}"
        COMMENT "Checking the game's globals are laid out as the checked-in layout says (${version})"
        VERBATIM)
    add_custom_target(lom_unit_layout_check${suffix} ALL DEPENDS "${stamp}")
    add_dependencies(lom_unit_layout_check${suffix} lom_native_unit_globals${suffix})
endforeach()
