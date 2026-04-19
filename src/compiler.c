/*
 * Astra Programming Language
 * compiler.c — Single-pass Pratt parser that emits bytecode directly
 *
 * This is the heart of Astra. It takes tokens from the scanner and
 * compiles them into bytecode for the VM in a single pass.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "scanner.h"
#include "object.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/* ── Parser state ────────────────────────────────────────── */

typedef struct {
    Token   current;
    Token   previous;
    bool    hadError;
    bool    panicMode;
} Parser;

/* ── Precedence levels (low to high) ─────────────────────── */

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  /* =            */
    PREC_PIPE,        /* |>           */
    PREC_OR,          /* or           */
    PREC_AND,         /* and          */
    PREC_EQUALITY,    /* == !=        */
    PREC_COMPARISON,  /* < > <= >=    */
    PREC_RANGE,       /* ..           */
    PREC_TERM,        /* + -          */
    PREC_FACTOR,      /* * / %        */
    PREC_POWER,       /* **           */
    PREC_UNARY,       /* - not        */
    PREC_CALL,        /* . () []      */
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn    prefix;
    ParseFn    infix;
    Precedence precedence;
} ParseRule;

/* ── Local variable ──────────────────────────────────────── */

typedef struct {
    Token name;
    int   depth;
    bool  isCaptured;
    bool  isMutable;
} Local;

/* ── Upvalue ─────────────────────────────────────────────── */

typedef struct {
    uint8_t index;
    bool    isLocal;
} Upvalue;

/* ── Function type ───────────────────────────────────────── */

typedef enum {
    TYPE_FUNCTION,
    TYPE_METHOD,
    TYPE_INITIALIZER,
    TYPE_SCRIPT,
} FunctionType;

/* ── Compiler state (nested for closures) ────────────────── */

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction*     function;
    FunctionType     type;

    Local   locals[UINT8_COUNT];
    int     localCount;
    Upvalue upvalues[UINT8_COUNT];
    int     scopeDepth;
} Compiler;

/* ── Struct compiler (for impl blocks) ───────────────────── */

typedef struct StructCompiler {
    struct StructCompiler* enclosing;
    bool                   hasSuperStruct;
} StructCompiler;

/* ── Loop tracking (for break/continue) ──────────────────── */

typedef struct Loop {
    struct Loop* enclosing;
    int          start;
    int          body;
    int          scopeDepth;
    int          breakJumps[256];
    int          breakCount;
} Loop;

/* ── Global state ────────────────────────────────────────── */

static Parser          parser;
static Compiler*       current       = NULL;
static StructCompiler* currentStruct = NULL;
static Loop*           currentLoop   = NULL;

/* ── Forward declarations ────────────────────────────────── */

static void     expression(void);
static void     statement(void);
static void     declaration(void);
static ParseRule* getRule(TokenType type);
static void     parsePrecedence(Precedence precedence);
static void     block(void);
static uint8_t  argumentList(void);

/* ── Chunk access ────────────────────────────────────────── */

static Chunk* currentChunk(void) {
    return &current->function->chunk;
}

/* ── Error reporting ─────────────────────────────────────── */

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "\n  \033[1;31m✖ Error\033[0m ");

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, "at end of file");
    } else if (token->type == TOKEN_ERROR) {
        /* nothing */
    } else {
        fprintf(stderr, "at '\033[1m%.*s\033[0m'",
                token->length, token->start);
    }

    fprintf(stderr, " [line %d, col %d]\n", token->line, token->column);
    fprintf(stderr, "    \033[90m│\033[0m %s\n\n", message);

    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

/* ── Token consumption ───────────────────────────────────── */

static void advance(void) {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();

        /* Skip newlines automatically in most contexts */
        if (parser.current.type == TOKEN_NEWLINE) continue;

        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/* ── Bytecode emission ───────────────────────────────────── */

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn(void) {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NONE);
    }
    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

/* ── Jump patching ───────────────────────────────────────── */

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void patchJump(int offset) {
    int jump = currentChunk()->count - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }
    currentChunk()->code[offset]     = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);
    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");
    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

/* ── Compiler initialization ─────────────────────────────── */

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing    = current;
    compiler->function     = NULL;
    compiler->type         = type;
    compiler->localCount   = 0;
    compiler->scopeDepth   = 0;
    compiler->function     = newFunction();
    current                = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(
            parser.previous.start, parser.previous.length);
    }

    /* Slot 0 is reserved for the function/method receiver */
    Local* local       = &current->locals[current->localCount++];
    local->depth       = 0;
    local->isCaptured  = false;
    local->isMutable   = false;

    if (type == TYPE_METHOD || type == TYPE_INITIALIZER) {
        local->name.start  = "self";
        local->name.length = 4;
    } else {
        local->name.start  = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler(void) {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(),
            function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

/* ── Scope management ────────────────────────────────────── */

static void beginScope(void) {
    current->scopeDepth++;
}

static void endScope(void) {
    current->scopeDepth--;

    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

/* ── Variable resolution ─────────────────────────────────── */

static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(
        copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index,
                      bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    /* Check if we already captured this */
    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index   = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name, bool isMutable) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local* local      = &current->locals[current->localCount++];
    local->name       = name;
    local->depth      = -1; /* Mark uninitialized */
    local->isCaptured = false;
    local->isMutable  = isMutable;
}

static void declareVariable(bool isMutable) {
    if (current->scopeDepth == 0) return;

    Token* name = &parser.previous;

    /* Check for duplicate in current scope */
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name, isMutable);
}

static uint8_t parseVariable(const char* errorMessage, bool isMutable) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable(isMutable);
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized(void) {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth =
        current->scopeDepth;
}

static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    emitBytes(OP_DEFINE_GLOBAL, global);
}

/* ── Named variable (get/set) ────────────────────────────── */

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int     arg = resolveLocal(current, &name);
    bool    isMutable = true;

    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
        isMutable = current->locals[arg].isMutable;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg   = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        if (!isMutable && getOp == OP_GET_LOCAL) {
            error("Cannot assign to immutable variable. Use 'mut' to make it mutable.");
        }
        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else if (canAssign && (check(TOKEN_PLUS_EQUAL) ||
               check(TOKEN_MINUS_EQUAL) ||
               check(TOKEN_STAR_EQUAL) ||
               check(TOKEN_SLASH_EQUAL))) {
        if (!isMutable && getOp == OP_GET_LOCAL) {
            error("Cannot assign to immutable variable. Use 'mut' to make it mutable.");
        }
        TokenType op = parser.current.type;
        advance();
        emitBytes(getOp, (uint8_t)arg);
        expression();
        switch (op) {
            case TOKEN_PLUS_EQUAL:  emitByte(OP_ADD); break;
            case TOKEN_MINUS_EQUAL: emitByte(OP_SUBTRACT); break;
            case TOKEN_STAR_EQUAL:  emitByte(OP_MULTIPLY); break;
            case TOKEN_SLASH_EQUAL: emitByte(OP_DIVIDE); break;
            default: break;
        }
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  EXPRESSION PARSING (Pratt Parser)
 * ═══════════════════════════════════════════════════════════ */

/* ── Literals ────────────────────────────────────────────── */

static void number(bool canAssign) {
    UNUSED(canAssign);

    /* Check if it's an integer or float */
    bool isFloat = false;
    for (int i = 0; i < parser.previous.length; i++) {
        if (parser.previous.start[i] == '.' ||
            parser.previous.start[i] == 'e' ||
            parser.previous.start[i] == 'E') {
            isFloat = true;
            break;
        }
    }

    if (isFloat) {
        double value = strtod(parser.previous.start, NULL);
        emitConstant(FLOAT_VAL(value));
    } else {
        int64_t value = strtoll(parser.previous.start, NULL, 10);
        emitConstant(INT_VAL(value));
    }
}

static void string(bool canAssign) {
    UNUSED(canAssign);

    /* Strip quotes and process escape sequences */
    const char* start = parser.previous.start + 1;
    int         rawLen = parser.previous.length - 2;

    /* Allocate buffer for processed string */
    char* buffer = ALLOCATE(char, rawLen + 1);
    int   len = 0;

    for (int i = 0; i < rawLen; i++) {
        if (start[i] == '\\' && i + 1 < rawLen) {
            i++;
            switch (start[i]) {
                case 'n':  buffer[len++] = '\n'; break;
                case 't':  buffer[len++] = '\t'; break;
                case 'r':  buffer[len++] = '\r'; break;
                case '\\': buffer[len++] = '\\'; break;
                case '"':  buffer[len++] = '"';  break;
                case '\'': buffer[len++] = '\''; break;
                case '0':  buffer[len++] = '\0'; break;
                default:
                    buffer[len++] = '\\';
                    buffer[len++] = start[i];
                    break;
            }
        } else if (start[i] == '{') {
            /* String interpolation: "Hello, {name}!" */
            /* For now, just include the braces literally */
            /* TODO: implement proper string interpolation */
            buffer[len++] = start[i];
        } else {
            buffer[len++] = start[i];
        }
    }
    buffer[len] = '\0';

    ObjString* str = copyString(buffer, len);
    FREE_ARRAY(char, buffer, rawLen + 1);
    emitConstant(OBJ_VAL(str));
}

static void literal(bool canAssign) {
    UNUSED(canAssign);
    switch (parser.previous.type) {
        case TOKEN_TRUE:  emitByte(OP_TRUE);  break;
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NONE:  emitByte(OP_NONE);  break;
        default: return; /* Unreachable */
    }
}

/* ── Variable reference ──────────────────────────────────── */

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void self_(bool canAssign) {
    UNUSED(canAssign);
    if (currentStruct == NULL) {
        error("Can't use 'self' outside of a struct method.");
        return;
    }
    variable(false);
}

/* ── Grouping: (expr) ────────────────────────────────────── */

static void grouping(bool canAssign) {
    UNUSED(canAssign);
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

/* ── Unary operators ─────────────────────────────────────── */

static void unary(bool canAssign) {
    UNUSED(canAssign);
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        case TOKEN_NOT:   emitByte(OP_NOT);    break;
        default: return;
    }
}

/* ── Binary operators ────────────────────────────────────── */

static void binary(bool canAssign) {
    UNUSED(canAssign);
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);

    /* Right-associative for ** */
    if (operatorType == TOKEN_STAR_STAR) {
        parsePrecedence((Precedence)(rule->precedence));
    } else {
        parsePrecedence((Precedence)(rule->precedence + 1));
    }

    switch (operatorType) {
        case TOKEN_PLUS:          emitByte(OP_ADD);           break;
        case TOKEN_MINUS:         emitByte(OP_SUBTRACT);      break;
        case TOKEN_STAR:          emitByte(OP_MULTIPLY);       break;
        case TOKEN_SLASH:         emitByte(OP_DIVIDE);         break;
        case TOKEN_PERCENT:       emitByte(OP_MODULO);         break;
        case TOKEN_STAR_STAR:     emitByte(OP_POWER);          break;
        case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL);          break;
        case TOKEN_BANG_EQUAL:    emitByte(OP_NOT_EQUAL);      break;
        case TOKEN_GREATER:       emitByte(OP_GREATER);        break;
        case TOKEN_LESS:          emitByte(OP_LESS);           break;
        case TOKEN_GREATER_EQUAL: emitByte(OP_GREATER_EQUAL);  break;
        case TOKEN_LESS_EQUAL:    emitByte(OP_LESS_EQUAL);     break;
        default: return;
    }
}

/* ── Logical and/or ──────────────────────────────────────── */

static void and_(bool canAssign) {
    UNUSED(canAssign);
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);
    patchJump(endJump);
}

static void or_(bool canAssign) {
    UNUSED(canAssign);
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump  = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);
    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

/* ── Function call ───────────────────────────────────────── */

static uint8_t argumentList(void) {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
    return argCount;
}

static void call(bool canAssign) {
    UNUSED(canAssign);
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

/* ── Property access: expr.field ─────────────────────────── */

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        /* method invocation: obj.method(args) */
        uint8_t argCount = argumentList();
        emitByte(OP_INVOKE);
        emitByte(name);
        emitByte(argCount);
    } else {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

/* ── Index access: expr[index] ───────────────────────────── */

static void indexExpr(bool canAssign) {
    UNUSED(canAssign);
    expression();

    /* Check for slice: a[start..end] */
    if (match(TOKEN_DOT_DOT)) {
        expression();
        consume(TOKEN_RIGHT_BRACKET, "Expected ']' after slice.");
        emitByte(OP_GET_SLICE);
        return;
    }

    consume(TOKEN_RIGHT_BRACKET, "Expected ']' after index.");

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitByte(OP_SET_INDEX);
    } else {
        emitByte(OP_GET_INDEX);
    }
}

/* ── List literal: [1, 2, 3] ─────────────────────────────── */

static void list(bool canAssign) {
    UNUSED(canAssign);
    int count = 0;

    if (!check(TOKEN_RIGHT_BRACKET)) {
        do {
            if (check(TOKEN_RIGHT_BRACKET)) break;
            expression();
            count++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expected ']' after list elements.");

    if (count > 255) {
        error("Can't have more than 255 elements in a list literal.");
    }
    emitBytes(OP_BUILD_LIST, (uint8_t)count);
}

/* ── Map literal: {key: value} ───────────────────────────── */

static void map(bool canAssign) {
    UNUSED(canAssign);
    int count = 0;

    if (!check(TOKEN_RIGHT_BRACE)) {
        do {
            if (check(TOKEN_RIGHT_BRACE)) break;

            /* Key can be string or identifier */
            if (check(TOKEN_IDENTIFIER)) {
                consume(TOKEN_IDENTIFIER, "Expected key.");
                emitConstant(OBJ_VAL(
                    copyString(parser.previous.start,
                               parser.previous.length)));
            } else {
                expression(); /* Key expression */
            }
            consume(TOKEN_COLON, "Expected ':' after map key.");
            expression(); /* Value */
            count++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after map entries.");
    emitBytes(OP_BUILD_MAP, (uint8_t)count);
}

/* ── Pipe operator: expr |> fn ───────────────────────────── */

static void pipe(bool canAssign) {
    UNUSED(canAssign);
    parsePrecedence(PREC_PIPE + 1);

    /* The pipe operator passes the left value as the first arg */
    emitBytes(OP_CALL, 1);
}

/* ── Lambda: fn(x) => expr  or  fn(x, y) { stmts } ──────── */

static void lambda(bool canAssign) {
    UNUSED(canAssign);

    Compiler compiler;
    initCompiler(&compiler, TYPE_FUNCTION);
    beginScope();

    /* Parameters */
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'fn'.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expected parameter name.", true);
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");

    if (match(TOKEN_FAT_ARROW)) {
        /* Short form: fn(x) => expr */
        expression();
        emitByte(OP_RETURN);
    } else {
        /* Long form: fn(x) { ... } */
        consume(TOKEN_LEFT_BRACE, "Expected '=>' or '{' for lambda body.");
        block();
    }

    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

/* ═══════════════════════════════════════════════════════════
 *  PARSE RULE TABLE
 * ═══════════════════════════════════════════════════════════ */

static ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]    = {grouping, call,    PREC_CALL},
    [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,    PREC_NONE},
    [TOKEN_LEFT_BRACE]    = {map,      NULL,    PREC_NONE},
    [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,    PREC_NONE},
    [TOKEN_LEFT_BRACKET]  = {list,     indexExpr, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL,     NULL,    PREC_NONE},
    [TOKEN_COMMA]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_DOT]           = {NULL,     dot,     PREC_CALL},
    [TOKEN_COLON]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_SEMICOLON]     = {NULL,     NULL,    PREC_NONE},

    [TOKEN_PLUS]          = {NULL,     binary,  PREC_TERM},
    [TOKEN_MINUS]         = {unary,    binary,  PREC_TERM},
    [TOKEN_STAR]          = {NULL,     binary,  PREC_FACTOR},
    [TOKEN_SLASH]         = {NULL,     binary,  PREC_FACTOR},
    [TOKEN_PERCENT]       = {NULL,     binary,  PREC_FACTOR},
    [TOKEN_STAR_STAR]     = {NULL,     binary,  PREC_POWER},

    [TOKEN_PLUS_EQUAL]    = {NULL,     NULL,    PREC_NONE},
    [TOKEN_MINUS_EQUAL]   = {NULL,     NULL,    PREC_NONE},
    [TOKEN_STAR_EQUAL]    = {NULL,     NULL,    PREC_NONE},
    [TOKEN_SLASH_EQUAL]   = {NULL,     NULL,    PREC_NONE},

    [TOKEN_EQUAL]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_EQUAL_EQUAL]   = {NULL,     binary,  PREC_EQUALITY},
    [TOKEN_BANG_EQUAL]    = {NULL,     binary,  PREC_EQUALITY},
    [TOKEN_GREATER]       = {NULL,     binary,  PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL,     binary,  PREC_COMPARISON},
    [TOKEN_LESS]          = {NULL,     binary,  PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]    = {NULL,     binary,  PREC_COMPARISON},

    [TOKEN_ARROW]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_FAT_ARROW]     = {NULL,     NULL,    PREC_NONE},
    [TOKEN_PIPE]          = {NULL,     pipe,    PREC_PIPE},
    [TOKEN_QUESTION]      = {NULL,     NULL,    PREC_NONE},
    [TOKEN_DOT_DOT]       = {NULL,     NULL,    PREC_RANGE},
    [TOKEN_DOT_DOT_DOT]   = {NULL,     NULL,    PREC_NONE},
    [TOKEN_HASH]          = {NULL,     NULL,    PREC_NONE},

    [TOKEN_IDENTIFIER]    = {variable, NULL,    PREC_NONE},
    [TOKEN_STRING]        = {string,   NULL,    PREC_NONE},
    [TOKEN_INT]           = {number,   NULL,    PREC_NONE},
    [TOKEN_FLOAT]         = {number,   NULL,    PREC_NONE},

    [TOKEN_LET]           = {NULL,     NULL,    PREC_NONE},
    [TOKEN_MUT]           = {NULL,     NULL,    PREC_NONE},
    [TOKEN_FN]            = {lambda,   NULL,    PREC_NONE},
    [TOKEN_RETURN]        = {NULL,     NULL,    PREC_NONE},
    [TOKEN_IF]            = {NULL,     NULL,    PREC_NONE},
    [TOKEN_ELIF]          = {NULL,     NULL,    PREC_NONE},
    [TOKEN_ELSE]          = {NULL,     NULL,    PREC_NONE},
    [TOKEN_FOR]           = {NULL,     NULL,    PREC_NONE},
    [TOKEN_WHILE]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_IN]            = {NULL,     NULL,    PREC_NONE},
    [TOKEN_BREAK]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_CONTINUE]      = {NULL,     NULL,    PREC_NONE},
    [TOKEN_MATCH]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_STRUCT]        = {NULL,     NULL,    PREC_NONE},
    [TOKEN_IMPL]          = {NULL,     NULL,    PREC_NONE},
    [TOKEN_USE]           = {NULL,     NULL,    PREC_NONE},
    [TOKEN_SELF]          = {self_,    NULL,    PREC_NONE},
    [TOKEN_TRUE]          = {literal,  NULL,    PREC_NONE},
    [TOKEN_FALSE]         = {literal,  NULL,    PREC_NONE},
    [TOKEN_NONE]          = {literal,  NULL,    PREC_NONE},
    [TOKEN_AND]           = {NULL,     and_,    PREC_AND},
    [TOKEN_OR]            = {NULL,     or_,     PREC_OR},
    [TOKEN_NOT]           = {unary,    NULL,    PREC_NONE},
    [TOKEN_IS]            = {NULL,     NULL,    PREC_NONE},
    [TOKEN_PRINT]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_SPAWN]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_ASYNC]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_AWAIT]         = {NULL,     NULL,    PREC_NONE},

    [TOKEN_ERROR]         = {NULL,     NULL,    PREC_NONE},
    [TOKEN_EOF]           = {NULL,     NULL,    PREC_NONE},
    [TOKEN_NEWLINE]       = {NULL,     NULL,    PREC_NONE},
};

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

/* ── Pratt parser core ───────────────────────────────────── */

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        error("Expected expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static void expression(void) {
    parsePrecedence(PREC_ASSIGNMENT);
}

/* ═══════════════════════════════════════════════════════════
 *  STATEMENT PARSING
 * ═══════════════════════════════════════════════════════════ */

static void block(void) {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

/* ── let / mut declaration ───────────────────────────────── */

static void letDeclaration(bool isMutable) {
    uint8_t global = parseVariable("Expected variable name.",
                                    isMutable);

    /* Optional type annotation: let x: int = ... */
    if (match(TOKEN_COLON)) {
        /* For now, skip type annotations — added for future use */
        advance(); /* consume type name */
    }

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NONE);
    }

    defineVariable(global);
}

/* ── fn declaration ──────────────────────────────────────── */

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }

            /* Check for 'self' in method */
            if (check(TOKEN_SELF) &&
                (type == TYPE_METHOD || type == TYPE_INITIALIZER)) {
                advance(); /* consume 'self' */
                current->function->arity--; /* self doesn't count */
                continue;
            }

            uint8_t constant = parseVariable("Expected parameter name.",
                                              true);
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");

    /* Optional return type: fn foo() -> int { ... } */
    if (match(TOKEN_ARROW)) {
        advance(); /* skip type for now */
    }

    consume(TOKEN_LEFT_BRACE, "Expected '{' before function body.");
    block();

    ObjFunction* fn = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(fn)));

    for (int i = 0; i < fn->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void fnDeclaration(void) {
    uint8_t global = parseVariable("Expected function name.", false);
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

/* ── struct declaration ──────────────────────────────────── */

static void structDeclaration(void) {
    consume(TOKEN_IDENTIFIER, "Expected struct name.");
    Token structName = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable(false);

    emitBytes(OP_STRUCT, nameConstant);
    defineVariable(nameConstant);

    StructCompiler structCompiler;
    structCompiler.hasSuperStruct = false;
    structCompiler.enclosing      = currentStruct;
    currentStruct                 = &structCompiler;

    /* Parse struct body for field declarations */
    consume(TOKEN_LEFT_BRACE, "Expected '{' before struct body.");

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        /* Fields: name: type or name: type = default */
        consume(TOKEN_IDENTIFIER, "Expected field name.");
        Token fieldName = parser.previous;

        /* Optional type annotation */
        if (match(TOKEN_COLON)) {
            advance(); /* skip type */
        }

        /* Optional default value */
        if (match(TOKEN_EQUAL)) {
            namedVariable(structName, false);
            expression();
            uint8_t name = identifierConstant(&fieldName);
            emitBytes(OP_SET_PROPERTY, name);
            emitByte(OP_POP);
        }

        /* Consume optional comma or newline */
        match(TOKEN_COMMA);
    }

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after struct body.");
    currentStruct = currentStruct->enclosing;
}

/* ── impl block ──────────────────────────────────────────── */

static void implDeclaration(void) {
    consume(TOKEN_IDENTIFIER, "Expected struct name after 'impl'.");
    Token structName = parser.previous;

    StructCompiler structCompiler;
    structCompiler.hasSuperStruct = false;
    structCompiler.enclosing      = currentStruct;
    currentStruct                 = &structCompiler;

    /* Push the struct onto the stack */
    namedVariable(structName, false);

    consume(TOKEN_LEFT_BRACE, "Expected '{' before impl body.");

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        consume(TOKEN_FN, "Expected 'fn' in impl block.");
        consume(TOKEN_IDENTIFIER, "Expected method name.");
        uint8_t constant = identifierConstant(&parser.previous);

        FunctionType type = TYPE_METHOD;
        if (parser.previous.length == 4 &&
            memcmp(parser.previous.start, "init", 4) == 0) {
            type = TYPE_INITIALIZER;
        }

        function(type);
        emitBytes(OP_METHOD, constant);
    }

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after impl body.");
    emitByte(OP_POP); /* Pop the struct */

    currentStruct = currentStruct->enclosing;
}

/* ── if / elif / else ────────────────────────────────────── */

static void ifStatement(void) {
    expression();
    consume(TOKEN_LEFT_BRACE, "Expected '{' after if condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    beginScope();
    block();
    endScope();

    int elseJump = emitJump(OP_JUMP);
    patchJump(thenJump);
    emitByte(OP_POP);

    /* elif chains */
    while (match(TOKEN_ELIF)) {
        expression();
        consume(TOKEN_LEFT_BRACE, "Expected '{' after elif condition.");

        thenJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);

        beginScope();
        block();
        endScope();

        int nextElseJump = emitJump(OP_JUMP);
        patchJump(thenJump);
        emitByte(OP_POP);

        patchJump(elseJump);
        elseJump = nextElseJump;
    }

    if (match(TOKEN_ELSE)) {
        consume(TOKEN_LEFT_BRACE, "Expected '{' after else.");
        beginScope();
        block();
        endScope();
    }

    patchJump(elseJump);
}

/* ── while loop ──────────────────────────────────────────── */

static void whileStatement(void) {
    Loop loop;
    loop.enclosing  = currentLoop;
    loop.start      = currentChunk()->count;
    loop.scopeDepth = current->scopeDepth;
    loop.breakCount = 0;
    currentLoop     = &loop;

    expression();
    consume(TOKEN_LEFT_BRACE, "Expected '{' after while condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    loop.body = currentChunk()->count;

    beginScope();
    block();
    endScope();

    emitLoop(loop.start);

    patchJump(exitJump);
    emitByte(OP_POP);

    /* Patch break jumps */
    for (int i = 0; i < loop.breakCount; i++) {
        patchJump(loop.breakJumps[i]);
    }

    currentLoop = loop.enclosing;
}

/* ── for-in loop ─────────────────────────────────────────── */

static void forStatement(void) {
    beginScope();

    /* for item in iterable { ... } */
    consume(TOKEN_IDENTIFIER, "Expected variable name after 'for'.");
    Token itemName = parser.previous;
    addLocal(itemName, true);
    markInitialized();
    emitByte(OP_NONE); /* placeholder for loop variable */
    int itemSlot = current->localCount - 1;

    consume(TOKEN_IN, "Expected 'in' after for variable.");

    /* Evaluate the iterable */
    expression();

    /* Create an iterator */
    emitByte(OP_ITERATOR);

    /* Hidden iterator local */
    Token iterName;
    iterName.start  = " iter";
    iterName.length = 5;
    addLocal(iterName, false);
    markInitialized();
    int iterSlot = current->localCount - 1;

    Loop loop;
    loop.enclosing  = currentLoop;
    loop.start      = currentChunk()->count;
    loop.scopeDepth = current->scopeDepth;
    loop.breakCount = 0;
    currentLoop     = &loop;

    /* Get next value from iterator */
    emitBytes(OP_GET_LOCAL, (uint8_t)iterSlot);
    int exitJump = emitJump(OP_ITERATOR_NEXT);

    /* Store the value in the loop variable */
    emitBytes(OP_SET_LOCAL, (uint8_t)itemSlot);
    emitByte(OP_POP);

    loop.body = currentChunk()->count;

    consume(TOKEN_LEFT_BRACE, "Expected '{' after for clause.");
    beginScope();
    block();
    endScope();

    emitLoop(loop.start);

    patchJump(exitJump);

    /* Patch break jumps */
    for (int i = 0; i < loop.breakCount; i++) {
        patchJump(loop.breakJumps[i]);
    }

    endScope();
    currentLoop = loop.enclosing;
}

/* ── match statement ─────────────────────────────────────── */

static void matchStatement(void) {
    expression();
    consume(TOKEN_LEFT_BRACE, "Expected '{' after match value.");

    int endJumps[256];
    int endCount = 0;

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        if (match(TOKEN_IDENTIFIER) &&
            parser.previous.length == 1 &&
            parser.previous.start[0] == '_') {
            /* Default case: _ => ... */
            emitByte(OP_POP);
            consume(TOKEN_FAT_ARROW, "Expected '=>' after '_'.");

            if (match(TOKEN_LEFT_BRACE)) {
                beginScope();
                block();
                endScope();
            } else {
                expression();
                emitByte(OP_POP);
            }

            if (endCount < 256) {
                endJumps[endCount++] = emitJump(OP_JUMP);
            }
        } else {
            /* Match case: value => ... */
            emitByte(OP_DUP);
            expression();
            emitByte(OP_EQUAL);

            int nextCase = emitJump(OP_JUMP_IF_FALSE);
            emitByte(OP_POP); /* pop the comparison result */
            emitByte(OP_POP); /* pop the match value */

            consume(TOKEN_FAT_ARROW, "Expected '=>' after match pattern.");

            if (match(TOKEN_LEFT_BRACE)) {
                beginScope();
                block();
                endScope();
            } else {
                expression();
                emitByte(OP_POP);
            }

            if (endCount < 256) {
                endJumps[endCount++] = emitJump(OP_JUMP);
            }

            patchJump(nextCase);
            emitByte(OP_POP); /* pop comparison result */
        }
    }

    /* Pop the match value if no case matched */
    emitByte(OP_POP);

    consume(TOKEN_RIGHT_BRACE, "Expected '}' after match body.");

    for (int i = 0; i < endCount; i++) {
        patchJump(endJumps[i]);
    }
}

/* ── return statement ────────────────────────────────────── */

static void returnStatement(void) {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (check(TOKEN_RIGHT_BRACE) || check(TOKEN_EOF) ||
        check(TOKEN_NEWLINE)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        emitByte(OP_RETURN);
    }
}

/* ── break / continue ────────────────────────────────────── */

static void breakStatement(void) {
    if (currentLoop == NULL) {
        error("Can't use 'break' outside of a loop.");
        return;
    }

    /* Close any locals in the loop */
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > currentLoop->scopeDepth;
         i--) {
        emitByte(OP_POP);
    }

    if (currentLoop->breakCount >= 256) {
        error("Too many break statements in loop.");
        return;
    }

    currentLoop->breakJumps[currentLoop->breakCount++] =
        emitJump(OP_JUMP);
}

static void continueStatement(void) {
    if (currentLoop == NULL) {
        error("Can't use 'continue' outside of a loop.");
        return;
    }

    /* Close any locals in the loop */
    for (int i = current->localCount - 1;
         i >= 0 && current->locals[i].depth > currentLoop->scopeDepth;
         i--) {
        emitByte(OP_POP);
    }

    emitLoop(currentLoop->start);
}

/* ── print statement (bootstrap — will become a function) ── */

static void printStatement(void) {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'print'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after print argument.");
    emitByte(OP_PRINT);
}

/* ── Expression statement ────────────────────────────────── */

static void expressionStatement(void) {
    expression();
    emitByte(OP_POP);
}

/* ── Error recovery: synchronize ─────────────────────────── */

static void synchronize(void) {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        switch (parser.current.type) {
            case TOKEN_LET:
            case TOKEN_MUT:
            case TOKEN_FN:
            case TOKEN_STRUCT:
            case TOKEN_IMPL:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_FOR:
            case TOKEN_MATCH:
            case TOKEN_RETURN:
            case TOKEN_USE:
            case TOKEN_PRINT:
                return;
            default:
                ; /* Do nothing */
        }
        advance();
    }
}

/* ── Statement dispatcher ────────────────────────────────── */

static void statement(void) {
    if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_MATCH)) {
        matchStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_BREAK)) {
        breakStatement();
    } else if (match(TOKEN_CONTINUE)) {
        continueStatement();
    } else if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

/* ── Declaration dispatcher ──────────────────────────────── */

static void declaration(void) {
    if (match(TOKEN_LET)) {
        letDeclaration(false);
    } else if (match(TOKEN_MUT)) {
        letDeclaration(true);
    } else if (match(TOKEN_FN)) {
        fnDeclaration();
    } else if (match(TOKEN_STRUCT)) {
        structDeclaration();
    } else if (match(TOKEN_IMPL)) {
        implDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

/* ═══════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════ */

ObjFunction* compile(const char* source) {
    initScanner(source);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError  = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots(void) {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
