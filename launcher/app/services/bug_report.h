#ifndef LOM_LAUNCHER_BUG_REPORT_H
#define LOM_LAUNCHER_BUG_REPORT_H

#include <QString>
#include <QUrl>

#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>

namespace lom::launcher {
class BugReport {
  public:
    static constexpr std::uintmax_t github_attachment_limit = 25ull * 1024 * 1024;

    struct Inputs {
        std::filesystem::path run_prefix;
        std::filesystem::path output_directory;
        std::filesystem::path launcher_log;
        std::filesystem::path launcher_settings;
        std::filesystem::path save_root;
        QString summary;
        QString home;
        std::time_t now = 0;
    };

    struct Result {
        std::filesystem::path zip;
        std::uintmax_t size = 0;
        QString os;
        QString gpu;
    };

    /**
     * @brief Find the newest run in the logs folder.
     * @param logs_directory Logs folder.
     * @return Its prefix (logs/game-<time>), or nothing when there's no run yet.
     */
    static std::optional<std::filesystem::path> latest_run(const std::filesystem::path& logs_directory);

    /**
     * @brief The run a log file belongs to.
     * @param file_name A file in the logs folder.
     * @return "game-<time>" for the run's .log, -system.txt, -crash.txt and .dmp; empty for anything else.
     */
    static std::string run_of(const std::string& file_name);

    /**
     * @brief Build and save the zip.
     * @param inputs What goes in.
     * @param error Receives a reason on failure.
     * @return The saved report, or nothing on failure.
     */
    static std::optional<Result> build(const Inputs& inputs, QString* error);

    /**
     * @brief The new-issue page for a report, with what's known filled in.
     * @param repository GitHub "owner/name".
     * @param version Launcher version.
     * @param result The saved report.
     * @return Issue form address.
     */
    static QUrl issue_url(const QString& repository, const QString& version, const Result& result);
};

}

#endif
