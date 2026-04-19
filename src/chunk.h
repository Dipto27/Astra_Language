/*
 * Astra Programming Language
 * chunk.h — Bytecode representation
 */

#ifndef astra_chunk_h
#define astra_chunk_h

#include "common.h"
#include "value.h"

/* ── Opcodes ─────────────────────────────────────────────── */

typedef enum {
    /* Constants & literals */
    OP_CONSTANT,        /* Push a constant value               */
    OP_NONE,            /* Push none                           */
    OP_TRUE,            /* Push true                           */
    OP_FALSE,           /* Push false                          */

    /* Stack manipulation */
    OP_POP,             /* Discard top of stack                */
    OP_DUP,             /* Duplicate top of stack              */

    /* Arithmetic */
    OP_ADD,             /* a + b                               */
    OP_SUBTRACT,        /* a - b                               */
    OP_MULTIPLY,        /* a * b                               */
    OP_DIVIDE,          /* a / b                               */
    OP_MODULO,          /* a % b                               */
    OP_POWER,           /* a ** b                              */
    OP_NEGATE,          /* -a                                  */

    /* Comparison */
    OP_EQUAL,           /* a == b                              */
    OP_NOT_EQUAL,       /* a != b                              */
    OP_GREATER,         /* a > b                               */
    OP_LESS,            /* a < b                               */
    OP_GREATER_EQUAL,   /* a >= b                              */
    OP_LESS_EQUAL,      /* a <= b                              */

    /* Logical */
    OP_NOT,             /* not a                               */

    /* String */
    OP_CONCAT,          /* String concatenation                */

    /* Variables */
    OP_GET_LOCAL,       /* Read local variable                 */
    OP_SET_LOCAL,       /* Write local variable                */
    OP_GET_GLOBAL,      /* Read global variable                */
    OP_DEFINE_GLOBAL,   /* Define global variable              */
    OP_SET_GLOBAL,      /* Write global variable               */
    OP_GET_UPVALUE,     /* Read captured variable              */
    OP_SET_UPVALUE,     /* Write captured variable             */
    OP_CLOSE_UPVALUE,   /* Close over upvalue                  */

    /* Control flow */
    OP_JUMP,            /* Unconditional jump                  */
    OP_JUMP_IF_FALSE,   /* Jump if top is falsy                */
    OP_LOOP,            /* Jump backwards (loop)               */

    /* Functions & calls */
    OP_CALL,            /* Call function/method                */
    OP_CLOSURE,         /* Create closure from function        */
    OP_RETURN,          /* Return from function                */

    /* Data structures */
    OP_BUILD_LIST,      /* Build list from N stack values      */
    OP_BUILD_MAP,       /* Build map from N key-value pairs    */
    OP_GET_INDEX,       /* a[b]                                */
    OP_SET_INDEX,       /* a[b] = c                            */
    OP_GET_SLICE,       /* a[b..c]                             */

    /* Structs & instances */
    OP_STRUCT,          /* Define a struct                     */
    OP_GET_PROPERTY,    /* instance.field                      */
    OP_SET_PROPERTY,    /* instance.field = val                */
    OP_METHOD,          /* Define a method on struct           */
    OP_INVOKE,          /* Optimized method call               */

    /* Iterators */
    OP_ITERATOR,        /* Get iterator from iterable          */
    OP_ITERATOR_NEXT,   /* Get next value from iterator        */

    /* Pipe operator */
    OP_PIPE,            /* Pipe value into function            */

    /* Import */
    OP_IMPORT,          /* Import a module                     */

    /* Misc */
    OP_PRINT,           /* Print (temporary, for bootstrapping)*/
} OpCode;

/* ── Bytecode chunk ──────────────────────────────────────── */

typedef struct {
    int        count;
    int        capacity;
    uint8_t*   code;
    int*       lines;       /* Source line for each byte        */
    ValueArray constants;   /* Constant pool                    */
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int  addConstant(Chunk* chunk, Value value);

#endif /* astra_chunk_h */
