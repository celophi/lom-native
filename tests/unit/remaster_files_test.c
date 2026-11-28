#include "../../src/remaster/remaster_internal.h"
#include "../support/platform/remaster_fixture.h"
#include "../support/platform/remaster_files_provider_fake.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../support/lom_check.h"

/**
 * @brief Check a zlib stream inflates to the fixture's text.
 * @param stream The stream.
 * @param size Its size.
 * @param expected How many bytes of the text it holds.
 */
static void inflates(const unsigned char* stream, size_t size, size_t expected)
{
    unsigned char out[sizeof(fixture_text)];
    size_t written = 0;
    CHECK(lom_remaster_inflate(stream, size, out, sizeof(out), &written) == 0);
    CHECK(written == expected && memcmp(out, fixture_text, expected) == 0);
}

/**
 * @brief Check inflating reads stored, fixed and dynamic blocks, and refuses a
 *        stream that's cut short, too big or has a bad header.
 */
static void inflate_reads_every_block_kind_and_refuses_bad_streams(void)
{
    inflates(fixture_stored, sizeof(fixture_stored), sizeof(fixture_text));
    inflates(fixture_fixed, sizeof(fixture_fixed), 40);
    inflates(fixture_dynamic, sizeof(fixture_dynamic), sizeof(fixture_text));

    unsigned char out[sizeof(fixture_text)];
    size_t written = 0;
    CHECK(lom_remaster_inflate(fixture_dynamic, sizeof(fixture_dynamic), out, sizeof(out) - 1, &written) != 0);
    CHECK(lom_remaster_inflate(fixture_dynamic, sizeof(fixture_dynamic) / 2, out, sizeof(out), &written) != 0);
    unsigned char broken[sizeof(fixture_dynamic)];
    memcpy(broken, fixture_dynamic, sizeof(broken));
    broken[0] ^= 0x01;
    CHECK(lom_remaster_inflate(broken, sizeof(broken), out, sizeof(out), &written) != 0);
}

/**
 * @brief Check a PSB reads the kinds of value the port uses: strings, numbers,
 *        paths, lists and chunks.
 * @param data The fixture PSB.
 * @param size Its size.
 */
static void reads_the_psb_values_the_port_uses(const unsigned char* data, size_t size)
{
    LomPsb psb;
    size_t value, arch, item;
    const unsigned char* bytes;
    size_t length;

    CHECK(lom_psb_open(&psb, data, size) == 0);
    CHECK(lom_psb_kind(&psb, psb.root) == LOM_PSB_OBJECT);
    CHECK(lom_psb_member(&psb, psb.root, "id", &value) == 0 && strcmp(lom_psb_string(&psb, value), "sound_archive") == 0);
    CHECK(lom_psb_member(&psb, psb.root, "negative", &value) == 0 && lom_psb_int(&psb, value) == -300);
    CHECK(lom_psb_member(&psb, psb.root, "missing", &value) != 0);

    CHECK(lom_psb_path(&psb, psb.root, "voice/music_voice/loopstr", &value) == 0 && strcmp(lom_psb_string(&psb, value), "all") == 0);
    CHECK(lom_psb_path(&psb, psb.root, "voice/music_voice/channelList", &value) == 0 && lom_psb_count(&psb, value) == 1);
    CHECK(lom_psb_path(&psb, psb.root, "voice/music_voice/channelList/1", &value) != 0);
    CHECK(lom_psb_path(&psb, psb.root, "voice/music_voice/channelList/0/archData", &arch) == 0);

    CHECK(lom_psb_member(&psb, arch, "fmt", &value) == 0 && lom_psb_chunk(&psb, value, &bytes, &length) == 0);
    CHECK(length == 16 && bytes[0] == 1 && bytes[2] == 2 && (bytes[4] | bytes[5] << 8) == 44100);
    CHECK(lom_psb_member(&psb, arch, "data", &value) == 0 && lom_psb_chunk(&psb, value, &bytes, &length) == 0);
    CHECK(length == FIXTURE_PCM_BYTES && bytes[0] == 0 && bytes[255] == 255 && bytes[256] == 0);

    CHECK(lom_psb_member(&psb, arch, "loop", &value) == 0 && lom_psb_kind(&psb, value) == LOM_PSB_LIST && lom_psb_count(&psb, value) == 2);
    CHECK(lom_psb_item(&psb, value, 0, &item) == 0 && lom_psb_int(&psb, item) == 16);
    CHECK(lom_psb_item(&psb, value, 1, &item) == 0 && lom_psb_int(&psb, item) == 200);
    CHECK(lom_psb_item(&psb, value, 2, &item) != 0);
    lom_psb_close(&psb);

    CHECK(lom_psb_open(&psb, fixture_text, sizeof(fixture_text)) != 0);
}

/**
 * @brief Check lom_remaster_load reads a packed file and a plain one the same, reading each from the Remaster's files
 *        once, and a missing one as NULL.
 * @note Given the Remaster has the same PSB packed (.psb.m) and plain (.psb), when the port loads each, then each comes
 *       out as the plain PSB, which reads as one, and was read from the Remaster's files once. When it loads one the
 *       Remaster hasn't got, then there's nothing, and the provider was asked for it.
 */
static void loads_a_packed_file_and_a_plain_one(void)
{
    RemasterFilesProviderFake fake;

    remaster_files_provider_fake_start(&fake);
    RemasterFilesProviderFakeFile* packed = remaster_files_provider_fake_add(&fake, "remaster/fixture.psb.m", fixture_mdf, sizeof(fixture_mdf));
    RemasterFilesProviderFakeFile* plain = remaster_files_provider_fake_add(&fake, "remaster/fixture.psb", fixture_psb, sizeof(fixture_psb));

    for (int i = 0; i < 2; ++i)
    {
        size_t size = 0;
        unsigned char* data = lom_remaster_load(i ? packed->path : plain->path, &size);

        CHECK(data && size == sizeof(fixture_psb) && memcmp(data, fixture_psb, size) == 0);
        reads_the_psb_values_the_port_uses(data, size);
        free(data);
    }

    CHECK(packed->reads == 1 && plain->reads == 1 && fake.misses == 0);

    size_t size = 0;
    CHECK(lom_remaster_load("remaster/missing.psb.m", &size) == NULL);
    CHECK(fake.misses == 1);
    remaster_files_provider_fake_stop();
}

int main(void)
{
    inflate_reads_every_block_kind_and_refuses_bad_streams();
    reads_the_psb_values_the_port_uses(fixture_psb, sizeof(fixture_psb));
    loads_a_packed_file_and_a_plain_one();
    puts("remaster-files: DEFLATE, mdf and PSB read as the Remaster's files need");
    return 0;
}
