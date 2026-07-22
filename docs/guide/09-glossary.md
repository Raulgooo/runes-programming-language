# Glossary

This glossary defines terms used by the handbook without assuming a programming
language or systems background.

**ABI (application binary interface)**

The machine-level agreement for calling functions and laying out data: symbol
names, parameter locations, return values, struct layout, and calling
conventions. FFI declarations must match the foreign ABI exactly.

**Allocation**

Reserving memory for a value. Stack locals, raw heap objects, arena objects, and
GC objects use different allocation and cleanup rules.

**Arena**

A memory region that serves many cheap allocations and frees them together.
Runes regional functions create nested arena trees.

**Borrow / borrowed view**

A temporary reference to storage owned elsewhere. A slice and `str` are borrowed
views. They cannot safely outlive their backing storage.

**Bounds check**

A check that an index or range stays within an array, slice, or string. Failed
dynamic checks stop the program with a diagnostic.

**Calling convention**

The ABI rules for how one machine-code function calls another. Runes currently
recognizes System V x86-64 and Windows x64 attributes on the C backend.

**Canonical path**

The normalized real filesystem identity of a file after resolving relative
segments and symbolic links. The module loader uses it to recognize reuse and
cycles.

**Closure**

A function value that carries variables captured from a surrounding function.
A borrowing closure references those variables; `move f` copies them into an
allocated environment.

**Compiler**

The program that reads Runes source, validates it, and emits C. `runes` is the
compiler; `runec` is the convenient build/run driver around it.

**Concrete type**

A specific type with a known representation, such as `i32`, `Counter`, or
`Pair<i32, bool>`, rather than an interface or unresolved generic parameter.

**Deep clone / promotion**

Copying an entire reachable owned object graph rather than only copying its
root pointer. Runes uses deep promotion to move arena-owned data into dynamic
or GC storage while preserving aliases and cycles.

**FFI (foreign function interface)**

The boundary used to call functions or access variables provided by C, an
operating system, or another language.

**Garbage collection (GC)**

Automatic reclamation of allocated memory that can no longer be reached from
live program values. Runes v0.1 uses precise non-moving mark/sweep collection
inside GC execution.

**Generic**

A declaration parameterized by a type, such as `Pair<T, U>`. The compiler
specializes it for concrete types used by the program.

**Heap**

Memory that is allocated independently of a function's local stack frame. Raw,
arena, and garbage-collected heaps differ in how they release memory.

**Interface**

A named set of required methods. A Runes interface value carries a reference to
concrete data and a table selecting that type's method implementations.

**Lifetime**

The period during which storage remains valid. Returning a pointer after its
storage dies creates an invalid reference, so the compiler rejects known escape
paths.

**Monomorphization**

Generating a concrete implementation of generic code for each combination of
types used by the program.

**Nominal type**

A type whose declared name contributes to its identity. Two separately declared
structs or error sets remain different even when their contents look identical.

**NUL termination**

The C convention where a zero byte marks the end of a string. Runes `str` uses
an explicit byte length and does not generally require NUL termination.

**Ownership**

Responsibility for keeping storage alive and eventually releasing it. A pointer
or slice does not automatically communicate a complete ownership policy.

**Prelude**

The small set of runtime declarations loaded automatically by `runec`. It is
not the complete standard library.

**Provenance**

The compiler's record of where a reference came from, such as stack, arena,
raw, GC, or external storage. Provenance supports escape and lifetime checks.

**Realm**

The allocation/lifetime strategy attached to a Runes function: stack, dynamic,
regional, GC, or inherited flex behavior.

**Runtime**

The C support code linked into generated programs for traps, allocation,
arenas, promotion, GC, strings, and compiler-generated metadata.

**Safepoint**

A point where GC collection may run and the compiler has made live references
visible to the collector. In v0.1 this occurs on relevant allocation slow paths
or explicit collection, not arbitrary instructions throughout the program.

**Scope**

A source region in which a declared name is available, commonly delimited by
braces. Scope also helps determine local storage lifetime and cleanup.

**Slice**

A non-owning pointer-and-length view over consecutive values. The slice does not
allocate, copy, or extend the lifetime of its elements.

**Stack**

Function-local storage created on entry and discarded on return. Returning a
reference to a local stack value is invalid.

**Standard library**

Reusable Runes modules for text, collections, files, networking, and other
common tasks. The `std` namespace exists, but most APIs are still planned.

**Trap**

Immediate program termination caused by a failed runtime safety check such as
overflow, division by zero, invalid Unicode, null unwrap, or out-of-bounds
indexing.

**Type descriptor**

Compiler-generated metadata describing a type's size, alignment, cloning, and
GC tracing behavior.

**Unsafe block**

A lexical block where operations requiring programmer-proven invariants are
allowed. It does not disable ordinary type, overflow, or bounds checks.

**Variant**

A tagged value holding exactly one named alternative, optionally with payload
data. It is also commonly called a tagged union or algebraic data type.

[Back to the handbook index](README.md)
