/*
 * Astra Programming Language
 * vm.c — Stack-based virtual machine
 *
 * This is the execution engine. It fetches bytecode instructions
 * and executes them against a value stack.
 */

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

/* Forward declaration for stdlib registration */
void registerStdlib(void);

/* ── Global VM instance ──────────────────────────────────── */

VM vm;

/* ── Stack operations ────────────────────────────────────── */

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop(void) {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

/* ── Truthiness ──────────────────────────────────────────── */

static bool isFalsy(Value value) {
    return IS_NONE(value) ||
           (IS_BOOL(value) && !AS_BOOL(value)) ||
           (IS_INT(value) && AS_INT(value) == 0) ||
           (IS_FLOAT(value) && AS_FLOAT(value) == 0.0) ||
           (IS_STRING(value) && AS_STRING(value)->length == 0) ||
           (IS_LIST(value) && AS_LIST(value)->items.count == 0);
}

/* ── Runtime errors ──────────────────────────────────────── */

static void resetStack(void) {
    vm.stackTop    = vm.stack;
    vm.frameCount  = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, "\n  \033[1;31m✖ Runtime Error\033[0m ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    va_end(args);

    /* Print stack trace */
    fprintf(stderr, "\n  \033[90mStack trace (most recent call first):\033[0m\n");
    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame*   frame    = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        size_t       offset   = frame->ip - function->chunk.code - 1;
        int          line     = function->chunk.lines[offset];

        fprintf(stderr, "    \033[90m│\033[0m ");
        fprintf(stderr, "[line %d] in ", line);
        if (function->name == NULL) {
            fprintf(stderr, "<script>\n");
        } else {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }
    fprintf(stderr, "\n");

    resetStack();
}

/* ── Native function helper ──────────────────────────────── */

void defineNative(const char* name, NativeFn function, int arity) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function, AS_STRING(vm.stack[0]), arity)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
}

/* ── Calling convention ──────────────────────────────────── */

static bool callClosure(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.",
                     closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure   = closure;
    frame->ip        = closure->function->chunk.code;
    frame->slots     = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return callClosure(AS_CLOSURE(callee), argCount);

            case OBJ_NATIVE: {
                ObjNative* native = AS_NATIVE(callee);
                if (native->arity != -1 && argCount != native->arity) {
                    runtimeError("Expected %d arguments but got %d.",
                                 native->arity, argCount);
                    return false;
                }
                Value result = native->function(argCount,
                                                vm.stackTop - argCount);
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }

            case OBJ_STRUCT: {
                /* Calling a struct creates an instance */
                ObjStruct* klass = AS_STRUCT(callee);
                vm.stackTop[-argCount - 1] =
                    OBJ_VAL(newInstance(klass));

                /* Call init method if it exists */
                Value initializer;
                if (tableGet(&klass->methods, vm.initString,
                             &initializer)) {
                    return callClosure(AS_CLOSURE(initializer),
                                       argCount);
                } else if (argCount != 0) {
                    runtimeError("Expected 0 arguments but got %d.",
                                 argCount);
                    return false;
                }
                return true;
            }

            case OBJ_BOUND_METHOD: {
                ObjBoundMethod* bound = AS_BOUND_METHOD(callee);
                vm.stackTop[-argCount - 1] = bound->receiver;
                return callClosure(bound->method, argCount);
            }

            default:
                break;
        }
    }

    runtimeError("Can only call functions and structs.");
    return false;
}

/* ── Method invocation ───────────────────────────────────── */

static bool invokeFromStruct(ObjStruct* klass, ObjString* name,
                              int argCount) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined method '%s'.", name->chars);
        return false;
    }
    return callClosure(AS_CLOSURE(method), argCount);
}

static bool invoke(ObjString* name, int argCount) {
    Value receiver = peek(argCount);

    /* Check for built-in methods on primitive types */
    if (IS_LIST(receiver)) {
        ObjList* list = AS_LIST(receiver);

        /* list.push(value) */
        if (name->length == 4 && memcmp(name->chars, "push", 4) == 0) {
            if (argCount != 1) {
                runtimeError("push() takes exactly 1 argument.");
                return false;
            }
            Value val = peek(0);
            writeValueArray(&list->items, val);
            pop(); /* value */
            pop(); /* list */
            push(NONE_VAL);
            return true;
        }

        /* list.pop() */
        if (name->length == 3 && memcmp(name->chars, "pop", 3) == 0) {
            if (list->items.count == 0) {
                runtimeError("Cannot pop from empty list.");
                return false;
            }
            Value val = list->items.values[--list->items.count];
            pop(); /* list */
            push(val);
            return true;
        }

        /* list.len() */
        if (name->length == 3 && memcmp(name->chars, "len", 3) == 0) {
            pop(); /* list */
            push(INT_VAL(list->items.count));
            return true;
        }

        /* list.reverse() */
        if (name->length == 7 && memcmp(name->chars, "reverse", 7) == 0) {
            for (int i = 0, j = list->items.count - 1; i < j; i++, j--) {
                Value temp = list->items.values[i];
                list->items.values[i] = list->items.values[j];
                list->items.values[j] = temp;
            }
            /* return the list itself */
            return true;
        }

        /* list.contains(value) */
        if (name->length == 8 && memcmp(name->chars, "contains", 8) == 0) {
            if (argCount != 1) {
                runtimeError("contains() takes exactly 1 argument.");
                return false;
            }
            Value needle = peek(0);
            bool found = false;
            for (int i = 0; i < list->items.count; i++) {
                if (valuesEqual(list->items.values[i], needle)) {
                    found = true;
                    break;
                }
            }
            pop(); /* value */
            pop(); /* list */
            push(BOOL_VAL(found));
            return true;
        }
    }

    if (IS_STRING(receiver)) {
        ObjString* str = AS_STRING(receiver);

        /* str.len() */
        if (name->length == 3 && memcmp(name->chars, "len", 3) == 0) {
            pop(); /* string */
            push(INT_VAL(str->length));
            return true;
        }

        /* str.upper() */
        if (name->length == 5 && memcmp(name->chars, "upper", 5) == 0) {
            char* buf = ALLOCATE(char, str->length + 1);
            for (int i = 0; i < str->length; i++) {
                buf[i] = (str->chars[i] >= 'a' && str->chars[i] <= 'z')
                         ? str->chars[i] - 32 : str->chars[i];
            }
            buf[str->length] = '\0';
            pop(); /* string */
            push(OBJ_VAL(takeString(buf, str->length)));
            return true;
        }

        /* str.lower() */
        if (name->length == 5 && memcmp(name->chars, "lower", 5) == 0) {
            char* buf = ALLOCATE(char, str->length + 1);
            for (int i = 0; i < str->length; i++) {
                buf[i] = (str->chars[i] >= 'A' && str->chars[i] <= 'Z')
                         ? str->chars[i] + 32 : str->chars[i];
            }
            buf[str->length] = '\0';
            pop(); /* string */
            push(OBJ_VAL(takeString(buf, str->length)));
            return true;
        }

        /* str.contains(substr) */
        if (name->length == 8 && memcmp(name->chars, "contains", 8) == 0) {
            if (argCount != 1) {
                runtimeError("contains() takes exactly 1 argument.");
                return false;
            }
            if (!IS_STRING(peek(0))) {
                runtimeError("contains() argument must be a string.");
                return false;
            }
            ObjString* needle = AS_STRING(peek(0));
            bool found = (strstr(str->chars, needle->chars) != NULL);
            pop(); /* needle */
            pop(); /* string */
            push(BOOL_VAL(found));
            return true;
        }

        /* str.split(delim) */
        if (name->length == 5 && memcmp(name->chars, "split", 5) == 0) {
            if (argCount != 1 || !IS_STRING(peek(0))) {
                runtimeError("split() takes exactly 1 string argument.");
                return false;
            }
            ObjString* delim = AS_STRING(peek(0));
            pop(); /* delim */
            pop(); /* string */

            ObjList* result = newList();
            push(OBJ_VAL(result)); /* protect from GC */

            char* copy = ALLOCATE(char, str->length + 1);
            memcpy(copy, str->chars, str->length + 1);

            if (delim->length == 0) {
                /* Split into characters */
                for (int i = 0; i < str->length; i++) {
                    writeValueArray(&result->items,
                        OBJ_VAL(copyString(&str->chars[i], 1)));
                }
            } else {
                char* token = copy;
                char* found_pos;
                while ((found_pos = strstr(token, delim->chars)) != NULL) {
                    int len = (int)(found_pos - token);
                    writeValueArray(&result->items,
                        OBJ_VAL(copyString(token, len)));
                    token = found_pos + delim->length;
                }
                writeValueArray(&result->items,
                    OBJ_VAL(copyString(token, (int)strlen(token))));
            }

            FREE_ARRAY(char, copy, str->length + 1);
            return true;
        }
    }

    /* For struct instances, look up methods */
    if (!IS_INSTANCE(receiver)) {
        runtimeError("Only instances have methods.");
        return false;
    }

    ObjInstance* instance = AS_INSTANCE(receiver);

    /* Check for field first */
    Value value;
    if (tableGet(&instance->fields, name, &value)) {
        vm.stackTop[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    return invokeFromStruct(instance->klass, name, argCount);
}

/* ── Upvalue management ──────────────────────────────────── */

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue     = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue     = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed     = *upvalue->location;
        upvalue->location   = &upvalue->closed;
        vm.openUpvalues     = upvalue->next;
    }
}

/* ── Define a method on a struct ─────────────────────────── */

static void defineMethod(ObjString* name) {
    Value   method = peek(0);
    ObjStruct* klass = AS_STRUCT(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

/* ── Bind a method to an instance ────────────────────────── */

static bool bindMethod(ObjStruct* klass, ObjString* name) {
    Value method;
    if (!tableGet(&klass->methods, name, &method)) {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod* bound = newBoundMethod(peek(0),
                                            AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  MAIN EXECUTION LOOP
 * ═══════════════════════════════════════════════════════════ */

static InterpretResult run(void) {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE()     (*frame->ip++)
#define READ_SHORT()    \
    (frame->ip += 2, \
     (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING()   AS_STRING(READ_CONSTANT())

    for (;;) {

#ifdef DEBUG_TRACE_EXECUTION
        /* Print stack */
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(
            &frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE()) {

            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }

            case OP_NONE:  push(NONE_VAL);      break;
            case OP_TRUE:  push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;

            case OP_POP: pop(); break;

            case OP_DUP: push(peek(0)); break;

            /* ── Arithmetic ──────────────────────────────── */

            case OP_ADD: {
                Value b = peek(0);
                Value a = peek(1);

                if (IS_STRING(a) && IS_STRING(b)) {
                    ObjString* sb = AS_STRING(b);
                    ObjString* sa = AS_STRING(a);
                    ObjString* result = concatenateStrings(sa, sb);
                    pop(); pop();
                    push(OBJ_VAL(result));
                } else if (IS_INT(a) && IS_INT(b)) {
                    int64_t result = AS_INT(a) + AS_INT(b);
                    pop(); pop();
                    push(INT_VAL(result));
                } else if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    double result = AS_NUMBER(a) + AS_NUMBER(b);
                    pop(); pop();
                    push(FLOAT_VAL(result));
                } else if (IS_LIST(a) && IS_LIST(b)) {
                    /* List concatenation */
                    ObjList* la = AS_LIST(a);
                    ObjList* lb = AS_LIST(b);
                    ObjList* result = newList();
                    for (int i = 0; i < la->items.count; i++) {
                        writeValueArray(&result->items,
                                        la->items.values[i]);
                    }
                    for (int i = 0; i < lb->items.count; i++) {
                        writeValueArray(&result->items,
                                        lb->items.values[i]);
                    }
                    pop(); pop();
                    push(OBJ_VAL(result));
                } else {
                    runtimeError(
                        "Operands must be numbers, strings, or lists.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SUBTRACT: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                if (IS_INT(a) && IS_INT(b))
                    push(INT_VAL(AS_INT(a) - AS_INT(b)));
                else
                    push(FLOAT_VAL(AS_NUMBER(a) - AS_NUMBER(b)));
                break;
            }

            case OP_MULTIPLY: {
                /* int * string = string repetition */
                if (IS_STRING(peek(0)) && IS_INT(peek(1))) {
                    ObjString* str = AS_STRING(pop());
                    int64_t n = AS_INT(pop());
                    if (n <= 0) {
                        push(OBJ_VAL(copyString("", 0)));
                    } else {
                        int len = str->length * (int)n;
                        char* buf = ALLOCATE(char, len + 1);
                        for (int64_t i = 0; i < n; i++) {
                            memcpy(buf + i * str->length,
                                   str->chars, str->length);
                        }
                        buf[len] = '\0';
                        push(OBJ_VAL(takeString(buf, len)));
                    }
                    break;
                }
                if (IS_INT(peek(0)) && IS_STRING(peek(1))) {
                    int64_t n = AS_INT(pop());
                    ObjString* str = AS_STRING(pop());
                    if (n <= 0) {
                        push(OBJ_VAL(copyString("", 0)));
                    } else {
                        int len = str->length * (int)n;
                        char* buf = ALLOCATE(char, len + 1);
                        for (int64_t i = 0; i < n; i++) {
                            memcpy(buf + i * str->length,
                                   str->chars, str->length);
                        }
                        buf[len] = '\0';
                        push(OBJ_VAL(takeString(buf, len)));
                    }
                    break;
                }

                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                if (IS_INT(a) && IS_INT(b))
                    push(INT_VAL(AS_INT(a) * AS_INT(b)));
                else
                    push(FLOAT_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
                break;
            }

            case OP_DIVIDE: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                if (IS_INT(b) && AS_INT(b) == 0) {
                    runtimeError("Division by zero.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (IS_FLOAT(b) && AS_FLOAT(b) == 0.0) {
                    runtimeError("Division by zero.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                /* Always produce float for division */
                push(FLOAT_VAL(AS_NUMBER(a) / AS_NUMBER(b)));
                break;
            }

            case OP_MODULO: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                if (IS_INT(a) && IS_INT(b)) {
                    if (AS_INT(b) == 0) {
                        runtimeError("Modulo by zero.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(INT_VAL(AS_INT(a) % AS_INT(b)));
                } else {
                    push(FLOAT_VAL(fmod(AS_NUMBER(a), AS_NUMBER(b))));
                }
                break;
            }

            case OP_POWER: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                if (IS_INT(a) && IS_INT(b) && AS_INT(b) >= 0) {
                    /* Integer power for non-negative exponents */
                    int64_t base = AS_INT(a);
                    int64_t exp  = AS_INT(b);
                    int64_t result = 1;
                    while (exp > 0) {
                        if (exp % 2 == 1) result *= base;
                        base *= base;
                        exp /= 2;
                    }
                    push(INT_VAL(result));
                } else {
                    push(FLOAT_VAL(pow(AS_NUMBER(a), AS_NUMBER(b))));
                }
                break;
            }

            case OP_NEGATE: {
                if (IS_INT(peek(0))) {
                    push(INT_VAL(-AS_INT(pop())));
                } else if (IS_FLOAT(peek(0))) {
                    push(FLOAT_VAL(-AS_FLOAT(pop())));
                } else {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            /* ── Comparison ──────────────────────────────── */

            case OP_EQUAL: {
                Value b = pop(), a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }

            case OP_NOT_EQUAL: {
                Value b = pop(), a = pop();
                push(BOOL_VAL(!valuesEqual(a, b)));
                break;
            }

            case OP_GREATER: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    /* String comparison */
                    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                        ObjString* b = AS_STRING(pop());
                        ObjString* a = AS_STRING(pop());
                        push(BOOL_VAL(strcmp(a->chars, b->chars) > 0));
                        break;
                    }
                    runtimeError("Operands must be numbers or strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                push(BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));
                break;
            }

            case OP_LESS: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                        ObjString* b = AS_STRING(pop());
                        ObjString* a = AS_STRING(pop());
                        push(BOOL_VAL(strcmp(a->chars, b->chars) < 0));
                        break;
                    }
                    runtimeError("Operands must be numbers or strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                push(BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));
                break;
            }

            case OP_GREATER_EQUAL: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                push(BOOL_VAL(AS_NUMBER(a) >= AS_NUMBER(b)));
                break;
            }

            case OP_LESS_EQUAL: {
                if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                    runtimeError("Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                Value b = pop(), a = pop();
                push(BOOL_VAL(AS_NUMBER(a) <= AS_NUMBER(b)));
                break;
            }

            /* ── Logical ─────────────────────────────────── */

            case OP_NOT: {
                push(BOOL_VAL(isFalsy(pop())));
                break;
            }

            /* ── String concatenation ────────────────────── */

            case OP_CONCAT: {
                if (!IS_STRING(peek(0)) || !IS_STRING(peek(1))) {
                    runtimeError("Operands must be strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                ObjString* b = AS_STRING(peek(0));
                ObjString* a = AS_STRING(peek(1));
                ObjString* result = concatenateStrings(a, b);
                pop(); pop();
                push(OBJ_VAL(result));
                break;
            }

            /* ── Variables ───────────────────────────────── */

            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }

            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!tableGet(&vm.globals, name, &value)) {
                    runtimeError("Undefined variable '%s'.",
                                 name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }

            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                tableSet(&vm.globals, name, peek(0));
                pop();
                break;
            }

            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (tableSet(&vm.globals, name, peek(0))) {
                    tableDelete(&vm.globals, name);
                    runtimeError("Undefined variable '%s'.",
                                 name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }

            case OP_CLOSE_UPVALUE: {
                closeUpvalues(vm.stackTop - 1);
                pop();
                break;
            }

            /* ── Control flow ────────────────────────────── */

            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (isFalsy(peek(0))) frame->ip += offset;
                break;
            }

            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

            /* ── Functions ───────────────────────────────── */

            case OP_CALL: {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

            case OP_CLOSURE: {
                ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure*  closure  = newClosure(function);
                push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalueCount; i++) {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index   = READ_BYTE();
                    if (isLocal) {
                        closure->upvalues[i] =
                            captureUpvalue(frame->slots + index);
                    } else {
                        closure->upvalues[i] =
                            frame->closure->upvalues[index];
                    }
                }
                break;
            }

            case OP_RETURN: {
                Value result = pop();
                closeUpvalues(frame->slots);
                vm.frameCount--;

                if (vm.frameCount == 0) {
                    pop();
                    return INTERPRET_OK;
                }

                vm.stackTop = frame->slots;
                push(result);
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

            /* ── Data structures ─────────────────────────── */

            case OP_BUILD_LIST: {
                int count = READ_BYTE();
                ObjList* list = newList();

                /* We need to push list first to protect from GC,
                   then move values */
                push(OBJ_VAL(list));
                for (int i = count; i > 0; i--) {
                    writeValueArray(&list->items,
                                    vm.stackTop[-1 - i]);
                }

                /* Remove the original values + list from stack */
                vm.stackTop -= count + 1;
                push(OBJ_VAL(list));
                break;
            }

            case OP_BUILD_MAP: {
                int count = READ_BYTE();
                ObjMap* map = newMap();

                push(OBJ_VAL(map));
                for (int i = count; i > 0; i--) {
                    Value value = vm.stackTop[-1 - (i * 2 - 1)];
                    Value key   = vm.stackTop[-1 - (i * 2)];

                    if (!IS_STRING(key)) {
                        runtimeError("Map keys must be strings.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    tableSet(&map->entries, AS_STRING(key), value);
                }

                vm.stackTop -= count * 2 + 1;
                push(OBJ_VAL(map));
                break;
            }

            case OP_GET_INDEX: {
                Value index = pop();
                Value obj   = pop();

                if (IS_LIST(obj)) {
                    if (!IS_INT(index)) {
                        runtimeError("List index must be an integer.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjList* list = AS_LIST(obj);
                    int64_t  i    = AS_INT(index);

                    /* Negative indexing */
                    if (i < 0) i += list->items.count;

                    if (i < 0 || i >= list->items.count) {
                        runtimeError("Index %lld out of bounds "
                                     "(list size: %d).",
                                     (long long)AS_INT(index),
                                     list->items.count);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(list->items.values[i]);

                } else if (IS_STRING(obj)) {
                    if (!IS_INT(index)) {
                        runtimeError("String index must be an integer.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjString* str = AS_STRING(obj);
                    int64_t    i   = AS_INT(index);

                    if (i < 0) i += str->length;
                    if (i < 0 || i >= str->length) {
                        runtimeError("Index out of bounds.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    push(OBJ_VAL(copyString(&str->chars[i], 1)));

                } else if (IS_MAP(obj)) {
                    if (!IS_STRING(index)) {
                        runtimeError("Map key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjMap* map = AS_MAP(obj);
                    Value   value;
                    if (tableGet(&map->entries, AS_STRING(index),
                                 &value)) {
                        push(value);
                    } else {
                        push(NONE_VAL);
                    }
                } else {
                    runtimeError("Only lists, strings, and maps "
                                 "can be indexed.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SET_INDEX: {
                Value value = pop();
                Value index = pop();
                Value obj   = pop();

                if (IS_LIST(obj)) {
                    if (!IS_INT(index)) {
                        runtimeError("List index must be an integer.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjList* list = AS_LIST(obj);
                    int64_t  i    = AS_INT(index);
                    if (i < 0) i += list->items.count;
                    if (i < 0 || i >= list->items.count) {
                        runtimeError("Index out of bounds.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    list->items.values[i] = value;
                    push(value);

                } else if (IS_MAP(obj)) {
                    if (!IS_STRING(index)) {
                        runtimeError("Map key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    ObjMap* map = AS_MAP(obj);
                    tableSet(&map->entries, AS_STRING(index), value);
                    push(value);
                } else {
                    runtimeError("Only lists and maps can be "
                                 "assigned by index.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_SLICE: {
                Value end = pop();
                Value start = pop();
                Value obj   = pop();

                if (!IS_LIST(obj) && !IS_STRING(obj)) {
                    runtimeError("Only lists and strings can be sliced.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (!IS_INT(start) || !IS_INT(end)) {
                    runtimeError("Slice indices must be integers.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                int64_t s = AS_INT(start);
                int64_t e = AS_INT(end);

                if (IS_LIST(obj)) {
                    ObjList* list = AS_LIST(obj);
                    if (s < 0) s += list->items.count;
                    if (e < 0) e += list->items.count;
                    if (s < 0) s = 0;
                    if (e > list->items.count) e = list->items.count;

                    ObjList* result = newList();
                    push(OBJ_VAL(result));
                    for (int64_t i = s; i < e; i++) {
                        writeValueArray(&result->items,
                                        list->items.values[i]);
                    }
                } else {
                    ObjString* str = AS_STRING(obj);
                    if (s < 0) s += str->length;
                    if (e < 0) e += str->length;
                    if (s < 0) s = 0;
                    if (e > str->length) e = str->length;
                    if (s >= e) {
                        push(OBJ_VAL(copyString("", 0)));
                    } else {
                        push(OBJ_VAL(copyString(
                            str->chars + s, (int)(e - s))));
                    }
                }
                break;
            }

            /* ── Structs & instances ─────────────────────── */

            case OP_STRUCT: {
                ObjString* name = READ_STRING();
                push(OBJ_VAL(newStruct(name)));
                break;
            }

            case OP_GET_PROPERTY: {
                if (IS_INSTANCE(peek(0))) {
                    ObjInstance* instance = AS_INSTANCE(peek(0));
                    ObjString*  name     = READ_STRING();

                    Value value;
                    if (tableGet(&instance->fields, name, &value)) {
                        pop(); /* instance */
                        push(value);
                        break;
                    }

                    if (!bindMethod(instance->klass, name)) {
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    break;
                } else if (IS_MAP(peek(0))) {
                    /* Allow map.key syntax */
                    ObjMap*    map  = AS_MAP(peek(0));
                    ObjString* name = READ_STRING();
                    Value value;
                    if (tableGet(&map->entries, name, &value)) {
                        pop();
                        push(value);
                    } else {
                        pop();
                        push(NONE_VAL);
                    }
                    break;
                }

                runtimeError("Only instances and maps have properties.");
                return INTERPRET_RUNTIME_ERROR;
            }

            case OP_SET_PROPERTY: {
                if (!IS_INSTANCE(peek(1)) && !IS_MAP(peek(1))) {
                    runtimeError("Only instances and maps have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjString* name = READ_STRING();

                if (IS_INSTANCE(peek(1))) {
                    ObjInstance* instance = AS_INSTANCE(peek(1));
                    tableSet(&instance->fields, name, peek(0));
                } else {
                    ObjMap* map = AS_MAP(peek(1));
                    tableSet(&map->entries, name, peek(0));
                }

                Value value = pop();
                pop(); /* instance/map */
                push(value);
                break;
            }

            case OP_METHOD: {
                ObjString* name = READ_STRING();
                defineMethod(name);
                break;
            }

            case OP_INVOKE: {
                ObjString* method   = READ_STRING();
                int        argCount = READ_BYTE();
                if (!invoke(method, argCount)) {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames[vm.frameCount - 1];
                break;
            }

            /* ── Iterators ───────────────────────────────── */

            case OP_ITERATOR: {
                Value iterable = pop();
                if (IS_LIST(iterable) || IS_STRING(iterable)) {
                    push(OBJ_VAL(newIterator(iterable)));
                } else {
                    runtimeError("Value is not iterable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_ITERATOR_NEXT: {
                uint16_t offset = READ_SHORT();
                Value iterVal = pop();

                if (!IS_OBJ(iterVal) ||
                    AS_OBJ(iterVal)->type != OBJ_ITERATOR) {
                    runtimeError("Expected iterator.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjIterator* iter = (ObjIterator*)AS_OBJ(iterVal);

                if (IS_LIST(iter->iterable)) {
                    ObjList* list = AS_LIST(iter->iterable);
                    if (iter->index >= list->items.count) {
                        /* Done iterating — jump to exit */
                        frame->ip += offset;
                    } else {
                        push(list->items.values[iter->index++]);
                    }
                } else if (IS_STRING(iter->iterable)) {
                    ObjString* str = AS_STRING(iter->iterable);
                    if (iter->index >= str->length) {
                        frame->ip += offset;
                    } else {
                        push(OBJ_VAL(copyString(
                            &str->chars[iter->index++], 1)));
                    }
                } else {
                    runtimeError("Cannot iterate over this type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            /* ── Pipe ────────────────────────────────────── */

            case OP_PIPE: {
                /* Already handled by the compiler as a call */
                break;
            }

            /* ── Import ──────────────────────────────────── */

            case OP_IMPORT: {
                ObjString* name = READ_STRING();
                /* TODO: implement module loading */
                runtimeError("Module '%s' not found. "
                             "Module system coming soon!",
                             name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }

            /* ── Print ───────────────────────────────────── */

            case OP_PRINT: {
                printValue(pop());
                printf("\n");
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
}

/* ═══════════════════════════════════════════════════════════
 *  VM LIFECYCLE
 * ═══════════════════════════════════════════════════════════ */

void initVM(void) {
    resetStack();
    vm.objects        = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC         = 1024 * 1024;
    vm.grayCount      = 0;
    vm.grayCapacity   = 0;
    vm.grayStack      = NULL;

    initTable(&vm.globals);
    initTable(&vm.strings);

    vm.initString = NULL;
    vm.initString = copyString("init", 4);

    /* Register the standard library */
    registerStdlib();
}

void freeVM(void) {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    vm.initString = NULL;
    freeObjects();
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callClosure(closure, 0);

    return run();
}
