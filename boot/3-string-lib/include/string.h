#ifndef STRING_H
#define STRING_H

typedef unsigned long size_t; 

size_t strlen(const char* s);
int strcmp(const char* s1, const char* s2);

void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n );
#endif