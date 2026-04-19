# ═══════════════════════════════════════════════════════════
#  Astra Programming Language — Makefile
# ═══════════════════════════════════════════════════════════

CC       = clang
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -Wno-unused-parameter
LDFLAGS  = -lm

# Source files
SRCS     = src/main.c      \
           src/chunk.c      \
           src/compiler.c   \
           src/debug.c      \
           src/memory.c     \
           src/object.c     \
           src/scanner.c    \
           src/table.c      \
           src/value.c      \
           src/vm.c         \
           src/stdlib/core.c

OBJS     = $(SRCS:.c=.o)
TARGET   = astra

# ── Build modes ──────────────────────────────────────────

.PHONY: all clean debug release test install help

all: release

release: CFLAGS += -O2 -DNDEBUG
release: $(TARGET)
	@echo ""
	@echo "  \033[1;36m✦ Astra\033[0m built successfully! (release)"
	@echo "  Run: \033[1m./astra\033[0m"
	@echo ""

debug: CFLAGS += -g -O0 -DDEBUG_PRINT_CODE -DDEBUG_TRACE_EXECUTION
debug: $(TARGET)
	@echo ""
	@echo "  \033[1;33m⚙ Astra\033[0m built in debug mode."
	@echo ""

# ── Compilation ──────────────────────────────────────────

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Testing ──────────────────────────────────────────────

test: release
	@echo ""
	@echo "  \033[1;35m▶ Running tests...\033[0m"
	@echo ""
	@passed=0; failed=0; total=0; \
	for f in tests/*.astra; do \
		if [ -f "$$f" ]; then \
			total=$$((total + 1)); \
			name=$$(basename $$f .astra); \
			expected="tests/$${name}.expected"; \
			if [ -f "$$expected" ]; then \
				output=$$(./astra "$$f" 2>&1); \
				expected_content=$$(cat "$$expected"); \
				if [ "$$output" = "$$expected_content" ]; then \
					echo "    \033[32m✓\033[0m $$name"; \
					passed=$$((passed + 1)); \
				else \
					echo "    \033[31m✗\033[0m $$name"; \
					echo "      Expected: $$expected_content"; \
					echo "      Got:      $$output"; \
					failed=$$((failed + 1)); \
				fi; \
			else \
				output=$$(./astra "$$f" 2>&1); \
				exit_code=$$?; \
				if [ $$exit_code -eq 0 ]; then \
					echo "    \033[32m✓\033[0m $$name"; \
					passed=$$((passed + 1)); \
				else \
					echo "    \033[31m✗\033[0m $$name (exit code: $$exit_code)"; \
					echo "      $$output"; \
					failed=$$((failed + 1)); \
				fi; \
			fi; \
		fi; \
	done; \
	echo ""; \
	echo "  Results: $$passed passed, $$failed failed, $$total total"; \
	echo ""

# ── Cleaning ─────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  Cleaned."

# ── Installation ─────────────────────────────────────────

install: release
	cp $(TARGET) /usr/local/bin/astra
	@echo "  \033[1;32m✦ Astra\033[0m installed to /usr/local/bin/astra"

# ── Help ─────────────────────────────────────────────────

help:
	@echo ""
	@echo "  \033[1;36mAstra Build System\033[0m"
	@echo ""
	@echo "  make           Build in release mode"
	@echo "  make debug     Build with debug output"
	@echo "  make test      Run the test suite"
	@echo "  make clean     Remove build artifacts"
	@echo "  make install   Install to /usr/local/bin"
	@echo ""
