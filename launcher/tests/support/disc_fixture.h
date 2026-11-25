#ifndef LOM_LAUNCHER_DISC_FIXTURE_H
#define LOM_LAUNCHER_DISC_FIXTURE_H

#include "lom/launcher/disc/file_manifest.h"
#include "lom/launcher/disc/release_catalog.h"
#include "synthetic_disc.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace lom::launcher::test {
struct DiscFixture {
    static constexpr char executable[] = "SLUS_999.99";
    static constexpr std::size_t field_bin_bytes = 5000;
    static constexpr std::uint32_t field_bin_seed = 2;

    std::filesystem::path directory;
    std::filesystem::path bin;
    std::filesystem::path cue;
    std::filesystem::path iso;
    std::filesystem::path catalog_path;
    std::string raw_sha1;
    ReleaseCatalog catalog;
    FileManifest manifest;

    /**
     * @brief Build the fixture.
     * @param root Scratch directory.
     * @return The fixture.
     */
    static DiscFixture create(const std::filesystem::path& root);

    /**
     * @brief Build the contents of the synthetic supported disc.
     * @return Disc with its standard files.
     */
    static SyntheticDisc standard_disc();
};

}

#endif
