#ifndef LOM_LAUNCHER_PROBLEM_PAGE_H
#define LOM_LAUNCHER_PROBLEM_PAGE_H

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

namespace lom::launcher {
class ProblemPage : public QWidget {
    Q_OBJECT

  public:
    static constexpr char primary_name[] = "problemPrimaryButton";
    static constexpr char secondary_name[] = "problemSecondaryButton";
    static constexpr char body_name[] = "problemBody";
    static constexpr char details_name[] = "problemDetails";

    /**
     * @brief Build the page.
     * @param parent Parent widget.
     */
    explicit ProblemPage(QWidget* parent);

    /**
     * @brief Show a problem.
     * @param title Short title.
     * @param body Explanation.
     * @param details Technical details, or empty to hide the Details control.
     * @param primary Primary action label.
     * @param secondary Secondary action label, or empty to hide it.
     */
    void show_problem(const QString& title, const QString& body, const QString& details, const QString& primary, const QString& secondary);

    /**
     * @brief Redraw icons after a theme change.
     */
    void refresh_icons();

  signals:
    /**
     * @brief The primary action was selected.
     */
    void primary();

    /**
     * @brief The secondary action was selected.
     */
    void secondary();

  private:
    QLabel* icon_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* body_ = nullptr;
    QToolButton* toggle_ = nullptr;
    QPlainTextEdit* details_ = nullptr;
    QPushButton* primary_ = nullptr;
    QPushButton* secondary_ = nullptr;
};

}

#endif
