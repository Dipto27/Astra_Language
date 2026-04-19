/*
 * Astra Programming Language
 * object.c — Heap-allocated object implementation
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

/* ── Allocate an object of a given type ──────────────────── */

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object    = (Obj*)reallocate(NULL, 0, size);
    object->type     = type;
    object->isMarked = false;
    object->next     = vm.objects;
    vm.objects       = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

/* ── String hashing (FNV-1a) ─────────────────────────────── */

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

/* ── String allocation (interned) ────────────────────────── */

static ObjString* allocateString(char* chars, int length,
                                  uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
    string->length = length;
    string->chars  = chars;
    string->hash   = hash;

    /* Intern into the VM's string table */
    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NONE_VAL);
    pop();

    return string;
}

ObjString* copyString(const char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    /* Check intern table first */
    ObjString* interned = tableFindString(&vm.strings, chars,
                                          length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

ObjString* takeString(char* chars, int length) {
    uint32_t hash = hashString(chars, length);

    ObjString* interned = tableFindString(&vm.strings, chars,
                                          length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

ObjString* concatenateStrings(ObjString* a, ObjString* b) {
    int   length = a->length + b->length;
    char* chars  = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';
    return takeString(chars, length);
}

ObjString* formatString(const char* format, ...) {
    char    buffer[1024];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (length < 0) length = 0;
    if (length >= (int)sizeof(buffer)) length = sizeof(buffer) - 1;

    return copyString(buffer, length);
}

/* ── List ────────────────────────────────────────────────── */

ObjList* newList(void) {
    ObjList* list = ALLOCATE_OBJ(ObjList, OBJ_LIST);
    initValueArray(&list->items);
    return list;
}

/* ── Map ─────────────────────────────────────────────────── */

ObjMap* newMap(void) {
    ObjMap* map = ALLOCATE_OBJ(ObjMap, OBJ_MAP);
    initTable(&map->entries);
    return map;
}

/* ── Function ────────────────────────────────────────────── */

ObjFunction* newFunction(void) {
    ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
    function->arity        = 0;
    function->upvalueCount = 0;
    function->name         = NULL;
    initChunk(&function->chunk);
    return function;
}

/* ── Closure ─────────────────────────────────────────────── */

ObjClosure* newClosure(ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure    = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
    closure->function      = function;
    closure->upvalues      = upvalues;
    closure->upvalueCount  = function->upvalueCount;
    return closure;
}

/* ── Upvalue ─────────────────────────────────────────────── */

ObjUpvalue* newUpvalue(Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
    upvalue->location   = slot;
    upvalue->closed     = NONE_VAL;
    upvalue->next       = NULL;
    return upvalue;
}

/* ── Native function ─────────────────────────────────────── */

ObjNative* newNative(NativeFn function, ObjString* name, int arity) {
    ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
    native->function  = function;
    native->name      = name;
    native->arity     = arity;
    return native;
}

/* ── Struct definition ───────────────────────────────────── */

ObjStruct* newStruct(ObjString* name) {
    ObjStruct* klass = ALLOCATE_OBJ(ObjStruct, OBJ_STRUCT);
    klass->name       = name;
    klass->fieldCount = 0;
    initTable(&klass->methods);
    initTable(&klass->fields);
    return klass;
}

/* ── Instance ────────────────────────────────────────────── */

ObjInstance* newInstance(ObjStruct* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
    instance->klass       = klass;
    initTable(&instance->fields);

    /* Copy default field values from the struct definition */
    tableAddAll(&klass->fields, &instance->fields);
    return instance;
}

/* ── Bound method ────────────────────────────────────────── */

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver       = receiver;
    bound->method         = method;
    return bound;
}

/* ── Iterator ────────────────────────────────────────────── */

ObjIterator* newIterator(Value iterable) {
    ObjIterator* iter = ALLOCATE_OBJ(ObjIterator, OBJ_ITERATOR);
    iter->iterable    = iterable;
    iter->index       = 0;
    return iter;
}

/* ── Print an object value ───────────────────────────────── */

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_LIST: {
            ObjList* list = AS_LIST(value);
            printf("[");
            for (int i = 0; i < list->items.count; i++) {
                if (i > 0) printf(", ");
                if (IS_STRING(list->items.values[i])) {
                    printf("\"%s\"", AS_CSTRING(list->items.values[i]));
                } else {
                    printValue(list->items.values[i]);
                }
            }
            printf("]");
            break;
        }

        case OBJ_MAP: {
            ObjMap* map = AS_MAP(value);
            printf("{");
            bool first = true;
            for (int i = 0; i < map->entries.capacity; i++) {
                Entry* entry = &map->entries.entries[i];
                if (entry->key == NULL) continue;
                if (!first) printf(", ");
                printf("\"%s\": ", entry->key->chars);
                if (IS_STRING(entry->value)) {
                    printf("\"%s\"", AS_CSTRING(entry->value));
                } else {
                    printValue(entry->value);
                }
                first = false;
            }
            printf("}");
            break;
        }

        case OBJ_FUNCTION: {
            ObjFunction* fn = AS_FUNCTION(value);
            if (fn->name == NULL) {
                printf("<script>");
            } else {
                printf("<fn %s>", fn->name->chars);
            }
            break;
        }

        case OBJ_CLOSURE:
            printObject(OBJ_VAL(AS_CLOSURE(value)->function));
            break;

        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;

        case OBJ_NATIVE:
            printf("<native fn %s>", AS_NATIVE(value)->name->chars);
            break;

        case OBJ_STRUCT:
            printf("<struct %s>", AS_STRUCT(value)->name->chars);
            break;

        case OBJ_INSTANCE:
            printf("<instance %s>",
                   AS_INSTANCE(value)->klass->name->chars);
            break;

        case OBJ_BOUND_METHOD:
            printObject(OBJ_VAL(
                AS_BOUND_METHOD(value)->method->function));
            break;

        case OBJ_ITERATOR:
            printf("<iterator>");
            break;
    }
}

/* ── Object to string (caller must free) ─────────────────── */

char* objectToString(Value value) {
    char buffer[256];

    switch (OBJ_TYPE(value)) {
        case OBJ_STRING: {
            ObjString* str = AS_STRING(value);
            char* result = (char*)reallocate(NULL, 0, str->length + 1);
            memcpy(result, str->chars, str->length + 1);
            return result;
        }

        case OBJ_LIST: {
            ObjList* list = AS_LIST(value);
            /* Build string like "[1, 2, 3]" */
            int capacity = 256;
            char* buf = (char*)reallocate(NULL, 0, capacity);
            int pos = 0;
            buf[pos++] = '[';
            for (int i = 0; i < list->items.count; i++) {
                if (i > 0) {
                    if (pos + 2 >= capacity) {
                        int old = capacity;
                        capacity *= 2;
                        buf = (char*)realloc(buf, capacity);
                        (void)old;
                    }
                    buf[pos++] = ',';
                    buf[pos++] = ' ';
                }
                char* elem;
                if (IS_STRING(list->items.values[i])) {
                    ObjString* s = AS_STRING(list->items.values[i]);
                    int needed = s->length + 2 + 1;
                    if (pos + needed >= capacity) {
                        int old = capacity;
                        capacity = (pos + needed) * 2;
                        buf = (char*)realloc(buf, capacity);
                        (void)old;
                    }
                    buf[pos++] = '"';
                    memcpy(buf + pos, s->chars, s->length);
                    pos += s->length;
                    buf[pos++] = '"';
                    continue;
                } else {
                    elem = valueToString(list->items.values[i]);
                }
                int elen = (int)strlen(elem);
                if (pos + elen >= capacity) {
                    int old = capacity;
                    capacity = (pos + elen) * 2;
                    buf = (char*)realloc(buf, capacity);
                    (void)old;
                }
                memcpy(buf + pos, elem, elen);
                pos += elen;
                reallocate(elem, elen + 1, 0);
            }
            if (pos + 2 >= capacity) {
                capacity = pos + 2;
                buf = (char*)realloc(buf, capacity);
            }
            buf[pos++] = ']';
            buf[pos] = '\0';
            return buf;
        }

        case OBJ_MAP: {
            ObjMap* map = AS_MAP(value);
            int capacity = 256;
            char* buf = (char*)reallocate(NULL, 0, capacity);
            int pos = 0;
            buf[pos++] = '{';
            bool first = true;
            for (int i = 0; i < map->entries.capacity; i++) {
                Entry* entry = &map->entries.entries[i];
                if (entry->key == NULL) continue;
                if (!first) {
                    if (pos + 2 >= capacity) { capacity *= 2; buf = (char*)realloc(buf, capacity); }
                    buf[pos++] = ','; buf[pos++] = ' ';
                }
                /* key */
                int klen = entry->key->length;
                if (pos + klen + 4 >= capacity) { capacity = (pos + klen + 4) * 2; buf = (char*)realloc(buf, capacity); }
                buf[pos++] = '"';
                memcpy(buf + pos, entry->key->chars, klen);
                pos += klen;
                buf[pos++] = '"'; buf[pos++] = ':'; buf[pos++] = ' ';
                /* value */
                char* vs;
                if (IS_STRING(entry->value)) {
                    ObjString* s = AS_STRING(entry->value);
                    if (pos + s->length + 3 >= capacity) { capacity = (pos + s->length + 3) * 2; buf = (char*)realloc(buf, capacity); }
                    buf[pos++] = '"';
                    memcpy(buf + pos, s->chars, s->length);
                    pos += s->length;
                    buf[pos++] = '"';
                } else {
                    vs = valueToString(entry->value);
                    int vl = (int)strlen(vs);
                    if (pos + vl >= capacity) { capacity = (pos + vl) * 2; buf = (char*)realloc(buf, capacity); }
                    memcpy(buf + pos, vs, vl);
                    pos += vl;
                    reallocate(vs, vl + 1, 0);
                }
                first = false;
            }
            if (pos + 2 >= capacity) { capacity = pos + 2; buf = (char*)realloc(buf, capacity); }
            buf[pos++] = '}';
            buf[pos] = '\0';
            return buf;
        }

        case OBJ_FUNCTION: {
            ObjFunction* fn = AS_FUNCTION(value);
            if (fn->name == NULL) {
                snprintf(buffer, sizeof(buffer), "<script>");
            } else {
                snprintf(buffer, sizeof(buffer), "<fn %s>",
                         fn->name->chars);
            }
            break;
        }

        case OBJ_CLOSURE:
            return objectToString(OBJ_VAL(
                AS_CLOSURE(value)->function));

        case OBJ_NATIVE:
            snprintf(buffer, sizeof(buffer), "<native fn %s>",
                     AS_NATIVE(value)->name->chars);
            break;

        case OBJ_STRUCT:
            snprintf(buffer, sizeof(buffer), "<struct %s>",
                     AS_STRUCT(value)->name->chars);
            break;

        case OBJ_INSTANCE:
            snprintf(buffer, sizeof(buffer), "<instance %s>",
                     AS_INSTANCE(value)->klass->name->chars);
            break;

        default:
            snprintf(buffer, sizeof(buffer), "<object>");
            break;
    }

    size_t len = strlen(buffer);
    char*  str = (char*)reallocate(NULL, 0, len + 1);
    memcpy(str, buffer, len + 1);
    return str;
}

/* ── Type name for error messages ────────────────────────── */

const char* objectTypeName(Obj* obj) {
    switch (obj->type) {
        case OBJ_STRING:       return "str";
        case OBJ_LIST:         return "list";
        case OBJ_MAP:          return "map";
        case OBJ_FUNCTION:     return "fn";
        case OBJ_CLOSURE:      return "fn";
        case OBJ_UPVALUE:      return "upvalue";
        case OBJ_NATIVE:       return "native fn";
        case OBJ_STRUCT:       return "struct";
        case OBJ_INSTANCE:     return "instance";
        case OBJ_BOUND_METHOD: return "method";
        case OBJ_ITERATOR:     return "iterator";
        default:               return "object";
    }
}
