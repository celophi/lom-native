#ifndef LOM_SPECIFICATION_H
#define LOM_SPECIFICATION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOM_SPECIFICATIONS 128

typedef struct
{
    const char* sentence;
    void (*run)(void);
    int line;
} LomSpecification;

static LomSpecification lom_specifications[LOM_SPECIFICATIONS];

static int lom_specification_count;

/**
 * @brief Note one specification, as SPECIFICATION does before main starts.
 * @param sentence What it says the system should do.
 * @param run The specification.
 * @param line Its line, which puts the specifications in the file's order.
 */
__attribute__((unused)) static void lom_specification_add(const char* sentence, void (*run)(void), int line)
{
    int at = lom_specification_count++;

    while (at > 0 && lom_specifications[at - 1].line > line)
    {
        lom_specifications[at] = lom_specifications[at - 1];
        --at;
    }

    lom_specifications[at].sentence = sentence;
    lom_specifications[at].run = run;
    lom_specifications[at].line = line;
}

#define LOM_SPECIFICATION_JOIN_INNER(a, b) a##b
#define LOM_SPECIFICATION_JOIN(a, b) LOM_SPECIFICATION_JOIN_INNER(a, b)

#define SPECIFICATION(name, sentence)                                                                                                                          \
    static void name(void);                                                                                                                                    \
    __attribute__((constructor)) static void LOM_SPECIFICATION_JOIN(name, _noted)(void)                                                                        \
    {                                                                                                                                                          \
        lom_specification_add(sentence, name, __LINE__);                                                                                                       \
    }                                                                                                                                                          \
    static void name(void)

/**
 * @brief Run one of the file's specifications, after the setup every one of them starts from, the way an xUnit
 *        specification class makes a new instance for each test. With no number, list them.
 * @param argc The argument count.
 * @param argv The arguments: the specification's number, from 1, in the file's order.
 * @param setup The setup: the fakes put in, with the happy path's defaults.
 * @return 0 when the specification held; a failed check ends the program with 1 first.
 */
__attribute__((unused)) static int lom_specifications_main(int argc, char** argv, void (*setup)(void))
{
    const int which = argc > 1 ? atoi(argv[1]) : 0;

    if (which < 1 || which > lom_specification_count)
    {
        for (int i = 0; i < lom_specification_count; ++i)
        {
            printf("%d. %s\n", i + 1, lom_specifications[i].sentence);
        }

        return argc > 1 ? 2 : 0;
    }

    const LomSpecification* specification = &lom_specifications[which - 1];
    fprintf(stderr, "%s\n", specification->sentence);
    setup();
    specification->run();
    printf("passed: %s\n", specification->sentence);
    return 0;
}

#endif
