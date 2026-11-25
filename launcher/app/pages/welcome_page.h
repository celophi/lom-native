#ifndef LOM_LAUNCHER_WELCOME_PAGE_H
#define LOM_LAUNCHER_WELCOME_PAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace lom::launcher {
class WelcomePage : public QWidget {
    Q_OBJECT

  public:
    static constexpr char browse_name[] = "browseImageButton";
    static constexpr char folder_name[] = "useFolderButton";
    static constexpr char destination_name[] = "changeDestinationButton";
    static constexpr char back_name[] = "welcomeBackButton";
    static constexpr char destination_label_name[] = "destinationFolder";
    static constexpr char body_name[] = "welcomeBody";

    /**
     * @brief Build the page.
     * @param parent Parent widget.
     */
    explicit WelcomePage(QWidget* parent);

    /**
     * @brief Show the selected image path.
     * @param path Path text, or empty for none.
     */
    void set_image_path(const QString& path);

    /**
     * @brief Show where extracted files will be stored.
     * @param path Folder text.
     */
    void set_destination(const QString& path);

    /**
     * @brief Say what to choose under the heading.
     * @param text What to say.
     */
    void set_body(const QString& text);

    /**
     * @brief Show or hide the route back to the current installation.
     * @param visible Whether an installation exists.
     */
    void set_back_visible(bool visible);

    /**
     * @brief Redraw icons after a theme change.
     */
    void refresh_icons();

    /**
     * @brief Give keyboard focus to Browse.
     */
    void focus_default();

  signals:
    /**
     * @brief The user wants to choose a disc image.
     */
    void choose_image();

    /**
     * @brief The user wants to choose an extracted folder.
     */
    void choose_folder();

    /**
     * @brief The user wants to change the game files folder.
     */
    void change_destination();

    /**
     * @brief The user wants to return to the installed game.
     */
    void back();

  private:
    QLabel* icon_ = nullptr;
    QLabel* body_ = nullptr;
    QLineEdit* image_path_ = nullptr;
    QPushButton* browse_ = nullptr;
    QLabel* destination_ = nullptr;
    QPushButton* back_ = nullptr;
};

}

#endif
