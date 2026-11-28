#include "launcher_spec.h"
#include "lom/launcher/common/atomic_file.h"
#include "window/app_paths.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

#include <string>

using namespace lom::launcher;
using namespace lom::launcher::test;

namespace {
struct ManageModsSpecification : LauncherSpec {
    /**
     * @brief Set up the game, with an empty mods folder in the launcher's data
     *        folder.
     */
    ManageModsSpecification() {
        folder = given_an_extracted_folder("Legend of Mana");
        given_the_game_is_set_up_from(folder);
        mods = data_directory / "mods";
    }

    /**
     * @brief Put a mod in a folder.
     * @param parent Where its folder goes.
     * @param folder_name Its folder's name.
     * @param id The id its mod.json gives.
     * @param api The mod API it says it needs.
     * @return Its folder.
     */
    std::filesystem::path given_a_mod(const std::filesystem::path& parent, const std::string& folder_name, const std::string& id, int api = 1) {
        const std::filesystem::path mod = parent / folder_name;
        const std::string json = "{\"id\": \"" + id + "\", \"name\": \"Mod " + id + "\", \"version\": \"1.0.0\", \"author\": \"tests\", " +
                                 "\"game\": \"SLUS-01013\", \"api\": " + std::to_string(api) + "}\n";
        EXPECT_TRUE(AtomicFile::write(mod / "mod.json", json, nullptr));
        return mod;
    }

    /**
     * @brief Open the Mods window and do something in it, then close it.
     * @param action What to do with the window.
     */
    void in_the_mods_window(std::function<void(QDialog* dialog, QListWidget* list)> action) {
        given_the_next_dialog_is_answered([action](QWidget* dialog) {
            auto* list = dialog->findChild<QListWidget*>("modList");
            ASSERT_NE(list, nullptr);
            action(static_cast<QDialog*>(dialog), list);
            static_cast<QDialog*>(dialog)->reject();
        });
        press("modsButton");
    }

    /**
     * @brief Read mods.ini.
     * @return What it says, or empty when it isn't there.
     */
    std::string enabled_list() const {
        return AtomicFile::read(mods / "mods.ini", nullptr).value_or(std::string());
    }

    std::filesystem::path folder;
    std::filesystem::path mods;
};
}

LOM_SPEC(ManageModsSpecification, "Mods should list the mods folder and turning one on should save it for the game") {
    given_a_mod(mods, "first", "first");
    given_a_mod(mods, "second", "second");
    open_launcher();

    in_the_mods_window([](QDialog*, QListWidget* list) {
        ASSERT_EQ(list->count(), 2);
        EXPECT_EQ(list->item(0)->checkState(), Qt::Unchecked);
        list->item(1)->setCheckState(Qt::Checked);
    });

    EXPECT_EQ(enabled_list(), "second\n");
}

LOM_SPEC(ManageModsSpecification, "A seed typed while a mod that asks for one is on should be saved for new games") {
    ASSERT_TRUE(AtomicFile::write(mods / "shuffler" / "mod.json",
                                  "{\"id\": \"shuffler\", \"name\": \"Shuffler\", \"version\": \"1.0.0\", \"game\": \"SLUS-01013\", \"api\": 5, "
                                  "\"seed\": true, \"scripts\": [\"scripts/main.lua\"]}\n",
                                  nullptr));
    ASSERT_TRUE(AtomicFile::write(mods / "shuffler" / "scripts" / "main.lua", "return 1\n", nullptr));
    open_launcher();

    in_the_mods_window([](QDialog* dialog, QListWidget* list) {
        auto* row = dialog->findChild<QWidget*>("modSeedRow");
        auto* seed = dialog->findChild<QLineEdit*>("modSeed");
        ASSERT_NE(row, nullptr);
        ASSERT_NE(seed, nullptr);
        EXPECT_TRUE(row->isHidden());

        list->item(0)->setCheckState(Qt::Checked);
        EXPECT_FALSE(row->isHidden());
        seed->setText("4294967295");
        emit seed->editingFinished();
    });

    EXPECT_EQ(enabled_list(), "shuffler\nseed=4294967295\n");

    in_the_mods_window([](QDialog* dialog, QListWidget*) {
        auto* seed = dialog->findChild<QLineEdit*>("modSeed");
        ASSERT_NE(seed, nullptr);
        EXPECT_EQ(seed->text(), "4294967295");
        seed->clear();
        emit seed->editingFinished();
    });

    EXPECT_EQ(enabled_list(), "shuffler\n");

    in_the_mods_window([](QDialog* dialog, QListWidget*) {
        auto* seed = dialog->findChild<QLineEdit*>("modSeed");
        ASSERT_NE(seed, nullptr);
        seed->setText("12345");
    });

    EXPECT_EQ(enabled_list(), "shuffler\nseed=12345\n");
}

LOM_SPEC(ManageModsSpecification, "Moving a mod up should change the order the game loads them in") {
    given_a_mod(mods, "first", "first");
    given_a_mod(mods, "second", "second");
    ASSERT_TRUE(AtomicFile::write(mods / "mods.ini", "first\nsecond\n", nullptr));
    open_launcher();

    in_the_mods_window([](QDialog* dialog, QListWidget* list) {
        list->setCurrentRow(1);
        dialog->findChild<QPushButton*>("moveModUpButton")->click();
        EXPECT_EQ(list->currentRow(), 0);
    });

    EXPECT_EQ(enabled_list(), "second\nfirst\n");
}

LOM_SPEC(ManageModsSpecification, "A mod the game would refuse should say why and have no checkbox") {
    given_a_mod(mods, "too-new", "too-new", 99);
    open_launcher();

    in_the_mods_window([](QDialog* dialog, QListWidget* list) {
        ASSERT_EQ(list->count(), 1);
        EXPECT_FALSE(list->item(0)->flags() & Qt::ItemIsUserCheckable);
        list->setCurrentRow(0);
        EXPECT_TRUE(dialog->findChild<QLabel*>("modDetails")->text().contains("needs a newer lom-native"));
    });
}

LOM_SPEC(ManageModsSpecification, "Adding a mod should copy its folder in under its id, turned off") {
    const std::filesystem::path downloads = data_directory / "downloads";
    mod_folder_choice = AppPaths::to_text(given_a_mod(downloads, "Cool Mod v2", "cool-mod"));
    open_launcher();

    in_the_mods_window([](QDialog* dialog, QListWidget* list) {
        dialog->findChild<QPushButton*>("addModButton")->click();
        ASSERT_EQ(list->count(), 1);
        EXPECT_EQ(list->item(0)->checkState(), Qt::Unchecked);
    });

    EXPECT_TRUE(disk::exists(mods / "cool-mod" / "mod.json"));
    EXPECT_EQ(enabled_list(), "");
}

LOM_SPEC(ManageModsSpecification, "Removing a mod should ask, then delete its folder and turn it off") {
    given_a_mod(mods, "unwanted", "unwanted");
    ASSERT_TRUE(AtomicFile::write(mods / "mods.ini", "unwanted\n", nullptr));
    open_launcher();

    given_the_next_dialog_is_answered([](QWidget* dialog) {
        auto* list = dialog->findChild<QListWidget*>("modList");
        ASSERT_NE(list, nullptr);
        list->setCurrentRow(0);
        QTimer::singleShot(0, dialog->findChild<QPushButton*>("removeModButton"), &QPushButton::click);
    });
    given_the_next_message_is_answered_with("Yes");
    given_the_next_dialog_is_answered([](QWidget* dialog) {
        EXPECT_EQ(dialog->findChild<QListWidget*>("modList")->count(), 0);
        static_cast<QDialog*>(dialog)->reject();
    });
    press("modsButton");

    EXPECT_FALSE(disk::exists(mods / "unwanted"));
    EXPECT_EQ(enabled_list(), "");
}

LOM_SPEC(ManageModsSpecification, "Play should wait until one of two mods that can't be on together is off") {
    ASSERT_TRUE(AtomicFile::write(mods / "author.a" / "mod.json",
                                  "{\"id\": \"author.a\", \"name\": \"Mod A\", \"version\": \"1.0.0\", \"game\": \"SLUS-01013\", \"api\": 2, "
                                  "\"incompatible\": {\"author.b\": \"*\"}}\n",
                                  nullptr));
    ASSERT_TRUE(AtomicFile::write(mods / "author.b" / "mod.json",
                                  "{\"id\": \"author.b\", \"name\": \"Mod B\", \"version\": \"1.0.0\", \"game\": \"SLUS-01013\", \"api\": 2}\n", nullptr));
    ASSERT_TRUE(AtomicFile::write(mods / "mods.ini", "author.a\nauthor.b\n", nullptr));
    open_launcher();

    press("playButton");

    EXPECT_EQ(heading(), "Some mods can't be on together");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("Mod A and Mod B"));
    EXPECT_TRUE(game_launches.empty());

    given_the_next_dialog_is_answered([](QWidget* dialog) {
        auto* list = dialog->findChild<QListWidget*>("modList");
        ASSERT_NE(list, nullptr);
        ASSERT_EQ(list->count(), 2);
        EXPECT_THAT(list->item(0)->text(), ContainsText("(can't be on with another mod)"));
        list->setCurrentRow(1);
        EXPECT_TRUE(dialog->findChild<QLabel*>("modDetails")->text().contains("It can't be on with author.a."));
        list->item(1)->setCheckState(Qt::Unchecked);
        EXPECT_FALSE(list->item(0)->text().contains("(can't be on with another mod)"));
        static_cast<QDialog*>(dialog)->reject();
    });
    press("problemPrimaryButton");
    press("playButton");

    EXPECT_EQ(game_launches.size(), 1u);
}
