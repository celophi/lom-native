#include "disc_fixture.h"

#include "lom/launcher/disc/disc_image.h"
#include "lom/launcher/disc/iso9660_reader.h"
#include "lom/launcher/common/json.h"
#include "lom/launcher/common/sha1.h"
#include "lom/launcher/common/sha256.h"
#include "synthetic_disc.h"

#include <fstream>
#include <stdexcept>

namespace lom::launcher::test {

namespace {

constexpr char system_cnf[] = "BOOT = cdrom:\\SLUS_999.99;1\r\nTCB = 4\r\n";
constexpr char manifest_name[] = "manifests/test.json";

/**
 * @brief Hash bytes with SHA-1.
 * @param bytes Bytes.
 * @return Digest.
 */
std::string sha1_of(const std::vector<std::uint8_t>& bytes) {
    Sha1 hash;
    hash.update(bytes.data(), bytes.size());
    return hash.hex_digest();
}

}

SyntheticDisc DiscFixture::standard_disc() {
    SyntheticDisc disc;
    disc.add("SYSTEM.CNF", std::vector<std::uint8_t>(system_cnf, system_cnf + sizeof(system_cnf) - 1));
    disc.add(executable, SyntheticDisc::pattern(6000, 1));
    disc.add("BIN/FIELD.BIN", SyntheticDisc::pattern(field_bin_bytes, field_bin_seed));
    disc.add("BIN/TINY.BIN", SyntheticDisc::pattern(1, 3));
    disc.add("MOVIE/OPEN.STR", SyntheticDisc::pattern(10240, 4));
    return disc;
}

DiscFixture DiscFixture::create(const std::filesystem::path& root) {
    const SyntheticDisc disc = standard_disc();
    DiscFixture fixture;
    fixture.directory = root;
    fixture.bin = root / "images" / "Test Disc (USA).bin";
    fixture.cue = root / "images" / "Test Disc (USA).cue";
    fixture.iso = root / "images" / "Test Disc.iso";
    const std::vector<std::uint8_t> raw = disc.raw();
    const std::vector<std::uint8_t> cooked = disc.cooked();
    SyntheticDisc::write(fixture.bin, raw);
    SyntheticDisc::write(fixture.iso, cooked);
    {
        std::ofstream cue(fixture.cue);
        cue << "FILE \"Test Disc (USA).bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
    }

    fixture.raw_sha1 = sha1_of(raw);

    DiscImage image;
    Iso9660Reader reader;

    if (image.open(fixture.bin) != DiscImage::Status::Ok || !reader.read(image)) {
        throw std::runtime_error("synthetic disc is unreadable: " + reader.error());
    }

    fixture.manifest.release_id = "us";
    fixture.manifest.parent_raw_sha1 = fixture.raw_sha1;
    fixture.manifest.generator = "test";
    std::vector<std::uint8_t> bytes;

    for (const Iso9660Reader::File& file : reader.files()) {
        Iso9660Reader::read_file(image, file, bytes);
        Sha256 hash;
        hash.update(bytes.data(), bytes.size());
        fixture.manifest.entries.push_back(FileManifest::Entry{file.path, file.bytes, hash.hex_digest()});
    }

    fixture.manifest.sort_entries();
    const std::filesystem::path data = root / "data";
    SyntheticDisc::write(data / manifest_name, {});
    {
        std::ofstream out(data / manifest_name, std::ios::binary | std::ios::trunc);
        out << fixture.manifest.to_json().dump(true);
    }

    Json us;
    us["id"] = "us";
    us["region"] = "NTSC-U";
    us["serial"] = "SLUS-01013";
    us["executable"] = executable;
    us["display_name"] = "North American version (NTSC-U)";
    us["card_prefix"] = "BASLUS-01013";
    us["runtime"] = "lom";
    us["raw"]["sha1"] = fixture.raw_sha1;
    us["raw"]["bytes"] = std::int64_t(raw.size());
    us["raw"]["layout"] = DiscImage::raw_single_track_layout;
    us["manifest"] = manifest_name;

    Json jp;
    jp["id"] = "jp";
    jp["region"] = "NTSC-J";
    jp["executable"] = "SLPS_999.98";
    jp["display_name"] = "Japanese version (NTSC-J)";
    jp["card_prefix"] = "BISLPS-02170";
    jp["raw"]["sha1"] = std::string(40, '0');
    jp["raw"]["layout"] = DiscImage::raw_single_track_layout;

    Json catalog;
    catalog["schema"] = 1;
    catalog["upstream_revision"] = "test";
    catalog["releases"].push_back(us);
    catalog["releases"].push_back(jp);
    fixture.catalog_path = data / "supported-discs.json";
    {
        std::ofstream out(fixture.catalog_path, std::ios::binary | std::ios::trunc);
        out << catalog.dump(true);
    }

    std::string error;

    if (!fixture.catalog.load(fixture.catalog_path, &error)) {
        throw std::runtime_error("synthetic catalog failed to load: " + error);
    }

    return fixture;
}

}
