#include "lom_native_akao.h"
#include "lom_native_ps1_memory.h"

#include <psyz/spu.h>
#include <libspu.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../support/platform/lom_test_posix.h"

static const char executable_signature[] = "PS-X EXE";

static const char executable_path_format[] = "%s/SLUS_010.13";

static const char binary_write_mode[] = "wb";

static const char reverb_error[] = "AKAO reverb query width, guards or mode branches failed.\n";

static const char reverb_success[] = "  Reverb: host-long slot guards, unchanged mode and changed mode passed.";

static const char temporary_directory_template[] = "/tmp/lom-akao-irq-XXXXXX";

static const char audio_driver_environment[] = "SDL_AUDIODRIVER";

static const char dummy_audio_driver[] = "dummy";

static const char initialize_error[] = "AKAO init failed\n";

static const char enable_bits_error[] = "AKAO init did not preserve the SPU enable/unmute bits.\n";

static const char clock_error[] = "LoM AKAO null-secondary IRQ smoke did not advance.\n";

static const char success_message[] = "LoM AKAO null-secondary IRQ smoke passed.\n";

static const char control_report_format[] = "  SPUCNT:             0x%04X\n";

static const char ticks_report_format[] = "  root-counter ticks: %u -> %u\n";
#define EXE_HEADER_BYTES 0x800
#define FIXTURE_PATH_CAPACITY 1024
static const unsigned long resident_load_address = 0x80010000u;
#define EXE_LOAD_ADDRESS_OFFSET 0x18
#define EXE_PAYLOAD_SIZE_OFFSET 0x1c
static const unsigned long leading_guard = 0x13579BDFUL;

static const unsigned long trailing_guard = 0x2468ACE0UL;

static const unsigned long spu_control_register = 0x1AAu;

static const unsigned long spu_enabled_bits = 0xC000u;

static const unsigned long warmup_nanoseconds = 30000000;

static const unsigned long observation_nanoseconds = 100000000;

static const unsigned long pending_driver_flag = 0x100u;

static const int initial_reverb_mode = 4;

static const int changed_reverb_mode = 5;

static const int fixture_exit = 2;

static const int initialize_exit = 3;

static const int enable_bits_exit = 4;

static const int clock_exit = 5;

static const int reverb_exit = 6;

static const int sfx_exit = 7;

static const int category_exit = 8;

/**
 * @brief Verify the real global SFX volume and fade commands.
 * @return Zero on success; nonzero on a failed probe.
 */
extern int lom_native_akao_sfx_volume_probe(void);

/**
 * @brief Verify the driver's effect voices reach the mixer as sound effects.
 * @return Zero on success; nonzero on a failed probe.
 */
extern int lom_native_akao_category_probe(void);

/**
 * @brief Apply the fixture reverb mode through the real driver.
 * @param reverb_type SDK reverb mode.
 */
extern void akao_apply_reverb_type(int reverb_type);

#include "main/audio/internal/akao_driver.h"
#define DRIVER_FLAGS_PENDING (*(volatile u32*)&g_akao_driver_flags.update_flags)

/**
 * @brief Write the smallest valid resident executable image needed by AKAO init.
 * @param root Temporary directory receiving the synthetic SLUS image.
 * @return Zero on success, or -1 when the image cannot be written.
 */
static int make_fake_exe(const char* root)
{
    unsigned char hdr[EXE_HEADER_BYTES];
    char path[FIXTURE_PATH_CAPACITY];
    FILE* f;
    uint32_t v;

    memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, executable_signature, sizeof(executable_signature) - 1);
    v = resident_load_address;
    memcpy(&hdr[EXE_LOAD_ADDRESS_OFFSET], &v, sizeof(v));
    v = 0;
    memcpy(&hdr[EXE_PAYLOAD_SIZE_OFFSET], &v, sizeof(v));
    snprintf(path, sizeof(path), executable_path_format, root);
    f = fopen(path, binary_write_mode);

    if (!f)
    {
        return -1;
    }

    if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr))
    {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

/**
 * @brief Verify host-long reverb queries preserve adjacent guards and update modes.
 * @return Zero on success; minus one on a guard or mode mismatch.
 */
static int verify_reverb_query(void)
{
    struct
    {
        unsigned long before;

        long mode;

        unsigned long after;
    } check = {leading_guard, -1, trailing_guard};

    long saved_mode;
    int failed;

    SpuGetReverbModeType(&saved_mode);
    SpuSetReverbModeType(initial_reverb_mode);
    SpuGetReverbModeType(&check.mode);
    failed = check.mode != initial_reverb_mode;
    akao_apply_reverb_type(initial_reverb_mode);
    SpuGetReverbModeType(&check.mode);
    failed |= check.mode != initial_reverb_mode;
    akao_apply_reverb_type(changed_reverb_mode);
    SpuGetReverbModeType(&check.mode);
    failed |= check.mode != (changed_reverb_mode | SPU_REV_MODE_CLEAR_WA);
    failed |= check.before != leading_guard || check.after != trailing_guard;
    SpuSetReverbModeType(saved_mode);

    if (failed)
    {
        fprintf(stderr, reverb_error);
        return -1;
    }

    puts(reverb_success);
    return 0;
}

/**
 * @brief Verify PS1 KUSEG-zero reads used by the first AKAO effect-mask flush.
 * @param argc 2: the program and your extracted disc.
 * @param argv argv[1] is the disc, whose SLUS_010.13 holds AKAO's tables
 *             (src/core/resident_native.c).
 * @return Zero when the real IRQ continues ticking with no secondary song.
 */
int main(int argc, char** argv)
{
    char tmpl[sizeof(temporary_directory_template)];
    memcpy(tmpl, temporary_directory_template, sizeof(tmpl));
    struct timespec ts;
    unsigned before;
    unsigned after;
    char* root;

    setenv(audio_driver_environment, dummy_audio_driver, 0);

    if (lom_native_map_ps1_ram() != 0)
    {
        return 1;
    }

    (void)tmpl;
    (void)make_fake_exe;

    if (argc != 2)
    {
        return fixture_exit;
    }

    root = argv[1];

    if (lom_native_akao_initialize(root) != 0)
    {
        fprintf(stderr, initialize_error);
        return initialize_exit;
    }

    lom_native_akao_disable_event(0);
    int reverb_result = verify_reverb_query();
    lom_native_akao_enable_event(0);

    if (reverb_result != 0)
    {
        lom_native_akao_shutdown();
        return reverb_exit;
    }

    if ((Psyz_SpuRead(spu_control_register) & spu_enabled_bits) != spu_enabled_bits)
    {
        fprintf(stderr, enable_bits_error);
        lom_native_akao_shutdown();
        return enable_bits_exit;
    }

    ts.tv_sec = 0;
    ts.tv_nsec = warmup_nanoseconds;
    nanosleep(&ts, NULL);

    lom_native_akao_disable_event(0);
    DRIVER_FLAGS_PENDING = pending_driver_flag;
    lom_native_akao_enable_event(0);

    before = lom_native_akao_tick_count();
    ts.tv_nsec = observation_nanoseconds;
    nanosleep(&ts, NULL);
    after = lom_native_akao_tick_count();

    lom_native_akao_shutdown();

    if (lom_native_akao_sfx_volume_probe() != 0)
    {
        return sfx_exit;
    }

    if (lom_native_akao_category_probe() != 0)
    {
        return category_exit;
    }

    if (after <= before)
    {
        fprintf(stderr, clock_error);
        return clock_exit;
    }

    printf(success_message);
    printf(control_report_format, Psyz_SpuRead(spu_control_register));
    printf(ticks_report_format, before, after);
    return 0;
}
