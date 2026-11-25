#include "common.h"
#include "../internal/field_calls.h"
#include <libetc.h>
#include <memory.h>
#include <libmcx.h>
#include "lom_native_card.h"
#include "lom_native_time.h"

#define FRAMES_PER_SECOND 60

#define FRAMES_PER_MINUTE (FRAMES_PER_SECOND * 60)

#define FRAMES_PER_HOUR (FRAMES_PER_MINUTE * 60)

#define BCD_HIGH(bcd) ((bcd) >> 4)

#define BCD_LOW(bcd) ((bcd) & 0xF)

#define BCD_TO_INT(bcd) (BCD_HIGH(bcd) * 10 + BCD_LOW(bcd))

#define BCD_YEAR_TO_INT(bcd) (BCD_HIGH((bcd)[0]) * 1000 + BCD_LOW((bcd)[0]) * 100 + BCD_HIGH((bcd)[1]) * 10 + BCD_LOW((bcd)[1]))

#define CARD_CLOCK_PORT 0

typedef struct
{
    u8 year[2];
    u8 month;
    u8 day;
    u8 weekday;
    u8 hour;
    u8 minute;
    u8 second;
} FieldCardClockBcd;

typedef struct FieldCardClock
{
    s16 year;
    u8 month;
    u8 day;
    u8 weekday;
    u8 hour;
    u8 minute;
    u8 second;
} FieldCardClock;

extern FieldCardClockBcd g_field_card_clock_snapshot;

extern s32 g_field_card_clock_vsync;

/**
 * @brief Decode the cached PocketStation clock and advance it by the frames elapsed since it was read.
 * @param clock Receives the decoded date and the advanced time of day.
 * @return 1 when a clock snapshot is available, otherwise 0.
 */
s32 field_get_card_clock(FieldCardClock* clock)
{
    s32 frame_offset;
    s32 vsync_count;
    u32 time_frames;

    if (g_field_card_clock_valid != 0)
    {
        clock->year = BCD_YEAR_TO_INT(g_field_card_clock_snapshot.year);
        clock->month = BCD_TO_INT(g_field_card_clock_snapshot.month);
        clock->day = BCD_TO_INT(g_field_card_clock_snapshot.day);
        clock->weekday = BCD_LOW(g_field_card_clock_snapshot.weekday);
        clock->hour = BCD_TO_INT(g_field_card_clock_snapshot.hour);
        clock->minute = BCD_TO_INT(g_field_card_clock_snapshot.minute);
        clock->second = BCD_TO_INT(g_field_card_clock_snapshot.second);

        vsync_count = lom_time_frames();
        frame_offset = clock->second * FRAMES_PER_SECOND + clock->minute * FRAMES_PER_MINUTE + clock->hour * FRAMES_PER_HOUR - g_field_card_clock_vsync;
        time_frames = frame_offset + vsync_count;
        clock->hour = time_frames / FRAMES_PER_HOUR;
        clock->minute = (time_frames - clock->hour * FRAMES_PER_HOUR) / FRAMES_PER_MINUTE;
        clock->second = (time_frames - clock->hour * FRAMES_PER_HOUR - clock->minute * FRAMES_PER_MINUTE) / FRAMES_PER_SECOND;
        return 1;
    }

    return 0;
}

/**
 * @brief Read the PocketStation clock into the cache if no snapshot was taken yet.
 * @note Also records the VSync counter so field_get_card_clock() can advance the time later.
 */
void field_capture_card_clock(void)
{
    s32 status;
    long command;
    long result;
    FieldCardClockBcd time;

    if (g_field_card_clock_valid == 0)
    {
        status = lom_native_field_clock_card_type(CARD_CLOCK_PORT);

        if (status == MCX_COMMAND_ISSUED)
        {
            lom_native_card_mcx_sync(MCX_SYNC_WAIT, &command, &result);

            if (result == McxErrSuccess || result == McxErrNewCard)
            {
                if (lom_native_card_mcx_get_time(CARD_CLOCK_PORT, (u8*)&time) == MCX_COMMAND_ISSUED)
                {
                    lom_native_card_mcx_sync(MCX_SYNC_WAIT, &command, &result);
                }

                if (result == McxErrSuccess)
                {
                    bcopy((u8*)&time, (u8*)&g_field_card_clock_snapshot, sizeof(time));
                    g_field_card_clock_vsync = lom_time_frames();
                    g_field_card_clock_valid = status;
                }
            }
        }
    }
}
