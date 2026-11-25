#include "../../support/synthetic_card.h"
#include "../../support/synthetic_disc.h"
#include "launcher_spec.h"
#include "window/launcher_window.h"
#include "spec.h"
#include "spec_matchers.h"
#include "spec_printers.h"

#include <filesystem>
#include <fstream>

using namespace lom::launcher::test;

namespace {
struct ChooseSaveFolderSpecification : LauncherSpec {
    /**
     * @brief Set up the game and an empty folder to choose.
     */
    ChooseSaveFolderSpecification() {
        given_the_game_is_set_up_from(given_an_extracted_folder("Legend of Mana"));
        std::filesystem::create_directories(saves);
        save_folder_choice = shown(saves);
    }

    std::filesystem::path saves = profile / "My saves";
};

}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should remember it for the edition") {
    open_launcher();

    press("browseSavesButton");

    EXPECT_TRUE(json_is(settings_member("save_roots"), R"({"us": "{profile}/My saves"})"));
    EXPECT_EQ(text_of("saveFolderPath"), shown(saves));
}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should offer its parent folder when a memory card slot folder is chosen") {
    std::filesystem::create_directories(saves / "slot0");
    save_folder_choice = shown(saves / "slot0");
    open_launcher();

    press("browseSavesButton");

    EXPECT_EQ(heading(), "Choose the save folder itself");
    EXPECT_EQ(text_of("problemBody"), "You selected a slot folder. Use its parent folder instead?\n" + shown(saves));
}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should keep the previous folder when the folder holds saves from another edition") {
    std::filesystem::create_directories(saves / "slot0");
    SyntheticDisc::write(saves / "slot0" / "BISLPS-021700-1", SyntheticCard::save(7));
    open_launcher();

    press("browseSavesButton");

    EXPECT_EQ(heading(), "This save folder can't be used");
    EXPECT_THAT(text_of("problemBody"),
                StartsWithText("It contains saves from another edition of the game. Keep NTSC-U and NTSC-J saves in separate folders."));
    EXPECT_TRUE(settings_member("save_roots").as_object().empty());
}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should keep the previous folder when the picker is cancelled") {
    save_folder_choice.clear();
    open_launcher();

    press("browseSavesButton");

    EXPECT_EQ(heading(), "Ready to play");
    EXPECT_EQ(text_of("saveFolderPath"), shown(data_directory / "cards"));
}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should accept a folder that already holds saves for this edition") {
    std::filesystem::create_directories(saves / "slot0");
    SyntheticDisc::write(saves / "slot0" / "BASLUS-010130-1", SyntheticCard::save(7));
    open_launcher();

    press("browseSavesButton");

    EXPECT_TRUE(json_is(settings_member("save_roots"), R"({"us": "{profile}/My saves"})"));
}

LOM_SPEC(ChooseSaveFolderSpecification, "Choosing a save folder should keep the previous folder when a slot holds a file that isn't a save") {
    std::filesystem::create_directories(saves / "slot1");
    std::ofstream(saves / "slot1" / "notes.txt") << "notes";
    open_launcher();

    press("browseSavesButton");

    EXPECT_EQ(heading(), "This save folder can't be used");
    EXPECT_THAT(text_of("problemBody"), StartsWithText("The game could not read it: the slot1 folder contains notes.txt, which is not a memory card save"));
    EXPECT_TRUE(settings_member("save_roots").as_object().empty());
}
