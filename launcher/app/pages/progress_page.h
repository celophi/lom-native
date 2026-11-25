#ifndef LOM_LAUNCHER_PROGRESS_PAGE_H
#define LOM_LAUNCHER_PROGRESS_PAGE_H

#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;

namespace lom::launcher {
class ProgressPage : public QWidget {
    Q_OBJECT

  public:
    static constexpr char cancel_name[] = "progressCancelButton";

    /**
     * @brief Build the page.
     * @param parent Parent widget.
     */
    explicit ProgressPage(QWidget* parent);

    /**
     * @brief Start showing a new operation.
     * @param stage Stage text.
     * @param subject File or folder being processed.
     */
    void begin(const QString& stage, const QString& subject);

    /**
     * @brief Change the stage text.
     * @param stage Stage text.
     */
    void set_stage(const QString& stage);

    /**
     * @brief Show the recognized edition.
     * @param edition Edition text, or empty to hide.
     */
    void set_edition(const QString& edition);

    /**
     * @brief Show work completed.
     * @param done Bytes done.
     * @param total Bytes expected.
     */
    void set_progress(quint64 done, quint64 total);

    /**
     * @brief Report the stage text, for tests.
     * @return Stage text.
     */
    QString stage() const;

    /**
     * @brief Redraw icons after a theme change.
     */
    void refresh_icons();

  signals:
    /**
     * @brief The user selected Cancel.
     */
    void cancel_requested();

  private:
    QLabel* icon_ = nullptr;
    QLabel* stage_ = nullptr;
    QLabel* subject_ = nullptr;
    QLabel* edition_ = nullptr;
    QProgressBar* bar_ = nullptr;
    QLabel* amount_ = nullptr;
    QPushButton* cancel_ = nullptr;
};

}

#endif
