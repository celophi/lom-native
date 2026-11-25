#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <QRect>
#include <QWidget>

#include <vector>

using namespace lom::launcher::test;
using ::testing::HasSubstr;
using ::testing::Not;

namespace {
constexpr char three_releases[] = R"([
    {"tag_name": "v0.4.0", "html_url": "https://github.com/example/lom/releases/tag/v0.4.0", "published_at": "2026-10-01T12:00:00Z",
     "body": "## Fixes\n\nThe chocobo stops running into walls."},
    {"tag_name": "v0.3.5", "html_url": "https://github.com/example/lom/releases/tag/v0.3.5", "published_at": "2026-09-01T12:00:00Z",
     "body": "Movies play on the Steam Deck."},
    {"tag_name": "v0.3.0", "html_url": "https://github.com/example/lom/releases/tag/v0.3.0", "published_at": "2026-08-01T12:00:00Z",
     "body": "The first one with a launcher."}
])";

struct ReadWhatsNewSpecification : LauncherSpec {
    /**
     * @brief Start on this version, with three releases out.
     */
    ReadWhatsNewSpecification() {
        given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "check_updates_on_startup": false,
                           "last_seen_version": "0.4.0"})");
        releases_response.body = three_releases;
    }

    /**
     * @brief Report whether the What's new panel is showing.
     */
    bool whats_new_is_showing() {
        auto* panel = launcher().findChild<QWidget*>("whatsNew");
        return panel != nullptr && panel->isVisible();
    }

    /**
     * @brief Open a newer version than the last one, 0.3.0, with the update check on.
     */
    void given_I_last_opened_an_older_version() {
        given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "check_updates_on_startup": true,
                           "last_seen_version": "0.3.0"})");
    }
};

}

LOM_SPEC(ReadWhatsNewSpecification, "Choosing What's new should show the notes of every release, newest first") {
    open_launcher();

    choose_help("What's new");

    ASSERT_TRUE(whats_new_is_showing());
    const QString notes = text_of("whatsNewNotes");
    EXPECT_THAT(notes.toStdString(), HasSubstr("The chocobo stops running into walls."));
    EXPECT_LT(notes.indexOf("0.4.0"), notes.indexOf("0.3.5"));
    EXPECT_LT(notes.indexOf("0.3.5"), notes.indexOf("0.3.0"));
    EXPECT_THAT(notes.toStdString(), HasSubstr("The first one with a launcher."));
}

LOM_SPEC(ReadWhatsNewSpecification, "What's new should point out the version I have and when each came out") {
    open_launcher();

    choose_help("What's new");

    EXPECT_THAT(text_of("whatsNewNotes").toStdString(), HasSubstr("October 1, 2026 · the version you have"));
    EXPECT_EQ(text_of("whatsNewSummary"), "You have version 0.4.0.");
}

LOM_SPEC(ReadWhatsNewSpecification, "What's new should leave out drafts and prereleases") {
    releases_response.body = R"([
        {"tag_name": "v0.6.0", "draft": true, "body": "Secret plans."},
        {"tag_name": "v0.5.0-beta.1", "prerelease": true, "body": "Try this."},
        {"tag_name": "v0.4.0", "body": "The real one."}
    ])";
    open_launcher();

    choose_help("What's new");

    const std::string notes = text_of("whatsNewNotes").toStdString();
    EXPECT_THAT(notes, HasSubstr("The real one."));
    EXPECT_THAT(notes, Not(HasSubstr("Secret plans.")));
    EXPECT_THAT(notes, Not(HasSubstr("Try this.")));
}

LOM_SPEC(ReadWhatsNewSpecification, "All releases on GitHub should open the releases page") {
    open_launcher();
    choose_help("What's new");

    press("whatsNewAllReleases");

    EXPECT_EQ(opened_pages, std::vector<QUrl>{QUrl("https://github.com/example/lom/releases")});
}

LOM_SPEC(ReadWhatsNewSpecification, "Close should put the panel away") {
    open_launcher();
    choose_help("What's new");

    press("whatsNewClose");

    EXPECT_FALSE(whats_new_is_showing());
}

LOM_SPEC(ReadWhatsNewSpecification, "What's new should say why when the notes can't be fetched") {
    releases_response.ok = false;
    releases_response.error = "Host not found";
    open_launcher();

    choose_help("What's new");

    EXPECT_EQ(text_of("whatsNewNotes"), "I couldn't get the release notes right now: Host not found");
}

LOM_SPEC(ReadWhatsNewSpecification, "What's new should say there are no notes when the build doesn't look up releases") {
    update_feed_configured = false;
    open_launcher();

    choose_help("What's new");

    EXPECT_EQ(text_of("whatsNewNotes"), "This build doesn't look up releases, so there are no notes to show here.");
}

LOM_SPEC(ReadWhatsNewSpecification, "Opening a newer version should show what changed since the last version I opened") {
    given_I_last_opened_an_older_version();

    open_launcher();

    ASSERT_TRUE(whats_new_is_showing());
    const std::string notes = text_of("whatsNewNotes").toStdString();
    EXPECT_THAT(notes, HasSubstr("The chocobo stops running into walls."));
    EXPECT_THAT(notes, HasSubstr("Movies play on the Steam Deck."));
    EXPECT_THAT(notes, Not(HasSubstr("The first one with a launcher.")));
    EXPECT_EQ(text_of("whatsNewSummary"), "You're on version 0.4.0 now. Here's what changed.");
}

LOM_SPEC(ReadWhatsNewSpecification, "Opening a newer version should remember it, so what's new shows only once") {
    given_I_last_opened_an_older_version();

    open_launcher();

    EXPECT_TRUE(json_is(settings_member("last_seen_version"), R"("0.4.0")"));
}

LOM_SPEC(ReadWhatsNewSpecification, "Opening the version I opened last time should not show what's new") {
    open_launcher();

    EXPECT_FALSE(whats_new_is_showing());
    EXPECT_EQ(release_list_requests, 0);
}

LOM_SPEC(ReadWhatsNewSpecification, "Opening the launcher for the first time should not show what's new") {
    std::filesystem::remove(config_directory / "launcher.json");

    open_launcher();
    press("noticeAcceptButton");

    EXPECT_FALSE(whats_new_is_showing());
    EXPECT_EQ(release_list_requests, 0);
    EXPECT_TRUE(json_is(settings_member("last_seen_version"), R"("0.4.0")"));
}

LOM_SPEC(ReadWhatsNewSpecification, "Opening a newer version with the update check off should not go online") {
    given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "check_updates_on_startup": false,
                       "last_seen_version": "0.3.0"})");

    open_launcher();

    EXPECT_EQ(release_list_requests, 0);
    EXPECT_FALSE(whats_new_is_showing());
    EXPECT_TRUE(json_is(settings_member("last_seen_version"), R"("0.4.0")"));
}

LOM_SPEC(ReadWhatsNewSpecification, "When the notes can't be fetched after an update, the next opening should try again") {
    given_I_last_opened_an_older_version();
    releases_response.ok = false;
    releases_response.error = "Host not found";

    open_launcher();

    EXPECT_FALSE(whats_new_is_showing());
    EXPECT_TRUE(json_is(settings_member("last_seen_version"), R"("0.3.0")"));
}

LOM_SPEC(ReadWhatsNewSpecification, "What's new should leave Play and the other buttons uncovered") {
    given_the_game_is_set_up_from(given_an_extracted_folder("My game"));
    open_launcher();
    choose_help("What's new");

    ASSERT_TRUE(whats_new_is_showing());
    auto* panel = launcher().findChild<QWidget*>("whatsNew");
    auto* play = launcher().findChild<QWidget*>("playButton");
    const QRect play_area(play->mapTo(&launcher(), QPoint(0, 0)), play->size());
    const QRect panel_area(panel->mapTo(&launcher(), QPoint(0, 0)), panel->size());
    EXPECT_LT(panel_area.bottom(), play_area.top());

    press("playButton");
    EXPECT_EQ(game_launches.size(), 1U);
}
