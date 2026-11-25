#ifndef LOM_LAUNCHER_THEME_H
#define LOM_LAUNCHER_THEME_H

#include "lom/launcher/common/launcher_settings.h"

#include <QColor>
#include <QIcon>
#include <QPixmap>

class QApplication;
class QWidget;

namespace lom::launcher {
class Theme {
  public:
    enum class Icon { Disc, Ready, Warning, Folder, Leaf, Sun, Moon };

    static constexpr char primary_button[] = "primaryButton";
    static constexpr char secondary_button[] = "secondaryButton";
    static constexpr char link_button[] = "linkButton";
    static constexpr char title_label[] = "titleLabel";
    static constexpr char heading_label[] = "headingLabel";
    static constexpr char muted_label[] = "mutedLabel";
    static constexpr char footer_label[] = "footerLabel";
    static constexpr char separator[] = "separator";
    static constexpr char options_box[] = "optionsBox";
    static constexpr char card[] = "card";
    static constexpr char glass_panel[] = "glassPanel";
    static constexpr char glass_button[] = "glassButton";
    static constexpr char release_notes[] = "releaseNotes";
    static constexpr char notes_panel[] = "notesPanel";
    static constexpr char notes_button[] = "notesButton";
    static constexpr char play_button[] = "playButton";
    static constexpr char hero_subtitle[] = "heroSubtitle";
    static constexpr char hero_detail[] = "heroDetail";
    static constexpr char top_bar[] = "topBar";
    static constexpr char top_bar_button[] = "topBarButton";
    static constexpr char brand_label[] = "brandLabel";
    static constexpr char status_bar[] = "statusBar";
    static constexpr char status_label[] = "statusLabel";
    static constexpr char banner_label[] = "bannerLabel";

    /**
     * @brief Give a widget a style role.
     * @param widget Widget to style.
     * @param role One of the role constants.
     */
    static void set_role(QWidget* widget, const char* role);

    /**
     * @brief Resolve a preference to light or dark using the operating system for System.
     * @param preference Saved preference.
     * @return True for dark.
     */
    static bool resolve_dark(LauncherSettings::Theme preference);

    /**
     * @brief Apply palette and style sheet to the application.
     * @param application Application.
     * @param dark Whether to use the dark variant.
     */
    static void apply(QApplication& application, bool dark);

    /**
     * @brief Report whether the dark variant is active.
     * @return True for dark.
     */
    static bool is_dark();

    /**
     * @brief Report the accent drawn on the painting: focus, "Ready to play", What's new headings and links.
     * @return Accent color of the active look.
     */
    static QColor accent();

    /**
     * @brief Draw an icon in the active theme.
     * @param icon Icon kind.
     * @param size Edge length in device-independent pixels.
     * @return High-DPI pixmap.
     */
    static QPixmap pixmap(Icon icon, int size);

    /**
     * @brief Build the application window icon.
     */
    static QIcon window_icon();

    /**
     * @brief Load the app icon (packaging/icon/lom-native.png) at a size.
     * @param size Edge length in device-independent pixels.
     * @return High-DPI pixmap, or a drawn leaf if the icon is missing.
     */
    static QPixmap app_icon(int size);

    /**
     * @brief Load the background painting.
     * @return The painting, or a null pixmap if it's missing.
     */
    static QPixmap background();

    /**
     * @brief Give text a soft shadow so it reads over the background.
     * @param widget Widget to shade.
     */
    static void add_text_shadow(QWidget* widget);

  private:
    static bool dark_;
};

}

#endif
