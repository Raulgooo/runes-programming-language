# Functions, Control Flow, and Errors

## Functions

A function groups reusable behavior. Parameters are named inputs:

```runes
f greet(name: str) {
    print("hello, ", name)
}

f main() {
    greet("Raul")
}
```

`f` is the normal stack function form. Other function forms select memory
realms and are explained in chapter 6.

Functions may be called before their declaration. Names are resolved for the
whole containing scope rather than only from top to bottom.

## Returning a value

Runes uses a named return variable:

```runes
f add(left: i32, right: i32) = result: i32 {
    result = left + right
}
```

Read `= result: i32` as “this function returns an `i32`, stored in the local
name `result`.” A value-returning function must assign its named result on every
path that reaches the end.

You can return early after assigning it:

```runes
f absolute(value: i32) = result: i32 {
    if value < 0 {
        result = -value
        return
    }
    result = value
}
```

A function with no result simply omits the return clause:

```runes
f log_ready() {
    print("ready")
}
```

`return` does not carry an expression. Assign the named result first.

## `main`

The executable entry point is exactly:

```runes
f main() {
    -- program starts here
}
```

Root `main` cannot have parameters, generic parameters, or a return value. A
function named `main` inside a module or another function is ordinary and does
not become the process entry point.

## Conditions

```runes
if temperature > 30 {
    print("hot")
} else if temperature < 10 {
    print("cold")
} else {
    print("mild")
}
```

Conditions must be `bool`; integers are not automatically truthy or falsey.

An `if` can produce a value when every path produces a compatible value:

```runes
i32 magnitude = if value < 0 { -value } else { value }
```

Value-producing `if` requires an `else` because a value must always exist.

## Loops

Condition-controlled loop:

```runes
while count > 0 {
    count = count - 1
}
```

Unconditional loop:

```runes
loop {
    if done {
        break
    }
}
```

`break` exits the nearest loop. `continue` starts its next iteration. Both are
errors outside a loop.

Range loop:

```runes
for (0..10) |index| {
    print(index)
}
```

`0..10` excludes `10`; `0..=10` includes it. Range bounds must be integers.

Array or slice iteration:

```runes
for (values) |value| {
    print(value)
}

for (values) |value, index| {
    print(index, ": ", value)
}
```

## Scopes

Braces create a scope. A name declared inside is unavailable after the closing
brace:

```runes
if ready {
    i32 temporary = 42
    print(temporary)
}
-- temporary no longer exists here
```

Scope also matters for stack lifetime, closure captures, arena cleanup, and GC
roots. The compiler rejects references that would outlive their storage.

## Error sets

An error set names a closed group of failures:

```runes
error ParseError = {
    | Empty
    | InvalidDigit
    | Overflow
}
```

Error sets are **nominal**: two sets remain different types even when their
variant names happen to match.

## Fallible functions

`!T` means “either a `T` value or an error”:

```runes
f parse_count(text: str) = result: !i32 {
    if text.len == 0 {
        result = error.ParseError.Empty
    } else {
        result = 42
    }
}
```

`!void` represents an operation that returns no useful success value but can
still fail.

## Propagating with `try`

Use `try` when the current function should return the same failure to its
caller:

```runes
f load_count(text: str) = result: !i32 {
    i32 value = try parse_count(text)
    result = value
}
```

On success, `try` unwraps the value. On failure, it assigns the error to the
current fallible result and exits after required realm cleanup. Therefore
`try` is only valid inside a fallible function.

## Recovering with `catch`

Use a fallback value:

```runes
i32 value = parse_count(text) catch 0
```

Or bind the error and execute a block:

```runes
i32 value = parse_count(text) catch |problem| {
    print("parse failed: ", problem)
    0
}
```

The handler must produce a value compatible with the successful result when
the entire `catch` expression is used as a value.

## Matching a result

Fallible values can also be matched explicitly:

```runes
i32 value = match parse_count(text) {
    Ok(number) -> number,
    Err(_) -> 0,
}
```

Use `try` for propagation, `catch` for local recovery, and `match` when success
and failure need equally explicit branches.

## Function values

A function can itself be a value:

```runes
f double(value: i32) = result: i32 {
    result = value * 2
}

f apply(operation: f(i32) -> i32, value: i32) = result: i32 {
    result = operation(value)
}

f main() {
    print(apply(double, 21))
}
```

Realm-qualified function types such as `[regional] f(i32) -> i32` record which
kind of function may be stored. This matters because callers must obey the
realm call rules in chapter 6.

## Common mistakes

- Writing `f value() = i32`; the required form is `= result: i32`.
- Writing `return expression`; assign the named result, then use bare `return`.
- Using an integer as an `if` condition.
- Using `try` in a non-fallible function.
- Forgetting that `0..end` excludes `end`.
- Returning a pointer, slice, interface, or closure that borrows a local value.

[Next: Data modeling](04-data-modeling.md)
