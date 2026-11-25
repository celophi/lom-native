#include "lom_native_remaster.h"
#include "lom_native_text.h"
#include "remaster_text.h"
#include "lom_native_cd.h"
#include "common/memory_map.h"
#include "common/saved_game.h"
#include "lom_native_env.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Say how many bytes of arguments follow a PS1 text code.
 * @param code The code.
 * @return 2 for code 18, 1 for the codes with one, otherwise 0.
 */
static int argument_bytes(unsigned code)
{
    if (code == 18)
    {
        return 2;
    }

    return code == TEXT_SPACES || code == TEXT_DELAY || code == TEXT_MACRO || code == TEXT_COLOR || (code >= TEXT_EXTRA && code <= TEXT_WIDE);
}

int remaster_text_ps1_length(const unsigned char* s, const unsigned char* limit)
{
    const unsigned char* p = s;

    while (p < limit)
    {
        const unsigned code = *p;
        p += 1 + argument_bytes(code);

        if (code == TEXT_END || code == TEXT_FINISH)
        {
            break;
        }
    }

    return (int)((p > limit ? limit : p) - s);
}

/**
 * @brief Check whether an answer starts with a word for yes or no, in any of
 *        the languages.
 * @param s The answer.
 * @param n Its length.
 * @return 1 for yes, 2 for no, 0 for neither.
 */
static int yes_or_no(const char* s, size_t n)
{
    static const char* const words[2][10] = {{"yes", "aye", "ja", "oui", "s\xC3\xAD", "si", "はい", "예", "是", "好"},
                                             {"no", "nein", "non", "いいえ", "아니요", "아니오", "否", "不", NULL}};

    while (n > 0 && (*s == ' ' || *s == '_'))
    {
        ++s;
        --n;
    }

    for (int answer = 0; answer < 2; ++answer)
    {
        for (int k = 0; k < 10 && words[answer][k]; ++k)
        {
            const char* w = words[answer][k];
            const size_t length = strlen(w);
            size_t i = 0;

            while (i < length && i < n)
            {
                const char a = s[i] >= 'A' && s[i] <= 'Z' ? (char)(s[i] - 'A' + 'a') : s[i];

                if (a != w[i])
                {
                    break;
                }

                ++i;
            }

            if (i == length && (i == n || !((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'))))
            {
                return answer + 1;
            }
        }
    }

    return 0;
}

/**
 * @brief Find the order of a PS1 yes or no question's answers.
 * @param s The PS1 text.
 * @param limit Where its buffer ends.
 * @return 12 for yes then no, 21 for no then yes, or 0 when it isn't such a
 *        question.
 */
static int ps1_choice_order(const unsigned char* s, const unsigned char* limit)
{
    int order = 0, count = 0;
    const int length = remaster_text_ps1_length(s, limit);

    for (int i = 0; i < length; ++i)
    {
        const unsigned code = s[i];

        if (argument_bytes(code))
        {
            i += argument_bytes(code);
        }
        else if (code == TEXT_CHOICE)
        {
            char label[32];
            size_t n = 0;

            while (i + 1 < length && s[i + 1] >= 0x20 && s[i + 1] < 0x7F && n < sizeof(label))
            {
                label[n++] = (char)s[++i];
            }

            const int answer = yes_or_no(label, n);

            if (answer == 0 || ++count > 2)
            {
                return 0;
            }

            order = order * 10 + answer;
        }
    }

    return count == 2 && (order == 12 || order == 21) ? order : 0;
}

/**
 * @brief Count the choices in PS1 text.
 * @param s The text.
 * @param length Its length.
 * @return How many.
 */
static int choice_count(const unsigned char* s, int length)
{
    int count = 0;

    for (int i = 0; i < length; ++i)
    {
        const unsigned code = s[i];

        if (code == TEXT_WIDE && i + 1 < length && s[i + 1] >= LOM_NATIVE_TEXT_WIDE_LEAD)
        {
            i += 2;
        }
        else if (argument_bytes(code))
        {
            i += argument_bytes(code);
        }
        else if (code == TEXT_CHOICE)
        {
            ++count;
        }
    }

    return count;
}

/**
 * @brief Find where an answer of the Remaster's text ends: at a line break or a
 *        mark.
 * @param s The answer.
 * @return Where it ends.
 */
static const char* answer_end(const char* s)
{
    while (*s && *s != '\n' && !remaster_text_starts(s, "↓") && !remaster_text_starts(s, "▼") && !remaster_text_starts(s, "◎") &&
           !remaster_text_starts(s, "▲") && !remaster_text_starts(s, "◆"))
    {
        ++s;
    }

    return s;
}

/**
 * @brief Put a yes or no question's answers in the PS1's order, since the
 *        game's script expects the answer it chose by position.
 * @param text The Remaster's text.
 * @param order The PS1's order, from ps1_choice_order().
 * @param out Where to write the text with its answers swapped.
 * @param room Bytes at @p out.
 * @return @p out when they were swapped, otherwise @p text.
 */
static const char* choice_order(const char* text, int order, char* out, size_t room)
{
    const char* answers[3];
    int count = 0;

    if (order == 0)
    {
        return text;
    }

    for (const char* p = strstr(text, "◎"); p; p = strstr(p, "◎"))
    {
        p += strlen("◎");

        if (count == 2)
        {
            return text;
        }

        answers[count++] = p;
    }

    if (count != 2)
    {
        return text;
    }

    const char* first_end = answer_end(answers[0]);
    const char* second_end = answer_end(answers[1]);
    const int first = yes_or_no(answers[0], (size_t)(first_end - answers[0]));
    const int second = yes_or_no(answers[1], (size_t)(second_end - answers[1]));

    if (first * 10 + second == order || second * 10 + first != order || strlen(text) + 1 > room)
    {
        return text;
    }

    size_t n = 0;

    const struct
    {
        const char* from;
        const char* to;
    } parts[] = {{text, answers[0]}, {answers[1], second_end}, {first_end, answers[1]}, {answers[0], first_end}, {second_end, text + strlen(text)}};

    for (size_t k = 0; k < sizeof(parts) / sizeof(parts[0]); ++k)
    {
        memcpy(out + n, parts[k].from, (size_t)(parts[k].to - parts[k].from));
        n += (size_t)(parts[k].to - parts[k].from);
    }

    out[n] = 0;
    return out;
}

typedef struct
{
    const char* name;
    size_t table;
    const unsigned char* strings;
    const unsigned char* limit;
    uint32_t span;
    int count;
    int remaster_count;
} SceneText;

/**
 * @brief Find a scene's table in the Remaster's text: its file's name in lower
 *        case, or with its last letter in upper case, as the Remaster has some.
 * @param disc_name The scene file's path on the disc.
 * @param name Receives the table's name.
 * @param name_size Bytes at @p name.
 * @param table Receives the table.
 * @return 0, or -1 when the Remaster has no text for the scene.
 */
static int find_scene_table(const char* disc_name, char* name, size_t name_size, size_t* table)
{
    const char* base = strrchr(disc_name, '/');
    size_t length = 0;
    base = base ? base + 1 : disc_name;

    for (; base[length] && base[length] != '.' && length + 1 < name_size; ++length)
    {
        name[length] = (char)(base[length] >= 'A' && base[length] <= 'Z' ? base[length] - 'A' + 'a' : base[length]);
    }

    name[length] = 0;

    if (lom_psb_member(&remaster_text_psb, remaster_text_psb.root, name, table) == 0)
    {
        return 0;
    }

    if (length == 0 || name[length - 1] < 'a' || name[length - 1] > 'z')
    {
        return -1;
    }

    name[length - 1] = (char)(name[length - 1] - 'a' + 'A');
    return lom_psb_member(&remaster_text_psb, remaster_text_psb.root, name, table) == 0 ? 0 : -1;
}

/**
 * @brief Check that the Remaster's table has a string for each of the scene's,
 *        allowing it one fewer when the PS1's last string is empty.
 * @param scene The scene, with its table, strings and count.
 * @return How many of the scene's strings the table has, or -1 when the two
 *         don't line up.
 */
static int remaster_string_count(const SceneText* scene)
{
    char key[16];
    size_t value;
    const int count = scene->count;

    if (count <= 0 || count > MAX_STRINGS)
    {
        return -1;
    }

    int remaster_count = count;
    snprintf(key, sizeof(key), "%d", count - 1);

    if (lom_psb_member(&remaster_text_psb, scene->table, key, &value) != 0)
    {
        const uint32_t last = (uint32_t)(scene->strings[(count - 1) * 2] | scene->strings[(count - 1) * 2 + 1] << 8);
        snprintf(key, sizeof(key), "%d", count - 2);

        if (count < 2 || last >= scene->span || scene->strings[last] != TEXT_END || lom_psb_member(&remaster_text_psb, scene->table, key, &value) != 0)
        {
            return -1;
        }

        remaster_count = count - 1;
    }

    snprintf(key, sizeof(key), "%d", count);
    return lom_psb_member(&remaster_text_psb, scene->table, key, &value) == 0 ? -1 : remaster_count;
}

/**
 * @brief Convert one of the Remaster's strings for a scene, in the PS1's order
 *        of choices, keeping the PS1's leading newlines.
 * @param scene The scene.
 * @param i Which string.
 * @param offset Where the PS1's string is in the scene's strings.
 * @param converted Receives the PS1 text.
 * @return Its length, or 0 or less when the PS1's string has to stay (no
 *         Remaster string, one the PS1 can't show, or a different number of
 *         choices).
 */
static int convert_scene_string(const SceneText* scene, int i, uint32_t offset, unsigned char* converted)
{
    static int s_log = -1;
    char reordered[MAX_STRING_BYTES * 2];
    char platform[MAX_STRING_BYTES * 2];
    char key[16];
    size_t value;
    const char* text = NULL;
    int n = -1;

    snprintf(key, sizeof(key), "%d", i);

    if (i < scene->remaster_count && lom_psb_member(&remaster_text_psb, scene->table, key, &value) == 0 && (text = lom_psb_string(&remaster_text_psb, value)))
    {
        const int order = offset < scene->span ? ps1_choice_order(scene->strings + offset, scene->limit) : 0;
        const char* one = remaster_text_one_platform(text, platform, sizeof(platform));
        n = one ? remaster_text_convert(choice_order(one, order, reordered, sizeof(reordered)), converted, MAX_STRING_BYTES) : -1;

        if (n > 0 && offset < scene->span &&
            choice_count(converted, n) != choice_count(scene->strings + offset, remaster_text_ps1_length(scene->strings + offset, scene->limit)))
        {
            n = -1;
        }
    }

    if (s_log < 0)
    {
        s_log = lom_native_env_on(LOM_ENV_REMASTER_TEXT_LOG);
    }

    if (s_log && n <= 0)
    {
        fprintf(stderr, "[lom-native] Text %s %d keeps the PS1's: %s\n", scene->name, i, text ? text : "(none)");
    }

    if (n <= 0)
    {
        return n;
    }

    int lead = 0;

    while (offset + (uint32_t)lead < scene->span && scene->strings[offset + lead] == TEXT_NEWLINE)
    {
        ++lead;
    }

    if (lead > 0 && converted[0] != TEXT_NEWLINE && n + lead <= MAX_STRING_BYTES)
    {
        memmove(converted + lead, converted, (size_t)n);
        memset(converted, TEXT_NEWLINE, (size_t)lead);
        n += lead;
    }

    return n;
}

/**
 * @brief Build the scene's new string table: its offsets, then each string,
 *        the Remaster's where it has one the PS1 can show and the PS1's
 *        otherwise, padded to a word.
 * @param scene The scene.
 * @param built Receives the table.
 * @param replaced Counts the strings that are the Remaster's.
 * @return The table's size, or -1 when it can't be built.
 */
static int build_scene_strings(const SceneText* scene, unsigned char* built, int* replaced)
{
    unsigned char converted[MAX_STRING_BYTES];
    int at = scene->count * 2;

    for (int i = 0; i < scene->count; ++i)
    {
        const uint32_t offset = (uint32_t)(scene->strings[i * 2] | scene->strings[i * 2 + 1] << 8);
        int n = convert_scene_string(scene, i, offset, converted);

        if (n > 0)
        {
            ++*replaced;
        }
        else
        {
            if (offset >= scene->span)
            {
                return -1;
            }

            n = remaster_text_ps1_length(scene->strings + offset, scene->limit);
            memcpy(converted, scene->strings + offset, (size_t)n);
        }

        built[i * 2] = (unsigned char)at;
        built[i * 2 + 1] = (unsigned char)(at >> 8);
        memcpy(built + at, converted, (size_t)n);
        at += n;

        if (at > 0xFFFF - MAX_STRING_BYTES)
        {
            return -1;
        }
    }

    while (at & 3)
    {
        built[at++] = 0;
    }

    return at;
}

/**
 * @brief With LOM_REMASTER_TEXT_DUMP set to a folder, save a scene's new string
 *        table there.
 * @param name The scene's table name.
 * @param built The table.
 * @param size Its size.
 */
static void dump_scene_strings(const char* name, const unsigned char* built, int size)
{
    const char* dump = lom_native_env_value(LOM_ENV_REMASTER_TEXT_DUMP);

    if (!dump)
    {
        return;
    }

    char path[PATH_SIZE];
    snprintf(path, sizeof(path), "%s/%s_%s.bin", dump, remaster_text_languages[remaster_text_language], name);
    FILE* file = fopen(path, "wb");

    if (file)
    {
        fwrite(built, 1, (size_t)size, file);
        fclose(file);
    }
}

int32_t lom_remaster_text_scene(const char* disc_name, unsigned char* data, int32_t size, size_t room)
{
    char name[32];
    SceneText scene = {name};

    if (!lom_native_remaster_text_in_use() || size < HEADER_WORDS * 4 || !remaster_text_open(lom_native_remaster_text_in_use()))
    {
        return size;
    }

    if (find_scene_table(disc_name, name, sizeof(name), &scene.table) != 0)
    {
        return size;
    }

    const uint32_t start = remaster_text_read_word(data + STRINGS_WORD * 4), end = remaster_text_read_word(data + (STRINGS_WORD + 1) * 4);

    if (start >= end || end > (uint32_t)size || end - start < 2)
    {
        return size;
    }

    scene.strings = data + start;
    scene.limit = data + end;
    scene.span = end - start;
    scene.count = (scene.strings[0] | scene.strings[1] << 8) / 2;
    scene.remaster_count = remaster_string_count(&scene);

    if (scene.remaster_count < 0)
    {
        return size;
    }

    unsigned char* built = malloc((size_t)scene.count * 2 + (size_t)scene.count * MAX_STRING_BYTES);

    if (!built)
    {
        return size;
    }

    int replaced = 0;
    const int at = build_scene_strings(&scene, built, &replaced);

    if (at < 0)
    {
        free(built);
        return size;
    }

    dump_scene_strings(name, built, at);

    const int32_t delta = at - (int32_t)scene.span;

    if ((size_t)(size + delta) > room)
    {
        free(built);
        return size;
    }

    memmove(data + end + delta, data + end, (size_t)(size - (int32_t)end));
    memcpy(data + start, built, (size_t)at);
    free(built);

    for (int w = 0; w < HEADER_WORDS; ++w)
    {
        const uint32_t offset = remaster_text_read_word(data + w * 4);

        if (w != STRINGS_WORD && offset >= end)
        {
            remaster_text_write_word(data + w * 4, offset + (uint32_t)delta);
        }
    }

    fprintf(stderr, "[lom-native] The Remaster's %s text for %s, %d of %d strings.\n", remaster_text_languages[remaster_text_language], name, replaced,
            scene.count);
    return size + delta;
}
