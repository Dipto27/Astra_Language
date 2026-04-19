/*
 * Astra Programming Language
 * stdlib/core.c — Built-in functions (print, input, type, len, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "../common.h"
#include "../value.h"
#include "../object.h"
#include "../memory.h"
#include "../vm.h"

/* ══════════════════════════════════════════════════════════
 *  CORE BUILT-IN FUNCTIONS
 * ══════════════════════════════════════════════════════════ */

/* ── println(value) — print with newline ─────────────────── */
static Value nativePrintln(int argCount, Value* args) {
    for (int i = 0; i < argCount; i++) {
        if (i > 0) printf(" ");
        printValue(args[i]);
    }
    printf("\n");
    return NONE_VAL;
}

/* ── print(value) — print without newline ────────────────── */
static Value nativePrintNoNl(int argCount, Value* args) {
    for (int i = 0; i < argCount; i++) {
        if (i > 0) printf(" ");
        printValue(args[i]);
    }
    fflush(stdout);
    return NONE_VAL;
}

/* ── input(prompt?) — read line from stdin ───────────────── */
static Value nativeInput(int argCount, Value* args) {
    if (argCount > 0 && IS_STRING(args[0])) {
        printf("%s", AS_CSTRING(args[0]));
        fflush(stdout);
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return OBJ_VAL(copyString("", 0));
    }

    /* Remove trailing newline */
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[--len] = '\0';
    }

    return OBJ_VAL(copyString(buffer, (int)len));
}

/* ── type(value) — get type name as string ───────────────── */
static Value nativeType(int argCount, Value* args) {
    UNUSED(argCount);
    const char* name = valueTypeName(args[0]);
    return OBJ_VAL(copyString(name, (int)strlen(name)));
}

/* ── len(value) — get length ─────────────────────────────── */
static Value nativeLen(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_STRING(args[0])) {
        return INT_VAL(AS_STRING(args[0])->length);
    }
    if (IS_LIST(args[0])) {
        return INT_VAL(AS_LIST(args[0])->items.count);
    }
    if (IS_MAP(args[0])) {
        return INT_VAL(AS_MAP(args[0])->entries.count);
    }
    return INT_VAL(0);
}

/* ── str(value) — convert to string ──────────────────────── */
static Value nativeStr(int argCount, Value* args) {
    UNUSED(argCount);
    char* s = valueToString(args[0]);
    ObjString* result = copyString(s, (int)strlen(s));
    reallocate(s, strlen(s) + 1, 0);
    return OBJ_VAL(result);
}

/* ── int(value) — convert to integer ─────────────────────── */
static Value nativeInt(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_INT(args[0])) return args[0];
    if (IS_FLOAT(args[0])) return INT_VAL((int64_t)AS_FLOAT(args[0]));
    if (IS_BOOL(args[0])) return INT_VAL(AS_BOOL(args[0]) ? 1 : 0);
    if (IS_STRING(args[0])) {
        char* end;
        int64_t val = strtoll(AS_CSTRING(args[0]), &end, 10);
        if (*end == '\0' || *end == '\n') return INT_VAL(val);
    }
    return NONE_VAL;
}

/* ── float(value) — convert to float ─────────────────────── */
static Value nativeFloat(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_FLOAT(args[0])) return args[0];
    if (IS_INT(args[0])) return FLOAT_VAL((double)AS_INT(args[0]));
    if (IS_BOOL(args[0])) return FLOAT_VAL(AS_BOOL(args[0]) ? 1.0 : 0.0);
    if (IS_STRING(args[0])) {
        char*  end;
        double val = strtod(AS_CSTRING(args[0]), &end);
        if (*end == '\0' || *end == '\n') return FLOAT_VAL(val);
    }
    return NONE_VAL;
}

/* ── bool(value) — convert to boolean ────────────────────── */
static Value nativeBool(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_BOOL(args[0])) return args[0];
    if (IS_NONE(args[0])) return BOOL_VAL(false);
    if (IS_INT(args[0])) return BOOL_VAL(AS_INT(args[0]) != 0);
    if (IS_FLOAT(args[0])) return BOOL_VAL(AS_FLOAT(args[0]) != 0.0);
    if (IS_STRING(args[0])) return BOOL_VAL(AS_STRING(args[0])->length > 0);
    if (IS_LIST(args[0])) return BOOL_VAL(AS_LIST(args[0])->items.count > 0);
    return BOOL_VAL(true);
}

/* ── range(start, end?, step?) — create a list of numbers ── */
static Value nativeRange(int argCount, Value* args) {
    int64_t start, end, step;

    if (argCount == 1) {
        if (!IS_INT(args[0])) return NONE_VAL;
        start = 0;
        end   = AS_INT(args[0]);
        step  = 1;
    } else if (argCount == 2) {
        if (!IS_INT(args[0]) || !IS_INT(args[1])) return NONE_VAL;
        start = AS_INT(args[0]);
        end   = AS_INT(args[1]);
        step  = start <= end ? 1 : -1;
    } else if (argCount >= 3) {
        if (!IS_INT(args[0]) || !IS_INT(args[1]) || !IS_INT(args[2]))
            return NONE_VAL;
        start = AS_INT(args[0]);
        end   = AS_INT(args[1]);
        step  = AS_INT(args[2]);
        if (step == 0) return NONE_VAL;
    } else {
        return NONE_VAL;
    }

    ObjList* list = newList();
    push(OBJ_VAL(list)); /* protect from GC */

    if (step > 0) {
        for (int64_t i = start; i < end; i += step) {
            writeValueArray(&list->items, INT_VAL(i));
        }
    } else {
        for (int64_t i = start; i > end; i += step) {
            writeValueArray(&list->items, INT_VAL(i));
        }
    }

    pop(); /* remove GC protection */
    return OBJ_VAL(list);
}

/* ── assert(condition, message?) — assertion ─────────────── */
static Value nativeAssert(int argCount, Value* args) {
    bool condition;
    if (IS_BOOL(args[0])) {
        condition = AS_BOOL(args[0]);
    } else {
        condition = !IS_NONE(args[0]);
    }

    if (!condition) {
        if (argCount > 1 && IS_STRING(args[1])) {
            fprintf(stderr, "\n  \033[1;31m✖ Assertion failed:\033[0m %s\n\n",
                    AS_CSTRING(args[1]));
        } else {
            fprintf(stderr, "\n  \033[1;31m✖ Assertion failed\033[0m\n\n");
        }
        /* Continue running but create a runtime error */
        return BOOL_VAL(false);
    }

    return BOOL_VAL(true);
}

/* ── exit(code?) — exit the program ──────────────────────── */
static Value nativeExit(int argCount, Value* args) {
    int code = 0;
    if (argCount > 0 && IS_INT(args[0])) {
        code = (int)AS_INT(args[0]);
    }
    exit(code);
    return NONE_VAL;
}

/* ── time() — current time in seconds ────────────────────── */
static Value nativeTime(int argCount, Value* args) {
    UNUSED(argCount); UNUSED(args);
    return FLOAT_VAL((double)clock() / CLOCKS_PER_SEC);
}

/* ── clock() — wall clock time ───────────────────────────── */
static Value nativeClock(int argCount, Value* args) {
    UNUSED(argCount); UNUSED(args);
    return FLOAT_VAL((double)time(NULL));
}

/* ── sleep(seconds) — pause execution ────────────────────── */
static Value nativeSleep(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_NUMBER(args[0])) {
        double seconds = AS_NUMBER(args[0]);
        struct timespec ts;
        ts.tv_sec  = (time_t)seconds;
        ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    }
    return NONE_VAL;
}

/* ── abs(number) ─────────────────────────────────────────── */
static Value nativeAbs(int argCount, Value* args) {
    UNUSED(argCount);
    if (IS_INT(args[0])) {
        int64_t v = AS_INT(args[0]);
        return INT_VAL(v < 0 ? -v : v);
    }
    if (IS_FLOAT(args[0])) return FLOAT_VAL(fabs(AS_FLOAT(args[0])));
    return NONE_VAL;
}

/* ── min(a, b) ───────────────────────────────────────────── */
static Value nativeMin(int argCount, Value* args) {
    if (argCount == 1 && IS_LIST(args[0])) {
        ObjList* list = AS_LIST(args[0]);
        if (list->items.count == 0) return NONE_VAL;
        Value min = list->items.values[0];
        for (int i = 1; i < list->items.count; i++) {
            if (IS_NUMBER(list->items.values[i]) && IS_NUMBER(min)) {
                if (AS_NUMBER(list->items.values[i]) < AS_NUMBER(min)) {
                    min = list->items.values[i];
                }
            }
        }
        return min;
    }
    if (argCount < 2) return NONE_VAL;
    if (IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
        return AS_NUMBER(args[0]) < AS_NUMBER(args[1]) ? args[0] : args[1];
    }
    return NONE_VAL;
}

/* ── max(a, b) ───────────────────────────────────────────── */
static Value nativeMax(int argCount, Value* args) {
    if (argCount == 1 && IS_LIST(args[0])) {
        ObjList* list = AS_LIST(args[0]);
        if (list->items.count == 0) return NONE_VAL;
        Value max = list->items.values[0];
        for (int i = 1; i < list->items.count; i++) {
            if (IS_NUMBER(list->items.values[i]) && IS_NUMBER(max)) {
                if (AS_NUMBER(list->items.values[i]) > AS_NUMBER(max)) {
                    max = list->items.values[i];
                }
            }
        }
        return max;
    }
    if (argCount < 2) return NONE_VAL;
    if (IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
        return AS_NUMBER(args[0]) > AS_NUMBER(args[1]) ? args[0] : args[1];
    }
    return NONE_VAL;
}

/* ── sum(list) ───────────────────────────────────────────── */
static Value nativeSum(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_LIST(args[0])) return INT_VAL(0);
    ObjList* list = AS_LIST(args[0]);
    bool hasFloat = false;
    double total = 0.0;
    for (int i = 0; i < list->items.count; i++) {
        if (IS_INT(list->items.values[i])) {
            total += (double)AS_INT(list->items.values[i]);
        } else if (IS_FLOAT(list->items.values[i])) {
            total += AS_FLOAT(list->items.values[i]);
            hasFloat = true;
        }
    }
    if (hasFloat) return FLOAT_VAL(total);
    return INT_VAL((int64_t)total);
}

/* ── Math functions ──────────────────────────────────────── */
static Value nativeSqrt(int argCount, Value* args)  { UNUSED(argCount); return FLOAT_VAL(sqrt(AS_NUMBER(args[0]))); }
static Value nativeFloor(int argCount, Value* args) { UNUSED(argCount); return INT_VAL((int64_t)floor(AS_NUMBER(args[0]))); }
static Value nativeCeil(int argCount, Value* args)  { UNUSED(argCount); return INT_VAL((int64_t)ceil(AS_NUMBER(args[0]))); }
static Value nativeRound(int argCount, Value* args) { UNUSED(argCount); return INT_VAL((int64_t)round(AS_NUMBER(args[0]))); }
static Value nativeLog(int argCount, Value* args)   { UNUSED(argCount); return FLOAT_VAL(log(AS_NUMBER(args[0]))); }
static Value nativeSin(int argCount, Value* args)   { UNUSED(argCount); return FLOAT_VAL(sin(AS_NUMBER(args[0]))); }
static Value nativeCos(int argCount, Value* args)   { UNUSED(argCount); return FLOAT_VAL(cos(AS_NUMBER(args[0]))); }
static Value nativeTan(int argCount, Value* args)   { UNUSED(argCount); return FLOAT_VAL(tan(AS_NUMBER(args[0]))); }

/* ── random() — random float between 0 and 1 ────────────── */
static Value nativeRandom(int argCount, Value* args) {
    UNUSED(argCount); UNUSED(args);
    return FLOAT_VAL((double)rand() / (double)RAND_MAX);
}

/* NOTE: map/filter implemented as Astra-level functions
   since native functions cannot call back into the VM */

/* ── sorted(list) — return a sorted copy ─────────────────── */
static void quickSort(Value* arr, int low, int high) {
    if (low >= high) return;

    Value pivot = arr[high];
    int i = low - 1;

    for (int j = low; j < high; j++) {
        bool less = false;
        if (IS_NUMBER(arr[j]) && IS_NUMBER(pivot)) {
            less = AS_NUMBER(arr[j]) < AS_NUMBER(pivot);
        } else if (IS_STRING(arr[j]) && IS_STRING(pivot)) {
            less = strcmp(AS_CSTRING(arr[j]),
                         AS_CSTRING(pivot)) < 0;
        }

        if (less) {
            i++;
            Value tmp = arr[i]; arr[i] = arr[j]; arr[j] = tmp;
        }
    }

    Value tmp = arr[i + 1]; arr[i + 1] = arr[high]; arr[high] = tmp;
    quickSort(arr, low, i);
    quickSort(arr, i + 2, high);
}

static Value nativeSorted(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_LIST(args[0])) return NONE_VAL;
    ObjList* src = AS_LIST(args[0]);

    ObjList* result = newList();
    push(OBJ_VAL(result));

    for (int i = 0; i < src->items.count; i++) {
        writeValueArray(&result->items, src->items.values[i]);
    }

    if (result->items.count > 1) {
        quickSort(result->items.values, 0, result->items.count - 1);
    }

    pop();
    return OBJ_VAL(result);
}

/* ── reversed(list) — return reversed copy ───────────────── */
static Value nativeReversed(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_LIST(args[0])) return NONE_VAL;
    ObjList* src = AS_LIST(args[0]);

    ObjList* result = newList();
    push(OBJ_VAL(result));

    for (int i = src->items.count - 1; i >= 0; i--) {
        writeValueArray(&result->items, src->items.values[i]);
    }

    pop();
    return OBJ_VAL(result);
}

/* ── enumerate(list) — returns list of [index, value] ────── */
static Value nativeEnumerate(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_LIST(args[0])) return NONE_VAL;
    ObjList* src = AS_LIST(args[0]);

    ObjList* result = newList();
    push(OBJ_VAL(result));

    for (int i = 0; i < src->items.count; i++) {
        ObjList* pair = newList();
        push(OBJ_VAL(pair));
        writeValueArray(&pair->items, INT_VAL(i));
        writeValueArray(&pair->items, src->items.values[i]);
        pop();
        writeValueArray(&result->items, OBJ_VAL(pair));
    }

    pop();
    return OBJ_VAL(result);
}

/* ── zip(list1, list2) — zip two lists ───────────────────── */
static Value nativeZip(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_LIST(args[0]) || !IS_LIST(args[1])) return NONE_VAL;
    ObjList* a = AS_LIST(args[0]);
    ObjList* b = AS_LIST(args[1]);

    ObjList* result = newList();
    push(OBJ_VAL(result));

    int count = a->items.count < b->items.count ?
                a->items.count : b->items.count;

    for (int i = 0; i < count; i++) {
        ObjList* pair = newList();
        push(OBJ_VAL(pair));
        writeValueArray(&pair->items, a->items.values[i]);
        writeValueArray(&pair->items, b->items.values[i]);
        pop();
        writeValueArray(&result->items, OBJ_VAL(pair));
    }

    pop();
    return OBJ_VAL(result);
}

/* ── join(list, separator) — join list to string ─────────── */
static Value nativeJoin(int argCount, Value* args) {
    if (!IS_LIST(args[0])) return NONE_VAL;
    ObjList* list = AS_LIST(args[0]);
    const char* sep = "";
    int sepLen = 0;

    if (argCount > 1 && IS_STRING(args[1])) {
        sep = AS_CSTRING(args[1]);
        sepLen = AS_STRING(args[1])->length;
    }

    /* Calculate total length */
    int totalLen = 0;
    for (int i = 0; i < list->items.count; i++) {
        char* s = valueToString(list->items.values[i]);
        totalLen += (int)strlen(s);
        reallocate(s, strlen(s) + 1, 0);
        if (i > 0) totalLen += sepLen;
    }

    char* buffer = ALLOCATE(char, totalLen + 1);
    int   pos = 0;

    for (int i = 0; i < list->items.count; i++) {
        if (i > 0) {
            memcpy(buffer + pos, sep, sepLen);
            pos += sepLen;
        }
        char* s = valueToString(list->items.values[i]);
        int   sl = (int)strlen(s);
        memcpy(buffer + pos, s, sl);
        pos += sl;
        reallocate(s, sl + 1, 0);
    }
    buffer[pos] = '\0';

    ObjString* result = takeString(buffer, totalLen);
    return OBJ_VAL(result);
}

/* ── File I/O ────────────────────────────────────────────── */

static Value nativeReadFile(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_STRING(args[0])) return NONE_VAL;

    FILE* file = fopen(AS_CSTRING(args[0]), "rb");
    if (file == NULL) return NONE_VAL;

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = ALLOCATE(char, fileSize + 1);
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    buffer[bytesRead] = '\0';
    fclose(file);

    return OBJ_VAL(takeString(buffer, (int)bytesRead));
}

static Value nativeWriteFile(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) return BOOL_VAL(false);

    FILE* file = fopen(AS_CSTRING(args[0]), "wb");
    if (file == NULL) return BOOL_VAL(false);

    ObjString* content = AS_STRING(args[1]);
    fwrite(content->chars, sizeof(char), content->length, file);
    fclose(file);
    return BOOL_VAL(true);
}

static Value nativeAppendFile(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_STRING(args[0]) || !IS_STRING(args[1])) return BOOL_VAL(false);

    FILE* file = fopen(AS_CSTRING(args[0]), "ab");
    if (file == NULL) return BOOL_VAL(false);

    ObjString* content = AS_STRING(args[1]);
    fwrite(content->chars, sizeof(char), content->length, file);
    fclose(file);
    return BOOL_VAL(true);
}

static Value nativeFileExists(int argCount, Value* args) {
    UNUSED(argCount);
    if (!IS_STRING(args[0])) return BOOL_VAL(false);
    FILE* file = fopen(AS_CSTRING(args[0]), "r");
    if (file) { fclose(file); return BOOL_VAL(true); }
    return BOOL_VAL(false);
}

/* ══════════════════════════════════════════════════════════
 *  REGISTRATION
 * ══════════════════════════════════════════════════════════ */

void registerStdlib(void) {
    srand((unsigned int)time(NULL));

    /* I/O */
    defineNative("println",     nativePrintln,     -1);
    defineNative("print_raw",   nativePrintNoNl,   -1);
    defineNative("input",       nativeInput,       -1);

    /* Type introspection & conversion */
    defineNative("type",        nativeType,         1);
    defineNative("len",         nativeLen,          1);
    defineNative("str",         nativeStr,          1);
    defineNative("int",         nativeInt,          1);
    defineNative("float",       nativeFloat,        1);
    defineNative("bool",        nativeBool,         1);

    /* Collections */
    defineNative("range",       nativeRange,       -1);
    defineNative("sorted",      nativeSorted,       1);
    defineNative("reversed",    nativeReversed,     1);
    defineNative("enumerate",   nativeEnumerate,    1);
    defineNative("zip",         nativeZip,          2);
    defineNative("join",        nativeJoin,        -1);
    defineNative("sum",         nativeSum,          1);

    /* Functional */
    /* defineNative("map",      nativeMap,          2); */
    /* defineNative("filter",   nativeFilter,       2); */

    /* Assertions & control */
    defineNative("assert",      nativeAssert,      -1);
    defineNative("exit",        nativeExit,        -1);

    /* Time */
    defineNative("time",        nativeTime,         0);
    defineNative("clock",       nativeClock,        0);
    defineNative("sleep",       nativeSleep,        1);

    /* Math */
    defineNative("abs",         nativeAbs,          1);
    defineNative("min",         nativeMin,         -1);
    defineNative("max",         nativeMax,         -1);
    defineNative("sqrt",        nativeSqrt,         1);
    defineNative("floor",       nativeFloor,        1);
    defineNative("ceil",        nativeCeil,         1);
    defineNative("round",       nativeRound,        1);
    defineNative("log",         nativeLog,          1);
    defineNative("sin",         nativeSin,          1);
    defineNative("cos",         nativeCos,          1);
    defineNative("tan",         nativeTan,          1);
    defineNative("random",      nativeRandom,       0);

    /* File I/O */
    defineNative("read_file",    nativeReadFile,    1);
    defineNative("write_file",   nativeWriteFile,   2);
    defineNative("append_file",  nativeAppendFile,  2);
    defineNative("file_exists",  nativeFileExists,  1);

    /* Math constants as globals */
    tableSet(&vm.globals,
        copyString("PI", 2), FLOAT_VAL(3.14159265358979323846));
    tableSet(&vm.globals,
        copyString("E", 1), FLOAT_VAL(2.71828182845904523536));
    tableSet(&vm.globals,
        copyString("TAU", 3), FLOAT_VAL(6.28318530717958647692));
}
