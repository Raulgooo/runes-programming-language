<p align="center">
  <img src="assets/runes_true.svg" alt="Runes" width="200"/>
</p>

<h1 align="center">Runes</h1>

<p align="center">A systems-level programming language designed for writing operating systems, compilers, and tooling without sacrificing expressiveness.</p>

```runes
-- The memory strategy lives on the function, not globally.
-- This one uses a bump-allocator arena, freed at scope exit.
regional f setup_paging() {
    PageTable pml4 = PageTable.new()
    try pml4.map(0xFFFF800000000000, 0x0, 0x3)
}

-- This one is GC-tracked, for high-level code.
gc f run_userspace() {
    Task t = Task.spawn(shell_main)
    scheduler.add(t)
}
```

**Status:** Experimental C bootstrap compiler. The v0.1 language core and its
compiler-required runtime are implemented and under hardening. No standard
library is included. Not production-ready or self-hosted.

---

## What Runes is

Runes sits between C and Rust. You get algebraic types, pattern matching, and a proper error model. You also get explicit memory strategies per function — no borrow checker, no GC tax unless you ask for it.

The core bet: **memory strategy belongs to the function, not the data**. You pick stack, arena, heap, or GC at the callsite. The compiler checks the implemented nesting rules and supports low-level constructs such as raw pointers and inline assembly in the frontend.

---

## Build

```bash
# Requires: gcc, make
make          # builds ./runes
make test     # focused unit, core semantics, and C bootstrap tests
make test-samples # full integration inventory, including known failures
make test-codegen # emit strict C for the executable core inventory
make test-sanitize # ASan/UBSan over every integration sample
make clean    # removes binary
```

No external dependencies. Pure C, zero third-party libraries.

For a short walkthrough, see [Writing and Running Runes Programs](docs/getting-started.md).
For the full implemented-language reference, see
[Runes v0.1 Language Usage Guide](docs/language-guide.md).

---

## Usage

```bash
./runes file.runes                      # full pipeline: lex → parse → resolve → typecheck
./runes src/std/prelude.runes file.runes # with minimal allocation/runtime contracts
./runes --lex-only  file.runes          # dump token stream
./runes --parse-only file.runes         # syntax check only
./runes --dump-ast  file.runes          # print the AST
./runes file.runes --emit-c out.c        # emit C for the supported subset
```

Multiple files are merged before analysis:

```bash
./runes src/std/prelude.runes src/tests/samples/10_kernel_bootstrap.runes
```

For normal development, use the `runec` driver:

```bash
./runec check program.runes
./runec run program.runes
./runec build program.runes -o build/program
./runec emit-c program.runes -o build/program.c
```

Exit code 0 = clean. Exit code 1 = errors (printed to stderr with `file:line:col`).

---

## Language at a Glance

### Variables and types

```runes
i32 x = 5
z    := 3.14      -- type inferred: f64
name := "hello"   -- type inferred: str

const i32 MAX = 512
u64 addr = 0xFFFF800000000000
```

Types include `i8`–`i64`, `u8`–`u64`, `usize`, `f32`, `f64`, `bool`,
Unicode `char`, length-bearing UTF-8 `str`, `*T`, `?*T`, `[N]T`, `[]T`,
`[]const T`, tuples, function values, structs, variants, and interfaces.

Fixed arrays require a positive integer literal size and exactly `N` elements, except `[]`, which zero-initializes the declared array. Array literals must be homogeneous and same-typed arrays copy by value. Both arrays and pointers support integer indexing; literal array indexes are checked at compile time. Pointer arithmetic is limited to `pointer + integer`, `integer + pointer`, and `pointer - integer`. Address-of requires an assignable expression. Pointer loop captures are valid only for fixed arrays.

On the current 64-bit bootstrap target, `usize` is an alias of `u64`. `*void` is the untyped FFI/allocation pointer and converts to or from any pointer type; other pointer element types remain invariant.

`print(value, ...)` is a compiler builtin. It requires at least one argument and accepts primitive values and raw pointers; structs, arrays, tuples, and other composite values must be formatted explicitly. Arguments are emitted consecutively with no implicit separators, followed by one newline.

Variant arms may carry zero, one, or multiple ordered payload values. Construction checks both payload arity and each value's declared type.

Module members are accessed with qualified paths such as `math.Value` and `math.double()`. Module bodies have their own scope; qualified functions and types retain their checked signatures. `use math.answer` imports the final public member name into the current scope.

Arithmetic operators require numeric operands, except `str + str` concatenation. `%` is integer-only. Bitwise operators require integer operands; `and` and `or` require booleans.

### Functions — named return required

```runes
f add(x: i32, y: i32) = result: i32 {
    result = x + y
}

-- void functions omit the return clause entirely
f greet(name: str) {
    print("hello " + name)
}

-- one-liner
f square(x: i32) = r: i32  r = x * x
```

Anonymous return types are not valid. `f foo() = i32 { ... }` is a compile error.

### Memory strategies

Every function carries a memory strategy keyword. The compiler enforces nesting rules.

| Keyword | Allocator | Freed by |
|---------|-----------|----------|
| `f` | Stack (default) | Auto on return |
| `stack f` | Stack, strictly no nesting | Auto on return |
| `dynamic f` | Raw heap (`raw_alloc`/`raw_free`) | Caller, explicitly |
| `regional f` | Arena bump allocator | Auto at scope exit |
| `gc f` | GC heap | GC runtime |
| `flex f` | Inherits caller's strategy | Whoever caller is |

```runes
-- arena f: all allocations freed when setup_tables() returns
regional f setup_tables() = r: *PageTable {
    PageTable t = PageTable()
    r = promote(&t) as dynamic  -- escape arena into raw heap; caller must raw_free
}
```

`promote(&val) as X` is the only way to move a value out of a short-lived scope. `promote` without `as X` is a compile error.

### Structs, variants, interfaces

```runes
type Vec2 = { x: f32, y: f32 }

type Color =
    | Red
    | Green
    | RGB(u8, u8, u8)

interface Drawable {
    f draw(self)
}

method Drawable for Vec2 {
    f draw(self) { render_point(self.x, self.y) }
}
```

### Pattern matching

```runes
match color {
    Red        -> print("red"),
    RGB(r,g,b) -> print(r, g, b),
    _          -> print("other"),
}

-- with guard
match x {
    n if n < 0 -> print("negative"),
    0          -> print("zero"),
    _          -> print("positive"),
}
```

### Error handling

```runes
error MathError = { | DivByZero | Overflow }

f divide(a: f32, b: f32) = result: !f32 {
    if b == 0.0 { result = error.MathError.DivByZero }
    else        { result = a / b }
}

-- try propagates, catch handles inline
f run() = r: !f32 {
    f32 val = try divide(10.0, 2.0)
    r = val * 2.0
}

f32 val = divide(10.0, 0.0) catch 0.0
```

### Systems programming

```runes
-- inline asm
f read_cr3() = r: u64 {
    asm { "mov %cr3, %rax" } -> r
}

-- volatile memory-mapped I/O (never optimized away)
volatile *u32 uart = 0x10000000 as *u32
*uart = 0x41

-- Interrupt entry points use an external assembly stub in v0.1.
-- The C backend rejects #[interrupt] rather than emitting an unsafe ABI.

-- FFI
extern f memset(ptr: *u8, val: i32, len: usize)
extern u64 KERNEL_START
```

### Quirks to know

- **Named returns are mandatory.** `f foo() = result: i32 { ... }` not `f foo() = i32 { ... }`. Void functions have no return clause at all.
- **Comments use `--`.** Single line: `-- comment`. Block: `--- ... ---`.
- **Generics are monomorphized.** Functions, structs, variants, and methods
  support type parameters and exact interface constraints.
- **C generation is the v0.1 bootstrap backend.** It lowers functions, modules,
  methods and interfaces, structs and variants, fixed arrays and pointers,
  matches, fallible values, strings, globals, externs, unsafe blocks, and inline
  assembly. Unsupported constructs fail with a code-generation diagnostic.
- **JSON and pipeline language features are not part of Runes.** The `|` token remains for variants, captures, catch bindings, and bitwise OR.
- **`flex f` inherits the active caller realm.** It enters a GC frame only when
  a GC scope is active and otherwise follows the caller's allocation context.
- **Prelude is a separate file.** Pass `src/std/prelude.runes` for the minimal
  compiler-runtime ABI contracts. `print` is a compiler builtin. Broad sample
  mocks live only in `src/tests/fixtures/sample_prelude.runes`.

---

## Project structure

```
runes/
├── src/
│   ├── main.c           # CLI and pipeline orchestration
│   ├── lexer.c/h        # Phase 1: tokenizer
│   ├── parser.c/h       # Phase 1: recursive-descent parser → AstNode tree
│   ├── ast.c/h          # AST node types and pretty-printer
│   ├── resolver.c/h     # Phase 2: name resolution, scope management
│   ├── symbol_table.c/h # Scoped hash-map (FNV-1a, arena-backed)
│   ├── typecheck.c/h    # Phase 3: type inference, realm enforcement
│   ├── types.c/h        # Semantic type representation
│   ├── codegen.c/h      # Bootstrap C emitter for the supported core subset
│   ├── utils/
│   │   ├── arena.c/h    # Bump-pointer arena allocator (64 KiB blocks)
│   │   └── strtab.c/h   # String interning (FNV-1a, open addressing)
│   ├── std/
│   │   └── prelude.runes  # extern declarations for runtime primitives
│   └── tests/
│       ├── tester.bash    # semantic integration runner
│       ├── codegen_inventory.bash # generated-C compile inventory
│       └── samples/       # positive and expected-negative programs
├── docs/
│   └── specv0_1.md        # Language specification
├── runes-lang/            # VS Code extension (.vsix included)
└── Makefile
```

See `.planning/codebase/ARCHITECTURE.md` for the full pipeline design and `.planning/codebase/STRUCTURE.md` for data structure details.

---

## Current compiler state

| Phase | Status |
|-------|--------|
| Lexer (Phase 1) | Broad, covered by focused unit tests |
| Parser (Phase 1) | v0.1 syntax covered by unit and integration tests |
| Name resolution (Phase 2) | Modules, imports, methods, and nested scopes implemented |
| Type checker (Phase 3) | Tested v0.1 core; diagnostics remain an active hardening area |
| Code generation (Phase 4) | Executable core corpus emits warning-clean C11 |

The maintained sample inventory currently covers more than 200 positive and
expected-negative programs. The executable generated-C inventory compiles 73
core programs with `-Wall -Wextra -Werror`; focused tests execute arenas, deep
promotion, the scoped collector, closures, generics, slices, Unicode, modules,
FFI attributes, checked arithmetic, and traps.

The type checker enforces:
- Memory realm nesting matrix (which strategy can nest inside which)
- Named return requirements
- `promote` rules and valid targets
- `!T` / `try` / `catch` consistency
- systems attributes, with explicit rejection of unsupported interrupt lowering
- Struct self-recursion, duplicate field names
- `break`/`continue` inside loops only

---

## VS Code extension

```bash
code --install-extension runes-lang/runes-lang-0.0.1.vsix
```

Syntax highlighting for `.runes` files. TextMate grammar, no build step needed.

## Zed extension

```bash
make install-zed
```

The installer registers the extension directly. Restart Zed if it was already
running. The extension includes Tree-sitter syntax highlighting, bracket
matching, and `--` comment toggling. See
[editors/zed/README.md](editors/zed/README.md).

---

## Contributing

The compiler is written in C with no third-party dependencies. To contribute:

1. **Read the spec:** `docs/specv0_1.md` is the language reference.
2. **Read the architecture:** `.planning/codebase/ARCHITECTURE.md` explains the pipeline and how phases connect.
3. **Run the tests:** `make test`, `make test-samples`, and `make test-codegen`.
4. **Check the runtime boundary:** `docs/v0.1-runtime-requirements.md` records
   what belongs in the compiler runtime and what remains user library work.

The compiler is a single-binary build (`make`), no linking step, no package manager. Start hacking.

---

_Bootstrap compiler written in C. The current backend emits C; self-hosting remains a long-term goal._
