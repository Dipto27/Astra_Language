# ✦ Astra Programming Language

> *"Power without complexity. Safety without ceremony. Speed without sacrifice."*

**Astra** is a modern, production-level programming language designed to be simpler than Python, faster than JavaScript, and safer than C++. It combines the best ideas from Python, Go, Rust, and TypeScript while discarding their weaknesses.

---

## 🚀 Quick Start

### Build
```bash
make
```

### Run a Script
```bash
./astra examples/hello.astra
```

### Interactive REPL
```bash
./astra
```

---

## 📖 Language at a Glance

```
-- Variables (immutable by default)
let name = "Astra"
mut counter = 0

-- Functions
fn greet(name) {
    return "Hello, " + name + "!"
}

-- Lambdas
let square = fn(x) => x * x

-- Control flow
if score >= 90 {
    println("A")
} elif score >= 80 {
    println("B")
} else {
    println("C")
}

-- For loops
for item in [1, 2, 3] {
    println(item)
}

-- Match (pattern matching)
match day {
    1 => println("Monday")
    2 => println("Tuesday")
    _ => println("Other")
}

-- Structs & methods
struct Point {
    x: int
    y: int
}

impl Point {
    fn init(self, x, y) {
        self.x = x
        self.y = y
    }

    fn distance(self, other) {
        let dx = self.x - other.x
        let dy = self.y - other.y
        return sqrt(float(dx * dx + dy * dy))
    }
}

-- Pipe operator
let result = 5 |> double |> add_one |> square

-- Closures
fn make_counter() {
    mut count = 0
    return fn() {
        count += 1
        return count
    }
}

-- Lists & maps
let nums = [1, 2, 3, 4, 5]
let config = {"host": "localhost", "port": 8080}

-- String methods
"hello".upper()       -- "HELLO"
"WORLD".lower()       -- "world"
"hello world".split(" ")  -- ["hello", "world"]
```

---

## ✨ Features

| Feature | Description |
|:---|:---|
| **Simple syntax** | Python-like readability with curly braces for clarity |
| **Immutable by default** | `let` for constants, `mut` for mutable |
| **First-class functions** | Functions are values. Closures capture state. |
| **Pattern matching** | Powerful `match` expression |
| **Pipe operator** | `x \|> f \|> g` for readable data pipelines |
| **Structs & methods** | Lightweight OOP with `struct` + `impl` |
| **Lists & maps** | Built-in collection types |
| **No null** | `none` replaces null with explicit handling |
| **Rich stdlib** | I/O, math, collections, time, file operations |
| **Beautiful errors** | Colored, context-rich error messages |
| **Fast** | Bytecode-compiled VM written in C |
| **GC** | Mark-and-sweep garbage collector |

---

## 📁 Project Structure

```
Astra_MY_Language/
├── Makefile            Build system
├── README.md           This file
├── src/
│   ├── main.c          Entry point, REPL, CLI
│   ├── common.h        Shared definitions
│   ├── scanner.h/c     Lexer (tokenizer)
│   ├── compiler.h/c    Parser + bytecode compiler
│   ├── chunk.h/c       Bytecode chunks
│   ├── vm.h/c          Virtual machine
│   ├── value.h/c       Value representation
│   ├── object.h/c      Heap-allocated objects
│   ├── table.h/c       Hash table
│   ├── memory.h/c      Memory management + GC
│   ├── debug.h/c       Disassembler
│   └── stdlib/
│       └── core.c      Standard library functions
├── tests/              Test suite
├── examples/           Example programs
└── docs/               Documentation
```

---

## 🛠 Building

### Requirements
- **C compiler**: Clang (default) or GCC
- **Make**

### Build Commands
```bash
make            # Release build (optimized)
make debug      # Debug build (with tracing)
make test       # Run test suite
make clean      # Clean build artifacts
make install    # Install to /usr/local/bin
```

---

## 📚 Documentation
- [Getting Started](docs/getting_started.md)
- [Language Reference](docs/language_reference.md)
- [Standard Library](docs/standard_library.md)

---

## 🎯 Design Philosophy

1. **Simplicity first** — If Python can do it in one line, Astra should too
2. **Safety by default** — Immutable variables, no null, clear error messages
3. **Performance matters** — Bytecode VM with garbage collection
4. **Batteries included** — Rich standard library out of the box
5. **Joy of programming** — Beautiful syntax, beautiful errors

---

## 📜 License

MIT License — See LICENSE file for details.

---

*Built with ❤️ for the joy of programming.*
