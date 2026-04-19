/*
 * Astra Programming Language
 * object.h — Heap-allocated objects (strings, lists, maps, functions, etc.)
 */

#ifndef astra_object_h
#define astra_object_h

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

/* ── Object types ────────────────────────────────────────── */

typedef enum {
    OBJ_STRING,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_FUNCTION,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_NATIVE,
    OBJ_STRUCT,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_ITERATOR,
} ObjType;

/* ── Base object (must be first field of every object) ───── */

struct Obj {
    ObjType     type;
    bool        isMarked;
    struct Obj* next;
};

/* ── String ──────────────────────────────────────────────── */

struct ObjString {
    Obj      obj;
    int      length;
    char*    chars;
    uint32_t hash;
};

/* ── List ────────────────────────────────────────────────── */

typedef struct {
    Obj        obj;
    ValueArray items;
} ObjList;

/* ── Map ─────────────────────────────────────────────────── */

typedef struct {
    Obj   obj;
    Table entries;
} ObjMap;

/* ── Function ────────────────────────────────────────────── */

typedef struct {
    Obj        obj;
    int        arity;
    int        upvalueCount;
    Chunk      chunk;
    ObjString* name;
} ObjFunction;

/* ── Upvalue ─────────────────────────────────────────────── */

typedef struct ObjUpvalue {
    Obj                obj;
    Value*             location;
    Value              closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

/* ── Closure ─────────────────────────────────────────────── */

typedef struct {
    Obj          obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int          upvalueCount;
} ObjClosure;

/* ── Native function ─────────────────────────────────────── */

typedef Value (*NativeFn)(int argCount, Value* args);

typedef struct {
    Obj        obj;
    NativeFn   function;
    ObjString* name;
    int        arity;      /* -1 = variadic */
} ObjNative;

/* ── Struct definition ───────────────────────────────────── */

typedef struct {
    Obj        obj;
    ObjString* name;
    Table      methods;
    Table      fields;     /* Default field values */
    int        fieldCount;
} ObjStruct;

/* ── Struct instance ─────────────────────────────────────── */

typedef struct {
    Obj        obj;
    ObjStruct* klass;
    Table      fields;
} ObjInstance;

/* ── Bound method ────────────────────────────────────────── */

typedef struct {
    Obj        obj;
    Value      receiver;
    ObjClosure* method;
} ObjBoundMethod;

/* ── Iterator ────────────────────────────────────────────── */

typedef struct {
    Obj   obj;
    Value iterable;
    int   index;
} ObjIterator;

/* ── Type check macros ───────────────────────────────────── */

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_LIST(value)         isObjType(value, OBJ_LIST)
#define IS_MAP(value)          isObjType(value, OBJ_MAP)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRUCT(value)       isObjType(value, OBJ_STRUCT)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)

/* ── Value unwrap macros ─────────────────────────────────── */

#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_LIST(value)         ((ObjList*)AS_OBJ(value))
#define AS_MAP(value)          ((ObjMap*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_NATIVE(value)       ((ObjNative*)AS_OBJ(value))
#define AS_NATIVE_FN(value)    (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRUCT(value)       ((ObjStruct*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))

/* ── Constructor functions ───────────────────────────────── */

ObjString*      copyString(const char* chars, int length);
ObjString*      takeString(char* chars, int length);
ObjString*      concatenateStrings(ObjString* a, ObjString* b);
ObjString*      formatString(const char* format, ...);
ObjList*        newList(void);
ObjMap*         newMap(void);
ObjFunction*    newFunction(void);
ObjClosure*     newClosure(ObjFunction* function);
ObjUpvalue*     newUpvalue(Value* slot);
ObjNative*      newNative(NativeFn function, ObjString* name, int arity);
ObjStruct*      newStruct(ObjString* name);
ObjInstance*    newInstance(ObjStruct* klass);
ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method);
ObjIterator*    newIterator(Value iterable);

/* ── Utility ─────────────────────────────────────────────── */

void        printObject(Value value);
char*       objectToString(Value value);
const char* objectTypeName(Obj* obj);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif /* astra_object_h */
