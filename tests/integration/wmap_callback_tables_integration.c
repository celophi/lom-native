#include "main/cdrom.h"
#include "main/cd_resources.h"
#include "lom_native_cd.h"
#include "lom_native_ps1_memory.h"
#include <stdint.h>
#include <stdio.h>

extern void* lom_native_wmap_resolve(uint32_t address);
#define ORBITER(n)                                                                                                                                             \
    extern void wmap_effect35_orbiter_##n##_start(void);                                                                                                       \
    extern void wmap_effect35_orbiter_##n##_update(void)
ORBITER(1);

ORBITER(2);

ORBITER(3);

ORBITER(4);

ORBITER(5);

ORBITER(6);

ORBITER(7);
#define EVENT17(n) extern void wmap_land_event_17_sequence_##n##_step_01(void)
EVENT17(1);

EVENT17(2);

EVENT17(3);

EVENT17(4);

EVENT17(5);

EVENT17(6);

EVENT17(7);

EVENT17(8);

EVENT17(9);

#define STEPS(name) extern void (*name[])(void)
STEPS(g_wmap_effect35_orbiter_1_steps);
STEPS(g_wmap_effect35_orbiter_2_steps);
STEPS(g_wmap_effect35_orbiter_3_steps);
STEPS(g_wmap_effect35_orbiter_4_steps);
STEPS(g_wmap_effect35_orbiter_5_steps);
STEPS(g_wmap_effect35_orbiter_6_steps);
STEPS(g_wmap_effect35_orbiter_7_steps);
STEPS(g_wmap_land_event_17_sequence_1_steps);
STEPS(g_wmap_land_event_17_sequence_2_steps);
STEPS(g_wmap_land_event_17_sequence_3_steps);
STEPS(g_wmap_land_event_17_sequence_4_steps);
STEPS(g_wmap_land_event_17_sequence_5_steps);
STEPS(g_wmap_land_event_17_sequence_6_steps);
STEPS(g_wmap_land_event_17_sequence_7_steps);
STEPS(g_wmap_land_event_17_sequence_8_steps);
STEPS(g_wmap_land_event_17_sequence_9_steps);

static void (** const orbiter_tables[7])(void) = {g_wmap_effect35_orbiter_1_steps, g_wmap_effect35_orbiter_2_steps, g_wmap_effect35_orbiter_3_steps,
                                                  g_wmap_effect35_orbiter_4_steps, g_wmap_effect35_orbiter_5_steps, g_wmap_effect35_orbiter_6_steps,
                                                  g_wmap_effect35_orbiter_7_steps};

static void (** const scatter_tables[9])(void) = {
    g_wmap_land_event_17_sequence_1_steps, g_wmap_land_event_17_sequence_2_steps, g_wmap_land_event_17_sequence_3_steps,
    g_wmap_land_event_17_sequence_4_steps, g_wmap_land_event_17_sequence_5_steps, g_wmap_land_event_17_sequence_6_steps,
    g_wmap_land_event_17_sequence_7_steps, g_wmap_land_event_17_sequence_8_steps, g_wmap_land_event_17_sequence_9_steps};

/**
 * @brief Check one step of a WMAP step table is the function expected.
 * @param table The table.
 * @param step Which step.
 * @param expected The function.
 * @return 0, or 1 when it isn't.
 */
static int check_table(void (**table)(void), unsigned step, void (*expected)(void))
{
    if (table[step] != expected)
    {
        fprintf(stderr, "WMAP step table %p step %u: %p, expected %p\n", (void*)table, step, (void*)table[step], (void*)expected);
        return 1;
    }

    return 0;
}

#define ORBITER_STEP(i, step, expected) check_table(orbiter_tables[i], step, expected)
#define SCATTER_STEP(i, step, expected) check_table(scatter_tables[i], step, expected)
#define WMAP_WINDOW ((u8*)OVERLAY_LOAD_BASE_ADDRESS)

int main(int argc, char** argv)
{
    if (argc != 2 || lom_native_map_ps1_ram() || lom_native_cd_set_disc_root(argv[1]))
    {
        return 2;
    }

    cdrom_init();
    int size = cdrom_stream(CD_RES_WMAP_BIN, WMAP_WINDOW);

    if (size <= (int)(0x800D7C8Cu - 0x8004FC70u) || *(const uint32_t*)WMAP_WINDOW != 15)
    {
        return 2;
    }

    void (*orbiters[7][2])(void) = {
#define PAIR(n) {wmap_effect35_orbiter_##n##_start, wmap_effect35_orbiter_##n##_update}
        PAIR(1), PAIR(2), PAIR(3), PAIR(4), PAIR(5), PAIR(6), PAIR(7)};
    void (*scatter[9])(void) = {
#define START(n) wmap_land_event_17_sequence_##n##_step_01
        START(1), START(2), START(3), START(4), START(5), START(6), START(7), START(8), START(9)};
    int failures = 0;

    for (unsigned i = 0; i < 7; ++i)
    {
        failures += ORBITER_STEP(i, 1, orbiters[i][0]);
        failures += ORBITER_STEP(i, 2, orbiters[i][1]);
    }

    for (unsigned i = 0; i < 9; ++i)
    {
        failures += SCATTER_STEP(i, 1, scatter[i]);
    }

    if (failures)
    {
        return 1;
    }

    puts("WMAP retail step-table bindings passed: 14 ending orbiters and 9 event17 scatter starts.");
    return 0;
}
