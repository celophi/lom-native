set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_library(lom_launcher_app_lib STATIC
    launcher.qrc
    window/app_paths.cpp
    window/backdrop.cpp
    window/background_task.cpp
    window/launcher_services.cpp
    window/launcher_window.cpp
    window/launcher_window_install.cpp
    window/launcher_window_play.cpp
    window/launcher_window_saves.cpp
    window/launcher_window_settings.cpp
    window/launcher_window_game_files.cpp
    window/launcher_window_mods.cpp
    window/launcher_window_updates.cpp
    window/single_instance.cpp
    window/theme.cpp
    window/ui_text.cpp
    pages/notice_page.cpp
    pages/page_builder.cpp
    pages/problem_page.cpp
    pages/problem_text.cpp
    pages/progress_page.cpp
    pages/ready_page.cpp
    pages/welcome_page.cpp
    pages/whats_new_panel.cpp
    dialogs/card_import_dialog.cpp
    dialogs/settings_dialog.cpp
    dialogs/game_files_dialog.cpp
    dialogs/mods_dialog.cpp
    services/game_process.cpp
    services/bug_report.cpp
    services/github_update_feed.cpp
    services/native_file_dialogs.cpp
    services/process_runner.cpp
    services/qt_process_runner.cpp
    services/release_history.cpp
    services/system_desktop.cpp
    services/system_update_installer.cpp
    services/update_checker.cpp)
target_include_directories(lom_launcher_app_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(lom_launcher_app_lib PUBLIC lom_launcher_core Qt6::Widgets Qt6::Network)
target_compile_options(lom_launcher_app_lib PRIVATE ${LOM_LAUNCHER_WARNINGS})
target_compile_definitions(lom_launcher_app_lib PUBLIC LOM_APP_ID="${LOM_APP_ID}")

add_executable(lom-launcher WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(lom-launcher PRIVATE lom_launcher_app_lib)
target_compile_options(lom-launcher PRIVATE ${LOM_LAUNCHER_WARNINGS})
target_compile_definitions(lom-launcher PRIVATE
    LOM_APP_VERSION="${LOM_VERSION}"
    LOM_UPDATE_REPOSITORY="${LOM_UPDATE_REPOSITORY}")
if(APPLE)
    string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" lom_bundle_version "${LOM_VERSION}")
    string(REPLACE "_" "-" lom_bundle_id "${LOM_APP_ID}")
    set(lom_minimum_macos "")
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(lom_minimum_macos "<key>LSMinimumSystemVersion</key>\n    <string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string>")
    endif()
    configure_file(Info.plist.in "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
    set_target_properties(lom-launcher PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
endif()

if(NOT CMAKE_SOURCE_DIR STREQUAL lom_launcher_SOURCE_DIR)
    set_target_properties(lom-launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()

if(APPLE)
    set(lom_launcher_data_destination "$<TARGET_BUNDLE_CONTENT_DIR:lom-launcher>/Resources/launcher-data")
else()
    set(lom_launcher_data_destination "$<TARGET_FILE_DIR:lom-launcher>/launcher-data")
endif()
add_custom_command(TARGET lom-launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${LOM_LAUNCHER_DATA_DIR}" "${lom_launcher_data_destination}"
    VERBATIM)

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS lom-launcher RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY "${LOM_LAUNCHER_DATA_DIR}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/lom-native/launcher-data")
else()
    install(TARGETS lom-launcher RUNTIME DESTINATION . BUNDLE DESTINATION .)
    if(NOT APPLE)
        install(DIRECTORY "${LOM_LAUNCHER_DATA_DIR}/" DESTINATION launcher-data)
    endif()
endif()
