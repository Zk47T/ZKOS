#include "string.h"

size_t strlen(const char* s)
{
    size_t n = 0;
    while(s[n])
        n++;

    return n;
}

int strcmp(const char* s1, const char* s2)
{
    while(*s1 && *s1 == *s2)
    {
        s1 ++;
        s2 ++;
    }

    return *s1 - *s2;
}

void *memset(void *dst, int value, size_t n)
{
    char *d = dst;
    while(n--)
        *d++ = value;
    return dst;
}

void *memcpy(void *dst, const void* src, size_t n)
{
    char *d = dst;
    const char *s = src;
    while(n--)
        *d++ = *s++;
    return dst;
}