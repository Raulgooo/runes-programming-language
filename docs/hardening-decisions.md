# Runes Hardening Decisions

Status: design lock for the v0.1 hardening program.

This record separates confirmed language decisions from questions that still
require an explicit answer. Implementation must not silently change a locked
decision. Changes require updating this record and the language specification.

## Locked Decisions

### Regional memory

- Every top-level `regional f` invocation creates its own arena.
- Its arena is destroyed on every exit path, including early and fallible
  returns.
- A `regional f` invoked from another regional function creates a child arena.
- A child arena remains attached to its parent after the child call returns.
  The parent destroys all attached child arenas recursively when the parent
  regional function exits.
- Values allocated in a child arena may therefore be returned to and used by
  the parent without promotion. They may not outlive the parent arena.
- Fixed-size locals remain ordinary value/stack storage.
- `alloc`, string concatenation, variable-sized compiler temporaries, and other
  realm-aware allocations use the active arena.
- `raw_alloc` always means explicit raw-heap allocation.
- `flex f` uses its caller's active allocator and realm restrictions.
- Pure inline values leave a regional function by value-copy into caller-owned
  return storage. Their language type does not change.
- Arena-derived pointers may move within their owning regional tree, including
  from a child call to an ancestor regional function. They may not escape that
  tree through returns, globals, outer-scope assignments, interfaces, variants,
  tuples, aggregates, or closures without an explicit lifetime transfer.

### Promotion

- Promotion exists specifically to move data out of a dying arena.
- Promotion does not apply to raw heap, external, MMIO, borrowed, or GC data.
- Promotion performs a deep clone of arena-owned object graphs.
- Deep cloning preserves aliases and cycles through an old-address to
  new-address clone context.
- Raw, external, MMIO, and borrowed pointers reachable from the graph are not
  followed as owned arena edges.
- The type/provenance system must distinguish arena-owned edges from raw or
  borrowed pointers before deep promotion is considered complete.

Because child arenas share the remaining lifetime of their parent, promotion
into the parent is unnecessary and has no separate syntax. Escaping the entire
regional tree still requires explicit deep promotion to a longer-lived,
implemented memory strategy.

### GC

- v0.1 uses a precise, non-moving, stop-the-world tracing mark/sweep collector.
- One process-wide GC heap is owned and mutated by one OS thread. Any number of
  `gc f` functions may use that heap on its owning thread.
- GC references may not cross to another thread in v0.1. Other threads may run
  stack, regional, and dynamic code without participating in GC pauses.
- Collection is cooperative. It may begin only on a GC allocation slow path or
  an explicit `gc_collect()` call, never from a timer, signal, arbitrary loop,
  raw allocation, or arena allocation.
- Functions that cannot reach a GC safepoint have no GC polls, root frames, or
  barriers. A normal function may borrow GC data while it remains unable to
  allocate or call GC-capable code.
- The compiler emits precise type descriptors and shadow-stack roots for GC
  references live across safepoints, including intermediate expression values.
- The full-heap, non-generational v0.1 collector uses no read or write barriers.
- Objects never move, so a rooted GC object has a stable address for slices and
  the duration of an FFI borrow.
- The runtime has exact root handles internally. A public language facility for
  retaining GC references in arbitrary foreign/raw storage is outside v0.1.
- Current realm calls prevent regional code from reaching GC-capable code, so
  regional arenas cannot contain live GC edges at a safepoint and are not
  scanned as GC root containers.
- v0.1 has no finalizers, weak references, public `gc_free`, compaction,
  generations, incremental marking, or concurrent marking.
- Executable GC allocation uses the implemented tracked heap. `gc f`, precise
  tracing, explicit collection, and arena-to-GC promotion are executable.

### Names and interfaces

- Generated internal method names may be changed freely to prevent collisions.
- Mangling will include enough module/interface/concrete-type/method identity
  to be unique.
- Only explicit external or link-name declarations form a stable foreign ABI.
- Interface conformance requires exact receiver, parameter, return,
  fallibility, and realm signatures.
- Error sets are nominal. Unrelated sets are incompatible without an explicit
  conversion.
- Unresolved types, fields, methods, and calls are hard errors. Platform mocks
  must use explicit declarations such as `extern`.

### Unsafe and pointers

- Runes follows a Rust-style unsafe boundary, with Go-style explicit isolation
  of non-portable facilities.
- Raw-pointer dereference, unchecked pointer arithmetic, volatile/MMIO access,
  inline assembly, unsafe foreign calls, and integer-to-pointer construction
  require `unsafe`.
- Taking an address and passing a typed pointer do not inherently require
  `unsafe`.
- Normal `*T` pointers are non-null.
- Nullable pointers use `?*T` and the `null` value.
- Optional pointers should retain pointer-sized representation when the target
  ABI permits it.
- Explicit C/raw pointer forms may be introduced for FFI and freestanding
  address-zero requirements rather than weakening normal `*T`.

### Strings and characters

- `str` is a length-bearing UTF-8 byte string, not a NUL-terminated language
  value.
- Strings support embedded NUL bytes.
- String allocation follows the active memory realm.
- `char` is a Unicode scalar value represented as `u32`.
- FFI conversion to and from NUL-terminated C strings is explicit.

### Text output

- `print(value, ...)` writes each formatted argument consecutively in source
  order and then writes exactly one newline.
- `print` never inserts spaces, commas, or other separators between arguments.
  All separators are explicit programmer-supplied string arguments.
- Exact output without a trailing newline uses the standard stream `write`
  API rather than a special `print` mode.

### Systems target

- The first concrete systems/backend target is GCC or Clang on Linux x86-64.
- Supported `volatile`, layout, section, symbol-name, calling-convention, and
  interrupt features must be emitted correctly for that target.
- Unsupported attributes or targets are rejected; they are never silently
  ignored.

### Bounds and arithmetic

- Safe array and slice indexing always performs a runtime bounds check.
- An out-of-bounds access traps with a deterministic runtime failure.
- Entering an `unsafe` block does not silently disable bounds checks.
- Unchecked indexing is a separately named operation and requires `unsafe`.
- Ordinary integer arithmetic has deterministic, safety-checked overflow
  semantics in every build mode and traps on overflow.
- Wrapping, saturating, and checked-result arithmetic are separate explicit
  operations. Release mode must not change their meaning.

### Nested functions

- Nested functions may capture bindings from enclosing lexical scopes in
  v0.1.
- Captures borrow by reference by default and may mutate a mutable captured
  binding.
- Borrowing closures may not escape the lifetime of any captured binding.
- Explicit `move f` closures capture values into their environment and may
  escape when the environment's realm and every captured value permit it.
- Closures are first-class values and may be passed, stored, and returned when
  their lifetime permits it.
- Closure environments follow the active memory realm and participate in
  escape analysis and arena promotion.
- Capturing a binding must never silently extend an invalid stack or arena
  lifetime.
- Arena-backed closures may move within their regional tree but require deep
  promotion to escape it.

### Generics

- v0.1 includes generic functions, structs, variants, and methods.
- Generics use compile-time monomorphization; v0.1 has no runtime generic type
  erasure.
- Type parameters may use exact interface constraints so generic bodies can
  rely only on declared operations.
- Instantiations receive deterministic, collision-free internal symbols and
  participate normally in modules, visibility, realm checking, and recursion.
- Higher-kinded types, specialization, and variance are outside v0.1.
- Const generics are not required for v0.1; slices cover variable-length
  algorithms without parameterizing fixed-array lengths.

### Slices

- `[]T` is a compiler-known, non-owning mutable slice represented by a pointer
  and length. `[]const T` is its read-only form.
- Slice representation has no hidden allocation or ownership.
- The compiler implements fixed-array-to-slice coercion, sub-slicing, checked
  indexing, mutability rules, and pointer/lifetime provenance.
- Standard-library generic code provides slice algorithms and views.
- A slice may not outlive its referenced stack, arena, heap ownership, or
  external storage contract.

### Pipelines

- The former linear `lex | parse | resolve` pipeline proposal is not part of
  v0.1 and will not be restored.
- A future pipeline design will model logical system wiring with branching
  while allowing the topology to be read as a straight-line declaration.
- Pipeline syntax and semantics remain deferred until that branching/dataflow
  model is designed; v0.1 reserves no incomplete runtime behavior for it.

### Modules and parsing

- Filesystem modules support both `name.runes` and `name/mod.runes`.
- Resolution order and duplicate-module diagnostics must be deterministic.
- `pub` visibility is enforced across module boundaries.
- Expressions are newline-insensitive inside delimiters and after incomplete
  operators.
- Unambiguous newlines terminate statements, preserving semicolon-free source
  without formatting-sensitive surprises.

### Verification

- Hardening adds lexer/parser fuzzing, malformed-input recovery tests,
  randomized type/property tests, generated-C differential tests, sanitizer
  execution, and regression tests for every fixed issue.
- These become maintained project gates rather than one-time scripts.

## Pending Decisions

None in the current hardening set.

## Implemented Hardening Order

1. Establish new regression/fuzzing infrastructure and strict unresolved-type
   diagnostics.
2. Fix symbol mangling and exact interface/error-set checking.
3. Implement pointer provenance, optional/non-null pointers, and unsafe gates.
4. Integrate runtime parent/child arenas and realm-aware allocation.
5. Implement escape analysis and deep arena promotion.
6. Implement checked bounds and arithmetic plus explicit unchecked/wrapping,
   saturating, and checked-result operations.
7. Implement monomorphized generics and compiler-known slices.
8. Replace C strings with length-bearing UTF-8 strings and Unicode `char`.
9. Emit or reject systems ABI features correctly on Linux x86-64.
10. Implement filesystem modules, visibility, and newline-quality rules.
11. Implement first-class borrowing and move closures.
12. Finish differential hardening and bootstrap-readiness validation.
