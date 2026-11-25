#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <vector>

using namespace lom::launcher::test;
using ::testing::ElementsAre;

namespace {
struct CheckForUpdatesSpecification : LauncherSpec {
    /**
     * @brief Turn off the startup check.
     */
    CheckForUpdatesSpecification() {
        given_settings(R"({"schema": 1, "ownership_notice_acknowledged": true, "theme": "light", "check_updates_on_startup": false})");
    }
};

}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should say the launcher is up to date when the latest release is this version") {
    open_launcher();

    choose_help("Check for updates");

    EXPECT_EQ(messages, std::vector<QString>{"You have the latest version (0.4.0)."});
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should offer the new version when a newer release exists") {
    update_response.body = R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0", "body": "New things"})";
    open_launcher();
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available. You have 0.4.0.\nOpen the download page?")));
    EXPECT_TRUE(opened_pages.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "Choosing Open download page should open the release page") {
    update_response.body = R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0", "body": "New things"})";
    open_launcher();
    given_the_next_message_is_answered_with("Open download page");

    choose_help("Check for updates");

    EXPECT_EQ(opened_pages, std::vector<QUrl>{QUrl("https://github.com/example/lom/releases/tag/v0.5.0")});
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should not offer an older release") {
    update_response.body = R"({"tag_name": "v0.3.0", "html_url": "https://github.com/example/lom/releases/tag/v0.3.0"})";
    open_launcher();

    choose_help("Check for updates");

    EXPECT_EQ(messages, std::vector<QString>{"You have the latest version (0.4.0)."});
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should not offer a prerelease") {
    update_response.body = R"({"tag_name": "v0.9.0", "prerelease": true, "html_url": "https://github.com/example/lom/releases/tag/v0.9.0"})";
    open_launcher();

    choose_help("Check for updates");

    EXPECT_EQ(messages, std::vector<QString>{"Could not check for updates: the latest release is not a usable stable version"});
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should say it could not check when the release feed is unreachable") {
    update_response.ok = false;
    update_response.error = "Host not found";
    open_launcher();

    choose_help("Check for updates");

    EXPECT_EQ(messages, std::vector<QString>{"Could not check for updates: Host not found"});
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should say update checks are not configured when the build has no release feed") {
    update_feed_configured = false;
    open_launcher();

    choose_help("Check for updates");

    EXPECT_EQ(messages, std::vector<QString>{"Update checks are not configured in this build."});
}
