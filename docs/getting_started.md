# Getting Started with Astra

Welcome to Astra! This guide will get you up and running in minutes.

---

## Installation

### Build from Source
```bash
# Clone the repository
cd Astra_MY_Language

# Build
make

# (Optional) Install system-wide
sudo make install
```

### Verify Installation
```bash
./astra --version
```

---

## Your First Program

Create a file called `hello.astra`:

```
println("Hello, World!")
```

Run it:
```bash
./astra hello.astra
```

---

## Variables

Astra has two kinds of variables:

```
-- Immutable (default — safe and predictable)
let name = "Astra"
let age = 1
let pi = 3.14159

-- Mutable (when you need to change values)
mut counter = 0
counter = counter + 1
counter += 1
```

> **Why immutable by default?** It prevents accidental mutations, makes code
> easier to reason about, and helps catch bugs at compile time.

---

## Data Types

```
-- Integers
let x = 42
let big = 1000000

-- Floats
let pi = 3.14159
let tiny = 0.001

-- Booleans
let yes = true
let no = false

-- Strings
let greeting = "Hello!"
let multiword = "Astra is great"

-- None (absence of value)
let nothing = none

-- Lists
let numbers = [1, 2, 3, 4, 5]
let mixed = [42, "hello", true]

-- Maps
let config = {"host": "localhost", "port": 8080}
```

---

## Functions

```
-- Basic function
fn add(a, b) {
    return a + b
}

-- Call it
println(add(3, 5))  -- 8

-- Lambda (short anonymous function)
let double = fn(x) => x * 2
println(double(21))  -- 42

-- Functions are first-class values
fn apply(f, x) {
    return f(x)
}
println(apply(double, 10))  -- 20
```

---

## Control Flow

### If / Elif / Else
```
let score = 85

if score >= 90 {
    println("A")
} elif score >= 80 {
    println("B")
} elif score >= 70 {
    println("C")
} else {
    println("F")
}
```

### For Loops
```
-- Loop over a range
for i in range(5) {
    println(i)  -- 0, 1, 2, 3, 4
}

-- Loop over a list
for fruit in ["apple", "banana", "cherry"] {
    println(fruit)
}

-- With range(start, end)
for i in range(1, 11) {
    println(i)  -- 1 through 10
}
```

### While Loops
```
mut count = 0
while count < 5 {
    println(count)
    count += 1
}
```

### Match
```
let day = "Monday"

match day {
    "Monday" => println("Start of the week")
    "Friday" => println("Almost weekend!")
    "Saturday" => println("Weekend!")
    "Sunday" => println("Rest day")
    _ => println("Regular day")
}
```

---

## Structs

```
struct Dog {
    name: str
    breed: str
    age: int
}

impl Dog {
    fn init(self, name, breed, age) {
        self.name = name
        self.breed = breed
        self.age = age
    }

    fn bark(self) {
        println(self.name + " says: Woof!")
    }

    fn info(self) {
        println(self.name + " (" + self.breed + ", " + str(self.age) + " years)")
    }
}

let dog = Dog("Buddy", "Labrador", 3)
dog.bark()
dog.info()
```

---

## Built-in Functions

| Function | Description |
|:---|:---|
| `println(value)` | Print with newline |
| `input(prompt?)` | Read user input |
| `len(value)` | Get length of string/list/map |
| `type(value)` | Get type name as string |
| `str(value)` | Convert to string |
| `int(value)` | Convert to integer |
| `float(value)` | Convert to float |
| `range(n)` | Create list [0, 1, ..., n-1] |
| `sorted(list)` | Return sorted copy |
| `reversed(list)` | Return reversed copy |
| `sum(list)` | Sum all numbers in list |
| `min(a, b)` / `max(a, b)` | Min/max values |
| `abs(n)` | Absolute value |
| `sqrt(n)` | Square root |
| `time()` | Current time in seconds |
| `random()` | Random float 0-1 |
| `read_file(path)` | Read file to string |
| `write_file(path, content)` | Write string to file |

---

## What's Next?

- Read the [Language Reference](language_reference.md) for full syntax documentation
- Check out [examples/](../examples/) for more code samples  
- Read the [Standard Library](standard_library.md) documentation

Happy coding with Astra! ✦
