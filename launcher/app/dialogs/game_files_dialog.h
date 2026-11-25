#ifndef LOM_LAUNCHER_GAME_FILES_DIALOG_H
#define LOM_LAUNCHER_GAME_FILES_DIALOG_H

#include <QDialog>
#include <QString>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QLabel;

namespace lom::launcher {

struct GameFilesEdition {
    QString id;
    QString name;
    QString files;
};

class GameFilesDialog : public QDialog {
    Q_OBJECT

  public:
    static constexpr char use_remaster_name[] = "useRemasterCheck";
    static constexpr char remaster_where_name[] = "remasterWhere";
    static constexpr char verify_name[] = "verifySelectedButton";

    /**
     * @brief The object name of an edition's radio button, which picks it as
     *        the one Play starts.
     * @param id The edition's release id.
     * @return The name.
     */
    static QString radio_name(const QString& id);

    /**
     * @brief The object name of an edition's Set up button.
     * @param id The edition's release id.
     * @return The name.
     */
    static QString set_up_name(const QString& id);

    /**
     * @brief The object name of the label saying where an edition's files are,
     *        or that it isn't set up.
     * @param id The edition's release id.
     * @return The name.
     */
    static QString status_name(const QString& id);

    /**
     * @brief Build the dialog: the game discs, at least one of which has to be
     *        set up, with the one Play starts picked; where disc images are
     *        copied; and the Remaster, which is optional.
     * @param editions Every edition the launcher can play, in order, each with
     *        where its files are, or empty when it isn't set up.
     * @param active The release id of the one Play starts, or empty for none.
     * @param games_root The folder a disc image's files are copied to.
     * @param playing Whether the game is running, which stops changing the
     *        game discs.
     * @param parent Parent widget.
     */
    GameFilesDialog(const std::vector<GameFilesEdition>& editions, const QString& active, const QString& games_root, bool playing, QWidget* parent);

    /**
     * @brief Show the Remaster's section.
     * @param use Whether the game uses it.
     * @param where What was found or chosen, as the player reads it.
     */
    void set_remaster(bool use, const QString& where);

    /**
     * @brief Show the folder a disc image's files are copied to.
     * @param path Folder text.
     */
    void set_games_root(const QString& path);

  signals:
    /**
     * @brief The user picked another edition for Play to start. The dialog
     *        stays open.
     * @param release_id Its release id.
     */
    void version_chosen(const QString& release_id);

    /**
     * @brief The user wants to set up an edition from its disc image or
     *        extracted folder.
     * @param release_id The edition's release id.
     */
    void set_up(const QString& release_id);

    /**
     * @brief The user wants to see an edition's game files.
     * @param release_id The edition's release id.
     */
    void show_game_files(const QString& release_id);

    /**
     * @brief The user wants to verify the files of the edition Play starts.
     */
    void verify_game_files();

    /**
     * @brief The user wants to change the folder a disc image's files are
     *        copied to.
     */
    void change_games_root();

    /**
     * @brief Whether the game uses the Remaster changed.
     * @param enabled New value.
     */
    void use_remaster_changed(bool enabled);

    /**
     * @brief The user wants to choose the Remaster's folder.
     */
    void choose_remaster();

    /**
     * @brief The user wants the launcher to look for the Remaster in Steam again.
     */
    void find_remaster();

  protected:
    /**
     * @brief Grow to the height the wrapped text needs once the width is known.
     * @param event Show event.
     */
    void showEvent(QShowEvent* event) override;

  private:
    QLabel* games_root_ = nullptr;
    QCheckBox* use_remaster_ = nullptr;
    QLabel* remaster_where_ = nullptr;
    QButtonGroup* radios_ = nullptr;
    QString active_;
};

}

#endif
