#ifndef TYPES_H
#define TYPES_H

/* ============================================================
 * Integer Types (AArch64 - 64-bit platform)
 * ============================================================ */

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long      uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long        int64_t;

/* ============================================================
 * Size and Pointer Types
 * ============================================================ */

typedef unsigned long      size_t;
typedef signed long        ssize_t;
typedef unsigned long      uintptr_t;
typedef signed long        intptr_t;

/* ============================================================
 * Boolean Type
 * ============================================================ */

typedef enum {
    false = 0,
    true = 1
} bool;

/* ============================================================
 * NULL Pointer
 * ============================================================ */

#define NULL ((void *)0)

#endif /* TYPES_H */
