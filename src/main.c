/*
 * Astra Programming Language
 * main.c — Entry point: REPL, file execution, and CLI
 *
 * Usage:
 *   astra              — Launch interactive REPL
 *   astra <file.astra> — Run a script file
 *   astra --version    — Show version
 *   astra --help       — Show help
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vm.h"

/* ── ANSI color codes ────────────────────────────────────── */

#define C_RESET   "\033[0m"
#define C_BOLD    "\033[1m"
#define C_DIM     "\033[90m"
#define C_RED     "\033[1;31m"
#define C_GREEN   "\033[1;32m"
#define C_YELLOW  "\033[1;33m"
#define C_BLUE    "\033[1;34m"
#define C_MAGENTA "\033[1;35m"
#define C_CYAN    "\033[1;36m"

/* ── Read a file into a string ───────────────────────────── */

static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s✖ Error:%s Could not open file \"%s\".\n",
                C_RED, C_RESET, path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "%s✖ Error:%s Not enough memory to read \"%s\".\n",
                C_RED, C_RESET, path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "%s✖ Error:%s Could not read file \"%s\".\n",
                C_RED, C_RESET, path);
        exit(74);
    }

    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

/* ── Run a script file ───────────────────────────────────── */

static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

/* ── Interactive REPL ────────────────────────────────────── */

static void repl(void) {
    printf("\n");
    printf("  %s✦ Astra %s%s — The language that just works.%s\n",
           C_CYAN, ASTRA_VERSION_STRING, C_RESET C_DIM, C_RESET);
    printf("  %sType 'exit()' to quit, 'help()' for help.%s\n\n",
           C_DIM, C_RESET);

    char line[4096];

    for (;;) {
        printf("  %s⟩%s ", C_CYAN, C_RESET);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* Strip trailing whitespace */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' ||
               line[len-1] == ' ' || line[len-1] == '\t')) {
            line[--len] = '\0';
        }

        /* Skip empty lines */
        if (len == 0) continue;

        /* Handle special REPL commands */
        if (strcmp(line, "exit()") == 0 || strcmp(line, "quit()") == 0) {
            printf("\n  %sGoodbye! ✦%s\n\n", C_DIM, C_RESET);
            break;
        }

        if (strcmp(line, "help()") == 0) {
            printf("\n");
            printf("  %s╭──────────────────────────────────────────╮%s\n", C_DIM, C_RESET);
            printf("  %s│%s  %sAstra Quick Reference%s                    %s│%s\n", C_DIM, C_RESET, C_BOLD, C_RESET, C_DIM, C_RESET);
            printf("  %s├──────────────────────────────────────────┤%s\n", C_DIM, C_RESET);
            printf("  %s│%s  %slet%s x = 42        %s-- immutable variable%s %s│%s\n", C_DIM, C_RESET, C_CYAN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %smut%s y = 0         %s-- mutable variable%s   %s│%s\n", C_DIM, C_RESET, C_CYAN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %sfn%s add(a, b) {}   %s-- function%s           %s│%s\n", C_DIM, C_RESET, C_CYAN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %sprintln%s(\"hello\")  %s-- print + newline%s    %s│%s\n", C_DIM, C_RESET, C_GREEN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %sif%s cond {}        %s-- conditional%s        %s│%s\n", C_DIM, C_RESET, C_CYAN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %sfor%s x %sin%s list {}  %s-- for loop%s           %s│%s\n", C_DIM, C_RESET, C_CYAN, C_RESET, C_CYAN, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s│%s  %sexit()%s             %s-- quit REPL%s          %s│%s\n", C_DIM, C_RESET, C_YELLOW, C_RESET, C_DIM, C_RESET, C_DIM, C_RESET);
            printf("  %s╰──────────────────────────────────────────╯%s\n", C_DIM, C_RESET);
            printf("\n");
            continue;
        }

        /* Check if this might be a multi-line input (ends with {) */
        if (len > 0 && line[len-1] == '{') {
            /* Read until matching closing brace */
            int braceCount = 0;
            for (size_t i = 0; i < len; i++) {
                if (line[i] == '{') braceCount++;
                if (line[i] == '}') braceCount--;
            }

            while (braceCount > 0) {
                printf("  %s·%s ", C_DIM, C_RESET);
                size_t current = strlen(line);
                if (!fgets(line + current, sizeof(line) - current, stdin)) {
                    break;
                }

                size_t newLen = strlen(line);
                for (size_t i = current; i < newLen; i++) {
                    if (line[i] == '{') braceCount++;
                    if (line[i] == '}') braceCount--;
                }
            }
        }

        interpret(line);
    }
}

/* ── Version info ────────────────────────────────────────── */

static void showVersion(void) {
    printf("\n  %s✦ Astra%s v%s\n", C_CYAN, C_RESET,
           ASTRA_VERSION_STRING);
    printf("  %sA simple, powerful, and scalable programming language.%s\n",
           C_DIM, C_RESET);
    printf("  %sCopyright (c) 2026 Astra Contributors%s\n\n",
           C_DIM, C_RESET);
}

/* ── Help ────────────────────────────────────────────────── */

static void showHelp(void) {
    printf("\n  %s✦ Astra%s — The language that just works.\n\n",
           C_CYAN, C_RESET);
    printf("  %sUsage:%s\n", C_BOLD, C_RESET);
    printf("    astra                    %sLaunch interactive REPL%s\n",
           C_DIM, C_RESET);
    printf("    astra <file.astra>       %sRun a script file%s\n",
           C_DIM, C_RESET);
    printf("    astra --version, -v      %sShow version info%s\n",
           C_DIM, C_RESET);
    printf("    astra --help, -h         %sShow this help%s\n\n",
           C_DIM, C_RESET);

    printf("  %sExamples:%s\n", C_BOLD, C_RESET);
    printf("    astra hello.astra       %sRun hello.astra%s\n",
           C_DIM, C_RESET);
    printf("    astra examples/fib.astra %sRun fibonacci example%s\n\n",
           C_DIM, C_RESET);

    printf("  %sDocumentation:%s\n", C_BOLD, C_RESET);
    printf("    See docs/getting_started.md\n\n");
}

/* ── Main ────────────────────────────────────────────────── */

int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        repl();
    } else if (argc == 2) {
        if (strcmp(argv[1], "--version") == 0 ||
            strcmp(argv[1], "-v") == 0) {
            showVersion();
        } else if (strcmp(argv[1], "--help") == 0 ||
                   strcmp(argv[1], "-h") == 0) {
            showHelp();
        } else {
            runFile(argv[1]);
        }
    } else {
        fprintf(stderr, "Usage: astra [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}
