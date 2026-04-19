/*
 * Astra Programming Language
 * scanner.h — Lexer / Tokenizer
 */

#ifndef astra_scanner_h
#define astra_scanner_h

/* ── Token types ─────────────────────────────────────────── */

typedef enum {
    /* Single-character tokens */
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,     /* ( ) */
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,     /* { } */
    TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET, /* [ ] */
    TOKEN_COMMA, TOKEN_DOT, TOKEN_COLON,     /* , . : */
    TOKEN_SEMICOLON,                         /* ; (optional) */

    /* Arithmetic operators */
    TOKEN_PLUS, TOKEN_MINUS,                 /* + - */
    TOKEN_STAR, TOKEN_SLASH,                 /* * / */
    TOKEN_PERCENT,                           /* % */
    TOKEN_STAR_STAR,                         /* ** */

    /* Compound assignment */
    TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,     /* += -= */
    TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,     /* *= /= */

    /* Comparison operators */
    TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,          /* = == */
    TOKEN_BANG_EQUAL,                        /* != */
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,      /* > >= */
    TOKEN_LESS, TOKEN_LESS_EQUAL,            /* < <= */

    /* Special operators */
    TOKEN_ARROW,                             /* -> */
    TOKEN_FAT_ARROW,                         /* => */
    TOKEN_PIPE,                              /* |> */
    TOKEN_QUESTION,                          /* ? */
    TOKEN_DOT_DOT,                           /* .. */
    TOKEN_DOT_DOT_DOT,                       /* ... */
    TOKEN_HASH,                              /* # */

    /* Literals */
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_INT,
    TOKEN_FLOAT,

    /* Keywords */
    TOKEN_LET, TOKEN_MUT,                   /* let mut */
    TOKEN_FN, TOKEN_RETURN,                  /* fn return */
    TOKEN_IF, TOKEN_ELIF, TOKEN_ELSE,        /* if elif else */
    TOKEN_FOR, TOKEN_WHILE, TOKEN_IN,        /* for while in */
    TOKEN_BREAK, TOKEN_CONTINUE,             /* break continue */
    TOKEN_MATCH,                             /* match */
    TOKEN_STRUCT, TOKEN_IMPL,                /* struct impl */
    TOKEN_USE,                               /* use */
    TOKEN_SELF,                              /* self */
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_NONE,     /* true false none */
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,          /* and or not */
    TOKEN_IS,                                /* is */
    TOKEN_PRINT,                             /* print (bootstrap) */
    TOKEN_SPAWN, TOKEN_ASYNC, TOKEN_AWAIT,   /* spawn async await */

    /* Special */
    TOKEN_ERROR,
    TOKEN_EOF,
    TOKEN_NEWLINE,
} TokenType;

/* ── Token ───────────────────────────────────────────────── */

typedef struct {
    TokenType   type;
    const char* start;
    int         length;
    int         line;
    int         column;
} Token;

/* ── Scanner API ─────────────────────────────────────────── */

void  initScanner(const char* source);
Token scanToken(void);
const char* tokenTypeName(TokenType type);

#endif /* astra_scanner_h */
