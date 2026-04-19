/*
 * Astra Programming Language
 * common.h — Shared definitions, macros, and debug flags
 *
 * Copyright (c) 2026 Astra Contributors
 * Licensed under the MIT License
 */

#ifndef astra_common_h
#define astra_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Version ─────────────────────────────────────────────── */
#define ASTRA_VERSION_MAJOR 1
#define ASTRA_VERSION_MINOR 0
#define ASTRA_VERSION_PATCH 0
#define ASTRA_VERSION_STRING "1.0.0"

/* ── Debug flags (uncomment to enable) ───────────────────── */
// #define DEBUG_PRINT_CODE        /* Print compiled bytecode     */
// #define DEBUG_TRACE_EXECUTION   /* Trace VM instruction steps  */
// #define DEBUG_STRESS_GC         /* GC on every allocation      */
// #define DEBUG_LOG_GC            /* Log GC events               */

/* ── Limits ──────────────────────────────────────────────── */
#define FRAMES_MAX     256
#define STACK_MAX      (FRAMES_MAX * 256)
#define UINT8_COUNT    (UINT8_MAX + 1)

/* ── Utility macros ──────────────────────────────────────── */
#define UNUSED(x)      ((void)(x))

#endif /* astra_common_h */
