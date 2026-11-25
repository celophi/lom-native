#include "window/theme.h"

#include <QApplication>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyleHints>
#include <QRegularExpression>
#include <QWidget>

/**
 * @brief Register launcher.qrc's resources (the background and the icon).
 *
 * The app code is a static library, so its resources only reach the program
 * once something calls this. It has to be outside any namespace.
 */
static void load_launcher_resources() {
    static const bool loaded = []() {
        Q_INIT_RESOURCE(launcher);
        return true;
    }();
    (void)loaded;
}

namespace lom::launcher {

namespace {
struct Colors {
    QColor window;
    QColor surface;
    QColor text;
    QColor muted;
    QColor primary;
    QColor primary_hover;
    QColor primary_text;
    QColor secondary;
    QColor secondary_border;
    QColor link;
    QColor separator;
    QColor success;
    QColor success_soft;
    QColor warning;
    QColor warning_soft;
    QColor disc;
};

struct Palette {
    Colors light;
    Colors dark;
    QHash<QString, QString> art;
};

constexpr char theme_resource[] = ":/launcher/theme.json";

/**
 * @brief Stop the launcher because theme.json is broken, which only a change
 *        to it can do.
 * @param what What's wrong with it.
 */
[[noreturn]] void broken_theme(const QString& what) {
    qFatal("%s: %s", theme_resource, qPrintable(what));
}

/**
 * @brief Read one of theme.json's color sets.
 * @param root The theme.
 * @param name "light" or "dark".
 * @return The colors.
 */
Colors read_colors(const QJsonObject& root, const char* name) {
    const QJsonObject set = root.value(QLatin1String(name)).toObject();
    const auto color = [&](const char* key) {
        const QColor value(set.value(QLatin1String(key)).toString());

        if (!value.isValid()) {
            broken_theme(QStringLiteral("%1.%2 isn't a color").arg(QLatin1String(name), QLatin1String(key)));
        }

        return value;
    };

    Colors colors;
    colors.window = color("window");
    colors.surface = color("surface");
    colors.text = color("text");
    colors.muted = color("muted");
    colors.primary = color("primary");
    colors.primary_hover = color("primary_hover");
    colors.primary_text = color("primary_text");
    colors.secondary = color("secondary");
    colors.secondary_border = color("secondary_border");
    colors.link = color("link");
    colors.separator = color("separator");
    colors.success = color("success");
    colors.success_soft = color("success_soft");
    colors.warning = color("warning");
    colors.warning_soft = color("warning_soft");
    colors.disc = color("disc");
    return colors;
}

/**
 * @brief Read theme.json, from the launcher's resources.
 * @return The palette.
 */
Palette load_palette() {
    load_launcher_resources();
    QFile file(QString::fromLatin1(theme_resource));

    if (!file.open(QIODevice::ReadOnly)) {
        broken_theme(QStringLiteral("isn't in the launcher's resources"));
    }

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);

    if (!document.isObject()) {
        broken_theme(error.errorString());
    }

    const QJsonObject root = document.object();
    Palette palette{read_colors(root, "light"), read_colors(root, "dark"), {}};
    const QJsonObject art = root.value(QStringLiteral("art")).toObject();

    for (auto it = art.begin(); it != art.end(); ++it) {
        palette.art.insert(it.key(), it.value().toString());
    }

    return palette;
}

/**
 * @brief The launcher's colors, read from theme.json the first time.
 * @return The palette.
 */
const Palette& palette() {
    static const Palette loaded = load_palette();
    return loaded;
}

/**
 * @brief One of the colors for the parts that sit on the background painting.
 * @param name Its name in theme.json's "art".
 * @return The color, as style sheet text.
 */
QString art(const char* name) {
    const auto found = palette().art.constFind(QLatin1String(name));

    if (found == palette().art.constEnd()) {
        broken_theme(QStringLiteral("art.%1 is missing").arg(QLatin1String(name)));
    }

    return *found;
}

constexpr int title_point_size = 22;
constexpr int heading_point_size = 14;
constexpr int body_point_size = 10;
constexpr int footer_point_size = 9;
constexpr qreal icon_stroke_ratio = 0.07;
constexpr char role_property[] = "role";
constexpr char background_resource[] = ":/launcher/background.png";
constexpr char icon_resource[] = ":/launcher/icon.png";
constexpr int text_shadow_blur = 18;
constexpr int text_shadow_alpha = 170;

/**
 * @brief Report the active color set.
 * @param dark Whether dark is active.
 * @return Colors.
 */
const Colors& colors(bool dark) {
    return dark ? palette().dark : palette().light;
}

/**
 * @brief Build the style sheet for a color set.
 * @param c Colors.
 * @return Style sheet text.
 */
QString style_sheet(const Colors& c) {
    return QStringLiteral(R"(
QWidget { color: %1; font-size: %15pt; }
QMainWindow { background: %19; }
QDialog { background: %2; }
QStackedWidget, QStackedWidget > QWidget { background: transparent; }
QLabel[role="titleLabel"] { font-size: %16pt; font-weight: 700; }
QLabel[role="headingLabel"] { font-size: %17pt; font-weight: 600; }
QLabel[role="mutedLabel"] { color: %3; }
QLabel[role="footerLabel"] { color: %3; font-size: %18pt; }
QFrame[role="separator"] { background: %4; max-height: 1px; min-height: 1px; border: none; }
QFrame[role="optionsBox"] { border: 1px solid %9; border-radius: 8px; }
QRadioButton { spacing: 10px; }
QRadioButton::indicator { width: 12px; height: 12px; border: 2px solid %3; border-radius: 8px; background: %11; }
QRadioButton::indicator:checked { border-color: %5; background: qradialgradient(cx:0.5, cy:0.5, radius:0.5, fx:0.5, fy:0.5, stop:0 %5, stop:0.5 %5, stop:0.6 %11, stop:1 %11); }
QRadioButton::indicator:disabled { border-color: %4; background: %8; }
QRadioButton:disabled { color: %3; }
QPushButton[role="primaryButton"] { background: %5; color: %6; border: 2px solid transparent; border-radius: 6px; padding: 7px 20px; font-weight: 600; }
QPushButton[role="primaryButton"]:hover { background: %7; }
QPushButton[role="primaryButton"]:disabled { background: %8; color: %3; }
QPushButton[role="secondaryButton"], QPushButton { background: %8; color: %1; border: 1px solid %9; border-radius: 6px; padding: 8px 18px; }
QPushButton[role="secondaryButton"]:hover, QPushButton:hover { border-color: %5; }
QPushButton:focus { border-color: %5; background: %11; }
QPushButton[role="primaryButton"]:focus { border-color: %1; background: %5; }
QComboBox:focus, QLineEdit:focus, QTreeWidget:focus { border-color: %5; }
QPushButton[role="linkButton"] { background: transparent; border: 1px solid transparent; color: %10; padding: 4px; }
QPushButton[role="linkButton"]:focus { border: 1px dashed %10; }
QToolButton:focus { border: 1px dashed %3; border-radius: 4px; }
QPushButton[role="linkButton"]:hover { text-decoration: underline; }
QLineEdit, QPlainTextEdit, QComboBox, QTreeWidget { background: %11; color: %1; border: 1px solid %9; border-radius: 5px; padding: 5px; }
QProgressBar { background: %8; border: none; border-radius: 4px; height: 8px; text-align: center; color: transparent; }
QProgressBar::chunk { background: %5; border-radius: 4px; }
QToolButton { background: transparent; border: none; color: %3; padding: 4px 6px; }
QToolButton:hover { color: %1; }
QMenu { background: %11; color: %1; border: 1px solid %9; }
QMenu::item:selected { background: %8; }
)")
        .arg(c.text.name(), c.window.name(), c.muted.name(), c.separator.name(), c.primary.name(), c.primary_text.name(), c.primary_hover.name(),
             c.secondary.name(), c.secondary_border.name(), c.link.name(), c.surface.name())
        .arg(QString::number(body_point_size), QString::number(title_point_size), QString::number(heading_point_size), QString::number(footer_point_size),
             art("window"));
}

/**
 * @brief Build the styles for the parts that sit on the background painting.
 * @param dark Whether the dark variant is active.
 * @return Style sheet text.
 */
QString art_style_sheet(bool dark) {
    QString sheet = QStringLiteral(R"(
QFrame[role="card"] { {card} border-radius: 16px; }
QWidget[role="topBar"], QWidget[role="statusBar"] { background: {bar}; }
QLabel[role="bannerLabel"] { background: {banner}; color: {text}; border: 2px solid {border}; border-radius: 8px; padding: 6px 14px; }
QLabel[role="heroSubtitle"] { color: {accent}; font-size: 15pt; font-weight: 700; }
QFrame[role="glassPanel"] { background: {glass}; border: 2px solid {border}; border-radius: 12px; }
QPushButton[role="playButton"] { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 {play_top}, stop:1 {play_bottom}); color: white; border: 2px solid {play_border}; border-top-color: {play_light}; border-radius: 8px; padding: 12px 28px; font-size: 18pt; font-weight: 800; }
QPushButton[role="playButton"]:hover { background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 {play_hover_top}, stop:1 {play_hover_bottom}); }
QPushButton[role="playButton"]:pressed { background: {play_bottom}; border-top-color: {play_border}; }
QPushButton[role="playButton"]:focus { border-color: {accent}; }
QPushButton[role="playButton"]:disabled { background: {play_disabled}; color: rgba(255, 255, 255, 140); border-color: transparent; }
QPushButton[role="glassButton"] { background: rgba({button_rgb}, 36); color: {text}; border: 2px solid {button_border}; border-radius: 8px; padding: 8px 12px; }
QPushButton[role="glassButton"]:hover { background: rgba({button_rgb}, 66); }
QPushButton[role="glassButton"]:focus { border-color: {accent}; }
QPushButton[role="glassButton"]:disabled { color: rgba({text_rgb}, 110); }
QLabel[role="brandLabel"] { color: {text}; font-size: 11pt; font-weight: 700; }
QLabel[role="statusLabel"] { color: rgba({text_rgb}, 200); font-size: 9pt; }
QToolButton[role="topBarButton"] { color: rgba({text_rgb}, 215); }
QToolButton[role="topBarButton"]:hover { color: {text}; }
QToolButton[role="topBarButton"]:focus { border: 1px dashed {accent}; }
QToolButton[role="topBarButton"][popupMode="2"] { padding-right: 16px; }
QToolButton[role="topBarButton"]::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; }
QLabel[role="heroDetail"] { color: rgba({text_rgb}, 215); }
QFrame[role="glassPanel"] QLabel { color: rgba({text_rgb}, 225); }
QFrame[role="notesPanel"] { background: {glass}; border: 1px solid rgba({accent_rgb}, 56); border-radius: 12px; }
QFrame[role="notesPanel"] QLabel { color: rgba({text_rgb}, 225); }
QPushButton[role="notesButton"] { background: rgba({button_rgb}, 26); color: rgba({text_rgb}, 230); border: 1px solid rgba({accent_rgb}, 90); border-radius: 8px; padding: 7px 18px; }
QPushButton[role="notesButton"]:hover { background: rgba({button_rgb}, 52); border-color: rgba({accent_rgb}, 140); color: {text}; }
QPushButton[role="notesButton"]:pressed { background: rgba({button_rgb}, 14); }
QPushButton[role="notesButton"]:focus { border-color: {accent}; }
QTextBrowser[role="releaseNotes"] { background: transparent; border: none; color: rgba({text_rgb}, 230); selection-background-color: {border}; font-size: 10.5pt; }
QFrame[role="notesPanel"] QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QFrame[role="notesPanel"] QScrollBar::handle:vertical { background: rgba({accent_rgb}, 110); border-radius: 3px; min-height: 30px; }
QFrame[role="notesPanel"] QScrollBar::handle:vertical:hover { background: rgba({accent_rgb}, 170); }
QFrame[role="notesPanel"] QScrollBar::add-line:vertical, QFrame[role="notesPanel"] QScrollBar::sub-line:vertical { height: 0; }
QFrame[role="notesPanel"] QScrollBar::add-page:vertical, QFrame[role="notesPanel"] QScrollBar::sub-page:vertical { background: transparent; }
)");
    const QString card = dark ? QStringLiteral("background: %1; border: 2px solid %2;").arg(art("glass"), art("border"))
                              : QStringLiteral("background: %1; border: 2px solid %2;").arg(art("paper"), art("paper_border"));
    sheet.replace(QStringLiteral("{card}"), card);

    for (auto it = palette().art.constBegin(); it != palette().art.constEnd(); ++it) {
        sheet.replace(QLatin1Char('{') + it.key() + QLatin1Char('}'), it.value());
    }

    const QRegularExpressionMatch missing = QRegularExpression(QStringLiteral("\\{([a-z_]+)\\}")).match(sheet);

    if (missing.hasMatch()) {
        broken_theme(QStringLiteral("art.%1 is missing").arg(missing.captured(1)));
    }

    return sheet;
}

}

bool Theme::dark_ = false;

void Theme::set_role(QWidget* widget, const char* role) {
    widget->setProperty(role_property, QString::fromLatin1(role));
}

bool Theme::resolve_dark(LauncherSettings::Theme preference) {
    if (preference == LauncherSettings::Theme::Light) {
        return false;
    }

    if (preference == LauncherSettings::Theme::Dark) {
        return true;
    }

    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

void Theme::apply(QApplication& application, bool dark) {
    dark_ = dark;
    const Colors& c = colors(dark);
    QPalette palette;
    palette.setColor(QPalette::Window, c.window);
    palette.setColor(QPalette::WindowText, c.text);
    palette.setColor(QPalette::Base, c.surface);
    palette.setColor(QPalette::AlternateBase, c.secondary);
    palette.setColor(QPalette::Text, c.text);
    palette.setColor(QPalette::Button, c.secondary);
    palette.setColor(QPalette::ButtonText, c.text);
    palette.setColor(QPalette::Highlight, c.primary);
    palette.setColor(QPalette::HighlightedText, c.primary_text);
    palette.setColor(QPalette::Link, c.link);
    palette.setColor(QPalette::PlaceholderText, c.muted);
    palette.setColor(QPalette::ToolTipBase, c.surface);
    palette.setColor(QPalette::ToolTipText, c.text);
    application.setPalette(palette);
    application.setStyleSheet(style_sheet(c) + art_style_sheet(dark));
}

bool Theme::is_dark() {
    return dark_;
}

QColor Theme::accent() {
    return QColor(art("accent"));
}

QPixmap Theme::pixmap(Icon icon, int size) {
    const Colors& c = colors(dark_);
    const qreal ratio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(int(size * ratio), int(size * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    const qreal stroke = qMax<qreal>(1.5, s * icon_stroke_ratio);
    const QRectF bounds(stroke, stroke, s - 2 * stroke, s - 2 * stroke);

    switch (icon) {
    case Icon::Disc:
        painter.setPen(QPen(c.disc, stroke));
        painter.setBrush(c.surface);
        painter.drawEllipse(bounds);
        painter.drawEllipse(QPointF(s / 2, s / 2), s * 0.13, s * 0.13);
        painter.setPen(QPen(c.primary, stroke * 0.8, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawArc(bounds.adjusted(s * 0.12, s * 0.12, -s * 0.12, -s * 0.12), 30 * 16, 60 * 16);
        break;

    case Icon::Ready: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.success_soft);
        painter.drawEllipse(bounds);
        QPainterPath check;
        check.moveTo(s * 0.30, s * 0.52);
        check.lineTo(s * 0.44, s * 0.66);
        check.lineTo(s * 0.71, s * 0.37);
        painter.setPen(QPen(c.success, s * 0.09, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(check);
        break;
    }

    case Icon::Warning: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.warning_soft);
        painter.drawEllipse(bounds);
        painter.setPen(QPen(c.warning, s * 0.09, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s / 2, s * 0.28), QPointF(s / 2, s * 0.56));
        painter.drawPoint(QPointF(s / 2, s * 0.72));
        break;
    }

    case Icon::Folder: {
        painter.setPen(QPen(c.muted, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        QPainterPath folder;
        folder.moveTo(s * 0.12, s * 0.28);
        folder.lineTo(s * 0.40, s * 0.28);
        folder.lineTo(s * 0.48, s * 0.36);
        folder.lineTo(s * 0.88, s * 0.36);
        folder.lineTo(s * 0.88, s * 0.78);
        folder.lineTo(s * 0.12, s * 0.78);
        folder.closeSubpath();
        painter.drawPath(folder);
        break;
    }

    case Icon::Leaf: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.success);
        QPainterPath leaf;
        leaf.moveTo(s * 0.18, s * 0.82);
        leaf.cubicTo(s * 0.10, s * 0.35, s * 0.45, s * 0.12, s * 0.86, s * 0.14);
        leaf.cubicTo(s * 0.86, s * 0.55, s * 0.62, s * 0.86, s * 0.18, s * 0.82);
        painter.drawPath(leaf);
        painter.setPen(QPen(c.surface, s * 0.05, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(QPointF(s * 0.22, s * 0.78), QPointF(s * 0.70, s * 0.30));
        break;
    }

    case Icon::Sun:
        painter.setPen(QPen(c.muted, stroke, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPointF(s / 2, s / 2), s * 0.18, s * 0.18);

        for (int i = 0; i < 8; i++) {
            painter.save();
            painter.translate(s / 2, s / 2);
            painter.rotate(45.0 * i);
            painter.drawLine(QPointF(0, s * 0.30), QPointF(0, s * 0.40));
            painter.restore();
        }

        break;

    case Icon::Moon: {
        painter.setPen(Qt::NoPen);
        painter.setBrush(c.muted);
        QPainterPath moon;
        moon.addEllipse(QPointF(s * 0.46, s / 2), s * 0.32, s * 0.32);
        QPainterPath cut;
        cut.addEllipse(QPointF(s * 0.62, s * 0.40), s * 0.26, s * 0.26);
        painter.drawPath(moon.subtracted(cut));
        break;
    }
    }

    return pixmap;
}

QIcon Theme::window_icon() {
    QIcon icon;

    for (const int size : {16, 32, 64, 128, 256}) {
        icon.addPixmap(app_icon(size));
    }

    return icon;
}

QPixmap Theme::app_icon(int size) {
    load_launcher_resources();
    const QPixmap source(QString::fromLatin1(icon_resource));

    if (source.isNull()) {
        return pixmap(Icon::Leaf, size);
    }

    const qreal ratio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap scaled = source.scaled(int(size * ratio), int(size * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(ratio);
    return scaled;
}

QPixmap Theme::background() {
    load_launcher_resources();
    return QPixmap(QString::fromLatin1(background_resource));
}

void Theme::add_text_shadow(QWidget* widget) {
    auto* shadow = new QGraphicsDropShadowEffect(widget);
    shadow->setBlurRadius(text_shadow_blur);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, text_shadow_alpha));
    widget->setGraphicsEffect(shadow);
}

}
