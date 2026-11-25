#ifndef LOM_VSYNC_CLOCK_H
#define LOM_VSYNC_CLOCK_H
#include <stdint.h>
#include <limits.h>
#include <string.h>

#if defined(__SIZEOF_INT128__)
typedef __uint128_t lvc_u128;
#else
typedef unsigned _BitInt(128) lvc_u128;
#endif

#define LVC_HISTORY_SPANS 16u

typedef struct
{
    lvc_u128 first_edge;

    uint64_t period, remaining, first_hcount;

    uint32_t numerator, lines;
} LomVSyncSpan;

typedef struct
{
    LomVSyncSpan history[LVC_HISTORY_SPANS];

    unsigned history_head, history_size;

    uint64_t count, queued, elapsed_ns, stamp_ns, phase, virtual_carry;

    uint64_t virtual_step_ns, hcount, hphase;

    uint32_t virtual_lines;

    uint32_t rate_num, rate_den, lines;

    int initialized, virtual_clock, paused;
} LomVSyncClock;

typedef struct
{
    uint64_t paused_ns, pause_start_ns;

    int paused;
} LomVSyncLifecycle;

/**
 * @brief Start a vertical blank clock.
 * @param c The clock.
 * @param now The time, in nanoseconds.
 * @param num Blanks per @p den seconds: the refresh rate as a fraction.
 * @param den The rate's denominator.
 * @param virtual_clock Nonzero to count blanks only when lvc_queue_virtual
 *        asks for one, as the tests' clock does, instead of by real time.
 * @return 0, or -1 for a zero rate.
 */
static int lvc_init(LomVSyncClock* c, uint64_t now, uint32_t num, uint32_t den, int virtual_clock)
{
    if (!num || !den)
    {
        return -1;
    }

    memset(c, 0, sizeof *c);
    c->stamp_ns = now;
    c->rate_num = num;
    c->rate_den = den;
    c->lines = 263;
    c->virtual_clock = !!virtual_clock;
    c->initialized = 1;
    return 0;
}

/**
 * @brief Remember a run of blanks that came due, so each can be delivered
 *        later with the time and horizontal count it really had. A run that
 *        carries straight on from the last one joins it.
 * @param c The clock.
 * @param first_edge When the first blank came, in rate units.
 * @param period The time between blanks, in the same units.
 * @param numerator The rate's numerator then.
 * @param count How many blanks.
 * @param first_hcount The horizontal count at the first one.
 * @param lines Lines per frame then.
 * @return 0, or -1 when the history is already full.
 */
static int lvc_append_span(LomVSyncClock* c, lvc_u128 first_edge, uint64_t period, uint32_t numerator, uint64_t count, uint64_t first_hcount, uint32_t lines)
{
    if (!count)
    {
        return 0;
    }

    if (c->history_size)
    {
        unsigned tail = (c->history_head + c->history_size - 1u) % LVC_HISTORY_SPANS;
        LomVSyncSpan* span = &c->history[tail];

        if (span->period == period && span->numerator == numerator && span->lines == lines &&
            (lvc_u128)span->first_hcount + (lvc_u128)span->remaining * lines == first_hcount &&
            span->first_edge + (lvc_u128)span->remaining * period == first_edge)
        {
            span->remaining += count;
            return 0;
        }
    }

    if (c->history_size == LVC_HISTORY_SPANS)
    {
        return -1;
    }

    unsigned tail = (c->history_head + c->history_size) % LVC_HISTORY_SPANS;
    c->history[tail] = (LomVSyncSpan){first_edge, period, count, first_hcount, numerator, lines};
    ++c->history_size;
    return 0;
}

/**
 * @brief Bring the clock up to a new time, queueing the blanks that have
 *        come due since the last call. A paused or virtual clock only moves
 *        its timestamp.
 * @param c The clock.
 * @param now The time, in nanoseconds; it can't go backwards.
 * @return 0, or -1 when time went backwards, a count would overflow, or the
 *         history is full.
 */
static int lvc_observe(LomVSyncClock* c, uint64_t now)
{
    if (!c->initialized || now < c->stamp_ns)
    {
        return -1;
    }

    if (c->paused || c->virtual_clock)
    {
        c->stamp_ns = now;
        return 0;
    }

    uint64_t delta = now - c->stamp_ns;
    lvc_u128 period = (lvc_u128)c->rate_den * 1000000000u;
    lvc_u128 total = (lvc_u128)delta * c->rate_num + c->phase;
    lvc_u128 due = total / period;
    lvc_u128 htotal = (lvc_u128)delta * c->rate_num * c->lines + c->hphase;
    lvc_u128 hadd = htotal / period;

    if (delta > UINT64_MAX - c->elapsed_ns || due > UINT64_MAX - c->queued || due + c->queued > UINT64_MAX - c->count || hadd > UINT64_MAX - c->hcount)
    {
        return -1;
    }

    lvc_u128 first_edge = (lvc_u128)c->elapsed_ns * c->rate_num + period - c->phase;
    lvc_u128 first_h = (lvc_u128)c->hcount * period + c->hphase + (lvc_u128)c->lines * (period - c->phase);

    if (due && first_h / period > UINT64_MAX)
    {
        return -1;
    }

    if (lvc_append_span(c, first_edge, (uint64_t)period, c->rate_num, (uint64_t)due, (uint64_t)(first_h / period), c->lines))
    {
        return -1;
    }

    c->elapsed_ns += delta;
    c->stamp_ns = now;
    c->phase = (uint64_t)(total % period);
    c->queued += (uint64_t)due;
    c->hcount += (uint64_t)hadd;
    c->hphase = (uint64_t)(htotal % period);
    return 0;
}

/**
 * @brief Read the horizontal (line) count, which keeps running between
 *        blanks.
 * @param c The clock.
 * @return Its low 32 bits.
 */
static uint32_t lvc_hcount(const LomVSyncClock* c)
{
    return (uint32_t)c->hcount;
}

/**
 * @brief Read the horizontal count the next blank to deliver had when it came.
 * @param c The clock.
 * @return That count, or the current one when nothing is queued or the clock
 *         is virtual.
 */
static uint32_t lvc_next_blank_hcount(const LomVSyncClock* c)
{
    if (!c->queued || c->virtual_clock)
    {
        return lvc_hcount(c);
    }

    return (uint32_t)c->history[c->history_head].first_hcount;
}

/**
 * @brief Queue the next blank of a virtual clock, one frame's time after the
 *        last, carrying the remainder so the rate stays exact.
 * @param c The clock, virtual and running, with nothing queued.
 * @return 0, or -1 when the clock isn't like that or a count would overflow.
 */
static int lvc_queue_virtual(LomVSyncClock* c)
{
    if (!c->initialized || !c->virtual_clock || c->paused || c->queued || c->count == UINT64_MAX || c->lines > UINT64_MAX - c->hcount)
    {
        return -1;
    }

    lvc_u128 time = (lvc_u128)c->rate_den * 1000000000u + c->virtual_carry;
    uint64_t step = (uint64_t)(time / c->rate_num);

    if (step > UINT64_MAX - c->elapsed_ns)
    {
        return -1;
    }

    c->virtual_step_ns = step;
    c->virtual_lines = c->lines;
    c->virtual_carry = (uint64_t)(time % c->rate_num);
    c->queued = 1;
    return 0;
}

/**
 * @brief Deliver the oldest queued blank, advancing the blank count (and,
 *        on a virtual clock, the time and horizontal count).
 * @param c The clock.
 * @return 0, or -1 when nothing is queued.
 */
static int lvc_pop(LomVSyncClock* c)
{
    if (!c->queued || c->count == UINT64_MAX)
    {
        return -1;
    }

    if (c->virtual_clock)
    {
        c->elapsed_ns += c->virtual_step_ns;
        c->virtual_step_ns = 0;
        c->phase = 0;
        c->hcount += c->virtual_lines;
        c->virtual_lines = 0;
    }
    else
    {
        if (!c->history_size || !c->history[c->history_head].remaining)
        {
            return -1;
        }

        LomVSyncSpan* span = &c->history[c->history_head];

        if (--span->remaining)
        {
            span->first_edge += span->period;
            span->first_hcount += span->lines;
        }
        else
        {
            c->history_head = (c->history_head + 1u) % LVC_HISTORY_SPANS;
            --c->history_size;
        }
    }

    ++c->count;
    --c->queued;
    return 0;
}

/**
 * @brief Work out how long until the next blank comes due.
 * @param c The clock.
 * @return Nanoseconds, rounded up.
 */
static uint64_t lvc_next_wait_ns(const LomVSyncClock* c)
{
    uint64_t period = (uint64_t)c->rate_den * 1000000000u;
    return (period - c->phase + c->rate_num - 1) / c->rate_num;
}

/**
 * @brief Change the display's timing, keeping how far into a frame the clock
 *        is.
 * @param c The clock.
 * @param num The new rate's numerator.
 * @param den The new rate's denominator.
 * @param lines Lines per frame: 263 for NTSC or 314 for PAL.
 * @return 0, or -1 for a zero rate, other line counts, or a clock that isn't
 *         started.
 */
static int lvc_display(LomVSyncClock* c, uint32_t num, uint32_t den, uint32_t lines)
{
    if (!c->initialized || !num || !den || (lines != 263 && lines != 314))
    {
        return -1;
    }

    lvc_u128 old_period = (lvc_u128)c->rate_den * 1000000000u;
    lvc_u128 new_period = (lvc_u128)den * 1000000000u;
    c->phase = (uint64_t)((lvc_u128)c->phase * new_period / old_period);
    c->hphase = (uint64_t)((lvc_u128)c->hphase * new_period / old_period);
    c->rate_num = num;
    c->rate_den = den;
    c->lines = lines;
    c->virtual_carry = 0;
    return 0;
}

/**
 * @brief Change the refresh rate, keeping the lines per frame.
 * @param c The clock.
 * @param num The new rate's numerator.
 * @param den The new rate's denominator.
 * @return 0, or -1 as lvc_display.
 */
static int lvc_rate(LomVSyncClock* c, uint32_t num, uint32_t den)
{
    return lvc_display(c, num, den, c->lines);
}

/**
 * @brief Switch between real and virtual time, with nothing queued.
 * @param c The clock.
 * @param now The time, in nanoseconds.
 * @param virtual_clock Nonzero for virtual time.
 * @return 0, or -1 while blanks are queued or when time went backwards.
 */
static int lvc_switch(LomVSyncClock* c, uint64_t now, int virtual_clock)
{
    if (!c->initialized || c->queued || now < c->stamp_ns)
    {
        return -1;
    }

    c->stamp_ns = now;
    c->virtual_clock = !!virtual_clock;
    return 0;
}

/**
 * @brief Stop counting blanks, after queueing the ones due up to now.
 * @param c The clock.
 * @param now The time, in nanoseconds.
 * @return 0, or -1 as lvc_observe.
 */
static int lvc_pause(LomVSyncClock* c, uint64_t now)
{
    if (lvc_observe(c, now))
    {
        return -1;
    }

    c->paused = 1;
    return 0;
}

/**
 * @brief Start counting blanks again from now. The paused time is skipped,
 *        not caught up on.
 * @param c The clock.
 * @param now The time, in nanoseconds.
 * @return 0, or -1 when time went backwards.
 */
static int lvc_resume(LomVSyncClock* c, uint64_t now)
{
    if (!c->initialized || now < c->stamp_ns)
    {
        return -1;
    }

    c->stamp_ns = now;
    c->paused = 0;
    return 0;
}

/**
 * @brief Note that the game has gone into the background, to leave that time
 *        out of lvc_active_time.
 * @param life The game's time in the foreground.
 * @param now The time, in nanoseconds.
 * @return 0.
 */
static int lvc_background(LomVSyncLifecycle* life, uint64_t now)
{
    if (!life->paused)
    {
        life->pause_start_ns = now;
        life->paused = 1;
    }

    return 0;
}

/**
 * @brief Note that the game is back in the foreground.
 * @param life The game's time in the foreground.
 * @param now The time, in nanoseconds.
 * @return 0, or -1 when time went backwards or the total would overflow.
 */
static int lvc_foreground(LomVSyncLifecycle* life, uint64_t now)
{
    if (life->paused)
    {
        if (now < life->pause_start_ns || now - life->pause_start_ns > UINT64_MAX - life->paused_ns)
        {
            return -1;
        }

        life->paused_ns += now - life->pause_start_ns;
        life->paused = 0;
    }

    return 0;
}

/**
 * @brief Work out how much of the time so far the game spent in the
 *        foreground.
 * @param life The game's time in the foreground.
 * @param now The time, in nanoseconds.
 * @param active Set to the foreground time, in nanoseconds.
 * @return 0, or -1 when time went backwards.
 */
static int lvc_active_time(const LomVSyncLifecycle* life, uint64_t now, uint64_t* active)
{
    uint64_t pause = life->paused_ns;

    if (life->paused)
    {
        if (now < life->pause_start_ns || now - life->pause_start_ns > UINT64_MAX - pause)
        {
            return -1;
        }

        pause += now - life->pause_start_ns;
    }

    if (pause > now)
    {
        return -1;
    }

    *active = now - pause;
    return 0;
}
#endif
