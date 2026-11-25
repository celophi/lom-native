#include "route_runner.h"

const char* const route_step_names[] = {"enter",      "exit",  "walk",       "talk",       "press",      "wait",   "wait_scene", "wait_battle",
                                        "checkpoint", "watch", "experience", "guest",      "companion",  "answer", "fight",      "land",
                                        "place",      "audio", "event",      "widescreen", "always_run", "switch"};

/**
 * @brief Read a plan line that sets something for the whole run: its frame
 *        limit, browsing the card, or how the title screen is gone through.
 * @param line The line.
 * @param word Its first word.
 * @return 1 when it was one of those, otherwise 0.
 */
static int read_run_setting(const char* line, const char* word)
{
    if (!strcmp(word, "limit"))
    {
        sscanf(line, "%*s %u", &g_run.vblank_limit);
        return 1;
    }

    if (!strcmp(word, "card_browse"))
    {
        g_run.card_browse = CARD_BROWSE_DOWN;
        return 1;
    }

    if (!strcmp(word, "title"))
    {
        char how[2][16] = {{0}};

        sscanf(line, "%*s %u %15s %15s", &g_run.title_item, how[0], how[1]);
        for (int i = 0; i < 2; i++)
        {
            g_run.title_up |= !strcmp(how[i], "up");
            g_run.title_back = !strcmp(how[i], "back") ? TITLE_BACK_PENDING : g_run.title_back;
            g_run.title_idle = !strcmp(how[i], "idle") ? TITLE_IDLE_WAITING : g_run.title_idle;
        }
        return 1;
    }

    return 0;
}

/**
 * @brief Find a step's kind by its name in a plan.
 * @param word The name.
 * @return The kind, or -1 when there's no such step.
 */
static int step_kind(const char* word)
{
    int kind = -1;

    for (int i = 0; i < (int)(sizeof(route_step_names) / sizeof(route_step_names[0])); i++)
    {
        if (!strcmp(word, route_step_names[i]))
        {
            kind = i;
        }
    }

    return kind;
}

/**
 * @brief Read a step's name or numbers from its plan line, and the points a
 *        walk goes via.
 * @param line The line.
 * @param step The step, with its kind set.
 */
static void read_step(const char* line, Step* step)
{
    if (step->kind == STEP_CHECKPOINT)
    {
        sscanf(line, "%*s %63s", step->name);
        return;
    }

    if (step->kind == STEP_SWITCH)
    {
        sscanf(line, "%*s %63s %i", step->name, &step->args[0]);
        return;
    }

    if (step->kind == STEP_AUDIO)
    {
        sscanf(line, "%*s %63s %i %i %i %i", step->name, &step->args[0], &step->args[1], &step->args[2], &step->args[3]);
        return;
    }

    const char* via = strstr(line, " via ");

    if (step->kind == STEP_TALK)
    {
        step->args[TALK_ARG_RECORD] = NO_RECORD;
    }

    sscanf(line, "%*s %i %i %i %i %i %i %i %i %i %i", &step->args[0], &step->args[1], &step->args[2], &step->args[3], &step->args[4], &step->args[5],
           &step->args[6], &step->args[7], &step->args[8], &step->args[9]);

    for (const char* cursor = via ? via + 5 : NULL; cursor && step->via_count < VIA_CAPACITY;)
    {
        int consumed;

        if (sscanf(cursor, "%i %i%n", &step->via[step->via_count][0], &step->via[step->via_count][1], &consumed) != 2)
        {
            break;
        }

        step->via_count++;
        cursor += consumed;
    }
}

/**
 * @brief Read the plan file.
 * @param path Plan file.
 * @return 0 on success, -1 on a bad plan.
 */
int read_plan(const char* path)
{
    FILE* file = fopen(path, "r");
    char line[256], word[32];

    if (!file)
    {
        fprintf(stderr, "route: can't open plan %s\n", path);
        return -1;
    }

    while (fgets(line, sizeof(line), file))
    {
        Step step = {0};

        if (line[0] == '#' || line[0] == '\n' || sscanf(line, "%31s", word) != 1 || read_run_setting(line, word))
        {
            continue;
        }

        const int kind = step_kind(word);

        if (kind < 0 || g_run.step_count >= STEP_CAPACITY)
        {
            fprintf(stderr, kind < 0 ? "route: bad plan line: %s" : "route: more than STEP_CAPACITY steps, at: %s", line);
            fclose(file);
            return -1;
        }

        step.kind = (StepKind)kind;
        read_step(line, &step);
        g_run.steps[g_run.step_count++] = step;
    }

    fclose(file);
    return 0;
}
