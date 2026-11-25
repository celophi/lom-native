#include <psyz/log.h>

#include <stdarg.h>
#include <stdio.h>

/**
 * @brief Provide logging for the SDL-independent PSY-Z bootstrap target.
 * @param level PSY-Z log level.
 * @param file Source file that emitted the message.
 * @param line Source line that emitted the message.
 * @param func Function that emitted the message.
 * @param fmt printf-style message format.
 */
void psyz_log(unsigned int level, const char* file, unsigned int line, const char* func, const char* fmt, ...)
{
    va_list args;

    (void)level;
    fprintf(stderr, "%s:%u %s: ", file, line, func);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}
