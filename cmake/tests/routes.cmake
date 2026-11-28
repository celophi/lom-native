function(lom_add_route_tests)
    if(NOT TARGET lom-route-runner)
        return()
    endif()
    set(routes_root "${CMAKE_CURRENT_SOURCE_DIR}/tests/routes")
    file(GLOB sets CONFIGURE_DEPENDS "${routes_root}/generated/*/*.json")
    set(set_names "")
    foreach(set_file IN LISTS sets)
        file(RELATIVE_PATH name "${routes_root}/generated" "${set_file}")
        string(REGEX REPLACE "\\.json$" "" name "${name}")
        string(REPLACE "/" "-" name "${name}")
        list(APPEND set_names "${name}")
        lom_add_test(NAME routes-${name} TIER 2 KIND e2e GPU NO_HEADLESS TIMEOUT 3600
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/lom_routes.py" suite --set "${set_file}" --no-scorecard
                --jobs 2 --disc @DISC@ --runner $<TARGET_FILE:lom-route-runner> --layout $<TARGET_FILE:lom-save-layout> --out @DIR@)
    endforeach()
    file(GLOB_RECURSE baselines CONFIGURE_DEPENDS "${routes_root}/*.baseline.json")
    set(folders "")
    foreach(baseline IN LISTS baselines)
        get_filename_component(folder "${baseline}" DIRECTORY)
        list(APPEND folders "${folder}")
    endforeach()
    list(REMOVE_DUPLICATES folders)
    foreach(folder IN LISTS folders)
        file(RELATIVE_PATH name "${routes_root}" "${folder}")
        string(REPLACE "/" "-" name "${name}")
        if(name IN_LIST set_names OR name MATCHES "^jp")
            continue()
        endif()
        lom_add_test(NAME routes-${name} TIER 2 KIND e2e GPU NO_HEADLESS TIMEOUT 3600
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/lom_routes.py" suite --routes "${folder}" --flat --no-scorecard
                --jobs 2 --disc @DISC@ --runner $<TARGET_FILE:lom-route-runner> --layout $<TARGET_FILE:lom-save-layout> --out @DIR@)
    endforeach()

    lom_add_test(NAME routes-widescreen-compare TIER 2 KIND e2e GPU NO_HEADLESS TIMEOUT 3600
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/lom_routes.py" widescreen
            "${routes_root}/handwritten/widescreen" "${routes_root}/handwritten/home" "${routes_root}/handwritten/domina/battle.json"
            "${routes_root}/generated/rui/battles.json#rui-rui-b000-battle-1" "${routes_root}/generated/wd1/bosses.json#wd1-wd1-bs02-boss"
            --disc @DISC@ --runner $<TARGET_FILE:lom-route-runner> --layout $<TARGET_FILE:lom-save-layout> --out @DIR@)
    if(LOM_DISC_ROOT_JP AND TARGET lom-route-runner-jp)
        lom_add_test(NAME routes-jp TIER 2 KIND e2e GPU NO_HEADLESS TIMEOUT 3600 DISC "${LOM_DISC_ROOT_JP}"
            COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/lom_routes.py" suite --routes "${routes_root}/jp" --flat --no-scorecard
                --jobs 2 --disc @DISC@ --runner $<TARGET_FILE:lom-route-runner-jp> --layout $<TARGET_FILE:lom-save-layout-jp> --out @DIR@)
    endif()
    lom_add_test(NAME routes-boundaries TIER 2 KIND e2e GPU NO_HEADLESS TIMEOUT 3600
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/testing/lom_routes.py" boundaries "${routes_root}/handwritten"
            --disc @DISC@ --runner $<TARGET_FILE:lom-route-runner> --layout $<TARGET_FILE:lom-save-layout> --out @DIR@)
endfunction()
