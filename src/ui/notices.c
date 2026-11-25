#include "lom_native_notices.h"

#include <stdio.h>
#include <string.h>

#define NOTICE_CAPACITY 8

#define SUBJECT_BYTES 72

#define DETAIL_BYTES 256

typedef struct
{
    LomNoticeKind kind;
    char subject[SUBJECT_BYTES];
    char detail[DETAIL_BYTES];
    unsigned number;
} Notice;

static Notice s_notices[NOTICE_CAPACITY];

static int s_count;

static unsigned s_posted;

/**
 * @brief Find a kept notice by its place, oldest first.
 * @param index The place.
 * @return The notice, or NULL when the index is out of range.
 */
static const Notice* notice_at(int index)
{
    return index >= 0 && index < s_count ? &s_notices[index] : NULL;
}

void lom_native_notice_post(LomNoticeKind kind, const char* subject, const char* detail)
{
    if (s_count == NOTICE_CAPACITY)
    {
        memmove(&s_notices[0], &s_notices[1], sizeof(s_notices[0]) * (NOTICE_CAPACITY - 1));
        --s_count;
    }

    Notice* notice = &s_notices[s_count++];
    notice->kind = kind;
    notice->number = ++s_posted;
    snprintf(notice->subject, sizeof(notice->subject), "%s", subject ? subject : "");
    snprintf(notice->detail, sizeof(notice->detail), "%s", detail ? detail : "");
    notice->detail[strcspn(notice->detail, "\r\n")] = 0;
}

int lom_native_notice_count(void)
{
    return s_count;
}

LomNoticeKind lom_native_notice_kind(int index)
{
    const Notice* notice = notice_at(index);
    return notice ? notice->kind : LOM_NOTICE_MOD_REFUSED;
}

const char* lom_native_notice_subject(int index)
{
    const Notice* notice = notice_at(index);
    return notice ? notice->subject : "";
}

const char* lom_native_notice_detail(int index)
{
    const Notice* notice = notice_at(index);
    return notice ? notice->detail : "";
}

unsigned lom_native_notice_number(int index)
{
    const Notice* notice = notice_at(index);
    return notice ? notice->number : 0;
}
