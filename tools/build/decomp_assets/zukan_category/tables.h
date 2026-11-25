#ifndef ZUKAN_CATEGORY_TABLES_H
#define ZUKAN_CATEGORY_TABLES_H

extern s32 g_zukan_category_ranges[13] __attribute__((aligned(4)));
extern s32 g_zukan_group_ranges[7] __attribute__((aligned(4)));
extern s16 g_zukan_entry_values[1018] __attribute__((aligned(2)));
extern u16 g_zukan_display_order[719] __attribute__((aligned(2)));

#define LOM_ZUKAN_10(table, start)                                                                                                                             \
    table[(start) + 0], table[(start) + 1], table[(start) + 2], table[(start) + 3], table[(start) + 4], table[(start) + 5], table[(start) + 6],                \
        table[(start) + 7], table[(start) + 8], table[(start) + 9]
#define LOM_ZUKAN_100(table, start)                                                                                                                            \
    LOM_ZUKAN_10(table, (start) + 0), LOM_ZUKAN_10(table, (start) + 10), LOM_ZUKAN_10(table, (start) + 20), LOM_ZUKAN_10(table, (start) + 30),                 \
        LOM_ZUKAN_10(table, (start) + 40), LOM_ZUKAN_10(table, (start) + 50), LOM_ZUKAN_10(table, (start) + 60), LOM_ZUKAN_10(table, (start) + 70),            \
        LOM_ZUKAN_10(table, (start) + 80), LOM_ZUKAN_10(table, (start) + 90)

#define ZUKAN_CATEGORY_RANGES LOM_ZUKAN_10(g_zukan_category_ranges, 0), g_zukan_category_ranges[10], g_zukan_category_ranges[11], g_zukan_category_ranges[12]

#define ZUKAN_GROUP_RANGES                                                                                                                                     \
    g_zukan_group_ranges[0], g_zukan_group_ranges[1], g_zukan_group_ranges[2], g_zukan_group_ranges[3], g_zukan_group_ranges[4], g_zukan_group_ranges[5],      \
        g_zukan_group_ranges[6]

#define ZUKAN_RESOURCE_VALUES                                                                                                                                  \
    LOM_ZUKAN_100(g_zukan_entry_values, 0), LOM_ZUKAN_100(g_zukan_entry_values, 100), LOM_ZUKAN_100(g_zukan_entry_values, 200),                                \
        LOM_ZUKAN_100(g_zukan_entry_values, 300), LOM_ZUKAN_100(g_zukan_entry_values, 400), LOM_ZUKAN_100(g_zukan_entry_values, 500),                          \
        LOM_ZUKAN_100(g_zukan_entry_values, 600), LOM_ZUKAN_100(g_zukan_entry_values, 700), LOM_ZUKAN_100(g_zukan_entry_values, 800),                          \
        LOM_ZUKAN_100(g_zukan_entry_values, 900), LOM_ZUKAN_10(g_zukan_entry_values, 1000), g_zukan_entry_values[1010], g_zukan_entry_values[1011],            \
        g_zukan_entry_values[1012], g_zukan_entry_values[1013], g_zukan_entry_values[1014], g_zukan_entry_values[1015], g_zukan_entry_values[1016],            \
        g_zukan_entry_values[1017]

#define ZUKAN_DISPLAY_ORDER                                                                                                                                    \
    LOM_ZUKAN_100(g_zukan_display_order, 0), LOM_ZUKAN_100(g_zukan_display_order, 100), LOM_ZUKAN_100(g_zukan_display_order, 200),                             \
        LOM_ZUKAN_100(g_zukan_display_order, 300), LOM_ZUKAN_100(g_zukan_display_order, 400), LOM_ZUKAN_100(g_zukan_display_order, 500),                       \
        LOM_ZUKAN_100(g_zukan_display_order, 600), LOM_ZUKAN_10(g_zukan_display_order, 700), g_zukan_display_order[710], g_zukan_display_order[711],           \
        g_zukan_display_order[712], g_zukan_display_order[713], g_zukan_display_order[714], g_zukan_display_order[715], g_zukan_display_order[716],            \
        g_zukan_display_order[717], g_zukan_display_order[718]

#endif
