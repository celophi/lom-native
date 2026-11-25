#ifndef LOM_LAUNCHER_WHATS_NEW_PANEL_H
#define LOM_LAUNCHER_WHATS_NEW_PANEL_H

#include "services/release_history.h"

#include <QFrame>

#include <vector>

class QLabel;
class QPushButton;
class QTextBrowser;

namespace lom::launcher {
class WhatsNewPanel : public QFrame {
    Q_OBJECT

  public:
    static constexpr char panel_name[] = "whatsNew";
    static constexpr char summary_name[] = "whatsNewSummary";
    static constexpr char notes_name[] = "whatsNewNotes";
    static constexpr char all_releases_name[] = "whatsNewAllReleases";
    static constexpr char close_name[] = "whatsNewClose";

    /**
     * @brief Build the panel, hidden.
     * @param parent The widget it sits on.
     */
    explicit WhatsNewPanel(QWidget* parent);

    /**
     * @brief Show the panel, waiting for notes.
     * @param summary The line under the heading.
     */
    void open(const QString& summary);

    /**
     * @brief Show releases, newest first.
     * @param releases Releases to show.
     * @param current_version This build's version, to point out.
     */
    void show_releases(const std::vector<ReleaseHistory::Release>& releases, const QString& current_version);

    /**
     * @brief Show a message instead of notes.
     * @param text Message.
     */
    void show_message(const QString& text);

  signals:
    /**
     * @brief Ask to open a link from the notes, or the releases page.
     * @param url Address.
     */
    void link_opened(const QUrl& url);

    /**
     * @brief Report that the panel was put away.
     */
    void closed();

  protected:
    /**
     * @brief Close the panel on Escape.
     * @param event The key.
     */
    void keyPressEvent(QKeyEvent* event) override;

  private:
    /**
     * @brief Hide the panel and say so.
     */
    void close_panel();

    QLabel* summary_ = nullptr;
    QTextBrowser* notes_ = nullptr;
    QPushButton* all_releases_ = nullptr;
    QPushButton* close_ = nullptr;
    QUrl releases_page_;
};

}

#endif
