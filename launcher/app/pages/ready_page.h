#ifndef LOM_LAUNCHER_READY_PAGE_H
#define LOM_LAUNCHER_READY_PAGE_H

#include <QWidget>

#include <array>

class QLabel;
class QPushButton;

namespace lom::launcher {
class ReadyPage : public QWidget {
    Q_OBJECT

  public:
    static constexpr char play_name[] = "playButton";
    static constexpr char settings_name[] = "settingsButton";
    static constexpr char mods_name[] = "modsButton";
    static constexpr char game_files_name[] = "gameFilesButton";
    static constexpr char browse_saves_name[] = "browseSavesButton";
    static constexpr char open_saves_name[] = "openSavesButton";
    static constexpr char import_card_name[] = "importCardButton";
    static constexpr char edition_name[] = "installedEdition";
    static constexpr char remaster_status_name[] = "remasterStatus";
    static constexpr char save_folder_name[] = "saveFolderPath";
    static constexpr char status_name[] = "readyStatus";

    /**
     * @brief Build the page.
     * @param parent Parent widget.
     */
    explicit ReadyPage(QWidget* parent);

    /**
     * @brief Show the installed edition.
     * @param edition Edition name.
     */
    void set_edition(const QString& edition);

    /**
     * @brief Say whether the game uses the Remaster, under the edition.
     * @param text What to say, or empty to say nothing.
     */
    void set_remaster_status(const QString& text);

    /**
     * @brief Show the active save folder.
     * @param path Folder text.
     */
    void set_save_folder(const QString& path);

    /**
     * @brief Show a short status message, such as an import result.
     * @param text Message, or empty to hide.
     */
    void set_status(const QString& text);

    /**
     * @brief Reflect whether the game is running; changes to the saves are
     *        disabled meanwhile.
     * @param playing Whether the game is running.
     */
    void set_playing(bool playing);

    /**
     * @brief Report the status message, for tests.
     * @return Status text.
     */
    QString status() const;

    /**
     * @brief Redraw icons after a theme change.
     */
    void refresh_icons();

    /**
     * @brief The panel with Play and the other buttons, so things can sit above it.
     * @return The panel.
     */
    QWidget* controls() const;

    /**
     * @brief How narrow the page can be with its buttons side by side.
     * @return Width in pixels.
     */
    int minimum_width() const;

    /**
     * @brief Show or hide "Ready to play", the edition and the Remaster line,
     *        for when something covers them.
     * @param visible Whether they show.
     */
    void set_heading_visible(bool visible);

    /**
     * @brief Give keyboard focus to Play.
     */
    void focus_default();

  signals:
    /**
     * @brief The user selected Play.
     */
    void play();

    /**
     * @brief The user selected Settings.
     */
    void settings();

    /**
     * @brief The user selected Mods.
     */
    void mods();

    /**
     * @brief The user selected Game files.
     */
    void game_files();

    /**
     * @brief The user wants to choose a save folder.
     */
    void browse_saves();

    /**
     * @brief The user wants to open the save folder.
     */
    void open_saves();

    /**
     * @brief The user wants to import an emulator memory card.
     */
    void import_card();

    /**
     * @brief The row's buttons were measured: the page now knows how narrow it can be (minimum_width()).
     */
    void buttons_measured();

  protected:
    /**
     * @brief Measure the buttons when the page is shown.
     * @param event The event.
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Measure the buttons again when the theme or font changes.
     * @param event Change event.
     */
    void changeEvent(QEvent* event) override;

  private:
    /**
     * @brief Give the row's buttons the widest one's width, once the styles are applied.
     */
    void equalize_buttons();

    QLabel* heading_ = nullptr;
    QLabel* edition_ = nullptr;
    QLabel* remaster_ = nullptr;
    QWidget* controls_ = nullptr;
    QLabel* save_folder_ = nullptr;
    QLabel* status_ = nullptr;
    QPushButton* play_ = nullptr;
    QPushButton* browse_saves_ = nullptr;
    QPushButton* import_card_ = nullptr;
    std::array<QPushButton*, 6> row_buttons_{};
};

}

#endif
