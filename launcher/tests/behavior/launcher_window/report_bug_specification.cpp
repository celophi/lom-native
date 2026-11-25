#include "launcher_spec.h"
#include "lom/launcher/common/zip_writer.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QPushButton>
#include <QUrlQuery>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

using namespace lom::launcher::test;

namespace {
struct ZipEntry {
    std::uint32_t crc = 0;
    std::uint32_t size = 0;
};

struct ReportBugSpecification : LauncherSpec {
    /**
     * @brief Set up the game from an extracted folder.
     */
    ReportBugSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
    }

    /** The run Play starts at the specs' fixed time. */
    std::filesystem::path run() const {
        return data_directory / "logs" / "game-20261015-120000";
    }

    /** The zip a report made at the specs' fixed time is saved as. */
    std::filesystem::path report_zip() const {
        return data_directory / "bug-reports" / "lom-native-bug-20261015-120000.zip";
    }

    /**
     * @brief Write a file, making its folder.
     * @param path File.
     * @param text Contents.
     */
    static void write(const std::filesystem::path& path, const std::string& text) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
    }

    /**
     * @brief Leave behind what the game writes for a run, as if it had played.
     * @param system The run's system report.
     */
    void given_the_game_left_its_reports(const std::string& system) {
        write(std::filesystem::path(run().u8string() + ".log"), "loaded " + home() + "/games/lom\n");
        write(std::filesystem::path(run().u8string() + "-system.txt"), system);
        write(std::filesystem::path(run().u8string() + "-crash.txt"), "Legend of Mana Native crash report\n");
    }

    /** The home folder, as the launcher sees it. */
    static std::string home() {
        return QDir::homePath().toStdString();
    }

    /**
     * @brief Read the zip's directory.
     * @param path Zip file.
     * @return Each file's checksum and size, by name.
     */
    static std::map<std::string, ZipEntry> zip_entries(const std::filesystem::path& path) {
        std::ifstream in(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::map<std::string, ZipEntry> entries;

        if (bytes.size() < 22) {
            return entries;
        }

        const auto read16 = [&](std::size_t at) {
            return std::uint32_t(std::uint8_t(bytes[at]) | (std::uint8_t(bytes[at + 1]) << 8));
        };
        const auto read32 = [&](std::size_t at) {
            return read16(at) | (read16(at + 2) << 16);
        };
        const std::size_t end = bytes.size() - 22;
        std::size_t at = read32(end + 16);

        for (std::uint32_t i = 0; i < read16(end + 10); ++i) {
            const std::uint32_t name_length = read16(at + 28);
            ZipEntry entry;
            entry.crc = read32(at + 16);
            entry.size = read32(at + 24);
            entries[bytes.substr(at + 46, name_length)] = entry;
            at += 46 + name_length + read16(at + 30) + read16(at + 32);
        }

        return entries;
    }

    /**
     * @brief Whether a zip entry holds exactly this text.
     * @param entry Entry from zip_entries().
     * @param text Expected contents.
     * @return True on a match.
     */
    static bool holds(const ZipEntry& entry, const std::string& text) {
        return entry.size == text.size() && entry.crc == lom::launcher::ZipWriter::crc32(text);
    }
};

const char crash_system_report[] = "Legend of Mana Native system report\n\n[system]\nos: Test OS 1\nsession: not recording\n\n"
                                   "[graphics]\nbackend: vulkan\ndevice: Test GPU\n";

}

LOM_SPEC(ReportBugSpecification, "Reporting a bug should save the run's logs and reports, the launcher's, and the cards in one zip") {
    open_launcher();
    press("playButton");
    write(data_directory / "cards" / "slot0" / "BASLUS-01013LOM", "save");
    given_the_game_left_its_reports(crash_system_report);
    the_game_exits(1);

    EXPECT_EQ(launcher().findChild<QPushButton*>("problemPrimaryButton")->text(), "Report this bug");
    press("problemPrimaryButton");

    ASSERT_TRUE(std::filesystem::exists(report_zip()));
    const std::map<std::string, ZipEntry> entries = zip_entries(report_zip());
    EXPECT_EQ(entries.count("report.txt"), 1u);
    EXPECT_EQ(entries.count("game/game-20261015-120000.log"), 1u);
    EXPECT_EQ(entries.count("game/game-20261015-120000-system.txt"), 1u);
    EXPECT_EQ(entries.count("game/game-20261015-120000-crash.txt"), 1u);
    EXPECT_EQ(entries.count("launcher/launcher.json"), 1u);
    EXPECT_EQ(entries.count("session/inputs.bin"), 0u);
    ASSERT_EQ(entries.count("cards/slot0/BASLUS-01013LOM"), 1u);
    EXPECT_TRUE(holds(entries.at("cards/slot0/BASLUS-01013LOM"), "save"));
    EXPECT_TRUE(holds(entries.at("game/game-20261015-120000.log"), "loaded ~/games/lom\n"));
    EXPECT_EQ(heading(), "Bug report saved");
}

LOM_SPEC(ReportBugSpecification, "Reporting a bug should open a new GitHub issue with what's known filled in, and show the zip") {
    open_launcher();
    press("playButton");
    given_the_game_left_its_reports(crash_system_report);
    the_game_exits(1);

    press("problemPrimaryButton");

    ASSERT_EQ(opened_pages.size(), 1u);
    const QUrl page = opened_pages.front();
    EXPECT_EQ(page.toString(QUrl::RemoveQuery), "https://github.com/example/lom/issues/new");
    const QUrlQuery query(page);
    EXPECT_EQ(query.queryItemValue("template"), "bug_report.yml");
    EXPECT_EQ(query.queryItemValue("version"), "0.4.0");
    EXPECT_EQ(query.queryItemValue("os"), "Test OS 1");
    EXPECT_EQ(query.queryItemValue("gpu"), "Test GPU");
    EXPECT_EQ(opened_folders, std::vector<QString>{shown(data_directory / "bug-reports")});
    EXPECT_EQ(text_of("problemBody"), "Everything needed to look into it is in:\n" + shown(report_zip()) +
                                          "\n\nYour browser is opening a new issue on GitHub. Drag the zip into it and say what you were doing "
                                          "when it happened. Nothing is sent until you submit the issue.");
}

LOM_SPEC(ReportBugSpecification, "Reporting a bug should include the run's session recording when there is one") {
    open_launcher();
    press("playButton");
    const std::filesystem::path session = data_directory / "sessions" / "session-20261015-120000-1";
    write(session / "inputs.bin", "LOMSESS1");
    write(session / "end", "signal 11\n");
    given_the_game_left_its_reports("[system]\nsession: " + session.u8string() + "\n");
    the_game_exits(1);

    press("problemPrimaryButton");

    const std::map<std::string, ZipEntry> entries = zip_entries(report_zip());
    ASSERT_EQ(entries.count("session/inputs.bin"), 1u);
    EXPECT_TRUE(holds(entries.at("session/inputs.bin"), "LOMSESS1"));
    EXPECT_TRUE(holds(entries.at("session/end"), "signal 11\n"));
}

LOM_SPEC(ReportBugSpecification, "Reporting a bug should be offered when the game's graphics didn't start") {
    open_launcher();
    press("playButton");
    given_the_game_left_its_reports(crash_system_report);
    the_game_exits(4);

    EXPECT_EQ(heading(), "The game could not start its graphics");
    press("problemPrimaryButton");

    EXPECT_TRUE(std::filesystem::exists(report_zip()));
    EXPECT_EQ(heading(), "Bug report saved");
}

LOM_SPEC(ReportBugSpecification, "Reporting a bug from Help should report the last run") {
    open_launcher();
    press("playButton");
    given_the_game_left_its_reports(crash_system_report);
    the_game_exits(0);

    choose_help("Report a bug");

    const std::map<std::string, ZipEntry> entries = zip_entries(report_zip());
    EXPECT_EQ(entries.count("game/game-20261015-120000-system.txt"), 1u);
    EXPECT_EQ(heading(), "Bug report saved");
}

LOM_SPEC(ReportBugSpecification, "Reporting a bug from Help should say there's nothing to report before the game was played") {
    open_launcher();

    choose_help("Report a bug");

    EXPECT_EQ(heading(), "There's nothing to report yet");
    EXPECT_FALSE(std::filesystem::exists(data_directory / "bug-reports"));
    EXPECT_TRUE(opened_pages.empty());
}

LOM_SPEC(ReportBugSpecification, "Recording play sessions should be off until I turn it on in settings") {
    open_launcher();

    given_the_next_dialog_is_answered([](QWidget* dialog) {
        for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
            if (box->text() == "Record play sessions for bug reports") {
                EXPECT_FALSE(box->isChecked());
                box->setChecked(true);
            }
        }

        static_cast<QDialog*>(dialog)->reject();
    });
    press("settingsButton");
    press("playButton");

    ASSERT_EQ(game_launches.size(), 1u);
    EXPECT_EQ(game_launches.front().environment.value("LOM_RECORD"), "1");
    EXPECT_EQ(game_launches.front().environment.value("LOM_SESSION_DIR"), shown(data_directory / "sessions"));
}

LOM_SPEC(ReportBugSpecification, "Playing should keep the last ten runs' logs and reports together") {
    for (int day = 1; day <= 11; ++day) {
        const std::string run = (data_directory / "logs" / ("game-202610" + std::string(day < 10 ? "0" : "") + std::to_string(day) + "-080000")).u8string();
        write(run + ".log", "log");
        write(run + "-system.txt", "system");
    }

    open_launcher();
    press("playButton");

    EXPECT_FALSE(std::filesystem::exists(data_directory / "logs" / "game-20261001-080000.log"));
    EXPECT_FALSE(std::filesystem::exists(data_directory / "logs" / "game-20261001-080000-system.txt"));
    EXPECT_TRUE(std::filesystem::exists(data_directory / "logs" / "game-20261002-080000.log"));
    EXPECT_TRUE(std::filesystem::exists(data_directory / "logs" / "game-20261002-080000-system.txt"));
}
