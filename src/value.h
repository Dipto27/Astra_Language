/*
 * Astra Programming Language
 * value.h — Value representation using tagged unions
 */

#ifndef astra_value_h
#define astra_value_h

#include "common.h"

/* Forward declarations for objects */
typedef struct Obj     Obj;
typedef struct ObjString ObjString;

/* ── Value types ─────────────────────────────────────────── */

typedef enum {
    VAL_BOOL,
    VAL_NONE,
    VAL_INT,
    VAL_FLOAT,
    VAL_OBJ,
} ValueType;

/* ── Value struct ────────────────────────────────────────── */

typedef struct {
    ValueType type;
    union {
        bool    boolean;
        int64_t integer;
        double  floating;
        Obj*    obj;
    } as;
} Value;

/* ── Type check macros ───────────────────────────────────── */

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NONE(value)    ((value).type == VAL_NONE)
#define IS_INT(value)     ((value).type == VAL_INT)
#define IS_FLOAT(value)   ((value).type == VAL_FLOAT)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)
#define IS_NUMBER(value)  (IS_INT(value) || IS_FLOAT(value))

/* ── Value unwrap macros ─────────────────────────────────── */

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_INT(value)     ((value).as.integer)
#define AS_FLOAT(value)   ((value).as.floating)
#define AS_OBJ(value)     ((value).as.obj)

/* ── Value creation macros ───────────────────────────────── */

#define BOOL_VAL(value)   ((Value){VAL_BOOL,  {.boolean  = (value)}})
#define NONE_VAL          ((Value){VAL_NONE,   {.integer  = 0}})
#define INT_VAL(value)    ((Value){VAL_INT,    {.integer  = (value)}})
#define FLOAT_VAL(value)  ((Value){VAL_FLOAT,  {.floating = (value)}})
#define OBJ_VAL(object)   ((Value){VAL_OBJ,    {.obj      = (Obj*)(object)}})

/* ── Number coercion helper ──────────────────────────────── */

static inline double AS_NUMBER(Value value) {
    if (value.type == VAL_INT) return (double)value.as.integer;
    return value.as.floating;
}

/* ── Value Array (dynamic array of values) ───────────────── */

typedef struct {
    int    capacity;
    int    count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

/* ── Value operations ────────────────────────────────────── */

bool  valuesEqual(Value a, Value b);
void  printValue(Value value);
char* valueToString(Value value);
const char* valueTypeName(Value value);

#endif /* astra_value_h */
