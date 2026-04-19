# Astra Language Reference

Complete syntax and semantics reference for the Astra programming language.

---

## Comments

```
-- Single line comment

{- 
   Block comment
   Can span multiple lines
   {- And can be nested -}
-}
```

## Variables

```
let x = 42        -- Immutable (cannot be reassigned)
mut y = 0         -- Mutable (can be reassigned)
y = 10            -- OK
y += 5            -- OK (compound assignment)
-- x = 10         -- ERROR: Cannot assign to immutable variable
```

### Compound Assignment
```
mut x = 10
x += 5     -- x = 15
x -= 3     -- x = 12
x *= 2     -- x = 24
x /= 4     -- x = 6.0
```

## Data Types

| Type | Example | Description |
|:---|:---|:---|
| `int` | `42`, `-7`, `1000000` | 64-bit integer |
| `float` | `3.14`, `2.5e10` | 64-bit floating point |
| `bool` | `true`, `false` | Boolean |
| `str` | `"hello"`, `'world'` | UTF-8 string |
| `none` | `none` | Absence of value |
| `list` | `[1, 2, 3]` | Dynamic array |
| `map` | `{"key": "value"}` | Hash map |
| `fn` | `fn(x) => x * 2` | Function |

## Operators

### Arithmetic
| Op | Description | Example |
|:---|:---|:---|
| `+` | Add / Concatenate | `3 + 4`, `"a" + "b"` |
| `-` | Subtract | `10 - 3` |
| `*` | Multiply / Repeat | `3 * 4`, `"ha" * 3` |
| `/` | Divide (always float) | `10 / 3` → `3.333...` |
| `%` | Modulo | `10 % 3` → `1` |
| `**` | Power | `2 ** 10` → `1024` |

### Comparison
| Op | Description |
|:---|:---|
| `==` | Equal |
| `!=` | Not equal |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

### Logical
| Op | Description |
|:---|:---|
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |

### Special
| Op | Description | Example |
|:---|:---|:---|
| `\|>` | Pipe | `5 \|> double \|> square` |
| `..` | Range/slice | `list[1..3]` |

## Functions

```
-- Named function
fn add(a, b) {
    return a + b
}

-- Lambda (arrow function)
let double = fn(x) => x * 2

-- Multi-line lambda
let process = fn(x) {
    let y = x * 2
    return y + 1
}

-- Recursive
fn factorial(n) {
    if n <= 1 { return 1 }
    return n * factorial(n - 1)
}

-- Higher-order
fn apply(f, x) { return f(x) }
```

## Control Flow

### If / Elif / Else
```
if condition {
    -- ...
} elif other_condition {
    -- ...
} else {
    -- ...
}
```

### While
```
while condition {
    -- body
}
```

### For-In
```
for item in iterable {
    -- body
}

-- With range
for i in range(10) { ... }
for i in range(1, 10) { ... }
for i in range(0, 100, 5) { ... }
```

### Match
```
match value {
    1 => println("one")
    2 => println("two")
    3 => {
        -- multi-statement case
        println("three")
    }
    _ => println("other")
}
```

### Break / Continue
```
for i in range(10) {
    if i == 5 { break }
    if i % 2 == 0 { continue }
    println(i)
}
```

## Structs

```
struct Name {
    field1: type
    field2: type
}

impl Name {
    fn init(self, field1, field2) {
        self.field1 = field1
        self.field2 = field2
    }

    fn method(self) {
        return self.field1
    }
}

let instance = Name(value1, value2)
instance.method()
```

## Lists

```
let list = [1, 2, 3, 4, 5]

-- Access
list[0]       -- first element
list[-1]      -- last element

-- Methods
list.push(6)       -- add to end
list.pop()         -- remove from end
list.len()         -- length
list.contains(3)   -- true/false
list.reverse()     -- reverse in place

-- Iteration
for item in list { ... }

-- Concatenation
[1, 2] + [3, 4]  -- [1, 2, 3, 4]
```

## Maps

```
let map = {"name": "Astra", "version": 1}

-- Access
map["name"]       -- "Astra"
map.name          -- "Astra" (dot syntax)

-- Modification
map["key"] = value

-- Iteration (coming soon)
```

## String Methods

```
"hello".upper()          -- "HELLO"
"HELLO".lower()          -- "hello"
"hello".len()            -- 5
"hello".contains("ell")  -- true
"a,b,c".split(",")      -- ["a", "b", "c"]

-- Indexing
"hello"[0]    -- "h"
"hello"[-1]   -- "o"
```

## Closures

Functions capture their enclosing scope:

```
fn make_adder(n) {
    return fn(x) => x + n
}

let add5 = make_adder(5)
println(add5(10))  -- 15
```

## Pipe Operator

Chain function calls in a readable way:

```
let result = value
    |> function1
    |> function2
    |> function3
```

This is equivalent to `function3(function2(function1(value)))`.

## Truthiness

The following values are "falsy":
- `false`
- `none`
- `0` (integer zero)
- `0.0` (float zero)
- `""` (empty string)
- `[]` (empty list)

Everything else is "truthy".
