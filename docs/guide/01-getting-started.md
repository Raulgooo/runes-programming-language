# Getting Started

## What the compiler does

A Runes source file ends in `.runes`. The compiler reads it, checks names and
types, generates C, and asks the host C compiler to create a native executable.
The `runec` script performs those steps for you.

You need Bash, Make, and GCC or Clang.

```bash
cd /path/to/runes
make
```

This creates two useful commands in the repository:

- `./runes`: the lower-level compiler frontend and C emitter;
- `./runec`: the normal check, build, and run driver.

## Your first program

Create `hello.runes`:

```runes
f main() {
    print("hello world")
}
```

`main` is where an executable begins. For v0.1, it must be a root-level
`f main()` with no parameters and no return value.

## Check, run, and build

Check without creating an executable:

```bash
./runec check hello.runes
```

Compile and immediately run it:

```bash
./runec run hello.runes
```

Build a reusable executable:

```bash
./runec build hello.runes -o build/hello
./build/hello
```

Inspect the generated C when debugging the compiler or an ABI problem:

```bash
./runec emit-c hello.runes -o build/hello.c
```

`runec` rebuilds the Runes compiler automatically when its C sources are newer
than the compiler binary. It links generated programs with the Runes runtime
and `-lm`.

## Printing

`print` is a compiler builtin. It writes every argument in order and then
writes one newline:

```runes
f main() {
    i32 answer = 42
    print("answer: ", answer)
}
```

Output:

```text
answer: 42
```

`print` does **not** insert spaces, commas, or other separators. These calls
produce different output:

```runes
print("hello", "Raul")   -- helloRaul
print("hello, ", "Raul") -- hello, Raul
```

It can directly print primitive values and raw pointers. Collections and
user-defined values need formatting code supplied by a library or application.

## Statements and whitespace

A statement usually ends at a newline:

```runes
i32 left = 20
i32 right = 22
print(left + right)
```

A semicolon is also accepted:

```runes
i32 left = 20; i32 right = 22
```

Newlines inside parentheses, brackets, or an unfinished expression do not end
the statement:

```runes
i32 answer = add(
    20,
    22
)
```

Comments use `--`:

```runes
-- one-line comment

---
block comment
over several lines
---
```

Identifiers may contain ASCII letters, digits, and `_`, but cannot begin with
a digit. Keywords such as `f`, `type`, and `if` cannot be identifiers.

## The automatic prelude

Normal `runec` commands automatically load `src/std/prelude.runes`. The prelude
declares compiler/runtime functions such as allocation and string primitives.
It is not a general standard library and does not add collections, files, or
networking.

Freestanding or kernel code can disable it:

```bash
./runec check --no-prelude kernel.runes
```

Such code must declare every foreign/runtime contract it uses.

## Passing arguments

For `run`, arguments after `--` go to the compiled program:

```bash
./runec run app.runes -- first second
```

For `build`, arguments after `--` are additional host compiler/linker flags:

```bash
./runec build app.runes -o build/app -- -lpthread
```

Accessing program arguments from Runes requires platform or standard-library
support; v0.1 has no compiler builtin for them.

## Projects

Single files are useful while learning. A real project uses `runes.toml`:

```toml
[project]
name = "hello"
entry = "src/main.runes"

[modules]
roots = ["src"]
```

Inside that directory, filenames become optional:

```bash
runec project
runec check
runec run
runec build -o build/hello
```

Chapter 5 explains files, modules, dependencies, and `pub`.

## Common first errors

**`main must not declare parameters`**

Use exactly `f main() { ... }` for the process entry point.

**`undefined identifier`**

The name is misspelled, outside its scope, private in another module, or its
module was not declared/imported.

**`Variable initializer does not match declared type`**

Runes does not silently convert between most numeric or pointer types. Use the
right literal/type or an explicit checked design with `as`.

**`Foreign function call requires an unsafe block`**

Raw C/OS calls are unsafe by default. Do not add `unsafe` blindly; understand
the function's pointer, lifetime, and error contract first. Chapter 7 explains
safe wrappers.

[Next: Values, types, and variables](02-values-and-types.md)
