#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <QProgressDialog>
#include <QPushButton>

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

namespace {
constexpr char installable_release[] = R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0", "body": "New things",
  "assets": [
    {"name": "lom-native-0.5.0-linux-x86_64.AppImage", "browser_download_url": "https://github.com/example/lom/releases/download/v0.5.0/lom-native-0.5.0-linux-x86_64.AppImage"},
    {"name": "SHA256SUMS", "browser_download_url": "https://github.com/example/lom/releases/download/v0.5.0/SHA256SUMS"}
  ]})";

}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should offer to download and install the new version when this launcher can update itself") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available. You have 0.4.0.\nDownload and install it now?")));
    EXPECT_TRUE(installs.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should only offer the download page when this launcher can't update itself") {
    update_response.body = installable_release;
    open_launcher();
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available. You have 0.4.0.\nOpen the download page?")));
}

LOM_SPEC(CheckForUpdatesSpecification, "Checking for updates should only offer the download page when the release has no checksums") {
    update_response.body = R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0",
      "assets": [{"name": "lom-native-0.5.0-linux-x86_64.AppImage", "browser_download_url": "https://github.com/example/lom/releases/download/v0.5.0/lom-native-0.5.0-linux-x86_64.AppImage"}]})";
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available. You have 0.4.0.\nOpen the download page?")));
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should fetch the release's package and checksums, then restart into it") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Restart now");

    choose_help("Check for updates");

    ASSERT_EQ(installs.size(), 1u);
    EXPECT_EQ(installs[0].version, "0.5.0");
    EXPECT_EQ(installs[0].package, QUrl("https://github.com/example/lom/releases/download/v0.5.0/lom-native-0.5.0-linux-x86_64.AppImage"));
    EXPECT_EQ(installs[0].checksums, QUrl("https://github.com/example/lom/releases/download/v0.5.0/SHA256SUMS"));
    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("Legend of Mana Native 0.5.0 is installed. Restart the launcher now to use it?")));
    EXPECT_EQ(restarts, 1);
}

LOM_SPEC(CheckForUpdatesSpecification, "Choosing Later after an update installs should not restart the launcher") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    EXPECT_EQ(restarts, 0);
}

LOM_SPEC(CheckForUpdatesSpecification, "A restart that can't start the new launcher should say to open it again") {
    update_response.body = installable_release;
    can_update_itself = true;
    restart_starts = false;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Restart now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."), StartsWithText("Legend of Mana Native 0.5.0 is installed."),
                                      StartsWithText("The update is installed, but the launcher couldn't start it. Open the launcher again to use version 0.5.0.")));
}

LOM_SPEC(CheckForUpdatesSpecification, "An update that fails to install should say why and offer the download page") {
    update_response.body = installable_release;
    can_update_itself = true;
    install_outcome = {false, false, QStringLiteral("the download didn't match its checksum, so it wasn't installed")};
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Open download page");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: the download didn't match its checksum, so it wasn't installed")));
    EXPECT_EQ(opened_pages, std::vector<QUrl>{QUrl("https://github.com/example/lom/releases/tag/v0.5.0")});
    EXPECT_EQ(restarts, 0);
}

LOM_SPEC(CheckForUpdatesSpecification, "Cancelling the download should stop it without saying it failed") {
    update_response.body = installable_release;
    can_update_itself = true;
    install_finishes = false;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_dialog_is_answered([](QWidget* dialog) {
        auto* progress = qobject_cast<QProgressDialog*>(dialog);
        ASSERT_NE(progress, nullptr);
        auto* cancel = progress->findChild<QPushButton*>();
        ASSERT_NE(cancel, nullptr);
        cancel->click();
    });

    choose_help("Check for updates");
    settle();

    EXPECT_EQ(install_cancels, 1);
    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available.")));
    EXPECT_EQ(restarts, 0);
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should wait until the game is closed") {
    given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"), R"("check_updates_on_startup": false)");
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    press("playButton");
    given_the_next_message_is_answered_with("Download and install");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."), StartsWithText("Close the game before installing the update.")));
    EXPECT_TRUE(installs.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "Opening the launcher should clear away what an earlier update left") {
    open_launcher();

    EXPECT_EQ(clean_ups, 1);
}
