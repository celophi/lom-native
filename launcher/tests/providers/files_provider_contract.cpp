#include "../support/files_provider_fake.h"
#include "lom/launcher/common/files_provider.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

#include <algorithm>
#include <istream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>

namespace {

using namespace lom::launcher;

enum class Disk { Real, Fake };

class FilesProviderContract : public ::testing::TestWithParam<Disk> {
  protected:
    /**
     * @brief Run each case on the real disk, in a folder of its own, and on the fake, in the same folder's name, so
     *        the fake is held to what the disk does.
     */
    FilesProviderContract() : root(std::filesystem::u8path(folder.path().toStdString())) {
        if (GetParam() == Disk::Fake) {
            fake = std::make_unique<test::FilesProviderFake>();
            fake->add_folder(root);
        }
    }

    /**
     * @brief Write a file through the provider.
     * @param path The file.
     * @param bytes What goes in it.
     * @param mode How it's opened.
     */
    void write(const std::filesystem::path& path, const std::string& bytes, std::ios::openmode mode = std::ios::binary) {
        auto stream = files_provider().open_write(path, mode);
        ASSERT_TRUE(*stream);
        *stream << bytes;
    }

    /**
     * @brief Read a file through the provider.
     * @param path The file.
     * @return What's in it, or "(unreadable)".
     */
    std::string read(const std::filesystem::path& path) {
        auto stream = files_provider().open_read(path, std::ios::binary);
        return *stream ? std::string(std::istreambuf_iterator<char>(*stream), {}) : std::string("(unreadable)");
    }

    /**
     * @brief The names in a folder, sorted.
     * @param folder The folder.
     * @param recursive Whether to go into its folders.
     * @return The paths, relative to the test's folder.
     */
    std::vector<std::string> names(const std::filesystem::path& folder, bool recursive) {
        std::error_code code;
        std::vector<std::string> found;

        for (const FileEntry& entry : files_provider().list(folder, recursive, code)) {
            found.push_back(entry.path.lexically_relative(root).generic_u8string() + (entry.directory ? "/" : ""));
        }

        std::sort(found.begin(), found.end());
        return found;
    }

    QTemporaryDir folder;
    std::filesystem::path root;
    std::unique_ptr<test::FilesProviderFake> fake;
    std::error_code code;
};

TEST_P(FilesProviderContract, writes_a_file_and_reads_it_back_byte_for_byte) {
    write(root / "save.bin", std::string("PSX\0\x01\xFF", 6));

    EXPECT_EQ(read(root / "save.bin"), std::string("PSX\0\x01\xFF", 6));
    EXPECT_EQ(files_provider().file_size(root / "save.bin", code), 6u);
    EXPECT_TRUE(files_provider().is_regular_file(root / "save.bin", code));
    EXPECT_FALSE(files_provider().is_directory(root / "save.bin", code));
}

TEST_P(FilesProviderContract, adds_to_a_file_opened_to_add_to_and_empties_one_opened_to_write) {
    write(root / "log.txt", "first\n");
    write(root / "log.txt", "second\n", std::ios::binary | std::ios::app);
    write(root / "settings.json", "old");
    write(root / "settings.json", "new");

    EXPECT_EQ(read(root / "log.txt"), "first\nsecond\n");
    EXPECT_EQ(read(root / "settings.json"), "new");
}

TEST_P(FilesProviderContract, cant_write_into_a_folder_that_isnt_there) {
    auto stream = files_provider().open_write(root / "missing" / "file.txt", std::ios::binary);

    EXPECT_FALSE(*stream);
}

TEST_P(FilesProviderContract, cant_read_or_measure_what_isnt_there) {
    EXPECT_EQ(read(root / "missing.bin"), "(unreadable)");
    EXPECT_FALSE(files_provider().exists(root / "missing.bin", code));
    files_provider().file_size(root / "missing.bin", code);
    EXPECT_TRUE(code);
}

TEST_P(FilesProviderContract, makes_folders_and_the_folders_they_are_in) {
    EXPECT_TRUE(files_provider().create_directories(root / "a" / "b" / "c", code));
    EXPECT_FALSE(code);
    EXPECT_FALSE(files_provider().create_directories(root / "a" / "b", code));

    EXPECT_TRUE(files_provider().is_directory(root / "a" / "b" / "c", code));
    EXPECT_TRUE(files_provider().exists(root / "a", code));
}

TEST_P(FilesProviderContract, lists_a_folder_or_everything_in_it) {
    files_provider().create_directories(root / "mods" / "dice", code);
    write(root / "mods" / "mods.ini", "dice");
    write(root / "mods" / "dice" / "mod.json", "{}");

    EXPECT_EQ(names(root / "mods", false), (std::vector<std::string>{"mods/dice/", "mods/mods.ini"}));
    EXPECT_EQ(names(root / "mods", true), (std::vector<std::string>{"mods/dice/", "mods/dice/mod.json", "mods/mods.ini"}));
}

TEST_P(FilesProviderContract, renames_a_file_over_another_and_moves_a_folder_with_what_is_in_it) {
    write(root / "new.json", "new");
    write(root / "settings.json", "old");
    files_provider().create_directories(root / "staging" / "data", code);
    write(root / "staging" / "data" / "game.bin", "game");

    files_provider().rename(root / "new.json", root / "settings.json", code);
    EXPECT_FALSE(code);
    files_provider().rename(root / "staging", root / "installed", code);
    EXPECT_FALSE(code);

    EXPECT_EQ(read(root / "settings.json"), "new");
    EXPECT_FALSE(files_provider().exists(root / "new.json", code));
    EXPECT_EQ(read(root / "installed" / "data" / "game.bin"), "game");
    EXPECT_FALSE(files_provider().exists(root / "staging", code));
}

TEST_P(FilesProviderContract, wont_rename_a_folder_over_one_that_holds_anything) {
    files_provider().create_directories(root / "from", code);
    files_provider().create_directories(root / "to", code);
    write(root / "to" / "kept.txt", "kept");

    files_provider().rename(root / "from", root / "to", code);

    EXPECT_TRUE(code);
    EXPECT_EQ(read(root / "to" / "kept.txt"), "kept");
}

TEST_P(FilesProviderContract, removes_a_file_or_an_empty_folder_but_not_a_full_one) {
    write(root / "file.txt", "x");
    files_provider().create_directories(root / "empty", code);
    files_provider().create_directories(root / "full", code);
    write(root / "full" / "file.txt", "x");

    EXPECT_TRUE(files_provider().remove(root / "file.txt", code));
    EXPECT_TRUE(files_provider().remove(root / "empty", code));
    EXPECT_FALSE(files_provider().remove(root / "full", code));
    EXPECT_TRUE(code);
    EXPECT_FALSE(files_provider().remove(root / "missing.txt", code));
    EXPECT_FALSE(code);
}

TEST_P(FilesProviderContract, removes_a_folder_and_everything_in_it_counting_them) {
    files_provider().create_directories(root / "old" / "data", code);
    write(root / "old" / "data" / "a.bin", "a");
    write(root / "old" / "b.bin", "b");

    EXPECT_EQ(files_provider().remove_all(root / "old", code), 4u);
    EXPECT_FALSE(files_provider().exists(root / "old", code));
    EXPECT_EQ(files_provider().remove_all(root / "old", code), 0u);
}

TEST_P(FilesProviderContract, copies_a_file_only_over_another_when_told_to) {
    write(root / "card.mcr", "card");
    write(root / "copy.mcr", "older");

    EXPECT_FALSE(files_provider().copy_file(root / "card.mcr", root / "copy.mcr", std::filesystem::copy_options::none, code));
    EXPECT_TRUE(code);
    EXPECT_FALSE(files_provider().copy_file(root / "card.mcr", root / "copy.mcr", std::filesystem::copy_options::skip_existing, code));
    EXPECT_EQ(read(root / "copy.mcr"), "older");
    EXPECT_TRUE(files_provider().copy_file(root / "card.mcr", root / "copy.mcr", std::filesystem::copy_options::overwrite_existing, code));
    EXPECT_EQ(read(root / "copy.mcr"), "card");
}

TEST_P(FilesProviderContract, copies_a_folder_with_everything_in_it) {
    files_provider().create_directories(root / "mod" / "scripts", code);
    write(root / "mod" / "mod.json", "{}");
    write(root / "mod" / "scripts" / "main.lua", "return");

    files_provider().copy(root / "mod", root / "installed", std::filesystem::copy_options::recursive, code);

    EXPECT_FALSE(code);
    EXPECT_EQ(read(root / "installed" / "mod.json"), "{}");
    EXPECT_EQ(read(root / "installed" / "scripts" / "main.lua"), "return");
}

TEST_P(FilesProviderContract, tells_when_two_paths_are_the_same_file) {
    files_provider().create_directories(root / "saves", code);

    EXPECT_TRUE(files_provider().equivalent(root / "saves", root / "saves" / ".." / "saves", code));
    EXPECT_FALSE(files_provider().equivalent(root / "saves", root, code));
    EXPECT_EQ(files_provider().weakly_canonical(root / "saves" / ".." / "saves", code), files_provider().weakly_canonical(root / "saves", code));
}

TEST_P(FilesProviderContract, marks_a_file_as_written_later_when_it_is_written_again) {
    write(root / "image.bin", "one");
    const auto first = files_provider().last_write_time(root / "image.bin", code);

    write(root / "image.bin", "two");

    EXPECT_GE(files_provider().last_write_time(root / "image.bin", code), first);
}

TEST_P(FilesProviderContract, says_how_much_room_there_is) {
    const auto room = files_provider().space(root, code);

    EXPECT_FALSE(code);
    EXPECT_GT(room.available, 0u);
}

INSTANTIATE_TEST_SUITE_P(RealAndFake, FilesProviderContract, ::testing::Values(Disk::Real, Disk::Fake), [](const auto& info) {
    return info.param == Disk::Real ? std::string("real_disk") : std::string("fake_disk");
});

}
