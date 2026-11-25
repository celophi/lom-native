#ifndef LOM_LAUNCHER_PAGE_BUILDER_H
#define LOM_LAUNCHER_PAGE_BUILDER_H

#include "window/theme.h"

#include <QString>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace lom::launcher {
class PageBuilder {
  public:
    static constexpr int content_width = 360;
    static constexpr int icon_size = 72;
    static constexpr char heading_name[] = "pageHeading";

    /**
     * @brief Start a page on a widget.
     * @param page Page widget.
     */
    explicit PageBuilder(QWidget* page);

    /**
     * @brief Add the page icon.
     * @param icon Icon kind.
     * @return The label showing it, for refreshing after a theme change.
     */
    QLabel* icon(Theme::Icon icon);

    /**
     * @brief Add a label with a style role.
     * @param text Label text.
     * @param role Theme object name, or empty for body text.
     * @return The label.
     */
    QLabel* label(const QString& text, const char* role);

    /**
     * @brief Add a full-width button.
     * @param text Button text.
     * @param role Theme object name.
     * @return The button.
     */
    QPushButton* button(const QString& text, const char* role);

    /**
     * @brief Add vertical space.
     * @param pixels Height.
     */
    void space(int pixels);

    /**
     * @brief Add a widget centered in the column.
     * @param widget Widget to add.
     */
    void add(QWidget* widget);

    /**
     * @brief Finish the page. The card fits its contents, so there's nothing to add.
     */
    void finish();

    /**
     * @brief Access the column layout.
     */
    QVBoxLayout* layout();

  private:
    QWidget* page_;
    QVBoxLayout* column_;
};

}

#endif
