# Implicit Realm Polymorphism, Overloads, and Specialization Plan

Status: accepted direction and active implementation plan. Milestones 0 and 1
are implemented for direct free-function `flex` calls. Milestone 2 is
implemented for free-function effective execution realms. Milestone 3 parses
and validates `in <realm>` declaration families and `except(...)`. Milestone 4
implements demanded exact-then-fallback selection for direct free-function and
method calls, including ordinary generics. Milestone 5 implements hidden
realm-qualified struct and variant identity, demanded layouts and descriptors,
constructor/parameter/return propagation, transitive aggregate
specialization, incompatible-assignment rejection, imports, and receiver
owner-realm method dispatch. Realm-specific interface semantics remain a later
milestone.

## Implemented foundation

The first compiler slice now provides:

- one centralized declared-to-effective realm mapping;
- demanded specializations for direct non-generic and generic `flex` calls;
- realm propagation through nested, recursive, imported, and aliased direct
  flex calls;
- deterministic realm-specific generated symbols;
- direct dynamic, regional, and GC lowering for explicit `alloc()` inside a
  specialized flex body;
- stack-specialization checking, so an allocating flex helper cannot bypass
  the stack allocation prohibition;
- generated-C tests proving all four realm identities and the absence of the
  general `runes_alloc` dispatcher in specialized allocation bodies.
- compile-time `when realm` blocks with optional `else`, inactive-arm pruning,
  generic flex support, and invalid-case diagnostics.
- parsed `in <realm>` and `except(...)` modifiers with canonical family links,
  duplicate/conflict validation, editor grammar support, and an explicit
  specialization boundary;
- demanded realm-overloaded function and method instances, exact-before-
  fallback selection, blacklist diagnostics, deterministic symbols, generic
  combination, import aliases, recursion, and direct allocation lowering.
- demanded realm-specific struct and variant instances with hidden semantic
  identity, generic and transitive aggregate layouts, automatic ordinary
  function signature specialization, owner-preserving returns, and
  owner-dispatched receiver methods.

Function values that erase the direct call target retain the pre-existing flex
runtime behavior for compatibility. Typed resize/release and interface
specialization remain later milestones.

## Discussion status

The agreed requirements are:

- `flex` code is implicitly specialized by its inferred concrete realm;
- routine allocation, resizing, release, and tracing differences should be
  handled by realm-aware primitives;
- small exceptional differences may use a compile-time realm block;
- realm-specific definitions are optional;
- the feature applies to types, interfaces, functions, and methods;
- callers never write or pass a realm;
- the compiler infers a concrete realm and monomorphizes it;
- a library can omit or explicitly blacklist unsupported realms;
- unavailable use is rejected statically.

The following details remain proposals until reviewed with executable
prototypes:

- the exact `when realm`, `in <realm>`, and `except(...)` spellings;
- how realm-specific container layouts flow across another execution realm;
- how an owner allocation context is represented for parent arenas;
- ambiguous merges of values with different hidden representations;
- interface and function-value ABI restrictions.

## Goal

Library authors must be able to give a type, interface, function, or method
different definitions for different memory realms. Realm-specific definitions
are optional: code that does not differ remains one ordinary definition.

Callers must never select a realm manually.

The intended application experience is:

```runes
dynamic f build_dynamic() {
    Vec<i32> values = Vec.new()
    values.push(10)
}

regional f build_regional() {
    Vec<i32> values = Vec.new()
    values.push(10)
}

gc f build_gc() {
    Vec<i32> values = Vec.new()
    values.push(10)
}
```

The source name and generic arguments are identical. The compiler infers the
realm, selects the valid definition, and monomorphizes the result.

This requirement applies uniformly to:

- types and their representations;
- interfaces and their method requirements;
- interface implementations;
- free functions;
- inherent methods;
- generic declarations;
- imported or aliased declarations;
- values passed through other generic functions.

## First-principles contract

The combined feature is:

> Implicit realm polymorphism with automatic realm-aware storage operations,
> optional compile-time blocks for small differences, and optional realm
> overloads for definitions that genuinely diverge.

The programmer defining a library may opt into realm-specific behavior. The
programmer using the library does not mention a realm at the call or type use.

Seven properties are mandatory:

1. **Ordinary definitions remain ordinary.** A type or operation that behaves
   the same everywhere needs no realm syntax and no duplicated body.
2. **Realm variants are explicit at their definition.** Different layout or
   behavior is visible to the library author and reviewer.
3. **Selection is implicit at every use.** There is no `foo<gc>()`,
   `Vec<T, regional>`, allocator argument, or runtime realm query.
4. **Unavailable realms fail at compile time.** Missing and blacklisted
   variants produce a precise diagnostic, like `alloc()` already does in a
   stack function.
5. **The selected realm is monomorphized.** Generated code contains a concrete
   definition, not a runtime tag or switch.
6. **Routine differences are automatic.** A container should not duplicate its
   full implementation merely to change allocation or release policy.
7. **Specialization scales with divergence.** A small difference uses a small
   compile-time block; a different body, contract, or layout uses a declaration
   overload.

## Why this needs a new overload marker

Existing function prefixes already mean “enter this declared function realm”:

```runes
dynamic f make_raw()
regional f parse_region()
gc f build_graph()
```

A dynamic caller may deliberately call a GC function to enter GC execution.
Realm overload selection is a different concept: it chooses an implementation
because the call is already operating under a particular effective or owner
realm.

Reusing `dynamic f` for both meanings would be ambiguous and dangerous. Adding
a second same-name declaration could silently change an existing cross-realm
call from “enter GC” to “select the dynamic overload.”

The proposal therefore uses a separate `in <realm>` modifier:

```runes
in dynamic f reserve(...)
in regional f reserve(...)
in gc f treserve(...)
```

`in gc f reserve` means:

> This is the `reserve` definition selected for GC dispatch.

It does not mean:

> Every caller should explicitly enter GC to call this function.

The exact keyword should receive a parser prototype before stabilization, but
this plan recommends `in` because it is short, applies uniformly to every
declaration kind, and keeps existing realm-entry syntax unambiguous.

## Realm vocabulary

Realm overload cases use the source-level names:

```text
stack
dynamic
regional
gc
```

`flex` is not a case. It means “inherit a concrete realm from the call
context.” `main` is not a case. Root/main owning allocation maps to the
dynamic policy.

Internally the compiler should use one concrete enum such as:

```text
EFFECTIVE_STACK
EFFECTIVE_DYNAMIC
EFFECTIVE_REGIONAL
EFFECTIVE_GC
```

No unresolved `flex` or `main` value may reach realm-overload selection or
code generation.

## Coexistence and contradiction analysis

The two accepted designs are compatible when treated as layers rather than
competing syntaxes.

### The three layers

1. **Implicit specialization and realm-aware primitives** handle the common
   case with one shared body.
2. **`when realm` blocks** handle a small local difference without duplicating
   the surrounding function.
3. **`in <realm>` declaration overloads** handle a different complete body,
   type layout, interface contract, or implementation.

Application callers see none of these choices. They use the normal declaration
name and ordinary generic arguments.

### Resolved collisions

| Potential contradiction | Resolution |
|---|---|
| Existing `dynamic f`/`gc f` versus realm overloads | Existing prefixes continue to enter a function realm; `in dynamic f`/`in gc f` are distinct overload declarations |
| Active execution realm versus a container's owner realm | Free `flex` calls dispatch from effective execution realm; ownership-sensitive receiver methods dispatch from the receiver's persistent owner realm |
| `when realm` inside an owner-dispatched method | `realm` means that specialization's dispatch realm, not blindly the lexical caller realm |
| Shared fallback versus exact overload | Exact `in <realm>` wins; the shared fallback is not instantiated for that declaration/realm pair |
| Blacklist versus an exact overload | Availability is checked first; a blacklisted realm cannot be revived accidentally by an overload |
| Realm-specific layout versus shared methods | The shared method is specialized and type-checked against each demanded layout; an incompatible layout needs an exact method overload |
| Runtime allocation realm versus owner-sensitive replacement | Fresh `alloc()` follows effective execution realm; owner-sensitive `tstorage_resize`/`release` follow the storage owner/dispatch context |
| Cross-realm container call | The value keeps its hidden owner variant; mutation either uses a valid owner context or is rejected, never silently rehomed |
| Inactive `when realm` names | The inactive branch is pruned before final resolution/type checking of that specialization |
| Stack blacklist versus stack allocation errors | An excluded declaration fails at availability selection; an allowed stack specialization may still fail if its selected body uses forbidden allocation |

### Remaining design pressure

One hard runtime/compiler issue remains: a regional value may belong to a
specific parent arena while a method is called from a nested child region.
Knowing only the enum `regional` is insufficient to allocate replacement
storage in the original owner. The eventual storage primitive needs an implicit
owner allocation context or the compiler must reject that mutation.

This is not a reason to expose a realm argument to callers. It is an
implementation capability and diagnostic problem.

## Implicit specialization of shared `flex` code

Existing `flex f` remains the normal way to write one implementation that
inherits memory policy:

```runes
flex f make_work_buffer(size: usize) = result: Buffer {
    result = Buffer.new(size)
}
```

The compiler specializes each reachable call by effective realm in addition
to ordinary generic type arguments:

```text
make_work_buffer<dynamic>
make_work_buffer<regional>
make_work_buffer<gc>
```

These are hidden specialization keys, not source syntax or runtime values.
Nested and recursive `flex` calls preserve the concrete inferred realm.

## Realm-aware operations are the first choice

A collection whose algorithm is otherwise identical should remain one body:

```runes
method Vec<T> {
    flex f treserve(self: *Vec<T>, additional: usize)
        = result: Result<usize, AllocationError> {
        usize required = capacity_for(self.len, additional)?
        self.data = resize(
            self.data,
            self.len,
            self.capacity,
            required,
        )?
        self.capacity = required
        result = Ok(required)
    }
}
```

`tstorage_resize` and `release` are typed, compiler-recognized operations. They
select dynamic, arena, or GC mechanics from the inferred dispatch/owner
context. They keep raw `free`, arena retention, and GC tracing out of ordinary
container source.

### Optional compile-time realm blocks

When only one local step differs:

```runes
when realm regional {
    record_abandoned_capacity(self.capacity)
}
```

Optional alternative:

```runes
when realm gc {
    compact_weak_entries(self)
} else {
    compact_strong_entries(self)
}
```

Rules:

- selection happens during specialization, never at runtime;
- valid cases are stack, dynamic, regional, and GC;
- flex and main are not cases;
- main/root owning policy selects dynamic;
- blocks are valid in functions and methods;
- in a free function, `realm` is the effective execution realm;
- in an ownership-sensitive receiver method, `realm` is the receiver-derived
  dispatch realm;
- the unselected arm is pruned before final resolution/type checking;
- `else` is optional and exhaustive matching is not required;
- shared statements around the block remain shared;
- an exact declaration overload should replace repeated large realm blocks.

This feature is an escape hatch between automatic primitives and full
declaration overloads.

## Proposed declaration syntax

### Types with one representation

A normal type remains unchanged:

```runes
type Buffer<T> = {
    data: ?*T,
    len: usize,
    capacity: usize,
}
```

This one representation is the fallback for every realm where the type is
available.

### Types with different representations

When representation differs, the author defines variants:

```runes
in dynamic type Cache<T> = {
    data: ?*T,
    len: usize,
    capacity: usize,
}

in regional type Cache<T> = {
    data: ?*T,
    len: usize,
    capacity: usize,
    retained_bytes: usize,
}

in gc type Cache<T> = {
    entries: []T,
    weak_entries: usize,
}
```

Application code still writes:

```runes
Cache<Item>
```

The concrete layout is inferred from the construction/use context and becomes
part of the compiler's hidden specialization identity.

No stack definition exists, so attempting to construct `Cache<Item>` under a
stack realm is a compile-time error.

### Default representation plus an override

A common definition may coexist with exact overrides:

```runes
type Table<T> = {
    data: ?*T,
    len: usize,
}

in gc type Table<T> = {
    data: []T,
    len: usize,
    traced_capacity: usize,
}
```

Selection prefers the exact realm definition. Dynamic, regional, and stack use
the common definition unless its availability is restricted; GC uses the
override.

### Methods

Methods that do not differ are written once:

```runes
method Vec<T> {
    f len(self) = result: usize {
        result = self.len
    }

    f is_empty(self) = result: bool {
        result = self.len == 0
    }
}
```

Methods that differ use same-name realm overloads:

```runes
method Vec<T> {
    in dynamic f treserve(self: *Vec<T>, additional: usize)
        = result: Result<usize, AllocationError> {
        -- Allocate replacement and release old raw storage.
    }

    in regional f treserve(self: *Vec<T>, additional: usize)
        = result: Result<usize, AllocationError> {
        -- Allocate replacement in the owning arena; do not individually free.
    }

    in gc f treserve(self: *Vec<T>, additional: usize)
        = result: Result<usize, AllocationError> {
        -- Allocate traced replacement; old storage becomes collectible.
    }
}
```

The caller always writes:

```runes
values.reserve(16)
```

If the common algorithm can be expressed using realm-sensitive primitives,
there should be only one `flex` method:

```runes
method Vec<T> {
    flex f treserve(self: *Vec<T>, additional: usize)
        = result: Result<usize, AllocationError> {
        self.data = resize(
            self.data,
            self.len,
            self.capacity,
            required_capacity(self.len, additional)?,
        )?
        result = Ok(self.capacity)
    }
}
```

Exact `in <realm>` methods may override a shared `flex` fallback when a realm
really needs a different algorithm.

### Free functions

Realm-overloaded functions use the same rule:

```runes
in dynamic f compact(cache: *Cache) {
    -- Raw-owner implementation.
}

in gc f compact(cache: *Cache) {
    -- GC implementation.
}
```

Calls remain:

```runes
compact(cache)
```

A `flex f` with the same signature may provide a shared fallback. Exact
variants win over the fallback.

Existing concrete realm-entry functions remain valid and separate:

```runes
gc f enter_collector_work() {
    -- This declaration explicitly enters a GC function as it does today.
}
```

### Interfaces

An interface with one contract remains ordinary:

```runes
interface Collection<T> {
    f len(self) = result: usize
}
```

If the contract differs:

```runes
in dynamic interface Growable<T> {
    f reserve(self, capacity: usize)
    f release(self)
}

in gc interface Growable<T> {
    f reserve(self, capacity: usize)
    f trace_count(self) = result: usize
}
```

Implementations use matching inferred variants:

```runes
in dynamic method Growable<T> for Vec<T> {
    f reserve(self, capacity: usize) { ... }
    f release(self) { ... }
}

in gc method Growable<T> for Vec<T> {
    f reserve(self, capacity: usize) { ... }
    f trace_count(self) = result: usize { ... }
}
```

The source type remains:

```runes
Growable<Item>
```

Interface values cannot erase incompatible realm layouts into one ABI without
a runtime tag. Version one therefore monomorphizes interface use by inferred
realm and rejects cases where the realm cannot remain statically known.

## Availability and blacklisting

### Availability by definition

If a declaration family contains only exact variants, the available realms are
the defined variants:

```runes
in dynamic type SharedCache<T> = { ... }
in gc type SharedCache<T> = { ... }
```

`SharedCache<T>` is automatically unavailable in stack and regional contexts.
No separate blacklist is required.

### Explicit blacklist for shared definitions

Sometimes one implementation is shared across several realms but invalid in a
few. The proposed restriction modifier is:

```runes
except(stack, regional)
type SharedCache<T> = {
    ...
}
```

The same modifier applies uniformly:

```runes
except(stack)
flex f allocate_table<T>() = result: Table<T> {
    ...
}
```

```runes
except(stack, regional)
interface ConcurrentQueue<T> {
    ...
}
```

```runes
method ResourcePool<T> {
    except(stack)
    flex f grow(self: *ResourcePool<T>) {
        ...
    }
}
```

Rules:

- valid names are stack, dynamic, regional, and GC;
- duplicates are diagnosed;
- `flex` and `main` are invalid blacklist entries;
- a concrete `in regional` definition cannot blacklist regional;
- exact definitions cannot re-enable a realm blacklisted by the declaration
  family's public contract unless the blacklist is narrowed explicitly at the
  same declaration family;
- restrictions on a type, interface, method, and called function compose by
  intersection;
- unavailable use is diagnosed before code generation;
- availability is part of a public declaration's semantic contract.

An allowlist spelling such as `only(dynamic, gc)` is intentionally deferred.
Exact variants already form a natural allowlist, and one restriction mechanism
is enough for the first implementation.

### Diagnostics

Examples:

```text
type 'SharedCache<Item>' is unavailable in the regional realm
available realms: dynamic, gc
```

```text
method 'Vec.reserve' has no stack definition
available definitions: dynamic, regional, gc
```

```text
function 'allocate_table' excludes the stack realm
declared at std/table.runes:18:1
```

Diagnostics must show source names and source realms, not hidden type
parameters or generated C symbols.

## Realm inference

### Absolute rule: no call-site realm arguments

These forms must not exist:

```runes
Vec<i32, gc>
Vec<i32>.new<regional>()
reserve<dynamic>(values)
values.reserve(in gc: 10)
```

If the compiler cannot infer one realm, it reports ambiguity. It never asks the
programmer to fix ambiguity by supplying a realm generic argument.

The programmer resolves ambiguity structurally:

- keep construction and use in a realm-known function;
- split control-flow paths;
- call a normal realm-entry function;
- promote/convert a value using an explicit ownership conversion already
  defined by the language;
- change the library boundary so one concrete specialization reaches the call.

### Construction inference

Creating a realm-overloaded type in a concrete function selects that function's
effective realm:

```runes
regional f parse() {
    Cache<Token> cache = Cache.new()
    -- Cache<Token, hidden regional specialization>
}
```

Creating it in `flex` code makes the containing function realm-polymorphic.
The compiler specializes the function separately for every demanded caller
realm.

### Value-carried inference

Once constructed, a value retains its hidden realm specialization:

```runes
dynamic f make_cache() = result: Cache<Item> {
    ...
}

regional f inspect(cache: Cache<Item>) {
    cache.len()
}
```

The cache does not become regional merely because it is observed inside a
regional function. Its hidden type specialization remains dynamic.

If an ownership-sensitive method is called, dispatch uses the receiver's
hidden realm specialization. If that operation requires an allocation context
that is unavailable from the current call, the compiler rejects it rather than
allocating in the wrong arena or silently promoting the value.

### Parameter and return inference

A function accepting a realm-overloaded type is itself specialized as needed:

```runes
f count<T>(values: Vec<T>) = result: usize {
    result = values.len()
}
```

Calls with dynamic and GC vectors may produce:

```text
count<i32, hidden dynamic Vec>
count<i32, hidden gc Vec>
```

The source signature does not expose the hidden realm.

Return specialization is inferred from the returned value and propagated to
the caller. Different realm layouts receive different generated ABI symbols.

### Method inference

Method selection first uses the nominal receiver family and ordinary generic
arguments, then the receiver's hidden realm:

```text
Vec<i32> + inferred dynamic -> in dynamic Vec.reserve<i32>
Vec<i32> + inferred regional -> in regional Vec.reserve<i32>
Vec<i32> + inferred GC -> in gc Vec.reserve<i32>
```

If no exact definition exists, the compiler considers an allowed shared
fallback. If neither exists, compilation fails.

### Free-function inference

A free realm-overload call uses:

1. a realm-specialized argument/expected result when the overload family
   declares one unambiguous controlling type;
2. otherwise the effective execution realm.

Version one should keep this conservative. If two owning arguments imply
different realm variants and no receiver identifies the destination, reject
the call as ambiguous. Prefer method form for destination-mutating operations:

```runes
destination.append(source)
```

Do not add explicit call-site dispatch syntax as an escape hatch.

### Interfaces

Interface conversion and method calls preserve the inferred realm
specialization. A control-flow merge of incompatible realm-specific interface
layouts is rejected unless a later explicit erased-interface feature defines a
runtime representation.

## Overload selection

Given a declaration family and inferred realm:

1. reject immediately if the family excludes that realm;
2. choose an exact `in <realm>` definition when present;
3. otherwise choose one compatible shared definition and specialize any
   `when realm` blocks inside it;
4. lower realm-aware primitives using the same dispatch/owner context;
5. otherwise report the available realms.

There is no runtime fallback.

An exact overload completely replaces the fallback for that declaration and
realm. The compiler does not run part of the fallback before or after the
overload. Authors who need shared setup should call an ordinary shared helper.

Conflicts are errors:

- two exact definitions for the same realm and signature;
- two equally applicable shared fallbacks;
- mismatched generic arity across one nominal type family;
- incompatible visibility across definitions intended as one public family;
- interface implementation variant without a compatible interface/type
  variant.

## Hidden semantic identity

Realm is not written as a source generic, but it still becomes part of semantic
and specialization identity.

Conceptually:

```text
canonical declaration
+ ordinary concrete type arguments
+ inferred realm variant
```

Consequences:

- dynamic and GC layouts with one source name are not assignment-compatible
  merely because their printed names match;
- a value never changes realm through ordinary assignment;
- promotion/conversion must create the destination specialization explicitly;
- aliases preserve the canonical declaration family;
- generated names include deterministic realm suffixes;
- diagnostics continue to print `Vec<i32>` plus a helpful realm note instead
  of exposing an internal `Vec<i32, __realm_gc>` spelling.

## Interaction with existing function realms

The design preserves current realm-entry semantics.

### Ordinary stack function

```runes
f len(...)
```

This remains an allocation-free stack function callable where current nesting
rules permit it. It may be ABI-specialized for realm-specific parameter
layouts without becoming an allocating realm function.

### Concrete realm function

```runes
gc f collect(...)
```

This still enters a GC function. It is not a GC overload unless separately
declared with the proposed `in gc` syntax.

### Flex function

```runes
flex f reserve(...)
```

This inherits the caller's effective realm and acts as the natural shared
fallback for behavior that can remain common.

### Realm overload

```runes
in gc f reserve(...)
```

This is selected because inference chose GC. It is not selected by an explicit
call annotation.

## Realm-sensitive storage primitives

Realm overloads should not force every container to duplicate ordinary memory
mechanics. Keep `alloc()` as the single realm-sensitive allocation operation
and add the minimum typed/fallible foundation:

```text
tstorage_allocate<T>(count)
tstorage_resize<T>(pointer, initialized, old_capacity, new_capacity)
release<T>(pointer, initialized, capacity)
```

These were placeholders until the typed-allocation API was designed. The
implemented public pairs are `allocate/tallocate`,
`allocate_array/tallocate_array`, and `resize_array/tresize_array`; the names
above remain internal compiler/runtime vocabulary.

Expected lowering:

| Dispatch/owner realm | `tstorage_resize` policy |
|---|---|
| dynamic | allocate replacement, move initialized values, free old storage |
| regional | allocate in owning arena, move values, retain old arena storage |
| GC | allocate traced sequence, move values, let old storage become collectible |
| stack | reject owning allocation |

`release` follows the owner realm:

| Owner realm | `release` policy |
|---|---|
| dynamic | release raw backing storage |
| regional | relinquish the handle; memory remains until region teardown |
| GC | relinquish the handle; memory becomes collectible when unreachable |
| stack | no owned backing allocation exists |

Element/resource cleanup is separate. Realm memory reclamation must not skip
closing files, unlocking mutexes, or running any future explicit element
cleanup contract.

Fresh `alloc()` with no owner anchor continues to use effective execution
realm. Replacement of existing owned storage uses the receiver/pointer's owner
context. If that context is unavailable—especially an ancestor arena—the
compiler rejects the operation until an implicit owner-context mechanism
exists.

The runtime must receive explicit:

- element size and alignment;
- type descriptor;
- capacity;
- initialized element count.

GC sequence tracing must not guess initialized length only from byte size.
Failure and capacity overflow must leave the original container valid.

Library authors use exact realm overloads only when behavior above those
primitives genuinely differs.

## Compiler changes

### Lexer and parser

- add `when realm <realm> { ... } [else { ... }]`;
- add `in` as a declaration modifier followed by one concrete realm;
- add `except(<realm-list>)`;
- allow `in <realm>` before type, interface, method implementation, function,
  and method-function declarations;
- preserve existing `dynamic f`, `regional f`, and `gc f` parsing;
- reject realm overload syntax on unsupported declarations;
- add recovery tests for missing realm, block, declaration, comma, and
  parenthesis.

### AST and declaration families

- record an optional overload realm separately from the existing declared
  function realm;
- add an AST node for compile-time realm blocks;
- record availability exclusions;
- group same-name canonical declarations into one realm-overload family;
- represent common fallback plus exact variants;
- clone and print the new metadata;
- clone realm blocks until a concrete specialization prunes them;
- keep canonical module/declaration identity independent of aliases.

### Effective-realm model

- introduce one concrete effective realm enum;
- centralize mappings from declared realm and caller realm;
- resolve `flex` at each demanded call;
- map root/main owning policy to dynamic;
- prevent unresolved flex/main strategies from reaching overload selection.

### Types

- extend semantic type identity with an inferred realm variant when its
  declaration family has realm-specific representation;
- keep the qualifier hidden from normal source printing;
- propagate it through pointers, arrays, slices as owners/backings require,
  variants, structs, function signatures, closures, and interfaces;
- make layout, descriptors, method lookup, and ABI names variant-aware;
- reject incompatible hidden-realm assignments.

### Monomorphization

- include inferred realm variant in specialization keys;
- specialize non-generic declarations when layout/behavior differs by realm;
- combine realm and ordinary generic specialization deterministically;
- propagate effective realm through nested and recursive flex calls;
- specialize non-generic flex declarations when realm blocks or
  realm-sensitive lowering require it;
- prune `when realm` before final resolution/type checking of each clone;
- deduplicate imports and aliases through canonical declaration identity;
- detect recursive specialization in progress;
- generate only demanded variants.

This work should probably generalize `monomorphize.c` into a specialization
engine rather than create an unrelated cloning system.

### Resolver and type checker

- collect and validate overload families;
- distinguish declared execution realm from overload applicability;
- infer construction, parameter, return, receiver, and interface realms;
- implement exact-then-fallback selection;
- define `when realm` from the selected dispatch realm;
- compose availability restrictions;
- retain existing nesting and provenance checks;
- diagnose ambiguous realm flow rather than inserting runtime selection;
- validate cross-realm mutation and arena ownership context;
- reject unsupported polymorphic function/interface erasure.

Current provenance bitsets remain useful for escape validation but are not
enough to represent a concrete hidden layout specialization. Do not use a
multi-bit provenance set as an overload choice.

### Reachability and code generation

- mark only selected realm variants reachable;
- emit deterministic realm suffixes;
- emit the selected concrete layout and body;
- emit only the selected realm-block arm;
- produce no runtime realm enum, branch, or lookup table;
- preserve source-level diagnostics and debug locations;
- ensure exact variants and fallbacks do not both emit as duplicate public ABI
  symbols.

### Runtime

- add fallible typed/sequence allocation;
- carry explicit initialized count for GC tracing;
- provide deterministic allocation failure and counters for tests;
- preserve dynamic, arena, GC, and freestanding backend separation;
- keep runtime instrumentation out of the required production ABI.

### Tooling

- LSP parsing, diagnostics, completion, hover, and definition for overload
  families;
- hover may show “GC definition selected” but call source remains unchanged;
- outline shows source definitions, not generated variants;
- Zed grammar highlights `when realm`, `in`, `except`, and realm cases;
- indentation and bracket behavior covers new declaration forms;
- rename/definition through aliases retains canonical overload identity.

## Expected file-level map

| Area | Likely files |
|---|---|
| Tokens/parser | `src/lexer.h`, `src/lexer.c`, `src/parser.c` |
| AST | `src/ast.h`, `src/ast.c`, AST printer |
| Specialization | `src/monomorphize.c`, possibly a new specialization module |
| Binding | `src/resolver.c`, `src/symbol_table.c` |
| Types/inference | `src/types.h`, `src/types.c`, `src/typecheck.c` |
| Reachability/codegen | `src/reachability.c`, `src/codegen.h`, `src/codegen.c` |
| Runtime/storage | `src/runtime.h`, `src/runtime.c`, platform allocation files |
| Standard library | `src/std/prelude.runes`, `src/std/core.runes`, later `std.alloc` and `Vec` |
| Tests | `src/tests/`, `Makefile`, fuzz corpus |
| LSP/Zed | `src/lsp/main.c`, `src/tests/lsp_test.py`, `editors/zed/` grammar and queries |
| Public docs after stabilization | memory guide/reference, syntax, semantics, feature matrix, status |

## Implementation milestones

### Milestone 0: characterize current behavior

Status: implemented for realm entry, direct flex inheritance, stack allocation
rejection, generated allocation paths, and existing regression coverage.
Broader owner-provenance characterization continues alongside later container
work.

- freeze current realm-entry and nesting behavior;
- test `flex` inheritance from every caller realm;
- record current provenance across calls, returns, aggregates, and methods;
- prove existing `dynamic f` to `gc f` cross-realm entry continues to work;
- centralize terminology and effective-realm mapping.

Exit: the new overload feature cannot accidentally reinterpret existing realm
function declarations.

### Milestone 1: effective-realm specialization of `flex`

Status: implemented for direct free functions, including generic, nested,
recursive, imported, and aliased calls. Method receivers and erased function
values are intentionally deferred to their dedicated milestones.

- introduce the concrete effective-realm model;
- include realm in specialization identity when a shared body requires it;
- specialize direct non-generic and generic flex calls;
- propagate realm through nested and recursive flex calls;
- emit deterministic realm-specialized symbols;
- generate only demanded variants.

Exit: one shared flex function called from dynamic, regional, and GC code
produces three statically concrete implementations without source realm
arguments or runtime dispatch.

### Milestone 2: compile-time realm blocks

Status: implemented for free functions using their effective execution realm.
Receiver owner-realm selection for ownership-sensitive methods remains
deferred until method/container specialization.

- parse and clone `when realm`;
- specialize it from effective realm in free functions;
- specialize it from receiver dispatch realm in ownership-sensitive methods;
- prune inactive arms before final resolution/type checking;
- support optional `else`;
- add invalid/unresolved realm diagnostics;
- prove inactive backend-only names do not poison other variants.

Exit: a small exceptional behavior can differ locally while common surrounding
code remains one body, and emitted C contains no realm condition.

### Milestone 3: declaration parsing and family validation

Status: implemented as a syntax and validation milestone. Executable selection
was deliberately rejected at this boundary; Milestone 4 now supplies it for
function and method behavior families.

- parse `in <realm>` for all proposed declaration kinds;
- parse `except(...)`;
- build canonical overload families;
- diagnose duplicates, conflicts, invalid exclusions, visibility differences,
  and inconsistent generic arity;
- update AST printing and parser fuzz seeds.

Exit: valid families have stable ASTs and invalid families have exact
diagnostics. No selection/codegen claim yet.

### Milestone 4: function and method behavior overloads

Status: implemented for direct calls dispatched from the effective execution
realm. This includes generic functions, generic methods, generic method owners,
recursion, imports/aliases, deterministic demanded emission, exact
definitions, shared fallbacks, and blacklists. Dispatch from a persistent
receiver owner realm is implemented when the receiver has the hidden
type-variant identity supplied by Milestone 5; ordinary receivers continue to
dispatch from effective execution realm.

- infer effective realm at direct calls;
- choose exact function/method definitions then shared fallback;
- skip fallback generation where an exact overload wins;
- preserve `when realm` specialization inside selected fallbacks;
- combine ordinary type generics with realm specialization;
- support nested and recursive flex calls;
- generate deterministic concrete C functions;
- reject unavailable realm calls.

Exit: one unchanged call expression selects dynamic, regional, or GC behavior
with no runtime branch and without changing existing realm-entry calls.

### Milestone 5: realm-specific type representation

Status: implemented for structs and variants. Interface representation remains
Milestone 6.

- infer type variant at construction;
- carry hidden variant through parameters and returns;
- specialize layout, descriptors, constructors, functions, and methods;
- support default representation plus exact override;
- reject incompatible assignment/merge;
- preserve the value's variant across a different execution context.

Exit: one source type name safely produces distinct dynamic and GC layouts,
and a value passed elsewhere retains its original layout.

### Milestone 6: interfaces and implementations

- select interface contract by inferred realm;
- match implementation variant to interface and concrete type variants;
- specialize direct interface calls;
- reject unknown or incompatible erased realm interfaces;
- validate imported/aliased interface families.

Exit: direct realm-specialized interfaces work across modules without runtime
realm dispatch.

### Milestone 7: typed realm-sensitive storage

Status: implemented and tested, including allocation, resize, release,
failure preservation, owner lowering, initialized-prefix publication, and
pointer-bearing forced-collection proofs; see
[GC initialized storage before `Vec<T>`](gc-initialized-storage-before-vec-plan.md).

- add fallible typed arrays;
- add checked layout/capacity operations;
- add replacement and release semantics;
- pass explicit descriptor and initialized count to GC;
- distinguish fresh effective-realm allocation from owner-context replacement;
- reject unavailable ancestor-arena mutation rather than allocating in a child;
- add failure injection and backend counters.

Exit: a shared buffer algorithm grows under dynamic, regional, and GC policies
and preserves state on every forced failure.

### Milestone 8: proof container

Status: implemented as an internal generic `Buffer<T>` compiler/runtime proof.
The proof covers pointer-bearing in-place push, pop, truncate, clear, resize,
release, and collection after locals leave scope. Public `std.vec.Vec<T>` now
uses the proven storage contract in dynamic, regional, and GC realms.

Build a small internal `Vec<T>` using:

- one ordinary API;
- one shared flex implementation for routine behavior;
- realm-aware primitives before any explicit specialization;
- `when realm` only for small exceptional steps;
- a shared representation where possible;
- exact realm type definitions only where layout really differs;
- shared methods where possible;
- exact methods only where algorithms differ;
- explicit exclusions for unsupported realms;
- no application-level allocator or unsafe calls.

Exit: the same caller syntax works in all declared realms, and use from an
excluded realm fails at compile time with the available definitions listed.

### Milestone 9: advanced flows and ABI

- closures and captured realm-overloaded values;
- recursive generic types;
- function values with one statically known realm variant;
- cross-module specialization deduplication;
- public ABI naming and separate compilation;
- decide whether any safe static form of realm-polymorphic function erasure is
  needed.

Exit: supported higher-order flows remain monomorphic; unsupported erasure is
diagnosed.

### Milestone 10: tooling and public contract

- complete LSP and Zed support;
- publish finalized syntax and semantics;
- update all reference/status/feature documentation;
- add application examples;
- mark accepted portions of this plan implemented.

## Testing and sample methodology

### Parser and AST tests

Cover every declaration form:

```text
when realm regional { ... }
when realm gc { ... } else { ... }
in dynamic type
in regional interface
in gc method Interface for Type
in dynamic f
in gc method function
except(stack, regional)
```

Also cover:

- realm blocks in free functions, methods, and generic flex bodies;
- nested realm blocks and malformed/missing arms;
- common definition plus exact override;
- multiple exact definitions;
- generic declarations;
- attributes combined with overload modifiers;
- public declarations;
- malformed realm names/lists;
- parser synchronization after errors;
- AST clone/print preservation.

### Existing-semantics regression

Before selection tests, prove:

- a dynamic function can still deliberately call a uniquely named GC
  realm-entry function;
- plain `f` remains stack/allocation-free;
- `flex f` still inherits;
- regional escape checks remain;
- stack `alloc()` rejection remains;
- promotion behavior is unchanged.

This regression specifically guards against confusing `gc f` with
`in gc f`.

### Positive executable samples

Add deterministic samples:

```text
core_codegen_implicit_flex_realms.runes
core_codegen_when_realm.runes
core_codegen_when_realm_owner_method.runes
core_codegen_realm_overload_functions.runes
core_codegen_realm_overload_methods.runes
core_codegen_realm_overload_types.runes
core_codegen_realm_overload_default.runes
core_codegen_realm_blacklist.runes
core_codegen_realm_generic.runes
core_codegen_realm_value_flow.runes
core_codegen_realm_interfaces.runes
core_codegen_realm_vec.runes
```

Required behavior:

- one flex body specializes from dynamic, regional, and GC callers;
- nested flex calls preserve the inferred concrete realm;
- a realm block selects the effective realm in a free function;
- the same block selects receiver owner realm in an ownership-sensitive method
  called from a different execution realm;
- inactive realm-block names are not resolved/emitted;
- exact function selection in all four concrete realms;
- exact method selection from receiver variant;
- shared fallback when no exact definition exists;
- exact definition preferred over fallback;
- different type field layouts and constructor behavior;
- hidden variant preserved through parameters and returns;
- generic type plus generic function plus realm specialization;
- aliases reuse canonical variants;
- excluded realms fail before runtime;
- pointer-bearing GC elements remain traced;
- regional growth never individually frees;
- dynamic growth frees exactly once.

Samples print exact short values or booleans. They must not rely on addresses,
allocator timing, or nondeterministic collection order.

### Call-site ergonomics assertions

Maintain an application-style sample with all of these forbidden:

```text
realm arguments
runtime realm queries
allocator parameters
direct raw_alloc/raw_free
direct GC runtime calls
application-level unsafe
different call names per realm
```

The sample must use the same:

```runes
Vec<Item>
Vec<Item>.new()
values.push(item)
values.reserve(count)
```

Recoverable versions keep the same inferred realm and prefix only the
operation: `Vec<Item>.tnew()`, `values.tpush(item)`, and
`values.treserve(count)`.

inside dynamic, regional, and GC functions.

Add source-level grep assertions if necessary so the sample cannot gradually
become explicit and still pass.

### Negative semantic tests

One fixture and exact diagnostic for:

- invalid stack/dynamic/regional/GC realm-block case spelling;
- flex/main used as a realm-block case;
- unresolved flex realm reaches a realm block;
- selected stack realm block performs forbidden allocation;
- missing definition in inferred realm;
- explicitly blacklisted realm;
- duplicate exact variant;
- conflicting fallback;
- concrete variant excluding itself;
- inconsistent generic arity;
- inconsistent public visibility;
- mismatched type layout assignment;
- control-flow merge of dynamic and GC variants;
- ambiguous free call driven by arguments from different variants;
- interface variant without matching implementation;
- selected stack operation allocating;
- regional storage escaping;
- mutation requiring an unavailable owning arena context;
- unresolved realm reaching specialization/codegen;
- function/interface realm erasure not supported in version one.

Every test asserts source location and primary message, not just exit status.

### Generated-C structural tests

Assert:

- no runtime realm variable or comparison;
- inactive realm-block arms are absent;
- shared flex functions receive distinct deterministic realm variants only
  when demanded;
- deterministic realm suffixes;
- only demanded variants emitted;
- distinct type layouts emitted when defined;
- exact override selected instead of fallback;
- fallback for an exact-overridden realm is absent;
- unused exact definitions absent after reachability;
- aliases do not duplicate variants;
- raw free only in dynamic selected behavior;
- regional behavior contains no individual free;
- GC allocations carry descriptor and initialized count;
- output compiles under `-Wall -Wextra -Werror`.

Do not snapshot complete generated files. Check stable semantic markers.

### Runtime instrumentation

Test-only hooks count:

- raw allocations/frees;
- arena allocations and root teardown;
- GC objects, collections, and traced sequence lengths;
- attempted and forced-failed replacements;
- initialized element copies/moves.

Assertions:

- dynamic allocation/release balances;
- regional paths perform zero raw frees;
- regional roots return to zero;
- GC retains every reachable pointer-bearing element;
- removed GC elements cease being traced when specified;
- failed growth leaves pointer, length, capacity, and contents unchanged;
- overflow performs no backend call.

### Sanitizers and stress

Run ASan/UBSan and LeakSanitizer where supported across:

- empty and one-element containers;
- repeated geometric growth;
- pointer-bearing elements;
- forced failure at every allocation point;
- clear/truncate/release;
- nested regional calls;
- forced collection between allocation, copy, and commit;
- at least 1,000 deterministic operations per realm variant.

Use arena/GC counters as well as sanitizers because retained arena memory and
collector ownership have different leak semantics from raw allocation.

### Fuzzing

Extend frontend fuzz seeds with:

- overload families;
- defaults plus overrides;
- exclusions;
- generic realm-overloaded types;
- interfaces and aliases;
- malformed modifier sequences;
- nested flex calls.

Add a model-based proof-container fuzzer comparing length, capacity, and
contents against a simple host vector while injecting allocation failures.

### Full regression matrix

For every completed milestone:

- parser, resolver, type checker, module, and codegen tests;
- existing allocation, arena, GC, promotion, and provenance samples;
- generated-C differential tests;
- sanitizer suite;
- fuzz smoke;
- LSP tests;
- Zed tests when syntax lands;
- documentation checks;
- freestanding compilation proving no hosted allocator dependency.

## Required diagnostics

Diagnostics must answer:

1. what declaration was requested;
2. which realm the compiler inferred;
3. why that realm is unavailable or ambiguous;
4. which variants are available;
5. where the relevant declaration/restriction lives.

Examples:

```text
cannot specialize 'reserve': dispatch realm is unresolved
```

```text
cannot construct 'Cache<Item>' in the stack realm
available definitions: dynamic, regional, gc
```

```text
cannot call 'Queue.push' in the regional realm
'Queue.push' excludes regional; available: dynamic, gc
```

```text
cannot merge 'Table<Item>' values with dynamic and GC representations
split the control flow or convert one value before the merge
```

```text
cannot infer one realm definition for 'append'
destination is dynamic but source is regional
use a destination method so dispatch is unambiguous
```

The remedy must never be “write a realm argument at the call site.”

## Deliberate non-goals for version one

- runtime realm reflection or tags;
- explicit source realm generic arguments;
- arbitrary runtime allocator objects;
- silent promotion/conversion between realm type variants;
- erased interface values mixing incompatible realm layouts;
- one container owning independent allocations from multiple realms;
- solving the entire safe-pointer/borrowing model;
- automatic cleanup of files, locks, and sockets merely because memory has a
  realm;
- allowing call-site realm selection as an ambiguity escape hatch.

## Review gates

Review and test after:

1. current semantics characterization;
2. direct flex specialization;
3. compile-time realm blocks;
4. overload syntax/family validation;
5. function/method overload selection;
6. type representation inference;
7. interfaces;
8. typed storage primitives;
9. proof container;
10. advanced ABI/tooling.

Do not implement the whole system as one compiler patch. At each gate, run an
application-style ergonomics sample and remove syntax or machinery callers do
not need.

## Completion criteria

The design is ready for public stabilization only when:

- realm-specific definitions are optional;
- callers never write or pass a realm;
- ordinary types and methods remain unchanged;
- shared flex code is specialized only for demanded realms;
- realm-aware primitives handle routine storage-policy differences;
- `when realm` provides a compile-time-only local escape hatch;
- types, interfaces, functions, and methods can each define exact variants;
- common definitions can exclude unsupported realms;
- missing variants are rejected at compile time;
- realm-specific layouts survive parameters and returns correctly;
- exact definitions override shared fallbacks deterministically;
- existing realm-entry functions retain their old meaning;
- generated code contains no runtime realm dispatch;
- a real `Vec<T>` proof uses one normal API in dynamic, regional, and GC code;
- dynamic cleanup, regional teardown, and GC tracing pass instrumentation and
  sanitizer tests;
- failure and overflow preserve the original container;
- module, alias, generic, interface, LSP, editor, fuzz, sanitizer,
  documentation, differential, and freestanding regressions pass.

The final proof is not merely parsing `in gc type`. It is a library author
being able to provide only the definitions that differ, blacklist unsupported
realms, and let every caller use the ordinary declaration name without knowing
how realm monomorphization works.
