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

**Status:** Bootstrap compiler (C) is complete through type checking. Code generation is the next milestone. Not yet production-ready.

---

## What Runes is

Runes sits between C and Rust. You get algebraic types, pattern matching, and a proper error model. You also get explicit memory strategies per function — no borrow checker, no GC tax unless you ask for it.

The core bet: **memory strategy belongs to the function, not the data**. You pick stack, arena, heap, or GC at the callsite. The compiler enforces the nesting rules. You can go as low as inline asm and interrupt handlers, or as high as GC-tracked objects and JSON schemas — in the same program.

---

## Build

```bash
# Requires: gcc, make
make          # builds ./runes
make clean    # removes binary
```

No external dependencies. Pure C, zero third-party libraries.

---

## Usage

```bash
./runes file.runes                      # full pipeline: lex → parse → resolve → typecheck
./runes src/std/prelude.runes file.runes # with standard prelude (needed for print, alloc, etc.)
./runes --lex-only  file.runes          # dump token stream
./runes --parse-only file.runes         # syntax check only
./runes --dump-ast  file.runes          # print the AST
```

Multiple files are merged before analysis:

```bash
./runes src/std/prelude.runes src/tests/samples/10_kernel_bootstrap.runes
```

Exit code 0 = clean. Exit code 1 = errors (printed to stderr with `file:line:col`).

---

## Language at a Glance

### Variables and types

```runes
i32 x = 5
z    = 3.14       -- type inferred: f64
name = "hello"    -- type inferred: str

const i32 MAX = 512
u64 addr = 0xFFFF800000000000
```

Types: `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `bool`, `str`, `char`, `*T`, `[N]T`.

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

-- interrupt handler (saves/restores all registers, ends with iretq)
#[interrupt]
f page_fault_handler() {
    u64 cr2 = read_cr2()
    handle_page_fault(cr2)
}

-- FFI
extern f memset(ptr: *u8, val: i32, len: usize)
extern u64 KERNEL_START
```

### Quirks to know

- **Named returns are mandatory.** `f foo() = result: i32 { ... }` not `f foo() = i32 { ... }`. Void functions have no return clause at all.
- **Comments use `--`.** Single line: `-- comment`. Block: `--- ... ---`.
- **No generics yet** (v0.1). The syntax is in the spec; the compiler doesn't support it yet.
- **No code generation yet.** The full pipeline through type checking works. Emitting LLVM IR is Phase 4, not yet implemented.
- **`flex f` is stack-only in v0.1.** Full monomorphization over memory strategies is v0.2.
- **Prelude is a separate file.** Pass `src/std/prelude.runes` as the first argument if your code uses `print`, `raw_alloc`, or other built-ins.

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
│   ├── codegen.c/h      # Phase 4: LLVM IR emitter (stub, not yet implemented)
│   ├── utils/
│   │   ├── arena.c/h    # Bump-pointer arena allocator (64 KiB blocks)
│   │   └── strtab.c/h   # String interning (FNV-1a, open addressing)
│   ├── std/
│   │   └── prelude.runes  # extern declarations for runtime primitives
│   └── tests/
│       ├── tester.bash    # shell test runner
│       └── samples/       # 35+ .runes integration tests
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
| Lexer (Phase 1) | Complete |
| Parser (Phase 1) | Complete |
| Name resolution (Phase 2) | Complete |
| Type checker (Phase 3) | Complete — memory realm enforcement, fallible types, pattern matching, ASM/systems constructs |
| Code generation (Phase 4) | Stub — not yet implemented |

The type checker enforces:
- Memory realm nesting matrix (which strategy can nest inside which)
- Named return requirements
- `promote` rules and valid targets
- `!T` / `try` / `catch` consistency
- `#[interrupt]` function signature rules
- Struct self-recursion, duplicate field names
- `break`/`continue` inside loops only

---

## VS Code extension

```bash
code --install-extension runes-lang/runes-lang-0.0.1.vsix
```

Syntax highlighting for `.runes` files. TextMate grammar, no build step needed.

---

## Contributing

The compiler is written in C with no external dependencies. If you want to contribute:

1. **Read the spec:** `docs/specv0_1.md` is the language reference.
2. **Read the architecture:** `.planning/codebase/ARCHITECTURE.md` explains the pipeline and how phases connect.
3. **Run the tests:** `src/tests/tester.bash` runs all sample files through the full pipeline.
4. **The next big thing is codegen.** `src/codegen.c` is an empty stub waiting for Phase 4. The type checker annotates every `AstNode` with its `resolved_type` — that's the input codegen will consume to emit LLVM IR.

The compiler is a single-binary build (`make`), no linking step, no package manager. Start hacking.

---

_Bootstrap compiler written in C. Backend target: LLVM IR. Self-hosted compiler in Runes is the long-term goal._
