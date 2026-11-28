#include "launcher_spec.h"
#include "lom/launcher/common/update_package.h"
#include "window/app_paths.h"
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
const QString windows_package = QStringLiteral("lom-native-0.5.0-windows-x64.zip");
const QString mac_package = QStringLiteral("lom-native-0.5.0-macos-arm64.dmg");

/**
 * @brief A newer release offering a package and its SHA256SUMS.
 * @param package The package's name.
 * @return The release, as the feed gives it.
 */
QByteArray release_offering(const QString& package) {
    return QStringLiteral(R"({"tag_name": "v0.5.0", "html_url": "https://github.com/example/lom/releases/tag/v0.5.0", "body": "New things",
  "assets": [
    {"name": "%1", "browser_download_url": "https://github.com/example/lom/releases/download/v0.5.0/%1"},
    {"name": "SHA256SUMS", "browser_download_url": "https://github.com/example/lom/releases/download/v0.5.0/SHA256SUMS"}
  ]})")
        .arg(package)
        .toUtf8();
}

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
    EXPECT_TRUE(downloads.empty());
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

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should fetch the release's checksums and then its package") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    EXPECT_EQ(downloads, (std::vector<QUrl>{QUrl("https://github.com/example/lom/releases/download/v0.5.0/SHA256SUMS"),
                                            QUrl("https://github.com/example/lom/releases/download/v0.5.0/lom-native-0.5.0-linux-x86_64.AppImage")}));
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should put the checked AppImage in place of the old one") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the new launcher");
    EXPECT_EQ(files_provider_fake.permissions_of(profile / "Applications" / "Legend of Mana Native.AppImage"),
              std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec |
                  std::filesystem::perms::others_read | std::filesystem::perms::others_exec);
    EXPECT_FALSE(disk::exists(profile / "Applications" / ".lom-native-update"));
    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("Legend of Mana Native 0.5.0 is installed. Restart the launcher now to use it?")));
}

LOM_SPEC(CheckForUpdatesSpecification, "Restart now should start the new launcher, telling it it was just updated") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Restart now");

    choose_help("Check for updates");

    ASSERT_EQ(restarts.size(), 1u);
    EXPECT_EQ(restarts[0].program, shown(profile / "Applications" / "Legend of Mana Native.AppImage"));
    EXPECT_EQ(restarts[0].arguments, QStringList{"--after-update"});
}

LOM_SPEC(CheckForUpdatesSpecification, "Choosing Later after an update installs should not restart the launcher") {
    update_response.body = installable_release;
    can_update_itself = true;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    EXPECT_TRUE(restarts.empty());
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

LOM_SPEC(CheckForUpdatesSpecification, "An update whose download doesn't match its checksum should keep the old version, say why and offer the download page") {
    update_response.body = installable_release;
    can_update_itself = true;
    release_package = "a tampered launcher";
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Open download page");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: the download didn't match its checksum, so it wasn't installed")));
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the old launcher");
    EXPECT_FALSE(disk::exists(profile / "Applications" / ".lom-native-update"));
    EXPECT_EQ(opened_pages, std::vector<QUrl>{QUrl("https://github.com/example/lom/releases/tag/v0.5.0")});
    EXPECT_TRUE(restarts.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "An update whose SHA256SUMS doesn't list the package should keep the old version and say so") {
    update_response.body = installable_release;
    can_update_itself = true;
    release_checksums = checksum_line("the ffmpeg source", "ffmpeg-source.tar.xz");
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: the release's SHA256SUMS doesn't list "
                                                     "lom-native-0.5.0-linux-x86_64.AppImage")));
    EXPECT_EQ(downloads.size(), 1u);
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the old launcher");
}

LOM_SPEC(CheckForUpdatesSpecification, "An update whose download fails should keep the old version and say why") {
    update_response.body = installable_release;
    can_update_itself = true;
    download_error = "Connection closed";
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: couldn't download lom-native-0.5.0-linux-x86_64.AppImage: "
                                                     "Connection closed")));
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the old launcher");
}

LOM_SPEC(CheckForUpdatesSpecification, "Cancelling the download should stop it without saying it failed") {
    update_response.body = installable_release;
    can_update_itself = true;
    download_finishes = false;
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_progress_is_answered([](QProgressDialog* progress) {
        auto* cancel = progress->findChild<QPushButton*>();
        ASSERT_NE(cancel, nullptr);
        cancel->click();
    });

    choose_help("Check for updates");
    settle();

    EXPECT_EQ(download_cancels, 1);
    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available.")));
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the old launcher");
    EXPECT_FALSE(disk::exists(profile / "Applications" / ".lom-native-update"));
    EXPECT_TRUE(restarts.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should unpack a Windows zip with the system's tar over the installed folder") {
    update_response.body = release_offering(windows_package);
    can_update_itself = true;
    update_target.kind = lom::launcher::UpdatePackage::Kind::WindowsFolder;
    update_target.path = profile / "Legend of Mana Native";
    update_target.staging = profile / "Legend of Mana Native" / ".lom-native-update";
    update_target.tools = profile / "Windows" / "System32";
    files_provider_fake.add_file(profile / "Windows" / "System32" / "tar.exe", "the system's tar");
    files_provider_fake.add_file(profile / "Legend of Mana Native" / "Legend of Mana Native.exe", "the old launcher");
    files_provider_fake.add_file(profile / "Legend of Mana Native" / "removed.dll", "only in the old version");
    release_package = "the new launcher's zip";
    release_checksums = checksum_line(release_package, windows_package);
    tool_does = [this](const ProgramRun& run) {
        files_provider_fake.add_file(lom::launcher::AppPaths::to_path(run.arguments.value(3)) / "Legend of Mana Native" / "Legend of Mana Native.exe",
                                     "the new launcher");
        files_provider_fake.add_file(lom::launcher::AppPaths::to_path(run.arguments.value(3)) / "Legend of Mana Native" / "licenses" / "a.txt",
                                     "the new licence");
    };
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    ASSERT_EQ(tool_runs.size(), 1u);
    EXPECT_EQ(tool_runs[0].program, shown(profile / "Windows" / "System32" / "tar.exe"));
    EXPECT_EQ(tool_runs[0].arguments, (QStringList{"-xf", shown(profile / "Legend of Mana Native" / ".lom-native-update" / windows_package.toStdString()), "-C",
                                                   shown(profile / "Legend of Mana Native" / ".lom-native-update" / "unpacked")}));
    EXPECT_EQ(disk::contents(profile / "Legend of Mana Native" / "Legend of Mana Native.exe"), "the new launcher");
    EXPECT_EQ(disk::contents(profile / "Legend of Mana Native" / "licenses" / "a.txt"), "the new licence");
    EXPECT_EQ(disk::contents(profile / "Legend of Mana Native" / "removed.dll"), "only in the old version");
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should say tar.exe is missing when Windows has none") {
    update_response.body = release_offering(windows_package);
    can_update_itself = true;
    update_target.kind = lom::launcher::UpdatePackage::Kind::WindowsFolder;
    update_target.path = profile / "Legend of Mana Native";
    update_target.staging = profile / "Legend of Mana Native" / ".lom-native-update";
    update_target.tools = profile / "Windows" / "System32";
    files_provider_fake.add_file(profile / "Legend of Mana Native" / "Legend of Mana Native.exe", "the old launcher");
    release_checksums = checksum_line(release_package, windows_package);
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: Windows' tar.exe is missing, so the download can't be unpacked")));
    EXPECT_TRUE(tool_runs.empty());
    EXPECT_EQ(disk::contents(profile / "Legend of Mana Native" / "Legend of Mana Native.exe"), "the old launcher");
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should say why when the system's tool fails to unpack the download") {
    update_response.body = release_offering(windows_package);
    can_update_itself = true;
    update_target.kind = lom::launcher::UpdatePackage::Kind::WindowsFolder;
    update_target.path = profile / "Legend of Mana Native";
    update_target.staging = profile / "Legend of Mana Native" / ".lom-native-update";
    update_target.tools = profile / "Windows" / "System32";
    files_provider_fake.add_file(profile / "Windows" / "System32" / "tar.exe", "the system's tar");
    files_provider_fake.add_file(profile / "Legend of Mana Native" / "Legend of Mana Native.exe", "the old launcher");
    release_checksums = checksum_line(release_package, windows_package);
    tool_exit_code = 1;
    tool_output = "tar: Error opening archive: Unrecognized archive format\n";
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Not now");

    choose_help("Check for updates");

    EXPECT_THAT(messages, ElementsAre(StartsWithText("Legend of Mana Native 0.5.0 is available."),
                                      StartsWithText("The update couldn't be installed: tar.exe failed: tar: Error opening archive: Unrecognized archive format")));
    EXPECT_EQ(disk::contents(profile / "Legend of Mana Native" / "Legend of Mana Native.exe"), "the old launcher");
}

LOM_SPEC(CheckForUpdatesSpecification, "Download and install should copy a Mac app out of its disk image and detach it") {
    update_response.body = release_offering(mac_package);
    can_update_itself = true;
    update_target.kind = lom::launcher::UpdatePackage::Kind::MacBundle;
    update_target.path = profile / "Applications" / "Legend of Mana Native.app";
    update_target.staging = profile / "Applications" / ".lom-native-update";
    update_target.tools = "/usr/bin";
    files_provider_fake.add_file(profile / "Applications" / "Legend of Mana Native.app" / "Contents" / "Info.plist", "the old launcher");
    release_package = "the new launcher's disk image";
    release_checksums = checksum_line(release_package, mac_package);
    tool_does = [this](const ProgramRun& run) {
        if (run.program.endsWith(QLatin1String("ditto"))) {
            files_provider_fake.add_file(lom::launcher::AppPaths::to_path(run.arguments.value(1)) / "Contents" / "Info.plist", "the new launcher");
        }
    };
    open_launcher();
    given_the_next_message_is_answered_with("Download and install");
    given_the_next_message_is_answered_with("Later");

    choose_help("Check for updates");

    const QString staging = shown(profile / "Applications" / ".lom-native-update");
    ASSERT_EQ(tool_runs.size(), 3u);
    EXPECT_EQ(tool_runs[0].program, shown(std::filesystem::path("/usr/bin") / "hdiutil"));
    EXPECT_EQ(tool_runs[0].arguments, (QStringList{"attach", shown(profile / "Applications" / ".lom-native-update" / mac_package.toStdString()), "-nobrowse",
                                                   "-readonly", "-noautoopen", "-mountpoint", shown(profile / "Applications" / ".lom-native-update" / "mount")}));
    EXPECT_EQ(tool_runs[1].program, shown(std::filesystem::path("/usr/bin") / "ditto"));
    EXPECT_EQ(tool_runs[1].arguments,
              (QStringList{shown(profile / "Applications" / ".lom-native-update" / "mount" / "Legend of Mana Native.app"),
                           shown(profile / "Applications" / ".lom-native-update" / "Legend of Mana Native.app")}));
    EXPECT_EQ(tool_runs[2].program, shown(std::filesystem::path("/usr/bin") / "hdiutil"));
    EXPECT_EQ(tool_runs[2].arguments, (QStringList{"detach", shown(profile / "Applications" / ".lom-native-update" / "mount"), "-force"}));
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.app" / "Contents" / "Info.plist"), "the new launcher");
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
    EXPECT_TRUE(downloads.empty());
}

LOM_SPEC(CheckForUpdatesSpecification, "Opening the launcher should clear away what an earlier update left") {
    can_update_itself = true;
    files_provider_fake.add_file(profile / "Applications" / "Legend of Mana Native.AppImage.old-update", "the launcher before the last update");
    files_provider_fake.add_file(profile / "Applications" / ".lom-native-update" / "lom-native-0.4.0-linux-x86_64.AppImage", "a half-finished download");

    open_launcher();

    EXPECT_FALSE(disk::exists(profile / "Applications" / "Legend of Mana Native.AppImage.old-update"));
    EXPECT_FALSE(disk::exists(profile / "Applications" / ".lom-native-update"));
    EXPECT_EQ(disk::contents(profile / "Applications" / "Legend of Mana Native.AppImage"), "the old launcher");
}
