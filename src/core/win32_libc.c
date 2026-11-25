#include <string.h>
#include <strings.h>

/**
 * @brief The C library's bcopy, which Windows doesn't have.
 * @param src What to copy.
 * @param dest Where to.
 * @param n How many bytes.
 */
void bcopy(const void* src, void* dest, size_t n)
{
    memmove(dest, src, n);
}

/**
 * @brief The C library's bzero, which Windows doesn't have.
 * @param s What to clear.
 * @param n How many bytes.
 */
void bzero(void* s, size_t n)
{
    memset(s, 0, n);
}
