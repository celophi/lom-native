#include "../../support/synthetic_card.h"
#include "../../support/synthetic_disc.h"
#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_printers.h"

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace lom::launcher::test;

namespace {

constexpr char first_save[] = "BASLUS-010130-1";
constexpr char second_save[] = "BASLUS-010131-2";

struct ImportMemoryCardSpecification : LauncherSpec {
    /**
     * @brief Set up the game and the default card.
     */
    ImportMemoryCardSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
        card.add(first_save, {0, 1}, SyntheticCard::save(1));
        card.add(second_save, {5, 3}, SyntheticCard::save(2));
        write_card();
        memory_card_choice = shown(card_file);
    }

    /**
     * @brief Write the card to its file.
     */
    void write_card() {
        SyntheticDisc::write(card_file, card.bytes());
    }

    /**
     * @brief Answer the import dialog by pressing Import, optionally choosing to replace existing saves.
     * @param replace Whether to tick "Replace the existing save".
     */
    void given_the_import_is_confirmed(bool replace) {
        given_the_next_dialog_is_answered([replace](QWidget* dialog) {
            if (replace) {
                for (QCheckBox* box : dialog->findChildren<QCheckBox*>()) {
                    if (box->text() == "Replace the existing save (it will be backed up)") {
                        box->setChecked(true);
                    }
                }
            }

            for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
                if (button->text() == "Import") {
                    button->click();
                }
            }
        });
    }

    /**
     * @brief Read a file.
     * @param path File.
     * @return Bytes.
     */
    static std::vector<std::uint8_t> bytes_of(const std::filesystem::path& path) {
        const auto in_file = disk::read(path, std::ios::binary);
        std::istream& in = *in_file;
        return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    SyntheticCard card;
    std::filesystem::path card_file = profile / "Old saves.mcr";
    std::filesystem::path slot0 = data_directory / "cards" / "slot0";
};

}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should copy its Legend of Mana saves into memory card 1 of the save folder") {
    open_launcher();
    given_the_import_is_confirmed(false);

    press("importCardButton");

    EXPECT_EQ(bytes_of(slot0 / first_save), SyntheticCard::save(1));
    EXPECT_EQ(bytes_of(slot0 / second_save), SyntheticCard::save(2));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should say how many saves were imported and where") {
    open_launcher();
    given_the_import_is_confirmed(false);

    press("importCardButton");

    EXPECT_EQ(text_of("readyStatus"), "Imported 2 save(s) into:\n" + shown(data_directory / "cards"));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should leave the memory card file unchanged") {
    open_launcher();
    given_the_import_is_confirmed(false);

    press("importCardButton");

    EXPECT_EQ(bytes_of(card_file), card.bytes());
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should show saves already in the save folder as already there and offer no import") {
    disk::create_directories(slot0);
    SyntheticDisc::write(slot0 / first_save, SyntheticCard::save(1));
    SyntheticDisc::write(slot0 / second_save, SyntheticCard::save(2));
    open_launcher();
    QStringList shown_statuses;
    bool import_offered = true;
    given_the_next_dialog_is_answered([&](QWidget* dialog) {
        auto* list = dialog->findChild<QTreeWidget*>();

        for (int row = 0; list != nullptr && row < list->topLevelItemCount(); row++) {
            shown_statuses << list->topLevelItem(row)->text(0) + ": " + list->topLevelItem(row)->text(2);
        }

        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Import") {
                import_offered = button->isEnabled();
            }
        }

        static_cast<QDialog*>(dialog)->reject();
    });

    press("importCardButton");

    EXPECT_EQ(shown_statuses, QStringList({QString(first_save) + ": Already in your save folder", QString(second_save) + ": Already in your save folder"}));
    EXPECT_FALSE(import_offered);
    EXPECT_TRUE(text_of("readyStatus").isEmpty());
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should keep the existing save when a different save has the same name") {
    disk::create_directories(slot0);
    SyntheticDisc::write(slot0 / first_save, SyntheticCard::save(99));
    open_launcher();
    given_the_import_is_confirmed(false);

    press("importCardButton");

    EXPECT_EQ(bytes_of(slot0 / first_save), SyntheticCard::save(99));
    EXPECT_EQ(bytes_of(slot0 / second_save), SyntheticCard::save(2));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should back up and replace the existing save when I choose to replace it") {
    disk::create_directories(slot0);
    SyntheticDisc::write(slot0 / first_save, SyntheticCard::save(99));
    open_launcher();
    given_the_import_is_confirmed(true);

    press("importCardButton");

    EXPECT_EQ(bytes_of(slot0 / first_save), SyntheticCard::save(1));
    EXPECT_EQ(bytes_of(data_directory / "cards" / "backups" / "1792065600-1" / "slot0" / first_save), SyntheticCard::save(99));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should not import saves from the other edition") {
    card.add("BISLPS-021700-1", {8, 9}, SyntheticCard::save(3));
    write_card();
    open_launcher();
    given_the_import_is_confirmed(false);

    press("importCardButton");

    EXPECT_TRUE(disk::exists(slot0 / first_save));
    EXPECT_FALSE(disk::exists(slot0 / "BISLPS-021700-1"));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should explain that the card can't be read when it is a DexDrive file") {
    std::vector<std::uint8_t> dexdrive(131072 + 3904, 0);
    std::copy_n("123-456-STD", 11, dexdrive.begin());
    SyntheticDisc::write(profile / "Card.gme", dexdrive);
    memory_card_choice = shown(profile / "Card.gme");
    open_launcher();

    press("importCardButton");

    EXPECT_EQ(messages, std::vector<QString>{"this is a DexDrive (.gme) card, which is not supported yet"});
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should say it has no Legend of Mana saves when it only holds other games") {
    card = SyntheticCard();
    card.add("BASLUS-00001DATA", {0}, SyntheticDisc::pattern(8192, 5));
    write_card();
    open_launcher();

    press("importCardButton");

    EXPECT_EQ(messages, std::vector<QString>{"This memory card has no Legend of Mana saves."});
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should show a damaged save as damaged and not import it") {
    std::vector<std::uint8_t> damaged = SyntheticCard::save(1);
    damaged[100] ^= 1;
    card = SyntheticCard();
    card.add(first_save, {0, 1}, damaged);
    card.add(second_save, {5, 3}, SyntheticCard::save(2));
    write_card();
    open_launcher();
    QStringList shown_statuses;
    given_the_next_dialog_is_answered([&](QWidget* dialog) {
        auto* list = dialog->findChild<QTreeWidget*>();

        for (int row = 0; list != nullptr && row < list->topLevelItemCount(); row++) {
            shown_statuses << list->topLevelItem(row)->text(0) + ": " + list->topLevelItem(row)->text(2);
        }

        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Import") {
                button->click();
            }
        }
    });

    press("importCardButton");

    EXPECT_EQ(shown_statuses, QStringList({QString(first_save) + ": Damaged", QString(second_save) + ": Ready to import"}));
    EXPECT_FALSE(disk::exists(slot0 / first_save));
    EXPECT_EQ(bytes_of(slot0 / second_save), SyntheticCard::save(2));
}

LOM_SPEC(ImportMemoryCardSpecification, "Importing a memory card should explain that memory card 1 is full and import nothing when the saves don't fit") {
    disk::create_directories(slot0);

    for (int i = 2; i <= 8; i++) {
        SyntheticDisc::write(slot0 / ("BASLUS-01013" + std::to_string(i) + "-" + std::to_string(i + 1)), SyntheticCard::save(40 + std::uint32_t(i)));
    }

    open_launcher();
    QStringList dialog_messages;
    given_the_next_dialog_is_answered([&](QWidget* dialog) {
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == "Import") {
                button->click();
            }
        }

        for (QLabel* label : dialog->findChildren<QLabel*>()) {
            if (label->text().contains("free blocks")) {
                dialog_messages << label->text();
            }
        }

        static_cast<QDialog*>(dialog)->reject();
    });

    press("importCardButton");

    EXPECT_EQ(dialog_messages, QStringList({"Memory card 1 does not have enough free blocks for these saves."}));
    EXPECT_FALSE(disk::exists(slot0 / first_save));
    EXPECT_FALSE(disk::exists(slot0 / second_save));
}
