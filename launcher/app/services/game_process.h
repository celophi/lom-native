#ifndef LOM_LAUNCHER_GAME_PROCESS_H
#define LOM_LAUNCHER_GAME_PROCESS_H

#include "services/file_lock_provider.h"
#include <QObject>
#include <QString>

#include <filesystem>
#include <memory>


namespace lom::launcher {

class ProcessRunnerProvider;

class GameProcess : public QObject {
    Q_OBJECT

  public:
    static constexpr char lock_file_name[] = ".lom-native-game.lock";
    static constexpr char card_root_variable[] = "LOM_CARD_ROOT";
    static constexpr char app_id_variable[] = "SDL_APP_ID";
    static constexpr char no_error_dialog_variable[] = "LOM_NO_ERROR_DIALOG";
    static constexpr char report_prefix_variable[] = "LOM_REPORT_PREFIX";
    static constexpr char record_variable[] = "LOM_RECORD";
    static constexpr char session_directory_variable[] = "LOM_SESSION_DIR";
    static constexpr char remaster_root_variable[] = "LOM_REMASTER_ROOT";
    static constexpr char language_variable[] = "LOM_LANGUAGE";
    static constexpr char mods_directory_variable[] = "LOM_MODS_DIR";
    static constexpr char bundled_mods_directory_variable[] = "LOM_BUNDLED_MODS_DIR";
    static constexpr int graphics_unavailable_exit_code = 4;

    enum class Ending { Quit, Error, GraphicsUnavailable };

    /**
     * @brief Create an idle monitor.
     * @param runner Process boundary; must outlive the monitor.
     * @param locks What locks the save folder; must outlive the monitor.
     * @param parent Owner.
     */
    GameProcess(ProcessRunnerProvider& runner, FileLockProvider& locks, QObject* parent);

    /**
     * @brief Release the save lock.
     */
    ~GameProcess() override;

    /**
     * @brief Start the game.
     * @param executable Game executable.
     * @param data_directory Verified extracted game data.
     * @param save_root Card root for the game's saves.
     * @param working_directory Working directory for the game.
     * @param log_file File receiving the game's output; its reports go beside it.
     * @param session_directory Where to record the session; empty doesn't record.
     * @param error Receives a user-readable reason on failure.
     * @return True when the start was requested; started() or failed() follows.
     */
    bool start(const std::filesystem::path& executable, const std::filesystem::path& data_directory, const std::filesystem::path& save_root,
               const std::filesystem::path& working_directory, const std::filesystem::path& log_file, const std::filesystem::path& session_directory,
               QString* error);

    /**
     * @brief Set the Remaster's folder for the next start, or none.
     * @param folder Its folder; empty tells the game not to use it.
     */
    void set_remaster_root(const std::filesystem::path& folder);

    /**
     * @brief Set the mods folder for the next start; the game loads the mods
     *        its mods.ini enables.
     * @param folder The folder; empty leaves the game to find its own.
     */
    void set_mods_directory(const std::filesystem::path& folder);

    /**
     * @brief Set where the example mods that come with lom-native are, for the
     *        next start.
     * @param folder The folder; empty for none.
     */
    void set_bundled_mods_directory(const std::filesystem::path& folder);

    /**
     * @brief Report whether a game is starting or running.
     * @return True while the process exists.
     */
    bool running() const;

    /**
     * @brief Report the current log file.
     * @return Log path.
     */
    const std::filesystem::path& log_file() const;

  signals:
    /**
     * @brief The game process started.
     */
    void started();

    /**
     * @brief The game process could not start.
     * @param message Reason.
     */
    void failed(const QString& message);

    /**
     * @brief The game process ended.
     * @param exit_code Exit status.
     * @param ending Whether it quit normally, failed or couldn't start its graphics.
     */
    void exited(int exit_code, GameProcess::Ending ending);

  private:
    /**
     * @brief Release the save-folder lock.
     */
    void release_lock();

    ProcessRunnerProvider& runner_;
    FileLockProvider& locks_;
    std::unique_ptr<FileLockProvider::Lock> lock_;
    std::filesystem::path log_file_;
    std::filesystem::path remaster_root_;
    std::filesystem::path mods_directory_;
    std::filesystem::path bundled_mods_directory_;
};

}

#endif
