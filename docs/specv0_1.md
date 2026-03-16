# Runes Language Specification — v0.8 (Draft)

> Systems-level language with high-level ergonomics.
> Designed for writing operating systems without sacrificing expressiveness.
> Bootstrap compiler: C → self-hosted.

---

## 1. Core Philosophy

- **Explicit over magic** — the programmer controls what happens, the compiler enforces contracts
- **Mutable by default** — immutability is opt-in via `const`
- **Scope-local memory** — memory strategy is declared per function, not globally
- **Zero runtime by default** — GC is available but not mandatory
- **Unsafe as an escape hatch** — not a loophole, a deliberate door
- **Elegant syntax** — readable at a glance, no cryptic abbreviations

---

## 2. Variables

```runes
-- Mutable by default
i32 x = 5
i64 y = 10

-- Type inference
z    = 3.14       -- inferred f64
name = "hello"    -- inferred str

-- Immutable
const i32 MAX   = 512
const     LIMIT = 1024  -- inferred

-- Explicit
u8  flags = 0xFF
u64 addr  = 0xFFFF800000000000
```

---

## 3. Primitive Types

| Type        | Description                    |
|-------------|--------------------------------|
| `i8`–`i64`  | Signed integers                |
| `u8`–`u64`  | Unsigned integers              |
| `f32`,`f64` | Floats                         |
| `bool`      | `true` / `false`               |
| `str`       | Immutable UTF-8 string         |
| `char`      | Single Unicode codepoint       |
| `*T`        | Raw pointer to T               |
| `[N]T`      | Fixed-size array of N elements |
| `sl`        | Singly linked list (dynamic)   |
| `dl`        | Doubly linked list (dynamic)   |

### Arrays

```runes
[5]i32   nums    = [1, 2, 3, 4, 5]
[4]u8    rgba    = [255, 0, 128, 255]
[512]u64 entries = []               -- zero initialized

-- Access
nums[0] = 10
i32 val = nums[2]

-- Iterate
for (nums) |n| { print(n) }

-- Mutate in place
for (nums) |*n| { *n = *n * 2 }
```

No dynamic arrays. Use `sl` or `dl` for growable collections.

---

## 4. Functions

All functions use `f`, optionally preceded by a memory strategy keyword.
**Named return variables are always required.** Anonymous return types are invalid.
**Void functions omit the return clause entirely.**

| Declaration   | Memory strategy                                      |
|---------------|------------------------------------------------------|
| `f`           | Stack — default, zero overhead, auto freed           |
| `dynamic f`   | Raw heap — like C `malloc`/`free`, no compiler help  |
| `regional f`  | Region — arena bump allocator, freed at scope exit   |
| `gc f`        | GC tracked — for userspace / high-level code         |
| `flex f`      | Inherits caller's memory strategy                  |

```runes
-- Stack (default) — named return required
f add(x: i32, y: i32) = result: i32 {
    result = x + y
}

-- Void — omit return clause entirely
f greet(name: str) {
    print("hello " + name)
}

-- One-liner — named return still required
f square(x: i32) = r: i32  r = x * x

-- Dynamic — raw heap, C-style, you own the memory
dynamic f alloc_buf(size: u64) = ptr: *u8 {
    ptr = raw_alloc(size)
    -- caller must raw_free(ptr)
}

-- Regional — arena allocated, freed in bulk at function scope exit
regional f make_table() = t: PageTable {
    t = PageTable(entries: [])
}

-- GC — garbage collected, for high-level userspace code
gc f run_shell(input: str) = result: sl {
    result = tokenize(input)
}

-- Lambdas / closures (stack allocated, inherit caller's strategy)
doubled = [1, 2, 3].map(|n| n * 2)
evens   = [1, 2, 3, 4].filter(|n| n % 2 == 0)
```

### Named return rules

```runes
-- ✅ valid — named return
f foo() = result: i32 {
    result = 42
}

-- ✅ valid — void, no return clause
f bar() {
    print("hi")
}

-- ❌ invalid — anonymous return type not allowed
f foo() = i32 {
    ...
}

-- ❌ invalid — void function with return clause
f bar() = v {
    ...
}
```

---

## 5. Memory Model

The memory strategy keyword before `f` defines how everything inside that function
is allocated. Violations of nesting rules are **compile errors** — the type checker
rejects invalid combinations.

| Keyword      | Allocator                    | Who frees             |
|--------------|------------------------------|-----------------------|
| `f`          | Stack                        | Auto on return        |
| `dynamic f`  | Raw heap (`raw_alloc`)       | Caller, explicitly    |
| `regional f` | Arena bump allocator         | Auto at scope exit    |
| `gc f`       | GC heap                      | GC runtime            |
| `flex f`     | Inherits caller's strategy   | Whoever caller is     |

### Nesting rules

Each function type has strict rules about what can be nested inside it.
The type checker enforces these at compile time.

| Outer function | Can contain                              | Cannot contain              |
|----------------|------------------------------------------|-----------------------------|
| `f` (top-level)| other `f` only                          | `dynamic f`, `regional f`, `gc f`, `flex f` |
| `f` (nested)   | other `f` only                          | any other strategy          |
| `dynamic f`    | `f`, other `dynamic f`, `gc f`          | `regional f`                |
| `regional f`   | `f` only                                | `dynamic f`, `gc f`, `flex f` |
| `gc f`         | `f`, other `gc f`                       | `dynamic f`, `regional f`   |
| `flex f`       | inherits — same rules as caller         | whatever caller cannot have |

```runes
-- ✅ f contains only f
f kernel_main() {
    f setup() {           -- ok: f inside f
        i32 x = 5
    }
    setup()
}

-- ✅ dynamic f contains f and gc f
dynamic f init_driver() {
    f helper() {          -- ok: f inside dynamic f
        i32 x = 5
    }
    gc f parse_config() = r: Node {   -- ok: gc f inside dynamic f
        r = build_ast()
    }
}

-- ✅ regional f contains only f
regional f build_tables() {
    f zero(p: *u8) {      -- ok: f inside regional f
        *p = 0
    }
}

-- ✅ gc f contains f and gc f
gc f run_shell() {
    f validate(s: str) = r: bool {   -- ok: f inside gc f
        r = s.len > 0
    }
    gc f parse(s: str) = r: Node {   -- ok: gc f inside gc f
        r = build_ast(s)
    }
}

-- ❌ compile error — regional f inside f (top-level)
f kernel_main() {
    regional f bad() {    -- ERROR: regional f cannot nest inside f
        PageTable t = PageTable()
    }
}

-- ❌ compile error — regional f inside dynamic f
dynamic f init() {
    regional f bad() {    -- ERROR: regional f cannot nest inside dynamic f
        PageTable t = PageTable()
    }
}

-- ❌ compile error — dynamic f inside regional f
regional f build() {
    dynamic f bad() {     -- ERROR: dynamic f cannot nest inside regional f
        *u8 p = raw_alloc(64)
    }
}
```

### `flex f` — inherits caller strategy

`flex f` takes on the memory strategy of whatever function calls it.
Designed for stdlib functions that should work in any context.

```runes
-- flex f — works in any context
flex f make_node(val: i32) = r: *Node {
    r = alloc(sizeof(Node))   -- uses caller's allocator
    r.val = val
}

-- called from regional f → make_node uses arena
regional f build_ast() {
    *Node n = make_node(42)   -- arena allocated
}

-- called from gc f → make_node uses GC
gc f parse() {
    *Node n = make_node(42)   -- GC allocated
}

-- called from dynamic f → make_node uses raw heap
dynamic f init() {
    *Node n = make_node(42)   -- raw heap allocated
}
```

`flex f` nesting rules follow the caller — if called from `regional f`,
it obeys `regional f` rules. If called from `dynamic f`, it obeys
`dynamic f` rules.

```runes
-- flex f inside regional f → becomes regional, can only nest f
regional f build() {
    flex f helper() {     -- becomes regional here
        f inner() { }     -- ok: f inside regional (via flex)
        -- dynamic f inner() { }  ← ERROR
    }
}
```

### Scope crossing rules

Values cannot escape their memory scope unless:

1. They are `Copy` types (primitives, small structs) — copied out automatically
2. `promote` is used — explicit unsafe override

```runes
-- ERROR: regional value escaping scope
regional f bad() = r: *PageTable {
    PageTable t = PageTable()
    r = &t               -- compile error: region escape
}

-- OK: u64 is Copy, value is copied to caller's stack
regional f get_id() = id: u64 {
    PageTable t = PageTable()
    id = t.id            -- safe copy
}

-- OK: promote — you take full responsibility
regional f risky() = r: *PageTable {
    PageTable t = PageTable()
    r = promote(&t)      -- unsafe: you are responsible for lifetime
}
```

---

## 6. Control Flow

```runes
-- if / else
if x > 0 {
    print("positive")
} else if x < 0 {
    print("negative")
} else {
    print("zero")
}

-- if as expression
str label = if x > 0 { "pos" } else { "neg" }

-- while
while running {
    tick()
}

-- infinite loop
loop {
    if done() { break }
}
```

### For Loop (Zig-style)

```runes
-- Range (exclusive end)
for (0..10) |i| {
    print(i)
}

-- Range (inclusive end)
for (0..=10) |i| {
    print(i)
}

-- Over array
[5]i32 nums = [1, 2, 3, 4, 5]
for (nums) |n| {
    print(n)
}

-- With index
for (items) |item, i| {
    print(i, item)
}

-- Pointer capture — mutate in place
for (nums) |*n| {
    *n = *n * 2
}

-- Over linked list
for (tasks) |task| {
    task.run()
}
```

---

## 7. Types — Structs and Variants

### Structs

```runes
-- One-liner
type Vec2 = x: f32, y: f32

-- Full struct with defaults
type Vec2 = {
    x: f32 = 0.0,
    y: f32 = 0.0,
}

-- Instantiation
Vec2 v      = Vec2(x: 1.0, y: 2.0)
Vec2 origin = Vec2()           -- uses defaults
Vec2 pt     = Vec2(y: 5.0)    -- x stays default

-- Methods
method Vec2 {
    f length(self) = r: f32 {
        r = sqrt(self.x * self.x + self.y * self.y)
    }

    f scale(self, factor: f32) = r: Vec2 {
        r = Vec2(x: self.x * factor, y: self.y * factor)
    }
}
```

### Variants (Algebraic Data Types)

```runes
type Color =
    | Red
    | Green
    | Blue
    | RGB(u8, u8, u8)
    | Hex(str)

type Option<T> =
    | Some(T)
    | None
```

### Interfaces

```runes
interface Drawable {
    f draw(self)
    f bbox(self) = r: (f32, f32, f32, f32)
}

method Drawable for Vec2 {
    f draw(self) {
        render_point(self.x, self.y)
    }
    f bbox(self) = r: (f32, f32, f32, f32) {
        r = (self.x, self.y, self.x, self.y)
    }
}

f render(d: Drawable) {
    d.draw()
}
```

---

## 8. Generics

```runes
type Stack<T> = {
    data: dl,
    top:  usize,
}

method Stack<T> {
    f push(self, val: T) { ... }
    f pop(self) = r: Option<T> { ... }
}

f first<T>(items: dl) = r: Option<T> {
    r = if items.len == 0 { None } else { Some(items[0]) }
}
```

---

## 9. Pattern Matching

```runes
-- Basic
match color {
    Red        -> print("red"),
    RGB(r,g,b) -> print(r, g, b),
    Hex(s)     -> print(s),
    _          -> print("other"),
}

-- As expression
str label = match color {
    Red   -> "red",
    Green -> "green",
    Blue  -> "blue",
    _     -> "custom",
}

-- Guards
match x {
    n if n < 0   -> print("negative"),
    0            -> print("zero"),
    n if n > 100 -> print("big"),
    _            -> print("normal"),
}

-- Destructuring
match point {
    Vec2(x: 0.0, y) -> print("on y-axis", y),
    Vec2(x, y: 0.0) -> print("on x-axis", x),
    Vec2(x, y)      -> print(x, y),
}
```

---

## 10. Error Handling (Zig-style)

`!T` means the function can fail. `try` propagates errors up. `catch` handles inline.

```runes
-- Define a named error set
error MathError = {
    | DivByZero
    | Overflow
}

error PageFault = {
    | NotMapped
    | PermissionDenied
    | OutOfMemory
}

-- Fallible function — ! on return type
f divide(a: f32, b: f32) = result: !f32 {
    if b == 0.0 {
        result = error.MathError.DivByZero
    } else {
        result = a / b
    }
}

-- try — propagate error to caller (caller must also return !T)
f run() = r: !f32 {
    f32 val = try divide(10.0, 2.0)
    r = val * 2.0
}

-- catch — handle inline
f safe_run() {
    f32 val = divide(10.0, 0.0) catch |e| {
        print("caught:", e)
        return
    }
    print(val)
}

-- catch with default value
f32 val = divide(10.0, 0.0) catch 0.0

-- match for full control
match divide(10.0, 0.0) {
    Ok(v)  -> print(v),
    Err(e) -> print("error:", e),
}
```

---

## 11. Unsafe and Systems Features

```runes
-- Unsafe block
f zero_page(addr: *u8, len: usize) {
    unsafe {
        for (0..len) |i| {
            *(addr + i) = 0
        }
    }
}

-- Inline assembly
f halt() {
    asm { "cli; hlt" }
}

f read_cr3() = r: u64 {
    asm { "mov %cr3, %rax" } -> r
}

-- Struct layout annotations
#[packed]
#[align(4096)]
type PageTable = {
    entries: [512]u64,
}

#[repr(C)]
type SyscallFrame = {
    rax: u64,
    rbx: u64,
    rcx: u64,
    rdx: u64,
}
```

---

## 12. OS-Critical Features

These four features are required for real OS development. All map directly to
existing LLVM IR constructs — the compiler emits the appropriate IR attributes.

### `extern` — Foreign Function Interface

Declare functions that live outside Runes (bootloader, UEFI, libgcc, C stdlib).

```runes
-- Declare external C functions
extern f memset(ptr: *u8, val: i32, len: usize)
extern f memcpy(dst: *u8, src: *u8, len: usize)
extern f memcmp(a: *u8, b: *u8, len: usize) = r: i32

-- External variables (MMIO base addresses, linker symbols)
extern u64 KERNEL_START
extern u64 KERNEL_END

-- Use them normally — typechecker enforces signatures
memset(buffer, 0, 4096)
u64 size = KERNEL_END - KERNEL_START
```

LLVM IR emitted:
```llvm
declare void @memset(i8*, i32, i64)
@KERNEL_START = external global i64
```

---

### `volatile` — Hardware Memory Access

Prevents the compiler/LLVM from optimizing away reads and writes to memory-mapped
hardware registers. Essential for UART, PIC, APIC, PCI, and any MMIO device.

```runes
-- Declare a volatile pointer
volatile *u32 uart   = 0x10000000 as *u32
volatile *u8  pic    = 0xFEC00000 as *u8

-- Read and write — compiler NEVER eliminates these
*uart = 0x41            -- write 'A' to UART
u32 status = *uart      -- read status register

-- Volatile struct field
type UARTRegs = {
    volatile data:    u8,
    volatile status:  u8,
    volatile control: u16,
}
```

LLVM IR emitted:
```llvm
store volatile i32 65, i32* %uart
%status = load volatile i32, i32* %uart
```

---

### `#[section]` and `#[link_name]` — Linker Control

Place functions and data into specific ELF sections. Required for bootloader
entry points, interrupt vectors, and DMA buffers.

```runes
-- Entry point at a specific ELF section
#[section(".text.boot")]
#[link_name("_start")]
pub f entry_point() {
    kernel_main()
}

-- Place data in a specific section
#[section(".rodata.tables")]
const [256]u64 IDT_TABLE = []

-- Align to page boundary in a specific section
#[section(".bss.stack")]
#[align(4096)]
[16384]u8 KERNEL_STACK = []
```

LLVM IR emitted:
```llvm
define void @_start() section ".text.boot" { ... }
@IDT_TABLE = constant [256 x i64] zeroinitializer, section ".rodata.tables"
```

---

### `#[callconv]` and `#[interrupt]` — Calling Conventions

Control how functions pass arguments and preserve registers. Required for
syscall handlers, interrupt service routines, and UEFI calls.

```runes
-- Explicit calling convention
#[callconv("sysv64")]
f syscall_entry(nr: u64, a: u64, b: u64, c: u64) = r: u64 {
    r = dispatch(nr, a, b, c)
}

-- Interrupt handler — compiler saves/restores ALL registers
-- No arguments, no return value, ends with iretq
#[interrupt]
f page_fault_handler() {
    u64 cr2 = read_cr2()
    handle_page_fault(cr2)
}

#[interrupt]
f double_fault_handler() {
    panic("double fault")
}

-- UEFI calling convention (win64) for UEFI bootloaders
#[callconv("win64")]
f efi_main(handle: *void, table: *EFISystemTable) = r: u64 {
    r = 0
}
```

LLVM IR emitted:
```llvm
define x86_64_sysvcc i64 @syscall_entry(i64, i64, i64, i64) { ... }
define x86_intr_cc void @page_fault_handler() { ... }
define win64cc i64 @efi_main(i8*, i8*) { ... }
```

---

### Complete OS boot example

```runes
use kernel.arch.x86

extern f memset(ptr: *u8, val: i32, len: usize)

#[section(".bss.stack")]
#[align(4096)]
[16384]u8 KERNEL_STACK = []

#[section(".text.boot")]
#[link_name("_start")]
pub f entry_point() {
    -- zero the BSS
    extern u64 BSS_START
    extern u64 BSS_END
    u64 len = BSS_END - BSS_START
    memset(BSS_START as *u8, 0, len)

    kernel_main()

    -- should never reach here
    loop { unsafe { asm { "cli; hlt" } } }
}

#[interrupt]
f page_fault_handler() {
    volatile *u32 uart = 0x10000000 as *u32
    *uart = 0x21    -- '!' to UART
    loop { unsafe { asm { "cli; hlt" } } }
}

pub f kernel_main() {
    regional f setup_paging() {
        PageTable pml4 = PageTable.new()
        try pml4.map(0xFFFF800000000000, 0x0, 0x3)
    }

    setup_paging()

    unsafe { asm { "mov %rax, %cr3" } }
}
```

---

## 13. JSON and Schemas

### The `J` type

`J` is a built-in type representing a JSON value. Any `type` or `schema` can be
converted to `J` using `as J`. The compiler generates the serializer at compile
time — zero runtime reflection, zero overhead.

```runes
-- J methods
j.string()        -- compact JSON string: {"name":"raul","age":18}
j.pretty()        -- indented JSON string
j.get("key")      -- access field by name
j.set("key", val) -- mutate field
j.has("key")      -- bool, check if field exists
```

### `as J` — serialize any type to JSON

```runes
type Point = { x: f32, y: f32 }

Point p = Point(x: 1.0, y: 2.0)
j = p as J

print(j.string())    -- {"x":1.0,"y":2.0}
print(j.pretty())
-- {
--   "x": 1.0,
--   "y": 2.0
-- }

f32 x = j.get("x")  -- 1.0
j.set("x", 5.0)
```

### `as T` — deserialize JSON back to a type

```runes
str raw = "{\"x\":3.0,\"y\":4.0}"
J j     = raw as J       -- parse string → J
Point p = j as Point     -- J → struct, fields validated at runtime
```

### `schema` — type with inheritance for validation

`schema` is like `type` but supports inheritance. A function that expects a
`schema` parent accepts any child schema — exactly like Pydantic.

```runes
schema Shoe = {
    brand: str,
    size:  f32,
}

schema RedShoe : Shoe = {   -- inherits brand and size
    color: str = "red",     -- extra field with default
}

-- function expects Shoe — accepts any child schema
f process(shoe: Shoe) {
    print(shoe.brand)
}

f handle_request() {
    RedShoe s = RedShoe(brand: "Nike", size: 10.5)
    process(s)              -- ✅ RedShoe satisfies Shoe

    j = s as J              -- serialize
    print(j.string())       -- {"brand":"Nike","size":10.5,"color":"red"}

    RedShoe s2 = j as RedShoe  -- deserialize back
}
```

### Field annotations

```runes
schema User = {
    name:     str,
    age:      i32,
    #[json("email_address")]   -- custom JSON key name
    email:    str,
    #[json_skip]               -- excluded from JSON output
    password: str,
}

User u = User(name: "raul", age: 18, email: "r@x.com", password: "secret")
j = u as J
print(j.string())   -- {"name":"raul","age":18,"email_address":"r@x.com"}
                    -- password is not included
```

### Nested schemas

```runes
schema Address = {
    street: str,
    city:   str,
}

schema Person = {
    name:    str,
    address: Address,
}

Person p = Person(
    name: "raul",
    address: Address(street: "Av. Morones", city: "Monterrey")
)

j = p as J
print(j.string())
-- {"name":"raul","address":{"street":"Av. Morones","city":"Monterrey"}}
```

---

## 14. Modules

```runes
-- Define
mod kernel {
    pub regional f alloc_page() = p: !*u8 { ... }
    pub f         free_page(p: *u8) { ... }
}

-- Import whole module
use kernel

-- Import specific symbol
use kernel.alloc_page

-- Nested path
use kernel.arch.x86.read_cr3
```

---

## 14. Comments

```runes
-- Single line comment

--- Multi-line comment
    spans as many lines as needed
---

--- Doc comment — attached to the next declaration
    Supports Markdown.
---
f add(x: i32, y: i32) = result: i32 {
    result = x + y
}
```

---

## 16. Full OS Example

```runes
use kernel.arch.x86

error MapError = {
    | AlreadyMapped
    | InvalidAddress
}

#[packed]
#[align(4096)]
type PageTable = {
    entries: [512]u64,
}

method PageTable {
    regional f new() = t: PageTable {
        t = PageTable(entries: [])
    }

    f map(self, vaddr: u64, paddr: u64, flags: u64) = r: !void {
        u64 idx = (vaddr >> 12) & 0x1FF
        if self.entries[idx] != 0 {
            r = error.MapError.AlreadyMapped
        } else {
            self.entries[idx] = paddr | flags
        }
    }
}

pub f kernel_main() {
    regional f setup() {
        PageTable pml4 = PageTable.new()
        PageTable pdpt = PageTable.new()
        try pml4.map(0xFFFF800000000000, &pdpt as u64, 0x3)
    }

    setup()

    unsafe {
        asm { "mov %rax, %cr3" }
    }

    gc f run_userspace() {
        Task t = Task.spawn(shell_main)
        scheduler.add(t)
    }

    run_userspace()
}
```

---

## 17. Keyword Reference

| Keyword      | Meaning                                        |
|--------------|------------------------------------------------|
| `f`          | Stack function                                 |
| `dynamic f`  | Raw heap function (C-style malloc)             |
| `regional f` | Arena/region allocated function                |
| `gc f`       | Garbage collected function                             |
| `flex f`     | Inherits caller's memory strategy (monomorphized v0.2)  |                     |
| `method`     | Method block for a type                        |
| `interface`  | Interface definition                           |
| `type`       | Type definition (struct or variant)            |
| `error`      | Error set definition                           |
| `mod`        | Module definition                              |
| `use`        | Import                                         |
| `pub`        | Public visibility                              |
| `const`      | Immutable binding                              |
| `match`      | Pattern match                                  |
| `if/else`    | Conditional                                    |
| `for`        | Loop — `for (iter) \|val\| { }`               |
| `while`      | Conditional loop                               |
| `loop`       | Infinite loop                                  |
| `break`      | Exit loop                                      |
| `try`        | Propagate error to caller                      |
| `catch`      | Handle error inline                            |
| `unsafe`     | Unsafe block                                   |
| `asm`        | Inline assembly                                |
| `schema`     | Schema definition (type with inheritance + JSON)       |
| `J`          | Built-in JSON type                                     |
| `extern`     | Foreign function / variable declaration                |
| `volatile`   | Prevent compiler optimization of memory access         |
| `promote`    | Escape value from region scope (unsafe)                |

---

## 18. Feature Roadmap

| Feature                        | Version |
|-------------------------------|---------|
| Variables, primitives          | v0.1    |
| Functions (f, dynamic, etc.)   | v0.1    |
| Structs, variants, interfaces  | v0.1    |
| Pattern matching               | v0.1    |
| Generics                       | v0.1    |
| flex f (stack only, v0.1)      | v0.1    |
| flex f (full monomorphization) | v0.2    |
| Error handling (!T, try/catch) | v0.1    |
| Memory model (f modifiers)     | v0.1    |
| Inline assembly, unsafe        | v0.1    |
| Struct layout annotations      | v0.1    |
| extern / FFI                   | v0.1    |
| volatile                       | v0.1    |
| #[section] / #[link_name]      | v0.1    |
| #[callconv] / #[interrupt]     | v0.1    |
| JSON (`as J`, `schema`)        | v0.1    |
| HTTP / web stdlib              | v0.2    |
| Async / await                  | v0.2    |
| Macros                         | v0.3    |
| Native AI primitives           | v0.4    |

---

*Runes — v0.8 draft. Backend: LLVM IR. Compiler: C bootstrap → self-hosted.*