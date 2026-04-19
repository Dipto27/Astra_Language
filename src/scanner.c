/*
 * Astra Programming Language
 * scanner.c — Hand-written lexer
 */

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

/* ── Scanner state ───────────────────────────────────────── */

typedef struct {
    const char* start;
    const char* current;
    int         line;
    int         column;
    int         startColumn;
} Scanner;

static Scanner scanner;

/* ── Initialize ──────────────────────────────────────────── */

void initScanner(const char* source) {
    scanner.start       = source;
    scanner.current     = source;
    scanner.line        = 1;
    scanner.column      = 1;
    scanner.startColumn = 1;
}

/* ── Helpers ─────────────────────────────────────────────── */

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd(void) {
    return *scanner.current == '\0';
}

static char advance(void) {
    scanner.current++;
    scanner.column++;
    return scanner.current[-1];
}

static char peek(void) {
    return *scanner.current;
}

static char peekNext(void) {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    scanner.column++;
    return true;
}

/* ── Token constructors ──────────────────────────────────── */

static Token makeToken(TokenType type) {
    Token token;
    token.type   = type;
    token.start  = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line   = scanner.line;
    token.column = scanner.startColumn;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.type   = TOKEN_ERROR;
    token.start  = message;
    token.length = (int)strlen(message);
    token.line   = scanner.line;
    token.column = scanner.startColumn;
    return token;
}

/* ── Skip whitespace and comments ────────────────────────── */

static void skipWhitespace(void) {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;

            case '-':
                if (peekNext() == '-') {
                    /* Single-line comment: -- ... */
                    while (peek() != '\n' && !isAtEnd()) advance();
                    break;
                }
                return;

            case '{':
                if (peekNext() == '-') {
                    /* Block comment: {- ... -} */
                    advance(); /* { */
                    advance(); /* - */
                    int depth = 1;
                    while (depth > 0 && !isAtEnd()) {
                        if (peek() == '{' && peekNext() == '-') {
                            advance(); advance();
                            depth++;
                        } else if (peek() == '-' && peekNext() == '}') {
                            advance(); advance();
                            depth--;
                        } else {
                            if (peek() == '\n') {
                                scanner.line++;
                                scanner.column = 0;
                            }
                            advance();
                        }
                    }
                    break;
                }
                return;

            default:
                return;
        }
    }
}

/* ── Keyword matching ────────────────────────────────────── */

static TokenType checkKeyword(int start, int length,
                              const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

static TokenType identifierType(void) {
    switch (scanner.start[0]) {
        case 'a':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'n': return checkKeyword(2, 1, "d", TOKEN_AND);
                    case 's':
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'y': return checkKeyword(3, 2, "nc", TOKEN_ASYNC);
                            }
                        }
                        break;
                    case 'w': return checkKeyword(2, 3, "ait", TOKEN_AWAIT);
                }
            }
            break;
        case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
        case 'c': return checkKeyword(1, 7, "ontinue", TOKEN_CONTINUE);
        case 'e':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'l':
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'i': return checkKeyword(3, 1, "f", TOKEN_ELIF);
                                case 's': return checkKeyword(3, 1, "e", TOKEN_ELSE);
                            }
                        }
                        break;
                }
            }
            break;
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'n': return (scanner.current - scanner.start == 2) ? TOKEN_FN : TOKEN_IDENTIFIER;
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                }
            }
            break;
        case 'i':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'f': return (scanner.current - scanner.start == 2) ? TOKEN_IF : TOKEN_IDENTIFIER;
                    case 'm': return checkKeyword(2, 2, "pl", TOKEN_IMPL);
                    case 'n': return (scanner.current - scanner.start == 2) ? TOKEN_IN : TOKEN_IDENTIFIER;
                    case 's': return (scanner.current - scanner.start == 2) ? TOKEN_IS : TOKEN_IDENTIFIER;
                }
            }
            break;
        case 'l': return checkKeyword(1, 2, "et", TOKEN_LET);
        case 'm':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "tch", TOKEN_MATCH);
                    case 'u': return checkKeyword(2, 1, "t", TOKEN_MUT);
                }
            }
            break;
        case 'n':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'o':
                        if (scanner.current - scanner.start > 2) {
                            switch (scanner.start[2]) {
                                case 'n': return checkKeyword(3, 1, "e", TOKEN_NONE);
                                case 't': return (scanner.current - scanner.start == 3) ? TOKEN_NOT : TOKEN_IDENTIFIER;
                            }
                        }
                        break;
                }
            }
            break;
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p': return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return checkKeyword(2, 2, "lf", TOKEN_SELF);
                    case 'p': return checkKeyword(2, 3, "awn", TOKEN_SPAWN);
                    case 't': return checkKeyword(2, 4, "ruct", TOKEN_STRUCT);
                }
            }
            break;
        case 't': return checkKeyword(1, 3, "rue", TOKEN_TRUE);
        case 'u': return checkKeyword(1, 2, "se", TOKEN_USE);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

/* ── Scan identifier ─────────────────────────────────────── */

static Token identifier(void) {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

/* ── Scan number ─────────────────────────────────────────── */

static Token number(void) {
    bool isFloat = false;

    /* Integer part */
    while (isDigit(peek())) advance();

    /* Fractional part */
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance(); /* consume '.' */
        while (isDigit(peek())) advance();
    }

    /* Exponent part */
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance();
        if (peek() == '+' || peek() == '-') advance();
        if (!isDigit(peek())) {
            return errorToken("Invalid number literal.");
        }
        while (isDigit(peek())) advance();
    }

    return makeToken(isFloat ? TOKEN_FLOAT : TOKEN_INT);
}

/* ── Scan string ─────────────────────────────────────────── */

static Token string(char quote) {
    while (peek() != quote && !isAtEnd()) {
        if (peek() == '\n') {
            scanner.line++;
            scanner.column = 0;
        }
        if (peek() == '\\') advance(); /* skip escape */
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    advance(); /* closing quote */
    return makeToken(TOKEN_STRING);
}

/* ── Main scan function ──────────────────────────────────── */

Token scanToken(void) {
    skipWhitespace();

    scanner.start       = scanner.current;
    scanner.startColumn = scanner.column;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    /* Newlines */
    if (c == '\n') {
        scanner.line++;
        scanner.column = 1;
        return makeToken(TOKEN_NEWLINE);
    }

    /* Identifiers & keywords */
    if (isAlpha(c)) return identifier();

    /* Numbers */
    if (isDigit(c)) return number();

    /* Operators & delimiters */
    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case '[': return makeToken(TOKEN_LEFT_BRACKET);
        case ']': return makeToken(TOKEN_RIGHT_BRACKET);
        case ',': return makeToken(TOKEN_COMMA);
        case ':': return makeToken(TOKEN_COLON);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case '#': return makeToken(TOKEN_HASH);
        case '?': return makeToken(TOKEN_QUESTION);
        case '%': return makeToken(TOKEN_PERCENT);

        case '+': return makeToken(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
        case '-':
            if (match('>')) return makeToken(TOKEN_ARROW);
            if (match('=')) return makeToken(TOKEN_MINUS_EQUAL);
            return makeToken(TOKEN_MINUS);
        case '*':
            if (match('*')) return makeToken(TOKEN_STAR_STAR);
            if (match('=')) return makeToken(TOKEN_STAR_EQUAL);
            return makeToken(TOKEN_STAR);
        case '/':
            if (match('=')) return makeToken(TOKEN_SLASH_EQUAL);
            return makeToken(TOKEN_SLASH);

        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_ERROR);
        case '=':
            if (match('=')) return makeToken(TOKEN_EQUAL_EQUAL);
            if (match('>')) return makeToken(TOKEN_FAT_ARROW);
            return makeToken(TOKEN_EQUAL);
        case '<': return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>': return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);

        case '|':
            if (match('>')) return makeToken(TOKEN_PIPE);
            return errorToken("Unexpected character '|'. Did you mean '|>'?");

        case '.':
            if (match('.')) {
                return makeToken(match('.') ? TOKEN_DOT_DOT_DOT : TOKEN_DOT_DOT);
            }
            return makeToken(TOKEN_DOT);

        case '"':  return string('"');
        case '\'': return string('\'');
    }

    return errorToken("Unexpected character.");
}

/* ── Human-readable token type name ──────────────────────── */

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TOKEN_LEFT_PAREN:    return "(";
        case TOKEN_RIGHT_PAREN:   return ")";
        case TOKEN_LEFT_BRACE:    return "{";
        case TOKEN_RIGHT_BRACE:   return "}";
        case TOKEN_LEFT_BRACKET:  return "[";
        case TOKEN_RIGHT_BRACKET: return "]";
        case TOKEN_COMMA:         return ",";
        case TOKEN_DOT:           return ".";
        case TOKEN_COLON:         return ":";
        case TOKEN_SEMICOLON:     return ";";
        case TOKEN_PLUS:          return "+";
        case TOKEN_MINUS:         return "-";
        case TOKEN_STAR:          return "*";
        case TOKEN_SLASH:         return "/";
        case TOKEN_PERCENT:       return "%";
        case TOKEN_STAR_STAR:     return "**";
        case TOKEN_PLUS_EQUAL:    return "+=";
        case TOKEN_MINUS_EQUAL:   return "-=";
        case TOKEN_STAR_EQUAL:    return "*=";
        case TOKEN_SLASH_EQUAL:   return "/=";
        case TOKEN_EQUAL:         return "=";
        case TOKEN_EQUAL_EQUAL:   return "==";
        case TOKEN_BANG_EQUAL:    return "!=";
        case TOKEN_GREATER:       return ">";
        case TOKEN_GREATER_EQUAL: return ">=";
        case TOKEN_LESS:          return "<";
        case TOKEN_LESS_EQUAL:    return "<=";
        case TOKEN_ARROW:         return "->";
        case TOKEN_FAT_ARROW:     return "=>";
        case TOKEN_PIPE:          return "|>";
        case TOKEN_QUESTION:      return "?";
        case TOKEN_DOT_DOT:       return "..";
        case TOKEN_DOT_DOT_DOT:   return "...";
        case TOKEN_HASH:          return "#";
        case TOKEN_IDENTIFIER:    return "identifier";
        case TOKEN_STRING:        return "string";
        case TOKEN_INT:           return "integer";
        case TOKEN_FLOAT:         return "float";
        case TOKEN_LET:           return "let";
        case TOKEN_MUT:           return "mut";
        case TOKEN_FN:            return "fn";
        case TOKEN_RETURN:        return "return";
        case TOKEN_IF:            return "if";
        case TOKEN_ELIF:          return "elif";
        case TOKEN_ELSE:          return "else";
        case TOKEN_FOR:           return "for";
        case TOKEN_WHILE:         return "while";
        case TOKEN_IN:            return "in";
        case TOKEN_BREAK:         return "break";
        case TOKEN_CONTINUE:      return "continue";
        case TOKEN_MATCH:         return "match";
        case TOKEN_STRUCT:        return "struct";
        case TOKEN_IMPL:          return "impl";
        case TOKEN_USE:           return "use";
        case TOKEN_SELF:          return "self";
        case TOKEN_TRUE:          return "true";
        case TOKEN_FALSE:         return "false";
        case TOKEN_NONE:          return "none";
        case TOKEN_AND:           return "and";
        case TOKEN_OR:            return "or";
        case TOKEN_NOT:           return "not";
        case TOKEN_IS:            return "is";
        case TOKEN_PRINT:         return "print";
        case TOKEN_SPAWN:         return "spawn";
        case TOKEN_ASYNC:         return "async";
        case TOKEN_AWAIT:         return "await";
        case TOKEN_ERROR:         return "error";
        case TOKEN_EOF:           return "EOF";
        case TOKEN_NEWLINE:       return "newline";
        default:                  return "unknown";
    }
}
