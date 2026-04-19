# Astra Standard Library Reference

Complete reference for all built-in functions and modules.

---

## I/O Functions

### `println(...values)`
Print values to stdout followed by a newline. Multiple values are separated by spaces.
```
println("Hello")           -- Hello
println("a", "b", "c")    -- a b c
println(42)                -- 42
```

### `print_raw(...values)`
Print values to stdout without a trailing newline.
```
print_raw("Enter name: ")
```

### `input(prompt?)`
Read a line from stdin. Optional prompt string.
```
let name = input("Your name: ")
println("Hello, " + name)
```

---

## Type Functions

### `type(value) -> str`
Returns the type name as a string.
```
type(42)       -- "int"
type(3.14)     -- "float"
type("hello")  -- "str"
type(true)     -- "bool"
type(none)     -- "none"
type([1,2])    -- "list"
```

### `len(value) -> int`
Returns the length of a string, list, or map.
```
len("hello")      -- 5
len([1, 2, 3])    -- 3
len({"a": 1})     -- 1
```

### `str(value) -> str`
Convert any value to its string representation.

### `int(value) -> int`
Convert a value to an integer. Returns `none` on failure.
```
int("42")    -- 42
int(3.14)    -- 3
int(true)    -- 1
```

### `float(value) -> float`
Convert a value to a floating-point number. Returns `none` on failure.

### `bool(value) -> bool`
Convert a value to a boolean based on truthiness rules.

---

## Collection Functions

### `range(end)` / `range(start, end)` / `range(start, end, step)`
Create a list of integers.
```
range(5)          -- [0, 1, 2, 3, 4]
range(2, 7)       -- [2, 3, 4, 5, 6]
range(0, 10, 2)   -- [0, 2, 4, 6, 8]
range(10, 0, -1)  -- [10, 9, 8, 7, 6, 5, 4, 3, 2, 1]
```

### `sorted(list) -> list`
Return a new sorted list (ascending order).
```
sorted([3, 1, 4, 1, 5])  -- [1, 1, 3, 4, 5]
```

### `reversed(list) -> list`
Return a new list in reverse order.
```
reversed([1, 2, 3])  -- [3, 2, 1]
```

### `enumerate(list) -> list`
Return list of [index, value] pairs.
```
enumerate(["a", "b", "c"])
-- [[0, "a"], [1, "b"], [2, "c"]]
```

### `zip(list1, list2) -> list`
Combine two lists into pairs.
```
zip([1, 2, 3], ["a", "b", "c"])
-- [[1, "a"], [2, "b"], [3, "c"]]
```

### `join(list, separator?) -> str`
Join list elements into a string.
```
join(["Hello", "World"], " ")  -- "Hello World"
join(["a", "b", "c"], ",")    -- "a,b,c"
```

### `sum(list) -> number`
Sum all numbers in a list.
```
sum([1, 2, 3, 4, 5])  -- 15
```

---

## Math Functions

### Constants
```
PI   -- 3.14159265358979323846
E    -- 2.71828182845904523536
TAU  -- 6.28318530717958647692
```

### Functions
| Function | Description |
|:---|:---|
| `abs(x)` | Absolute value |
| `min(a, b)` or `min(list)` | Minimum value |
| `max(a, b)` or `max(list)` | Maximum value |
| `sqrt(x)` | Square root |
| `floor(x)` | Round down to integer |
| `ceil(x)` | Round up to integer |
| `round(x)` | Round to nearest integer |
| `log(x)` | Natural logarithm |
| `sin(x)` | Sine (radians) |
| `cos(x)` | Cosine (radians) |
| `tan(x)` | Tangent (radians) |
| `random()` | Random float in [0, 1) |

---

## Time Functions

### `time() -> float`
CPU time in seconds since program start.
```
let start = time()
-- ... work ...
let elapsed = time() - start
println("Took " + str(elapsed) + " seconds")
```

### `clock() -> float`
Wall clock time (Unix timestamp).

### `sleep(seconds)`
Pause execution for the given number of seconds.
```
sleep(1.5)  -- pause for 1.5 seconds
```

---

## Assertion & Control

### `assert(condition, message?)`
Assertion check. Prints error if condition is falsy.
```
assert(x > 0, "x must be positive")
assert(len(items) > 0)
```

### `exit(code?)`
Exit the program with an optional exit code (default: 0).
```
exit()    -- exit with code 0
exit(1)   -- exit with code 1
```

---

## File I/O

### `read_file(path) -> str | none`
Read a file's contents as a string. Returns `none` if file not found.
```
let content = read_file("data.txt")
if content != none {
    println(content)
}
```

### `write_file(path, content) -> bool`
Write a string to a file (overwrites existing content). Returns success status.
```
write_file("output.txt", "Hello, File!")
```

### `append_file(path, content) -> bool`
Append a string to a file. Creates the file if it doesn't exist.

### `file_exists(path) -> bool`
Check if a file exists.
```
if file_exists("config.txt") {
    let config = read_file("config.txt")
}
```

---

## String Methods

Called on string values using dot syntax:

| Method | Description | Example |
|:---|:---|:---|
| `.upper()` | Uppercase | `"hello".upper()` → `"HELLO"` |
| `.lower()` | Lowercase | `"HELLO".lower()` → `"hello"` |
| `.len()` | Length | `"hello".len()` → `5` |
| `.contains(s)` | Check substring | `"hello".contains("ell")` → `true` |
| `.split(delim)` | Split string | `"a,b".split(",")` → `["a", "b"]` |

## List Methods

Called on list values using dot syntax:

| Method | Description |
|:---|:---|
| `.push(val)` | Add element to end |
| `.pop()` | Remove and return last element |
| `.len()` | Get length |
| `.contains(val)` | Check if value exists |
| `.reverse()` | Reverse list in place |
