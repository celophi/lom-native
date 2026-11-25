#ifndef LOM_CHECK_H
#define LOM_CHECK_H
#include <stdio.h>
#include <stdlib.h>

static unsigned lom_check_count;

static unsigned lom_check_failures;

/**
 * @brief Report a failed CHECK and end the test with exit status 1.
 * @param file The source file the check is in.
 * @param line Its line.
 * @param condition The condition, as written.
 */
__attribute__((noreturn, unused)) static void lom_check_stop(const char* file, int line, const char* condition)
{
    fprintf(stderr, "%s:%d: check failed: %s\n", file, line, condition);
    exit(1);
}

/**
 * @brief Report a failed CHECK_DETAIL, with what the test knows about it, and
 *        end the test with exit status 1.
 * @param file The source file the check is in.
 * @param line Its line.
 * @param condition The condition, as written.
 * @param detail What else to print, like the events seen so far.
 */
__attribute__((noreturn, unused)) static void lom_check_stop_detail(const char* file, int line, const char* condition, const char* detail)
{
    fprintf(stderr, "%s:%d: check failed: %s (%s)\n", file, line, condition, detail);
    exit(1);
}

/**
 * @brief Count one CHECK_COUNTED, and report it if it failed. The test carries
 *        on either way, so one run reports every failure.
 * @param passed Whether the condition held.
 * @param file The source file the check is in.
 * @param line Its line.
 * @param condition The condition, as written.
 */
__attribute__((unused)) static void lom_check_counted(int passed, const char* file, int line, const char* condition)
{
    ++lom_check_count;

    if (!passed)
    {
        ++lom_check_failures;
        fprintf(stderr, "%s:%d: check failed: %s\n", file, line, condition);
    }
}

/**
 * @brief Print how the CHECK_COUNTED checks went, and turn that into the
 *        test's exit status.
 * @param name The test's name, for the summary line.
 * @return 0 when every check passed, 1 otherwise.
 */
__attribute__((unused)) static int lom_check_summary(const char* name)
{
    if (lom_check_failures)
    {
        fprintf(stderr, "%s: %u of %u checks failed\n", name, lom_check_failures, lom_check_count);
        return 1;
    }

    printf("%s: %u checks passed\n", name, lom_check_count);
    return 0;
}

#define CHECK(condition) ((condition) ? (void)0 : lom_check_stop(__FILE__, __LINE__, #condition))

#define CHECK_DETAIL(condition, detail) ((condition) ? (void)0 : lom_check_stop_detail(__FILE__, __LINE__, #condition, (detail)))

#define CHECK_COUNTED(condition) lom_check_counted((condition) ? 1 : 0, __FILE__, __LINE__, #condition)

#endif
