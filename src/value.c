/*
 * Astra Programming Language
 * value.c — Value representation implementation
 */

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

/* ── Value Array ─────────────────────────────────────────── */

void initValueArray(ValueArray* array) {
    array->values   = NULL;
    array->capacity = 0;
    array->count    = 0;
}

void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values   = GROW_ARRAY(Value, array->values,
                                     oldCapacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/* ── Value equality ──────────────────────────────────────── */

bool valuesEqual(Value a, Value b) {
    /* Handle numeric comparisons: int == float should work */
    if (IS_NUMBER(a) && IS_NUMBER(b)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }

    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:  return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NONE:  return true;
        case VAL_INT:   return AS_INT(a) == AS_INT(b);
        case VAL_FLOAT: return AS_FLOAT(a) == AS_FLOAT(b);
        case VAL_OBJ:   return AS_OBJ(a) == AS_OBJ(b);
        default:        return false;
    }
}

/* ── Print a value ───────────────────────────────────────── */

void printValue(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NONE:
            printf("none");
            break;
        case VAL_INT:
            printf("%lld", (long long)AS_INT(value));
            break;
        case VAL_FLOAT: {
            double f = AS_FLOAT(value);
            /* Print integers without trailing zeros */
            if (f == (int64_t)f && f >= -1e15 && f <= 1e15) {
                printf("%.1f", f);
            } else {
                printf("%g", f);
            }
            break;
        }
        case VAL_OBJ:
            printObject(value);
            break;
    }
}

/* ── Convert value to string (caller must free) ──────────── */

char* valueToString(Value value) {
    char buffer[256];

    switch (value.type) {
        case VAL_BOOL:
            snprintf(buffer, sizeof(buffer), "%s",
                     AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NONE:
            snprintf(buffer, sizeof(buffer), "none");
            break;
        case VAL_INT:
            snprintf(buffer, sizeof(buffer), "%lld",
                     (long long)AS_INT(value));
            break;
        case VAL_FLOAT:
            snprintf(buffer, sizeof(buffer), "%g", AS_FLOAT(value));
            break;
        case VAL_OBJ:
            return objectToString(value);
    }

    size_t len = strlen(buffer);
    char*  str = (char*)reallocate(NULL, 0, len + 1);
    memcpy(str, buffer, len + 1);
    return str;
}

/* ── Human-readable type name ────────────────────────────── */

const char* valueTypeName(Value value) {
    switch (value.type) {
        case VAL_BOOL:  return "bool";
        case VAL_NONE:  return "none";
        case VAL_INT:   return "int";
        case VAL_FLOAT: return "float";
        case VAL_OBJ:   return objectTypeName(AS_OBJ(value));
        default:        return "unknown";
    }
}
