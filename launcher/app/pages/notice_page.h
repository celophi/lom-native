#ifndef LOM_LAUNCHER_NOTICE_PAGE_H
#define LOM_LAUNCHER_NOTICE_PAGE_H

#include <QWidget>

class QLabel;
class QPushButton;

namespace lom::launcher {
class NoticePage : public QWidget {
    Q_OBJECT

  public:
    static constexpr char accept_name[] = "noticeAcceptButton";
    static constexpr char quit_name[] = "noticeQuitButton";
    static constexpr char body_name[] = "noticeBody";

    /**
     * @brief Build the page.
     * @param parent Parent widget.
     */
    explicit NoticePage(QWidget* parent);

    /**
     * @brief Redraw icons after a theme change.
     */
    void refresh_icons();

    /**
     * @brief Give keyboard focus to the accept button.
     */
    void focus_default();

  signals:
    /**
     * @brief The user selected "I legally own the game".
     */
    void accepted();

    /**
     * @brief The user selected Quit.
     */
    void quit_requested();

  private:
    QLabel* icon_ = nullptr;
    QPushButton* accept_ = nullptr;
};

}

#endif
