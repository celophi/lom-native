#ifndef LOM_LAUNCHER_UI_TEXT_H
#define LOM_LAUNCHER_UI_TEXT_H

#include <QString>
#include <QStringList>

#include <vector>

namespace lom::launcher::ui_text {
class Text {
  public:
    /**
     * @brief Declare a text; it joins all().
     * @param key Its name, which the translations use.
     * @param english The English.
     */
    Text(const char* key, const char* english);

    /** @brief The text in the language chosen. */
    operator QString() const;

    /** @brief Its name. */
    const char* key() const {
        return key_;
    }

    /** @brief The English. */
    const char* english() const {
        return english_;
    }

  private:
    const char* key_;
    const char* english_;
};

/** @brief Every text, in the order declared. */
const std::vector<const Text*>& all();

struct Language {
    const char* code;
    const char* name;
};

/** @brief The languages, English first, in the Remaster's order. */
const std::vector<Language>& languages();

/**
 * @brief Use a language from now on.
 * @param code One of languages(), or empty for the system's; anything else is English.
 */
void set_language(const QString& code);

/** @brief The language in use. */
QString language();

/** @brief The language of the system (QLocale's default), or English if the launcher hasn't it. */
QString system_language();

/**
 * @brief The first of some languages the launcher has.
 * @param preferred Languages, best first, as QLocale::uiLanguages() gives them ("fr-CA", "zh-Hant-TW").
 * @return Its code, or English if it has none of them.
 */
QString language_for(const QStringList& preferred);

/**
 * @brief A translation's texts, by name, as its file has them.
 * @param code One of languages() other than English.
 * @return The texts, empty if there's no such file.
 */
std::vector<std::pair<QString, QString>> translation(const QString& code);

#define LOM_UI_TEXT(name, english)                                                                                                                             \
    inline const Text name {                                                                                                                                   \
        #name, english                                                                                                                                         \
    }

/** The app's own name, in the title bar, the top bar and the cards. Not the game's name on its own. */
LOM_UI_TEXT(window_title, "Legend of Mana Native");

LOM_UI_TEXT(notice_title, "Own the game before you play");
LOM_UI_TEXT(notice_body, "To use this application, you must have legally purchased and own a supported original "
                         "PlayStation copy of Legend of Mana.<br><br>"
                         "This application does not include the game, its assets, or a disc image. You must supply your "
                         "own game files from a supported NTSC-U or NTSC-J copy.<br><br>"
                         "If you do not own the game, buy a copy before continuing. Please support the game and its "
                         "creators. <b>Don't pirate it. Buy the game.</b>");
LOM_UI_TEXT(notice_accept, "I legally own the game");
LOM_UI_TEXT(notice_quit, "Quit");
LOM_UI_TEXT(notice_save_failed, "Your answer could not be saved");
LOM_UI_TEXT(notice_save_failed_body, "The launcher could not save that you acknowledged the notice:\n%1\n\n"
                                     "Try again, or quit and check that your user folder is writable.");
LOM_UI_TEXT(retry, "Try again");

LOM_UI_TEXT(welcome_heading, "Welcome");
LOM_UI_TEXT(welcome_body, "Choose your game disc image to get started.");
LOM_UI_TEXT(image_label, "Disc image");
LOM_UI_TEXT(image_placeholder, "No disc image selected");
LOM_UI_TEXT(browse, "Browse...");
LOM_UI_TEXT(use_folder, "Use an already extracted folder");
LOM_UI_TEXT(destination_label, "A disc image's files are copied to:");
LOM_UI_TEXT(change_folder, "Change folder...");
LOM_UI_TEXT(image_dialog_title, "Choose a Legend of Mana disc image");
LOM_UI_TEXT(image_filter, "Disc images (*.iso *.bin *.cue *.ISO *.BIN *.CUE);;All files (*)");
LOM_UI_TEXT(folder_dialog_title, "Choose the folder containing the extracted game");
LOM_UI_TEXT(games_root_dialog_title, "Choose the folder disc images are copied to");

LOM_UI_TEXT(checking_image, "Checking disc image...");
LOM_UI_TEXT(extracting, "Extracting game files...");
LOM_UI_TEXT(verifying, "Verifying files...");
LOM_UI_TEXT(checking_folder, "Checking all game files...");
LOM_UI_TEXT(checking_installed, "Checking your game files...");
LOM_UI_TEXT(recognized_edition, "Recognized: %1");
LOM_UI_TEXT(progress_bytes, "%1 of %2");
LOM_UI_TEXT(cancel, "Cancel");

LOM_UI_TEXT(ready_heading, "Ready to play");
LOM_UI_TEXT(installed_edition, "%1 installed");
LOM_UI_TEXT(play, "Play");
LOM_UI_TEXT(playing, "Playing...");
LOM_UI_TEXT(settings, "Settings");
LOM_UI_TEXT(game_files, "Game files");
LOM_UI_TEXT(save_folder_label, "Save folder");
LOM_UI_TEXT(save_folder_note, "The game loads and saves here.");
LOM_UI_TEXT(open_folder, "Open folder");
LOM_UI_TEXT(import_card, "Import memory card...");
LOM_UI_TEXT(save_dialog_title, "Choose your save folder (the folder containing slot0 and slot1)");
LOM_UI_TEXT(card_dialog_title, "Choose an emulator memory card");
LOM_UI_TEXT(card_filter, "Memory cards (*.mcr *.mcd *.MCR *.MCD);;All files (*)");

LOM_UI_TEXT(unsupported_title, "Unsupported game version");
LOM_UI_TEXT(unsupported_body, "The selected game data does not match a supported Legend of Mana release.\n"
                              "Supported releases: NTSC-U (SLUS-01013) and NTSC-J (SLPS-02170).\n"
                              "PAL versions and modified disc images are not supported.\n\nChoose another game source.");
LOM_UI_TEXT(choose_another, "Choose another source");
LOM_UI_TEXT(back, "Back");
LOM_UI_TEXT(details, "Details");
LOM_UI_TEXT(missing_track_title, "A disc file is missing");
LOM_UI_TEXT(missing_track_body, "The CUE file refers to a BIN file that is not there. Keep the CUE and BIN files together "
                                "in the same folder, then choose the CUE again.");
LOM_UI_TEXT(unreadable_title, "The file could not be read");
LOM_UI_TEXT(unreadable_body, "The selected file could not be read completely. Check that it is still available and try again.");
LOM_UI_TEXT(malformed_title, "This is not a usable disc image");
LOM_UI_TEXT(malformed_body, "The selected file is not a disc image in a form this launcher understands.");
LOM_UI_TEXT(layout_title, "Unsupported disc layout");
LOM_UI_TEXT(layout_body, "This disc image uses a track layout that is not supported. Use the original single-track "
                         "BIN and CUE files.");
LOM_UI_TEXT(runtime_title, "This edition can't be played yet");
LOM_UI_TEXT(runtime_body, "%1 was verified, but this build does not include a game runtime for it yet.");
LOM_UI_TEXT(region_title, "This edition can't be verified yet");
LOM_UI_TEXT(region_body, "%1 was recognized, but there is no trusted file list for it yet, so its files cannot be verified.");
LOM_UI_TEXT(space_title, "Not enough free space");
LOM_UI_TEXT(space_body, "Extracting the game needs about %1 of free space in:\n%2\n\nFree some space or choose another folder.");
LOM_UI_TEXT(destination_title, "The game files folder can't be used");
LOM_UI_TEXT(destination_body, "The launcher could not write to the game files folder:\n%1\n\nChoose another folder or try again.");
LOM_UI_TEXT(extraction_title, "The game files could not be prepared");
LOM_UI_TEXT(extraction_body, "Extracting or verifying the game files failed. Your previous game files, if any, are unchanged.");
LOM_UI_TEXT(source_changed_title, "The disc image changed");
LOM_UI_TEXT(source_changed_body, "The disc image changed while it was being used. Try again once nothing else is using it.");
LOM_UI_TEXT(not_game_folder_title, "This folder doesn't contain the game");
LOM_UI_TEXT(not_game_folder_body, "Choose the folder that contains SLUS_010.13 or SLPS_021.70 and the game's BIN folder.");
LOM_UI_TEXT(layout_names_title, "These game files can't be used in place");
LOM_UI_TEXT(layout_names_body, "The files match the supported game, but their names differ from the exact names the game "
                               "opens (for example lower case or a \";1\" suffix). Use the disc image instead, or a folder "
                               "with the original upper-case names.");
LOM_UI_TEXT(unsafe_title, "This folder contains links");
LOM_UI_TEXT(unsafe_body, "The folder contains links that point outside it, so it cannot be verified.");
LOM_UI_TEXT(folder_missing_title, "Your game files are not available");
LOM_UI_TEXT(folder_missing_body, "The game files at\n%1\ncould not be found. If they are on a removable drive, connect it "
                                 "and try again, or choose another source.");
LOM_UI_TEXT(folder_changed_title, "Your game files have changed");
LOM_UI_TEXT(folder_changed_body, "The game files at\n%1\nno longer match the supported game. Choose the source again to "
                                 "repair them.");
LOM_UI_TEXT(changed_during_title, "Files changed during the check");
LOM_UI_TEXT(changed_during_body, "A file changed while it was being checked. Try again once nothing else is using the folder.");

LOM_UI_TEXT(save_slot_title, "Choose the save folder itself");
LOM_UI_TEXT(save_slot_body, "You selected a slot folder. Use its parent folder instead?\n%1");
LOM_UI_TEXT(save_problem_title, "This save folder can't be used");
LOM_UI_TEXT(save_other_region, "It contains saves from another edition of the game. Keep NTSC-U and NTSC-J saves in separate folders.");
LOM_UI_TEXT(save_not_writable, "It is read-only, so the game could not save there.");
LOM_UI_TEXT(save_game_cannot_read, "The game could not read it: %1");
LOM_UI_TEXT(save_missing_title, "Your save folder is not available");
LOM_UI_TEXT(save_missing_body, "The save folder\n%1\ncould not be found. Connect the drive and try again, or choose another folder.");
LOM_UI_TEXT(save_kept, "Your previous save folder is still selected.");
LOM_UI_TEXT(choose_folder, "Choose folder...");

LOM_UI_TEXT(card_import_title, "Import memory card");
LOM_UI_TEXT(card_import_intro, "Choose the saves to copy into your save folder. The memory card file is not changed.");
LOM_UI_TEXT(card_column_name, "Save");
LOM_UI_TEXT(card_column_edition, "Edition");
LOM_UI_TEXT(card_column_status, "Status");
LOM_UI_TEXT(card_slot_label, "Copy into:");
LOM_UI_TEXT(card_slot_name, "Memory card %1");
LOM_UI_TEXT(card_status_ready, "Ready to import");
LOM_UI_TEXT(card_status_identical, "Already in your save folder");
LOM_UI_TEXT(card_status_conflict, "Different save with the same name");
LOM_UI_TEXT(card_replace, "Replace the existing save (it will be backed up)");
LOM_UI_TEXT(card_status_corrupt, "Damaged");
LOM_UI_TEXT(card_status_other_region, "Other edition");
LOM_UI_TEXT(card_import_button, "Import");
LOM_UI_TEXT(card_no_saves, "This memory card has no Legend of Mana saves.");
LOM_UI_TEXT(card_unsupported_title, "This memory card can't be read");
LOM_UI_TEXT(card_imported, "Imported %1 save(s) into:\n%2");
LOM_UI_TEXT(card_nothing, "Nothing was imported. Your existing saves were kept.");
LOM_UI_TEXT(card_failed_title, "The saves could not be imported");

LOM_UI_TEXT(launch_failed_title, "The game could not start");
LOM_UI_TEXT(launch_failed_body, "%1\n\nDetails were written to:\n%2");
LOM_UI_TEXT(game_crashed_title, "The game stopped unexpectedly");
LOM_UI_TEXT(game_crashed_body, "The game exited with an error (code %1).\n\nDetails were written to:\n%2");
LOM_UI_TEXT(graphics_unavailable_title, "The game could not start its graphics");
LOM_UI_TEXT(graphics_unavailable_body_windows, "It needs a graphics card and driver that support Direct3D 12. Updating your graphics driver usually fixes "
                                               "this.\n\nDetails were written to:\n%1");
LOM_UI_TEXT(graphics_unavailable_body_mac, "It needs Metal, which every Apple Silicon Mac has.\n\nDetails were written to:\n%1");
LOM_UI_TEXT(graphics_unavailable_body_linux, "It needs a graphics card and driver that support Vulkan. Updating your graphics driver usually fixes "
                                             "this.\n\nDetails were written to:\n%1");
#if defined(_WIN32)
inline const Text& graphics_unavailable_body = graphics_unavailable_body_windows;
#elif defined(__APPLE__)
inline const Text& graphics_unavailable_body = graphics_unavailable_body_mac;
#else
inline const Text& graphics_unavailable_body = graphics_unavailable_body_linux;
#endif
LOM_UI_TEXT(runtime_missing, "The game program was not found next to the launcher.");
LOM_UI_TEXT(save_locked, "Another copy of the game is already using this save folder.");
LOM_UI_TEXT(busy_title, "The game is running");
LOM_UI_TEXT(busy_body, "Close the game before changing game data or saves.");

LOM_UI_TEXT(report_this_bug, "Report this bug");
LOM_UI_TEXT(report_a_bug, "Report a bug");
LOM_UI_TEXT(show_report, "Show report");
LOM_UI_TEXT(bug_report_saved_title, "Bug report saved");
LOM_UI_TEXT(bug_report_saved_body, "Everything needed to look into it is in:\n%1");
LOM_UI_TEXT(bug_report_issue, "\n\nYour browser is opening a new issue on GitHub. Drag the zip into it and say what you were doing when it happened. "
                              "Nothing is sent until you submit the issue.");
LOM_UI_TEXT(bug_report_too_big, "\n\nThe zip is %1 MB, over GitHub's 25 MB limit for attachments. Share it another way, like a file sharing "
                                "link, and put the link in the issue.");
LOM_UI_TEXT(bug_report_failed_title, "The bug report could not be saved");
LOM_UI_TEXT(bug_report_failed_body, "This file could not be written:\n%1");
LOM_UI_TEXT(nothing_to_report_title, "There's nothing to report yet");
LOM_UI_TEXT(nothing_to_report_body, "A bug report is made from a game run. Play the game, and if something goes wrong, come back here.");
LOM_UI_TEXT(report_while_running, "Close the game first, so its logs are complete.");

LOM_UI_TEXT(settings_title, "Launcher settings");
LOM_UI_TEXT(theme_label, "Appearance");
LOM_UI_TEXT(theme_system, "Match system");
LOM_UI_TEXT(theme_light, "Light");
LOM_UI_TEXT(theme_dark, "Dark");
LOM_UI_TEXT(toggle_to_dark, "Dark mode");
LOM_UI_TEXT(toggle_to_light, "Light mode");
LOM_UI_TEXT(check_updates_on_startup, "Check for updates when the launcher opens");
LOM_UI_TEXT(record_sessions, "Record play sessions for bug reports");
LOM_UI_TEXT(record_sessions_note, "Lets a bug be replayed exactly as it happened. It takes about 16 MB an hour of play, and no more than 50 MB "
                                  "is kept.");
LOM_UI_TEXT(game_discs_label, "Game discs (at least one is required)");
LOM_UI_TEXT(game_discs_note, "Set up the North American version, the Japanese version, or both, then choose which one you want to play.");
LOM_UI_TEXT(play_edition, "Play the %1");
LOM_UI_TEXT(set_up, "Set up...");
LOM_UI_TEXT(set_up_again, "Set up again...");
LOM_UI_TEXT(show_files, "Show files");
LOM_UI_TEXT(not_set_up, "Not set up yet.");
LOM_UI_TEXT(files_at, "Files: %1");
LOM_UI_TEXT(folder_at, "Folder: %1");
LOM_UI_TEXT(games_folder_label, "Game files folder");
LOM_UI_TEXT(verify_selected, "Verify the selected game's files");
LOM_UI_TEXT(remaster_optional_label, "Legend of Mana Remaster (optional)");
LOM_UI_TEXT(set_up_edition_body, "Choose the disc image or extracted folder of the %1.");
LOM_UI_TEXT(wrong_edition_title, "That's the other version");
LOM_UI_TEXT(wrong_edition_body, "That's the %1, and this is setting up the %2. Choose the %2's disc, or go back and set the %1 up in its own row.");
LOM_UI_TEXT(remaster_in_use, "With the Remaster's art, music and text");
LOM_UI_TEXT(remaster_hint, "Own the Legend of Mana Remaster? Link it in Game files.");
LOM_UI_TEXT(use_remaster, "Use the Remaster's art, music and text");
LOM_UI_TEXT(remaster_note, "If you own the Remaster, the game can use its repainted backgrounds and pictures, its soundtracks "
                           "and its text in eight languages, read from where it's installed. Turn them on in the game's menu (F1).");
LOM_UI_TEXT(remaster_found, "Found in %1");
LOM_UI_TEXT(remaster_chosen, "Using %1");
LOM_UI_TEXT(remaster_not_found, "Not found in Steam. If it's installed somewhere else, choose its folder.");
LOM_UI_TEXT(remaster_not_it, "%1 doesn't look like the Remaster's folder: it should have windata/hdmap in it.");
LOM_UI_TEXT(choose_remaster, "Choose folder...");
LOM_UI_TEXT(find_remaster, "Look in Steam");
LOM_UI_TEXT(remaster_dialog_title, "Choose the Legend of Mana Remaster's folder");
LOM_UI_TEXT(mods, "Mods");
LOM_UI_TEXT(mods_title, "Mods");
LOM_UI_TEXT(mods_note, "Mods load when the game starts. When two mods set the same thing, the one lower in the list wins.");
LOM_UI_TEXT(mods_empty, "No mods yet. Add a mod's folder here, or copy it into the mods folder.");
LOM_UI_TEXT(mod_add, "Add mod...");
LOM_UI_TEXT(mod_remove, "Remove");
LOM_UI_TEXT(mod_up, "Move up");
LOM_UI_TEXT(mod_down, "Move down");
LOM_UI_TEXT(mod_open_folder, "Open mods folder");
LOM_UI_TEXT(mod_dialog_title, "Choose a mod's folder, the one with its mod.json");
LOM_UI_TEXT(mod_by, "by %1");
LOM_UI_TEXT(mod_example, "It comes with lom-native as an example, so you can turn it on and off but not remove it.");
LOM_UI_TEXT(mod_refused, "The game won't load it: %1.");
LOM_UI_TEXT(mod_changes_gameplay, "It changes how the game plays, so it keeps its own saves.");
LOM_UI_TEXT(mod_sets_switches, "It sets: %1.");
LOM_UI_TEXT(mod_overridden, "A mod lower in the list overrides its %1.");
LOM_UI_TEXT(mod_overrides, "It overrides %1, higher in the list.");
LOM_UI_TEXT(mod_in_game_toggle, "While it's on, you can also turn it off and on in the game's menu (F1), under Mods.");
LOM_UI_TEXT(mod_row_clash, "(can't be on with another mod)");
LOM_UI_TEXT(mod_row_unmet, "(won't run)");
LOM_UI_TEXT(mod_clashes, "It can't be on with %1. Turn one of them off; Play waits until you do.");
LOM_UI_TEXT(mod_unmet, "It won't run: %1.");
LOM_UI_TEXT(mod_warning, "Note: %1.");
LOM_UI_TEXT(mods_clash_title, "Some mods can't be on together");
LOM_UI_TEXT(mods_clash_body, "%1\n\nTurn one of each pair off in Mods, then press Play again.");
LOM_UI_TEXT(mods_clash_pair, "%1 and %2");
LOM_UI_TEXT(mod_remove_title, "Remove %1?");
LOM_UI_TEXT(mod_remove_body, "This deletes its folder from the mods folder.");
LOM_UI_TEXT(mod_replace_title, "Replace %1?");
LOM_UI_TEXT(mod_replace_body, "A mod with the same id is already installed. Replace it with the one you chose?");
LOM_UI_TEXT(mod_add_failed, "That mod couldn't be added: %1.");
LOM_UI_TEXT(mod_remove_failed, "That mod couldn't be removed: %1.");
LOM_UI_TEXT(mod_save_failed, "Which mods are on couldn't be saved: %1");
LOM_UI_TEXT(mod_seed, "Seed:");
LOM_UI_TEXT(mod_seed_placeholder, "A new one for each new game");
LOM_UI_TEXT(mod_seed_help,
            "A mod that's on uses a seed. A new game starts with this one, or a new one if it's empty, so you can play a seed someone gave you. A game keeps "
            "its seed in "
            "its saves, so changing it here doesn't change a game you've started. The game's menu (F1) shows the seed you're playing, under Mods.");
LOM_UI_TEXT(mod_seed_invalid, "A seed is a whole number from 0 to 4294967295.");
LOM_UI_TEXT(games_root_note, "Setting up from a disc image copies its files into this folder. Changing it doesn't move a game that's already set up, and an "
                             "extracted folder is used where it is.");
LOM_UI_TEXT(verified_ok, "All game files match the supported game.");
LOM_UI_TEXT(close, "Close");
LOM_UI_TEXT(help, "Help");
LOM_UI_TEXT(show_notice, "Ownership notice");
LOM_UI_TEXT(check_for_updates, "Check for updates");
LOM_UI_TEXT(open_logs, "Open logs folder");
LOM_UI_TEXT(about, "About");
LOM_UI_TEXT(about_body, "<b>Legend of Mana Native</b> launcher<br>Version %1<br><br>"
                        "This application includes no game content. Supported editions: NTSC-U (SLUS-01013) and NTSC-J (SLPS-02170).");
LOM_UI_TEXT(settings_save_failed, "Your settings could not be saved");
LOM_UI_TEXT(settings_corrupt, "Your launcher settings could not be read and were reset");
LOM_UI_TEXT(data_files_damaged, "The launcher's data files are missing or damaged. Reinstall the launcher.");
LOM_UI_TEXT(language_label, "Language");
LOM_UI_TEXT(language_system, "Match system");

LOM_UI_TEXT(update_available_title, "A new version is available");
LOM_UI_TEXT(update_available_body, "Legend of Mana Native %1 is available. You have %2.\nOpen the download page?");
LOM_UI_TEXT(update_now, "Open download page");
LOM_UI_TEXT(not_now, "Not now");
LOM_UI_TEXT(release_notes, "Release notes");
LOM_UI_TEXT(up_to_date, "You have the latest version (%1).");
LOM_UI_TEXT(could_not_check, "Could not check for updates: %1");
LOM_UI_TEXT(updates_not_configured, "Update checks are not configured in this build.");
LOM_UI_TEXT(no_update_title, "Check for updates");
LOM_UI_TEXT(update_banner, "Version %1 is available.");
LOM_UI_TEXT(update_install_body, "Legend of Mana Native %1 is available. You have %2.\nDownload and install it now?");
LOM_UI_TEXT(install_update, "Download and install");
LOM_UI_TEXT(update_downloading, "Downloading Legend of Mana Native %1...");
LOM_UI_TEXT(update_installing, "Installing Legend of Mana Native %1...");
LOM_UI_TEXT(update_close_game, "Close the game before installing the update.");
LOM_UI_TEXT(update_installed_title, "Update installed");
LOM_UI_TEXT(update_installed_body, "Legend of Mana Native %1 is installed. Restart the launcher now to use it?");
LOM_UI_TEXT(restart_now, "Restart now");
LOM_UI_TEXT(later, "Later");
LOM_UI_TEXT(update_failed_title, "Update not installed");
LOM_UI_TEXT(update_failed_body, "The update couldn't be installed: %1\n\nYou can still get it from the download page.");
LOM_UI_TEXT(update_restart_failed, "The update is installed, but the launcher couldn't start it. Open the launcher again to use version %1.");

LOM_UI_TEXT(whats_new, "What's new");
LOM_UI_TEXT(whats_new_current, "You have version %1.");
LOM_UI_TEXT(whats_new_updated, "You're on version %1 now. Here's what changed.");
LOM_UI_TEXT(whats_new_loading, "Getting the release notes from GitHub...");
LOM_UI_TEXT(whats_new_failed, "I couldn't get the release notes right now: %1");
LOM_UI_TEXT(whats_new_none, "There are no release notes to show yet.");
LOM_UI_TEXT(whats_new_not_configured, "This build doesn't look up releases, so there are no notes to show here.");
LOM_UI_TEXT(whats_new_this_version, "the version you have");
LOM_UI_TEXT(whats_new_newer, "newer than yours");
LOM_UI_TEXT(whats_new_all_releases, "All releases on GitHub");

#undef LOM_UI_TEXT

}

#endif
