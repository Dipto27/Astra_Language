/*
 * Astra Programming Language
 * vm.h — Virtual machine
 */

#ifndef astra_vm_h
#define astra_vm_h

#include "common.h"
#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

/* ── Call frame ──────────────────────────────────────────── */

typedef struct {
    ObjClosure* closure;
    uint8_t*    ip;
    Value*      slots;
} CallFrame;

/* ── VM state ────────────────────────────────────────────── */

typedef struct {
    CallFrame   frames[FRAMES_MAX];
    int         frameCount;

    Value       stack[STACK_MAX];
    Value*      stackTop;

    Table       globals;
    Table       strings;
    ObjString*  initString;
    ObjUpvalue* openUpvalues;

    Obj*        objects;

    /* GC state */
    int         grayCount;
    int         grayCapacity;
    Obj**       grayStack;
    size_t      bytesAllocated;
    size_t      nextGC;
} VM;

/* ── Interpretation results ──────────────────────────────── */

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

/* ── Global VM instance ──────────────────────────────────── */

extern VM vm;

/* ── Public API ──────────────────────────────────────────── */

void            initVM(void);
void            freeVM(void);
InterpretResult interpret(const char* source);
void            push(Value value);
Value           pop(void);
void            defineNative(const char* name, NativeFn function,
                             int arity);

#endif /* astra_vm_h */
