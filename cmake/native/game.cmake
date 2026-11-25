set(LOM_NATIVE_GAME_UNITS slus title gname wsel field wmap addhero carda cload golem gosub gover menu niki shop zukan)

set(LOM_NATIVE_UNIT_GENERATOR "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/generate_unit_globals.py")
set(LOM_NATIVE_UNIT_SETTINGS "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/unit_globals.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LOM_NATIVE_UNIT_SETTINGS}")
file(READ "${LOM_NATIVE_UNIT_SETTINGS}" LOM_NATIVE_UNIT_SETTINGS_JSON)

set(LOM_NATIVE_GAME_DEFINITIONS
    __psyz
    LOM_NATIVE_GAME_CODE
    rand=lom_native_rand
    srand=lom_native_srand
    csin=rsin
    ccos=rcos
)
set(LOM_NATIVE_GAME_OPTIONS
    -std=gnu11
    -fms-extensions
    -fgnu89-inline
    -fwrapv
    -fno-strict-aliasing
    -fno-delete-null-pointer-checks
    -fno-builtin
    -ffunction-sections
    -fdata-sections
    -Werror=return-mismatch
    -Werror=return-type
    -Wno-deprecated-non-prototype
    -Wno-macro-redefined
    -Werror=implicit-function-declaration
    -Werror=int-conversion
    -Werror=incompatible-pointer-types
    -Werror=pointer-to-int-cast
    -Werror=int-to-pointer-cast
    "SHELL:-idirafter ${LOM_DECOMP_DIR}/include/sdk"
)
set(LOM_NATIVE_GAME_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/native_sdk"
    "${LOM_DECOMP_DIR}/src"
    "${LOM_DECOMP_DIR}/include"
    "${LOM_PSYZ_DIR}/psyz/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/decomp_assets"
)

set(LOM_NATIVE_ROUTE_WRAPS lom_native_field_script_handler akao_play_song akao_switch_song
    field_start_interaction field_script_run cdrom_queue_seek lom_native_akao_start_song_channels)
if(WIN32)
    get_filename_component(LOM_MSYS2_ROOT "${CMAKE_C_COMPILER}/../../.." ABSOLUTE)
    find_program(LOM_UNIT_LD NAMES ld.bfd PATHS "${LOM_MSYS2_ROOT}/mingw64/bin" NO_DEFAULT_PATH)
    find_program(LOM_UNIT_OBJCOPY NAMES objcopy PATHS "${LOM_MSYS2_ROOT}/mingw64/bin" NO_DEFAULT_PATH)
    if(NOT LOM_UNIT_LD OR NOT LOM_UNIT_OBJCOPY)
        message(FATAL_ERROR "The game's units need GNU ld and objcopy for Windows objects. "
            "In MSYS2: pacman -S mingw-w64-x86_64-binutils, or set LOM_UNIT_LD and LOM_UNIT_OBJCOPY.")
    endif()
else()
    set(LOM_UNIT_LD "${CMAKE_LINKER}")
    set(LOM_UNIT_OBJCOPY "${CMAKE_OBJCOPY}")
endif()


function(lom_native_add_game version)
    string(TOUPPER "${version}" version_define)
    if(version STREQUAL "us")
        set(suffix "")
        set(LOM_NATIVE_UNITS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/units")
    else()
        set(suffix "_${version}")
        set(LOM_NATIVE_UNITS_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/units-${version}")
    endif()
    set(available_targets)
    set(available_args)
    if(WIN32)
        set(available_targets lom_native_port${suffix} lom_native_memory psyz)
        foreach(target IN LISTS available_targets)
            list(APPEND available_args --available "$<TARGET_FILE:${target}>")
        endforeach()
    endif()

    set(LOM_NATIVE_UNIT_GLOBALS)
    foreach(unit IN LISTS LOM_NATIVE_GAME_UNITS)
        list(APPEND LOM_NATIVE_UNIT_GLOBALS "${LOM_NATIVE_UNITS_DIR}/${unit}_globals.c")
    endforeach()
    list(JOIN LOM_NATIVE_GAME_UNITS "," LOM_NATIVE_GAME_UNIT_LIST)
    add_custom_command(OUTPUT ${LOM_NATIVE_UNIT_GLOBALS}
        COMMAND ${Python3_EXECUTABLE} "${LOM_NATIVE_UNIT_GENERATOR}" "${LOM_DECOMP_DIR}" "${LOM_NATIVE_UNITS_DIR}" --version ${version}
            --units ${LOM_NATIVE_GAME_UNIT_LIST} --depfile "${LOM_NATIVE_UNITS_DIR}/globals.d"
        BYPRODUCTS "${LOM_NATIVE_UNITS_DIR}/layout.txt"
        DEPFILE "${LOM_NATIVE_UNITS_DIR}/globals.d"
        DEPENDS "${LOM_NATIVE_UNIT_GENERATOR}" "${LOM_NATIVE_UNIT_SETTINGS}"
        COMMENT "Generating the units' globals (${version})"
        VERBATIM)
    add_custom_target(lom_native_unit_globals${suffix} DEPENDS ${LOM_NATIVE_UNIT_GLOBALS})

    set(LOM_NATIVE_UNIT_LINK_ARGS)
    set(LOM_NATIVE_UNIT_OBJECTS)
    set(LOM_NATIVE_UNIT_DEPENDS)
    foreach(unit IN LISTS LOM_NATIVE_GAME_UNITS)
        execute_process(
            COMMAND ${Python3_EXECUTABLE} "${LOM_NATIVE_UNIT_GENERATOR}" "${LOM_DECOMP_DIR}" - --list-sources ${unit}
            OUTPUT_VARIABLE unit_sources OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE unit_result)
        if(NOT unit_result EQUAL 0)
            message(FATAL_ERROR "Couldn't list ${unit}'s sources. The error is above.")
        endif()
        set(globals "${LOM_NATIVE_UNITS_DIR}/${unit}_globals.c")

        add_library(lom_unit${suffix}_${unit} OBJECT ${unit_sources} "${globals}")
        target_compile_definitions(lom_unit${suffix}_${unit} PRIVATE ${LOM_NATIVE_GAME_DEFINITIONS} VERSION_${version_define})
        target_compile_options(lom_unit${suffix}_${unit} PRIVATE ${LOM_NATIVE_GAME_OPTIONS})
        target_include_directories(lom_unit${suffix}_${unit} BEFORE PRIVATE ${LOM_NATIVE_GAME_INCLUDES})
        add_dependencies(lom_unit${suffix}_${unit} lom_native_unit_globals${suffix})
        if(WIN32)
            target_compile_options(lom_unit${suffix}_${unit} PRIVATE -fno-data-sections)
        endif()

        string(JSON by_file ERROR_VARIABLE no_renames GET "${LOM_NATIVE_UNIT_SETTINGS_JSON}" units ${unit} defines_by_file)
        if(NOT no_renames)
            string(JSON pattern_count LENGTH "${by_file}")
            math(EXPR last "${pattern_count} - 1")
            foreach(i RANGE ${last})
                string(JSON pattern MEMBER "${by_file}" ${i})
                string(JSON defines GET "${by_file}" "${pattern}")
                string(JSON define_count LENGTH "${defines}")
                math(EXPR last_define "${define_count} - 1")
                set(define_list)
                foreach(j RANGE ${last_define})
                    string(JSON define GET "${defines}" ${j})
                    list(APPEND define_list "${define}")
                endforeach()
                if(pattern MATCHES "\\*\\*")
                    string(REPLACE "/**/" "/" flat "${pattern}")
                    file(GLOB_RECURSE matched "${LOM_DECOMP_DIR}/${flat}")
                else()
                    file(GLOB matched "${LOM_DECOMP_DIR}/${pattern}")
                endif()
                set(real_matched)
                foreach(file IN LISTS matched)
                    file(REAL_PATH "${file}" real_file)
                    list(APPEND real_matched "${real_file}")
                endforeach()
                set_property(SOURCE ${real_matched} TARGET_DIRECTORY lom_unit${suffix}_${unit} APPEND PROPERTY COMPILE_DEFINITIONS ${define_list})
            endforeach()
        endif()

        file(GENERATE OUTPUT "${LOM_NATIVE_UNITS_DIR}/${unit}.objects"
            CONTENT "$<JOIN:$<TARGET_OBJECTS:lom_unit${suffix}_${unit}>,\n>\n")
        list(APPEND LOM_NATIVE_UNIT_LINK_ARGS "${unit}=${LOM_NATIVE_UNITS_DIR}/${unit}.objects")
        list(APPEND LOM_NATIVE_UNIT_OBJECTS "${LOM_NATIVE_UNITS_DIR}/${unit}.o")
        list(APPEND LOM_NATIVE_UNIT_DEPENDS lom_unit${suffix}_${unit} "$<TARGET_OBJECTS:lom_unit${suffix}_${unit}>")
    endforeach()

    add_custom_command(OUTPUT ${LOM_NATIVE_UNIT_OBJECTS}
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/link_units.py" "${LOM_NATIVE_UNITS_DIR}"
            ${LOM_NATIVE_UNIT_LINK_ARGS} --ld "${LOM_UNIT_LD}" --nm "${CMAKE_NM}" --objcopy "${LOM_UNIT_OBJCOPY}" ${available_args}
        DEPENDS ${LOM_NATIVE_UNIT_DEPENDS} ${available_targets} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/link_units.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/coff_units.py"
        COMMENT "Linking the game's units (${version})"
        VERBATIM)
    set_source_files_properties(${LOM_NATIVE_UNIT_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    add_library(lom_native_game${suffix} STATIC ${LOM_NATIVE_UNIT_OBJECTS})
    set_target_properties(lom_native_game${suffix} PROPERTIES LINKER_LANGUAGE C)
    add_custom_target(lom_native_unit_link${suffix} DEPENDS ${LOM_NATIVE_UNIT_OBJECTS})
    add_dependencies(lom_native_game${suffix} lom_native_unit_link${suffix})

    set(LOM_NATIVE_ROUTE_UNITS_DIR "${LOM_NATIVE_UNITS_DIR}/routes")
    list(TRANSFORM LOM_NATIVE_UNIT_OBJECTS REPLACE "^${LOM_NATIVE_UNITS_DIR}/" "${LOM_NATIVE_ROUTE_UNITS_DIR}/"
        OUTPUT_VARIABLE LOM_NATIVE_ROUTE_UNIT_OBJECTS)
    list(TRANSFORM LOM_NATIVE_ROUTE_WRAPS PREPEND "--wrap=" OUTPUT_VARIABLE route_wrap_args)
    add_custom_command(OUTPUT ${LOM_NATIVE_ROUTE_UNIT_OBJECTS}
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/link_units.py" "${LOM_NATIVE_ROUTE_UNITS_DIR}"
            ${LOM_NATIVE_UNIT_LINK_ARGS} --ld "${LOM_UNIT_LD}" --nm "${CMAKE_NM}" --objcopy "${LOM_UNIT_OBJCOPY}" ${available_args} ${route_wrap_args}
        DEPENDS ${LOM_NATIVE_UNIT_DEPENDS} ${available_targets} "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/link_units.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build/coff_units.py"
        COMMENT "Linking the game's units for the route runner (${version})"
        VERBATIM)
    set_source_files_properties(${LOM_NATIVE_ROUTE_UNIT_OBJECTS} PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    add_library(lom_native_game_routes${suffix} STATIC ${LOM_NATIVE_ROUTE_UNIT_OBJECTS})
    set_target_properties(lom_native_game_routes${suffix} PROPERTIES LINKER_LANGUAGE C)
    add_custom_target(lom_native_route_unit_link${suffix} DEPENDS ${LOM_NATIVE_ROUTE_UNIT_OBJECTS})
    add_dependencies(lom_native_game_routes${suffix} lom_native_route_unit_link${suffix})

    set(LOM_NATIVE_UNITS_DIR${suffix} "${LOM_NATIVE_UNITS_DIR}" PARENT_SCOPE)
    set(LOM_NATIVE_UNIT_GLOBALS${suffix} "${LOM_NATIVE_UNIT_GLOBALS}" PARENT_SCOPE)
    set(LOM_NATIVE_UNIT_OBJECTS${suffix} "${LOM_NATIVE_UNIT_OBJECTS}" PARENT_SCOPE)
endfunction()

lom_native_add_game(us)
lom_native_add_game(jp)
