#ifndef LOM_LAUNCHER_LAUNCHER_SPEC_H
#define LOM_LAUNCHER_LAUNCHER_SPEC_H

#include "../../support/disc_fixture.h"
#include "../../support/disk.h"
#include "../../support/files_provider_fake.h"
#include "lom/launcher/common/json.h"
#include "mocks/clock_provider_mock.h"
#include "mocks/desktop_provider_mock.h"
#include "mocks/download_provider_mock.h"
#include "mocks/file_dialogs_provider_mock.h"
#include "mocks/file_lock_provider_mock.h"
#include "mocks/process_runner_provider_mock.h"
#include "mocks/random_provider_mock.h"
#include "mocks/tools_provider_mock.h"
#include "mocks/update_feed_provider_mock.h"
#include "services/process_runner_provider.h"
#include "services/update_feed_provider.h"
#include "services/update_installer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class QWidget;

namespace lom::launcher {
class LauncherWindow;
}

namespace lom::launcher::test {
struct LauncherSpecDisk {
    FilesProviderFake files_provider_fake;
};

class LauncherSpec : public LauncherSpecDisk, public ::testing::Test {
  public:
    struct ProgramRun {
        QString program;
        QStringList arguments;
    };

    static constexpr char version[] = "0.4.0";
    static constexpr std::int64_t default_now = 1792065600;

    /**
     * @brief Create the working defaults in a fresh temporary profile.
     */
    LauncherSpec();

    /**
     * @brief Close the launcher.
     */
    virtual ~LauncherSpec();

    DiscFixture edition;
    QString disc_image_choice;
    QString game_folder_choice;
    QString games_root_choice;
    QString save_folder_choice;
    QString memory_card_choice;
    QString remaster_folder_choice;
    QString mod_folder_choice;
    bool update_feed_configured = true;
    UpdateFeedProvider::Response update_response;
    UpdateFeedProvider::Response releases_response;
    bool installer_configured = true;
    bool can_update_itself = false;
    UpdateInstaller::Target update_target;
    QByteArray release_package;
    QByteArray release_checksums;
    bool download_finishes = true;
    QString download_error;
    std::function<void(const ProgramRun& run)> tool_does;
    int tool_exit_code = 0;
    QString tool_output;
    bool restart_starts = true;
    bool game_fails_to_start = false;
    bool save_folder_in_use = false;
    QString game_start_failure;
    std::int64_t now = default_now;

    ::testing::NiceMock<FileDialogsProviderMock>* file_dialogs_provider_mock = nullptr;
    ::testing::NiceMock<ProcessRunnerProviderMock>* process_runner_provider_mock = nullptr;
    ::testing::NiceMock<UpdateFeedProviderMock>* update_feed_provider_mock = nullptr;
    ::testing::NiceMock<DesktopProviderMock>* desktop_provider_mock = nullptr;
    ::testing::NiceMock<FileLockProviderMock>* file_lock_provider_mock = nullptr;
    ::testing::NiceMock<DownloadProviderMock>* download_provider_mock = nullptr;
    ::testing::NiceMock<ToolsProviderMock>* tools_provider_mock = nullptr;
    ::testing::NiceMock<ClockProviderMock> clock_provider_mock;
    ::testing::NiceMock<RandomProviderMock> random_provider_mock;

    std::vector<ProcessRunnerProvider::Request> game_launches;
    std::vector<QString> opened_folders;
    std::vector<QUrl> opened_pages;
    int update_requests = 0;
    int release_list_requests = 0;
    std::vector<QUrl> downloads;
    std::function<void(const DownloadProvider::Done&)> unfinished_download;
    int download_cancels = 0;
    std::vector<ProgramRun> tool_runs;
    std::vector<ProgramRun> restarts;
    std::vector<QString> messages;

    /**
     * @brief Replace the launcher settings file before the launcher opens.
     * @param json Settings document; "{...}" placeholders as in expand().
     */
    void given_settings(const std::string& json);

    /**
     * @brief Copy the edition's verified files into a new folder, as a user's own extraction.
     * @param name Folder name under the profile.
     * @return The folder.
     */
    std::filesystem::path given_an_extracted_folder(const std::string& name);

    /**
     * @brief Remember an extracted folder as the active game files.
     * @param folder Folder from given_an_extracted_folder().
     * @param more_settings More members of the settings object, as JSON, or empty.
     */
    void given_the_game_is_set_up_from(const std::filesystem::path& folder, const std::string& more_settings = std::string());

    /**
     * @brief Answer the next message box or dialog the launcher shows.
     * @param answer Called with the dialog once it is open.
     */
    void given_the_next_dialog_is_answered(std::function<void(QWidget* dialog)> answer);

    /**
     * @brief Answer the next message box by pressing a button.
     * @param button Button text.
     */
    void given_the_next_message_is_answered_with(const QString& button);

    /**
     * @brief Open the launcher on the profile and wait until it settles.
     * @return The launcher.
     */
    LauncherWindow& open_launcher();

    /**
     * @brief Access the open launcher.
     * @return The launcher.
     */
    LauncherWindow& launcher();

    /**
     * @brief Press a button by its object name and wait until the launcher settles.
     * @param object_name Button object name.
     */
    void press(const char* object_name);

    /**
     * @brief Choose an entry of the Help menu and wait until the launcher settles.
     * @param entry Menu text.
     */
    void choose_help(const QString& entry);

    /**
     * @brief Let queued events and background work finish.
     */
    void settle();

    /**
     * @brief End the running game.
     * @param exit_code Exit status.
     */
    void the_game_exits(int exit_code);

    /**
     * @brief End the running game with a crash.
     * @param exit_code What the process runner reports for it (the signal on Unix).
     */
    void the_game_crashes(int exit_code);

    /**
     * @brief Read the heading of the visible page.
     * @return Heading text.
     */
    QString heading() const;

    /**
     * @brief Read a label or text box by object name.
     * @param object_name Object name.
     * @return Its text, or empty when absent.
     */
    QString text_of(const char* object_name) const;

    /**
     * @brief Read a JSON file written by the launcher.
     * @param path File.
     * @return The document, or null when absent or malformed.
     */
    Json read_json(const std::filesystem::path& path) const;

    /**
     * @brief Read one member of the launcher settings file.
     * @param key Member name.
     * @return The member, or null when absent.
     */
    Json settings_member(const std::string& key) const;

    /**
     * @brief Compare a JSON document with an expected one written out in the specification.
     * @param actual Document read from disk.
     * @param expected Expected document text; "{...}" placeholders as in expand().
     * @return Success when equal; otherwise a failure showing both documents.
     */
    ::testing::AssertionResult json_is(const Json& actual, const std::string& expected) const;

    /**
     * @brief Substitute path placeholders: {profile}, {data}, {cards}, {logs}, {program}, {sha1}.
     * @param text Text with placeholders.
     * @return Text with JSON-safe paths.
     */
    std::string expand(const std::string& text) const;

    /**
     * @brief Format a path as the launcher shows it.
     * @param path Path.
     */
    static QString shown(const std::filesystem::path& path);

    /**
     * @brief A path with the platform's own separators, to compare with shown().
     *
     * Specifications write paths with /, which Windows takes as well as \.
     * Elsewhere this returns the path as it is.
     * @param path Path text.
     */
    static QString native(const QString& path);

    /**
     * @brief A SHA256SUMS line for a release file.
     * @param bytes The file's bytes.
     * @param name Its name.
     * @return The line.
     */
    static QByteArray checksum_line(const QByteArray& bytes, const QString& name);

    static const char* const game_program;

    std::filesystem::path profile;
    std::filesystem::path config_directory;
    std::filesystem::path data_directory;
    std::filesystem::path program_directory;
    std::filesystem::path steam_directory;

    /**
     * @brief Install a stand-in Remaster in Steam's own library.
     * @return Its folder.
     */
    std::filesystem::path given_the_remaster_is_in_steam() {
        const std::filesystem::path folder = steam_directory / "steamapps" / "common" / "Legend of Mana";
        disk::create_directories(folder / "windata" / "hdmap");
        return folder;
    }

  private:
    std::unique_ptr<::testing::NiceMock<FileDialogsProviderMock>> owned_file_dialogs_;
    std::unique_ptr<::testing::NiceMock<ProcessRunnerProviderMock>> owned_process_runner_;
    std::unique_ptr<::testing::NiceMock<UpdateFeedProviderMock>> owned_update_feed_;
    std::unique_ptr<::testing::NiceMock<DesktopProviderMock>> owned_desktop_;
    std::unique_ptr<::testing::NiceMock<FileLockProviderMock>> owned_locks_;
    std::unique_ptr<::testing::NiceMock<DownloadProviderMock>> owned_downloads_;
    std::unique_ptr<::testing::NiceMock<ToolsProviderMock>> owned_tools_;
    bool game_running_ = false;
    std::atomic<std::uint32_t> random_counter_{0};
    std::deque<std::function<void(QWidget*)>> dialog_answers_;
    std::unique_ptr<LauncherWindow> window_;
};

}

#endif
