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

int strncmp(const char* s1, const char* s2, size_t n)
{
    while(n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    char *d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
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
