# Writing and Running Runes Programs

This is the short setup path. New language users should continue with the
[chapter-based handbook](guide/README.md), which explains syntax, projects,
memory realms, FFI, and limitations without assuming systems-language
experience.

## 1. Build the compiler

Runes needs GCC and Make.

```bash
make
```

The repository provides two commands:

- `./runes` is the compiler frontend and C emitter.
- `./runec` is the convenient check, build, and run driver.

## 2. Write a program

Create `hello.runes`:

```runes
f add(a: i32, b: i32) = result: i32 {
    result = a + b
}

f main() {
    i32 answer = add(20, 22)
    print("answer:", answer)
}
```

Functions returning a value use a named result. A void function, including
`main`, omits the return clause.

## 3. Check, run, or build it

```bash
./runec check hello.runes
./runec run hello.runes
./runec build hello.runes -o build/hello
./build/hello
```

`runec` rebuilds the compiler when its C sources are newer than the binary.
Generated files go in `build/` unless `-o` is provided.

To inspect the generated C:

```bash
./runec emit-c hello.runes -o build/hello.c
```

## 4. Use arrays and pointers

Runes v0.1 has fixed arrays, non-owning slices, and raw pointers. Owning dynamic
containers remain user-library types.

```runes
f sum(values: [4]i32) = result: i32 {
    result = 0
    for (values) |value| {
        result = result + value
    }
}

f main() {
    [4]i32 values = [10, 20, 30, 40]
    *i32 first = &values[0]
    *first = 12
    print(sum(values))
}
```

## 5. Start a project

Create `runes.toml` beside the project source directory:

```toml
[project]
name = "hello"
entry = "src/main.runes"

[modules]
roots = ["src"]
```

Then commands discover the entry automatically:

```bash
./runec project
./runec check
./runec run
```

Normal `runec` commands load the runtime prelude automatically. Pass
`--no-prelude` for freestanding programs. See
[projects-and-modules.md](projects-and-modules.md) for module roots, the `std`
namespace, and local path dependencies.

Multiple explicit source files can still be analyzed as one program:

```bash
./runec build types.runes server.runes -o build/server
```

For platform APIs, declare C functions with `extern` and pass required linker
flags after `--`:

```bash
./runec build app.runes -o build/app -- -lpthread
```

External modules are discovered relative to the declaring file and then from
configured module roots. `mod parser` loads exactly one of `parser.runes` or
`parser/mod.runes`; supplying both is an error.

## 6. Useful compiler commands

```bash
./runes --lex-only hello.runes
./runes --parse-only hello.runes
./runes --dump-ast hello.runes
./runes hello.runes --emit-c build/hello.c
```

Run the project verification gates with:

```bash
make test
make test-samples
make test-codegen
make test-sanitize
```

## 7. Editor highlighting and icons

VS Code support is in `runes-lang/`. For Zed, install the local Tree-sitter
language extension and Runes file-icon theme with:

```bash
make install-zed
```

Select **Runes Material Icon Theme** with Zed's icon-theme selector, then
restart Zed if it was already running. Use `make test-zed` to validate the
grammar, highlighting query, installer, and icon asset locally.
