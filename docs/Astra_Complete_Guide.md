# 🚀 The Complete Astra Programming Guide
### From Beginner to Advanced

Welcome to **Astra**, a fast, dynamic, strongly-typed programming language compiled to bytecode running on a high-performance Virtual Machine. Astra brings the ease-of-use of Python, the speed of C, and modern features from languages like Rust and Swift into one unified ecosystem.

This guide will take you from absolute scratch to writing advanced algorithms.

---

## 📖 Table of Contents
1. [Getting Started & Installation](#1-getting-started)
2. [Variables & Data Types](#2-variables--data-types)
3. [Operators & Math](#3-operators--math)
4. [Control Flow & Match](#4-control-flow)
5. [Loops & Advanced Itertools](#5-loops--itertools)
6. [Data Structures (Lists & Maps)](#6-data-structures)
7. [Functions, Lambdas & The Pipe Operator](#7-functions--closures)
8. [Object-Oriented Astra (Structs & Impl)](#8-structs--oop)
9. [Advanced: The Master Standard Library](#9-master-standard-library)
10. [File Systems (I/O) & Time](#10-file-io-and-time)
11. [Virtual Machine Architecture & GC](#11-virtual-machine-architecture)
12. [Master Example: Graph Algorithms](#12-master-example)

---

## 1. Getting Started

### The REPL (Live Coding)
To write code interactively line-by-line, open your terminal and simply type:
```bash
astra
```

### Running Scripts
To run full programs, create a file named `main.astra` and execute it from the terminal:
```bash
astra main.astra
```

### Comments
Comments are ignored by the compiler.
```swift
-- This is a single line comment

{- 
   This is a block comment
   It can span multiple lines!
-}
```

---

## 2. Variables & Data Types

Astra has a strict mutability model to prevent bugs. Variables are defined using `let` (constants) or `mut` (variables you want to change later).

```swift
let name = "Developer"    -- A string that cannot be changed
mut age = 20              -- An integer that CAN be changed

-- Mutating the variable
age = 21                  
age += 5                  -- age is now 26
```

### Core Built-in Types:
* `int`: Whole numbers (`42`, `-7`)
* `float`: Decimals (`3.14`, `-0.99`)
* `bool`: True or False (`true`, `false`)
* `str`: Strings (`"Hello"`, `'Astra'`)
* `none`: Represents nothingness/null

---

## 3. Operators & Math

Astra supports all foundational logic operators out of the box.

```swift
-- Math
let sum = 10 + 5
let div = 10 / 3          -- Always outputs floats (3.333)
let mod = 10 % 3          -- 1
let power = 2 ** 10       -- 1024

-- Logic 
let logic = (10 > 5) and (not false)
let or_logic = (1 == 2) or (3 == 3)
```

---

## 4. Control Flow

### If / Elif / Else
No parentheses are needed around conditions.
```swift
let weather = "rain"

if weather == "rain" {
    println("Get the umbrella")
} elif weather == "snow" {
    println("Get the coat")
} else {
    println("Go outside!")
}
```

### Match Statements (Advanced Switch)
For checking multiple states cleanly, use `match`:
```swift
let code = 404

match code {
    200 => println("Success")
    404 => println("Not Found")
    500 => {
        println("Server Error")
        println("Rebooting")
    }
    _ => println("Unknown")    -- '_' is the default fallback!
}
```

---

## 5. Loops & Itertools

### While Loops
```swift
mut count = 0
while count < 3 {
    println(count)
    count += 1
}
```

### For-In Loops
The `for` loop dynamically iterates over any collection!

```swift
-- Looping over a Range
for i in range(5) { println(i) }

-- Iterating with Index (Enumerate)
for item in enumerate(["Apple", "Banana"]) {
    -- item is an array: [0, "Apple"]
    println("Index: " + str(item[0]) + " " + item[1])
}

-- Zipping two arrays
for pair in zip([1, 2], ["A", "B"]) {
    -- pair is [1, "A"]
}
```

---

## 6. Data Structures

### Lists (Dynamic Arrays)
Lists can shrink and grow dynamically in memory.
```swift
let nums = [10, 20, 30]

nums.push(40)         -- adds 40 to the end
let last = nums.pop() -- removes and returns 40

println(nums[0])      -- 10
println(nums[-1])     -- 30 (Negative indexing grabs from the back)

-- Array Concatenation
let big = [1] + [2, 3] -- [1, 2, 3]
```

### Maps (Dictionaries / Hash Maps)
Maps are perfect for associating Keys with Values.
```swift
let user = {
    "name": "Alex",
    "level": 99
}

println(user["name"])    -- "Alex"
user["weapon"] = "Sword" -- Dynamically registers new fields!
```

---

## 7. Functions & Closures

### Defining Functions
```swift
fn greet(name) {
    return "Hello " + name
}
println(greet("Astra"))
```

### Lambdas (Arrow Functions)
For extreme speed and inline coding, you can use arrow syntax:
```swift
let square = fn(x) => x * x
```

### The Pipe Operator `|>` (Ultimate Power)
If you have data that needs to be passed sequentially through functions, you don't need to wrap them: `foo(bar(baz(data)))`. You can pipe them gracefully:
```swift
fn double(x) { return x * 2 }
fn square(x) { return x * x }

let result = 5 |> double |> square 
println(result) -- 100
```

---

## 8. Structs & OOP

Astra fully supports Object-Oriented design without relying on confusing classes. Use `struct` to define data, and `impl` to define bound methods.

```swift
-- 1. Struct defines the shape in memory
struct Vector {
    x: float
    y: float
}

-- 2. Impl binds functions to the struct
impl Vector {
    -- The init function builds it!
    fn init(self, x, y) {
        self.x = x
        self.y = y
    }

    -- Standard bound method
    fn magnitude(self) {
        return sqrt((self.x * self.x) + (self.y * self.y))
    }
}

-- 3. Instantiation and execution
let v = Vector(3.0, 4.0)
println(v.magnitude())   -- 5.0
```

---

## 9. Master Standard Library

Astra has an incredibly dense and fast native 'C' standard library baked directly into the language.

### Core Utilities
```swift
println("Auto adds newline")
print_raw("No newline")
let user_input = input("Enter name: ")
exit(0) -- Terminates the program
```

### Type Checking & Conversion
```swift
let x = int("42")       -- Casts to int
let y = float("3.14")   -- Casts to float
let b = bool("")        -- Evaluates truthiness (false)
let z = str(42)         -- "42"

let type_name = type(10)  -- "int"
```

### Strings & Arrays (Iterables)
```swift
let text = "a,b,c"
let arr = text.split(",")      -- ["a", "b", "c"]
let joined = join(arr, "-")    -- "a-b-c"

let big = sorted([3, 1, 2])    -- [1, 2, 3]
let rev = reversed([1, 2, 3])  -- [3, 2, 1]
let num = sum([10, 20])        -- 30
let length = len(arr)          -- 3
```

### The Math Library
Astra includes deep floating-point mathematical calculation via native bindings:
```swift
println(sqrt(16.0))    -- 4.0
println(abs(-10))      -- 10
println(min([1, 5]))   -- 1
println(floor(3.9))    -- 3.0
println(round(3.5))    -- 4.0
println(random())      -- Returns a random float between 0 and 1
println(sin(0))        
println(log(10))
```

---

## 10. File I/O and Time

Astra gives you full access to the underlying Operating System's file handlers and clocks.

### Reading and Writing Data
You can easily build databases, loggers, or scrapers:
```swift
if file_exists("data.txt") {
    let content = read_file("data.txt")
    println(content)
} else {
    write_file("data.txt", "Initial Data")
}

append_file("data.txt", "\nNew Line")
```

### Sleep and Timing
Great for timing algorithmic speeds or writing server delays!
```swift
let start = clock()

sleep(2)  -- Pauses script for 2 seconds

let end = clock()
println("Execution took: " + str(end - start) + " seconds")
```

---

## 11. Virtual Machine Architecture

When you write Astra code, it is not executed slowly like bash scripts. The compiler utilizes a fast **Single-Pass Pratt Parsing algorithm**, turning your text directly into C Bytecode.

### Garbage Collection (Tri-Color Mark & Sweep)
You do not need to `free()` memory in Astra! Your Virtual Machine uses an advanced Tri-Color Mark-and-Sweep Garbage Collector. It traces references starting from the VM stack, flags active "reachable" Heap Objects, and effortlessly deletes "dark" unreferenced nodes—preventing any memory leaks, completely invisibly.

---

## 12. Master Example: Solving Graph Algorithms

To tie it all together, here is an advanced Script utilizing Maps, Lists, Booleans, and Recursive Functions.

```swift
let graph = {
    "A": ["B", "C"],
    "B": ["D"],
    "C": ["E"],
    "D": [],
    "E": []
}

mut visited = {}

fn dfs(node, map) {
    if visited[node] == true { return none }
    
    println("Traversing: " + node)
    visited[node] = true
    
    for neighbor in map[node] {
        dfs(neighbor, map)
    }
}

dfs("A", graph)
```

**Congratulations! You are now a master of the Astra programming language.** Use this document as your primary desk reference. Happy coding! 🚀
