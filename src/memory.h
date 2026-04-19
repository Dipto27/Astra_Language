/*
 * Astra Programming Language
 * memory.h — Memory management and garbage collection
 */

#ifndef astra_memory_h
#define astra_memory_h

#include "common.h"
#include "value.h"

/* ── Allocation macros ───────────────────────────────────── */

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) \
    reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
                      sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

/* ── Functions ───────────────────────────────────────────── */

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void  freeObjects(void);
void  collectGarbage(void);
void  markValue(Value value);
void  markObject(Obj* object);

#endif /* astra_memory_h */
