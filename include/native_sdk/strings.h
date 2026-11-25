#include <string.h>
#include_next <strings.h>

#ifdef _WIN32
#include <stddef.h>
void bcopy(const void* src, void* dest, size_t n);
void bzero(void* s, size_t n);
#endif
