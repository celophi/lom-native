#ifndef LOM_LAUNCHER_LAUNCHER_WINDOW_H
#define LOM_LAUNCHER_LAUNCHER_WINDOW_H

#include "window/app_paths.h"
#include "window/launcher_services.h"
#include "lom/launcher/disc/file_manifest.h"
#include "lom/launcher/disc/file_snapshot.h"
#include "lom/launcher/common/launcher_settings.h"
#include "lom/launcher/disc/release_catalog.h"
#include "pages/problem_text.h"
#include "services/update_checker.h"

#include <QMainWindow>

#include <functional>
#include <map>
#include <memory>
#include <optional>

class QLabel;
class QStackedWidget;
class QToolButton;

namespace lom::launcher {

class Backdrop;
class BackgroundTask;
class GameProcess;
class NoticePage;
class ProblemPage;
class ProgressPage;
class ReadyPage;
class WelcomePage;
class WhatsNewPanel;

class LauncherWindow : public QMainWindow {
    Q_OBJECT

  public:
    enum class Page { Notice, Welcome, Progress, Ready, Problem };

    /**
     * @brief Build the window.
     * @param paths Launcher paths.
     * @param services File choices, game process, release feed, desktop, time and randomness.
     * @param version This build's version.
     * @param parent Parent widget.
     */
    LauncherWindow(AppPaths paths, LauncherServices services, QString version, QWidget* parent = nullptr);

    /**
     * @brief Cancel running work before destruction.
     */
    ~LauncherWindow() override;

    /**
     * @brief Load settings and data, then show the first page.
     */
    void start();

    /**
     * @brief Report the visible page.
     */
    Page page() const;

    /**
     * @brief Report the in-memory settings.
     */
    const LauncherSettings& settings() const;

    /**
     * @brief Report the banner text, for tests.
     * @return Banner text.
     */
    QString banner() const;

    /**
     * @brief Block until background work finishes and deliver its result, for tests.
     */
    void wait_for_work();

    /**
     * @brief Report whether a game process is running.
     * @return True while running.
     */
    bool game_running() const;

  signals:
    /**
     * @brief The language changed in the settings; the window wants to be built again in it (main.cpp does).
     */
    void rebuild_requested();

  protected:
    /**
     * @brief Hide instead of closing while the game runs; cancel work otherwise.
     * @param event Close event.
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief Keep What's new in place as the window and pages change size.
     * @param watched The page area or the Ready page's buttons.
     * @param event Event.
     * @return False, so the event carries on.
     */
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    /**
     * @brief Show a page.
     * @param page Page to show.
     */
    void show_page(Page page);

    /**
     * @brief Show a problem with its actions.
     * @param message Title, body and details.
     * @param primary_label Primary action label.
     * @param primary Primary action.
     * @param secondary_label Secondary action label, or empty.
     * @param secondary Secondary action.
     */
    void show_problem(const ProblemText::Message& message, const QString& primary_label, std::function<void()> primary, const QString& secondary_label,
                      std::function<void()> secondary);

    /**
     * @brief Persist settings.
     * @param error Receives the reason on failure.
     * @return True when saved.
     */
    bool save_settings(QString* error);

    /**
     * @brief Persist settings, reporting a failure in the banner.
     * @return True when saved.
     */
    bool save_settings_or_warn();

    /**
     * @brief Show or clear the banner.
     * @param text Message, or empty.
     */
    void set_banner(const QString& text);

    /**
     * @brief Apply the theme preference and redraw icons.
     */
    void apply_theme();

    /**
     * @brief Flip between light and dark and remember the choice.
     */
    void toggle_theme();

    /**
     * @brief Record the ownership acknowledgement and continue.
     */
    void accept_notice();

    /**
     * @brief Continue after the notice: update check, stale cleanup and the first useful page.
     */
    void after_notice();

    /**
     * @brief Show Ready for an installation, otherwise Welcome.
     */
    void show_home();

    /**
     * @brief Show Welcome with the current destination.
     */
    void show_welcome();

    /**
     * @brief Check the active installation before enabling Play.
     * @param full_check Whether to re-hash every file instead of only files changed since the last verification.
     */
    void validate_installation(bool full_check);

    /**
     * @brief Remember a verified folder's snapshot; failure only costs a full check next time.
     * @param snapshot Snapshot from a verification or install.
     * @param file Snapshot file.
     */
    void save_snapshot(const FileSnapshot& snapshot, const std::filesystem::path& file);

    /**
     * @brief Ask for and check a disc image.
     */
    void choose_image();

    /**
     * @brief Check an image on the worker thread.
     * @param path Image path.
     */
    void identify_image(const std::filesystem::path& path);

    /**
     * @brief Extract an identified image.
     * @param identification Supported identification.
     */
    void install_image(const DiscImporter::Identification& identification);

    /**
     * @brief Ask for and verify an extracted folder.
     */
    void choose_folder();

    /**
     * @brief Verify a folder on the worker thread.
     * @param path Folder.
     */
    void verify_folder(const std::filesystem::path& path);

    /**
     * @brief Ask for a new folder for future game files.
     * @param parent Dialog parent.
     * @return True when a new folder was chosen and saved.
     */
    bool change_games_root(QWidget* parent);

    /**
     * @brief Ask for a save folder for the active edition.
     */
    void browse_saves();

    /**
     * @brief Validate and remember a save folder.
     * @param root Chosen folder.
     */
    void use_save_root(const std::filesystem::path& root);

    /**
     * @brief Open the save folder in the file manager.
     */
    void open_saves();

    /**
     * @brief Import saves from an emulator memory card.
     */
    void import_card();

    /**
     * @brief Start the game for the active installation.
     */
    void launch();

    /**
     * @brief Show launcher settings: the launcher's own preferences.
     */
    void open_settings();

    /**
     * @brief Show where the game's files come from: the PS1 game's, set up
     *        from a disc, and the Remaster's.
     */
    void open_game_files();

    /**
     * @brief Show the mods: turn them on and off, order, add and remove them.
     */
    void open_mods();

    /**
     * @brief The Legend of Mana Remaster's folder the game should use: the
     *        one chosen, or the one in Steam, or none (empty) when it's off
     *        or not there.
     */
    std::filesystem::path remaster_folder() const;

    /**
     * @brief What the ready page says about the Remaster.
     * @return That the game uses it, a hint to link it when it's on but not
     *         found, or empty when the player turned it off.
     */
    QString remaster_status() const;

    /**
     * @brief Say in the game files dialog where the Remaster is.
     * @param dialog The dialog.
     */
    void show_remaster(class GameFilesDialog& dialog) const;

    /**
     * @brief Check for a newer release.
     * @param manual Whether the user asked, so every outcome is reported.
     */
    void check_updates(bool manual);

    /**
     * @brief Present an update check result.
     * @param result Result.
     */
    void show_update_result(const UpdateChecker::Result& result);

    /**
     * @brief Download and install a release, showing how it's going, then offer to restart.
     * @param result The update check that found it.
     * @param package The release file this copy of the launcher updates from.
     */
    void install_update(const UpdateChecker::Result& result, const QString& package);

    /**
     * @brief Offer to restart into a newly installed version.
     * @param version Its version.
     */
    void offer_restart(const QString& version);

    /**
     * @brief Say an update couldn't be installed, offering the download page.
     * @param error Why.
     * @param page The release page, or invalid.
     */
    void show_update_failed(const QString& error, const QUrl& page);

    /**
     * @brief Show every recent release's notes, from the Help menu.
     */
    void show_whats_new();

    /**
     * @brief After an update, show the notes of the releases since the last version opened, once.
     */
    void show_whats_new_after_update();

    /**
     * @brief Put What's new over the picture: above the Ready page's buttons, or over any other page, hiding what it covers.
     */
    void place_whats_new();

    /**
     * @brief Show again whatever What's new hid.
     */
    void uncover_page();

    /**
     * @brief Remember this version as the newest one opened.
     */
    void remember_this_version();

    /**
     * @brief Show the ownership notice for reading only.
     */
    void show_notice_again();

    /**
     * @brief Show version information.
     */
    void show_about();

    /**
     * @brief Bundle a game run into a bug report zip, show it and open the GitHub issue form.
     * @param run_prefix The run's log path without ".log".
     * @param ending How the run ended, for report.txt.
     */
    void report_bug(const std::filesystem::path& run_prefix, const QString& ending);

    /**
     * @brief Report the newest run, from Help > Report a bug.
     */
    void report_latest_run();

    /**
     * @brief Write report.txt: the launcher's side of a bug report.
     * @param run_prefix The run's log path without ".log".
     * @param ending How the run ended.
     * @return Report text.
     */
    QString bug_report_summary(const std::filesystem::path& run_prefix, const QString& ending) const;

    /**
     * @brief Report the root for new game files.
     * @return Chosen or default root.
     */
    std::filesystem::path games_root() const;

    /**
     * @brief Report the release of the active installation.
     * @return Release, or null.
     */
    const ReleaseCatalog::Release* active_release() const;

    /**
     * @brief Report the save folder of the active edition.
     * @return Chosen or default folder.
     */
    std::filesystem::path save_root() const;

    /**
     * @brief Load a release's trusted manifest once.
     * @param release Release.
     * @return The manifest, or null when unavailable.
     */
    const FileManifest* manifest_for(const ReleaseCatalog::Release& release);

    /**
     * @brief Make a new installation active and show Ready. It's also kept as
     *        its edition's, beside the other edition's, if that's set up.
     * @param installation Verified installation.
     */
    void activate(const Installation& installation);

    /**
     * @brief Play the other edition set up instead: make its installation
     *        active and save that. Its files are checked after.
     * @param release_id The edition's release id; one that isn't set up, or
     *        a change while the game runs, is ignored.
     * @return True when it's the one Play starts now.
     */
    bool switch_version(const std::string& release_id);

    /**
     * @brief Set up one edition: show the welcome page asking for its disc,
     *        and refuse the other edition's until Back.
     * @param release_id The edition's release id.
     */
    void set_up_edition(const std::string& release_id);

    /**
     * @brief When an edition was asked for, check the disc chosen is it, and
     *        say so when it isn't.
     * @param release The edition the disc is.
     * @return True when it's the wrong one, and the problem is showing.
     */
    bool wrong_edition(const ReleaseCatalog::Release& release);

    /**
     * @brief Refresh the Ready page from settings.
     */
    void refresh_ready();

    AppPaths paths_;
    LauncherServices services_;
    QString version_;
    ReleaseCatalog catalog_;
    std::string expected_release_;
    std::map<std::string, FileManifest> manifests_;
    LauncherSettings settings_;
    QStackedWidget* stack_ = nullptr;
    NoticePage* notice_ = nullptr;
    WelcomePage* welcome_ = nullptr;
    ProgressPage* progress_ = nullptr;
    ReadyPage* ready_ = nullptr;
    ProblemPage* problem_ = nullptr;
    WhatsNewPanel* whats_new_ = nullptr;
    QWidget* covered_page_ = nullptr;
    QToolButton* theme_toggle_ = nullptr;
    QToolButton* help_ = nullptr;
    QLabel* banner_ = nullptr;
    Backdrop* backdrop_ = nullptr;
    QLabel* brand_icon_ = nullptr;
    BackgroundTask* task_ = nullptr;
    GameProcess* game_ = nullptr;
    UpdateChecker* updates_ = nullptr;
    bool manual_update_check_ = false;
    bool startup_update_checked_ = false;
    bool updated_ = false;
    std::function<void()> problem_primary_;
    std::function<void()> problem_secondary_;
    std::function<void()> task_done_;
};

}

#endif
