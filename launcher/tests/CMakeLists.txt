find_package(GTest 1.12 CONFIG QUIET)

if(NOT TARGET GTest::gmock)
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.2 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(googletest)
endif()

add_library(lom_launcher_test_support STATIC
    support/disc_fixture.cpp
    support/files_provider_fake.cpp
    support/synthetic_card.cpp
    support/synthetic_disc.cpp
    support/test_harness.cpp)
target_link_libraries(lom_launcher_test_support PUBLIC lom_launcher_core GTest::gtest)
target_compile_options(lom_launcher_test_support PRIVATE ${LOM_LAUNCHER_WARNINGS})

add_executable(lom-launcher-unit-tests
    unit/cue_sheet_test.cpp
    unit/disc_image_test.cpp
    unit/disc_importer_test.cpp
    unit/file_snapshot_test.cpp
    unit/iso9660_reader_test.cpp
    unit/json_test.cpp
    unit/lom_save_test.cpp
    unit/memory_card_test.cpp
    unit/mod_library_test.cpp
    unit/remaster_install_test.cpp
    unit/semantic_version_test.cpp
    unit/sha1_test.cpp
    unit/sha256_test.cpp
    unit/update_package_test.cpp
    unit/zip_writer_test.cpp
    support/fake_disk_each_test.cpp)
target_link_libraries(lom-launcher-unit-tests PRIVATE lom_launcher_test_support GTest::gtest_main)
target_compile_options(lom-launcher-unit-tests PRIVATE ${LOM_LAUNCHER_WARNINGS})

set(LOM_LAUNCHER_TEST_IMAGE "" CACHE FILEPATH
    "User-supplied US disc image (CUE or BIN) for real-disc launcher checks; empty skips them")

if(COMMAND lom_add_test)
    lom_add_test(NAME launcher-unit TIER 0 KIND unit COMMAND lom-launcher-unit-tests)
    if(LOM_LAUNCHER_TEST_IMAGE)
        lom_add_test(NAME launcher-real-image TIER 2 KIND integration LABELS disc
            COMMAND lom-disc-tool identify "${LOM_LAUNCHER_TEST_IMAGE}"
                --catalog "${LOM_LAUNCHER_DATA_DIR}/supported-discs.json")
    endif()
    if(LOM_DISC_ROOT)
        lom_add_test(NAME launcher-real-folder TIER 2 KIND integration TIMEOUT 900
            COMMAND lom-disc-tool verify-folder @DISC@
                --catalog "${LOM_LAUNCHER_DATA_DIR}/supported-discs.json")
    endif()
else()
    enable_testing()
    add_test(NAME launcher-unit COMMAND lom-launcher-unit-tests)
    if(LOM_LAUNCHER_TEST_IMAGE)
        add_test(NAME launcher-real-image
            COMMAND lom-disc-tool identify "${LOM_LAUNCHER_TEST_IMAGE}"
                --catalog "${LOM_LAUNCHER_DATA_DIR}/supported-discs.json")
    endif()
endif()

if(TARGET lom-launcher)
    add_executable(lom-launcher-specs
        behavior/common/behavior_main.cpp
        behavior/common/launcher_spec.cpp
        behavior/game_process/game_exits_specification.cpp
        behavior/launcher_paths/portable_mode_specification.cpp
        behavior/launcher_window/accept_ownership_notice_specification.cpp
        behavior/launcher_window/change_game_files_location_specification.cpp
        behavior/launcher_window/change_language_specification.cpp
        behavior/launcher_window/check_for_updates_specification.cpp
        behavior/launcher_window/choose_disc_image_specification.cpp
        behavior/launcher_window/choose_save_folder_specification.cpp
        behavior/launcher_window/close_launcher_specification.cpp
        behavior/launcher_window/decline_ownership_notice_specification.cpp
        behavior/launcher_window/import_memory_card_specification.cpp
        behavior/launcher_window/manage_game_files_specification.cpp
        behavior/launcher_window/manage_mods_specification.cpp
        behavior/launcher_window/open_launcher_specification.cpp
        behavior/launcher_window/open_save_folder_specification.cpp
        behavior/launcher_window/play_specification.cpp
        behavior/launcher_window/read_whats_new_specification.cpp
        behavior/launcher_window/report_bug_specification.cpp
        behavior/launcher_window/toggle_theme_specification.cpp
        behavior/launcher_window/use_extracted_folder_specification.cpp)
    target_include_directories(lom-launcher-specs PRIVATE behavior/common)
    target_link_libraries(lom-launcher-specs PRIVATE lom_launcher_app_lib lom_launcher_test_support GTest::gmock GTest::gtest)
    target_compile_options(lom-launcher-specs PRIVATE ${LOM_LAUNCHER_WARNINGS})

    add_executable(lom-launcher-fake-game providers/fake_game.cpp)
    add_executable(lom-launcher-provider-checks
        providers/contract_main.cpp
        providers/clock_provider_contract.cpp
        providers/desktop_provider_contract.cpp
        providers/download_provider_contract.cpp
        providers/file_dialogs_provider_contract.cpp
        providers/file_lock_provider_contract.cpp
        providers/files_provider_contract.cpp
        providers/process_runner_provider_contract.cpp
        providers/random_provider_contract.cpp
        providers/tools_provider_contract.cpp
        providers/update_feed_provider_contract.cpp)
    set_target_properties(lom-launcher-provider-checks PROPERTIES AUTOMOC ON)
    target_link_libraries(lom-launcher-provider-checks PRIVATE lom_launcher_app_lib lom_launcher_test_support GTest::gtest)
    target_compile_options(lom-launcher-provider-checks PRIVATE ${LOM_LAUNCHER_WARNINGS})
    target_compile_definitions(lom-launcher-provider-checks PRIVATE LOM_FAKE_GAME="$<TARGET_FILE:lom-launcher-fake-game>")
    add_dependencies(lom-launcher-provider-checks lom-launcher-fake-game)

    if(COMMAND lom_add_test)
        lom_add_test(NAME launcher-specs TIER 0 KIND behavior COMMAND lom-launcher-specs ENV QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=)
        lom_add_test(NAME launcher-providers TIER 0 KIND contract COMMAND lom-launcher-provider-checks ENV QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=)
    else()
        add_test(NAME launcher-specs COMMAND lom-launcher-specs)
        set_tests_properties(launcher-specs PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME=")
        add_test(NAME launcher-providers COMMAND lom-launcher-provider-checks)
    endif()
endif()
