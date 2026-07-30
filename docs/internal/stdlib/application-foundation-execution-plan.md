# Application Foundation Execution Plan

Status: active implementation plan. Resolved decisions are binding for this
implementation cycle; unresolved public API decisions remain non-normative.

Owner: Runes project maintainer.

Scope: the shortest complete path from the current bootstrap standard library
to comfortable hosted command-line application development.

This plan refines milestones 3 and 6 through 10 of the broader
[application-readiness plan](app-readiness-plan.md). It deliberately stops
before networking, concurrency, graphics, ML, and a general collection suite.

## 1. Target outcome

At the end of this plan, a normal hosted application can:

- accept command-line arguments and inspect its environment;
- manipulate borrowed and owned UTF-8 text;
- format values and parse user input;
- perform bounded and buffered terminal I/O;
- manipulate paths and safely open, read, write, seek, flush, and close files;
- use monotonic time, wall time, sleep, and secure operating-system randomness;
- report portable, useful errors;
- use `Vec<T>` and `String` without application-level pointer operations;
- compile and run without importing `std.os.linux`;
- clean up every owned allocation and operating-system handle explicitly;
- pass the same portable behavior tests against real Linux and deterministic
  fake backends.

“Application-ready” in this plan does not mean that Runes has every familiar
standard-library facility. It means one nontrivial CLI can be maintained
without raw syscalls, raw allocation, application-level `unsafe`, or bespoke
text and file infrastructure.

## 2. Current baseline

The plan begins from features already implemented and tested:

- `Option<T>` and `Result<T, E>`;
- portable `IoError`, `ParseError`, and `AllocationError` identities;
- byte-slice fill, copy, equality, search, and prefix operations;
- explicit targets and compile-time `#[cfg]`;
- hosted Linux x86-64 terminal I/O behind portable `std.io` signatures;
- deterministic fake syscall coverage;
- typed realm-sensitive allocation and initialized-prefix publication;
- realm-aware `Vec<T>` under dynamic, regional, and GC ownership;
- associated constructors and ordinary/recoverable `t` operation pairs;
- lexical `defer`;
- runtime UTF-8 validation, scalar-boundary, decode, and encode primitives.

The following limitations shape every milestone:

- owning values and OS handles remain copyable because move-only enforcement
  does not exist;
- deterministic destructors do not exist;
- method and field privacy are incomplete;
- `str` and slices are non-owning views;
- generic method values and realm-specific interfaces have limitations;
- the only complete hosted platform backend is Linux x86-64;
- stack code cannot construct owning realm-sensitive collections;
- the compiler emits C11 rather than native object code.

None of these facts may be hidden by library documentation.

## 3. Non-goals

Do not expand this plan to include:

- `HashMap`, `HashSet`, trees, deques, or a complete iterator framework;
- sockets, DNS, HTTP, TLS, or async execution;
- threads, atomics, or synchronization;
- graphics or window-system bindings;
- Unicode grapheme segmentation, normalization, locale collation, or full
  case mapping;
- compile-time checked format strings;
- automatic resource destruction;
- a Windows, macOS, or Runes OS backend;
- a native compiler backend.

Those are later work. A milestone may create a clean extension seam for them,
but it must not implement them speculatively.

## 4. Dependency order

```text
M0 contracts and test harness
 |
 v
M1 borrowed text
 |
 +--------------------+
 v                    |
M2 owning String      |
 |                    |
 +----------+---------+
            v
       M3 formatting
            |
       M4 parsing
            |
       M5 I/O contracts and buffering
            |
       M6 paths
            |
       M7 files
            |
       +----+----------------+
       v                     v
M8 args/environment     M9 time/randomness
       |                     |
       +----------+----------+
                  v
          M10 regression CLI
```

Milestones merge only after their exit gates pass. Later milestones may be
prototyped, but their public APIs cannot be stabilized against an unfinished
dependency.

Current checkpoint: M0 through M7 are complete. M8 arguments, environment, and
selected process basics is next. Directory iteration remains an explicit M7
follow-up rather than part of the initial file gate.

## 5. Design governance

### 5.1 Maintainer decisions

Any unresolved choice affecting public syntax, semantics, ownership,
portability, error behavior, or naming is a maintainer decision.

The implementation process must:

1. record the issue in the decision register at the end of this document;
2. state concrete alternatives and consequences;
3. give a recommendation, clearly labeled as a recommendation;
4. ask the maintainer before changing public source or normative docs;
5. record the answer and affected files;
6. add tests that lock the chosen behavior.

An implementation convenience is not permission to settle a language or
library design question.

### 5.2 Changes that do not require a design decision

Work may proceed without a new decision when it:

- implements an already approved, documented contract;
- adds missing boundary, failure, sanitizer, or regression coverage;
- fixes behavior that contradicts the approved contract;
- refactors private code without changing observable behavior;
- improves comments or documentation accuracy.

If apparently private work leaks into a public ABI, error identity, allocation
policy, or ownership rule, it becomes a design decision.

### 5.3 API proposal lifecycle

Every new module passes through:

1. **Contract draft**: names, inputs, outputs, invariants, complexity, failure,
   ownership, realm behavior, platform behavior.
2. **Maintainer approval**: all open public choices resolved.
3. **Skeleton**: module structure and compile-only API examples.
4. **Reference implementation**: smallest correct backend.
5. **Adversarial testing**: failures, boundaries, fake backend, sanitizers.
6. **Public documentation**: only implemented behavior described as current.
7. **Dogfood use**: consumed by the regression CLI.
8. **Stabilization review**: remove accidental APIs before declaring the
   milestone complete.

## 6. Common library contracts

### 6.1 Ordinary and recoverable operations

The existing rule remains:

- an ordinary operation is concise and invokes the documented non-returning
  policy when the only alternative is exposing a recoverable resource failure;
- prefixing that operation directly with `t` exposes its recoverable form;
- the two forms share one implementation.

This convention does not automatically apply to naturally fallible operations.
Parsing malformed input, opening a missing file, and reaching EOF are ordinary
domain outcomes, not “try variants” of terminating operations. Their exact
error types require maintainer approval.

### 6.2 Ownership

Until move-only enforcement exists:

- every owning type documents that copying duplicates an ownership handle;
- constructors identify who must call `deinit` or `close`;
- consuming-looking operations cannot pretend the compiler invalidates the
  source;
- examples use `defer` immediately after successful acquisition where useful;
- cleanup is idempotent on the same cleared value when the representation can
  support it;
- tests include repeated cleanup and stale-copy hazards;
- safe APIs never create two owning handles accidentally.

### 6.3 Failure guarantees

Every mutating operation states one of:

- **no mutation on failure**;
- **basic guarantee**: invariants and existing logical values remain valid,
  but capacity or other nonlogical state may change;
- **destructive failure**: explicitly named and avoided in foundation APIs.

Tests verify the stated guarantee with deterministic failure at every internal
allocation or backend step.

### 6.4 Realms

Owning text and buffers follow `Vec<T>`:

- callers do not write a realm argument;
- `flex` implementation specializes from effective or persistent owner realm;
- dynamic, regional, and GC behavior uses typed storage operations;
- stack construction fails at compile time;
- nested regional owner mismatches are recoverable where the API is
  recoverable;
- GC tests use pointer-bearing values whenever the representation can contain
  them.

OS resources such as files are not memory-realm allocations. Their kernel
handle lifetime remains explicit in every realm.

### 6.5 Platform layering

```text
application API
    -> portable std module
        -> private backend contract
            -> std.os.linux
```

Portable signatures must not expose:

- Linux errno values;
- Linux descriptors;
- syscall numbers;
- Linux-only flag layouts;
- C library ownership assumptions.

The fake backend implements the private contract, not a second public API.

## 7. Testing architecture

Testing is part of each contract, not a cleanup phase.

### 7.1 Test layers

| Layer | Purpose |
|---|---|
| Compiler-positive sample | Prove public Runes syntax, imports, inference, methods, and realms |
| Compiler-negative sample | Lock diagnostics for invalid ownership, targets, boundaries, and types |
| Generated-C build | Compile emitted C11 with `-Wall -Wextra -Werror` |
| Runes behavior test | Verify values, state changes, and errors through public APIs |
| C runtime unit test | Verify low-level UTF-8 or ABI helpers independently of stdlib wrappers |
| Deterministic fake backend | Force every short operation, interruption, error, and invalid backend response |
| Real Linux integration | Verify actual kernel ABI behavior |
| Property/differential test | Compare broad generated cases to an independent oracle |
| Sanitizer test | Detect invalid memory, undefined behavior, leaks, and double release |
| Fuzz test | Exercise parsers, UTF-8, paths, and state machines with malformed input |
| Documentation example | Keep advertised syntax executable |
| Regression application | Prove the modules compose in a real workflow |

### 7.2 Required case classes

Every applicable API covers:

- empty input;
- one element or byte;
- ordinary small input;
- boundary value;
- just below and just above a boundary;
- maximum representable arithmetic;
- invalid encoding or syntax;
- embedded NUL;
- allocation failure at each allocation point;
- interrupted backend operation;
- partial backend operation;
- zero progress;
- backend reporting more data than requested;
- repeated cleanup;
- use on every supported realm;
- unsupported target rejection.

### 7.3 Independent oracles

Tests must not calculate expectations through the code under test.

- UTF-8 vectors come from fixed byte/scalar tables plus independently encoded
  cases.
- integer parsing/formatting vectors include hand-calculated extrema and can
  be differentially checked against C only where C semantics match the
  approved Runes contract.
- path tests use explicit expected components rather than the same splitting
  helper.
- fake I/O scripts prescribe exact operations and results.
- Linux constants continue to be checked against installed UAPI headers.

### 7.4 Failure injection

Each owning or I/O module exposes test-only control through runtime/fixture
symbols, never through the public application API.

For an operation with `N` internal failure points, run `N + 1` scenarios:

- fail before step 1;
- fail before each later step;
- allow all steps to succeed.

After each failure assert:

- the documented state guarantee;
- no leaked dynamic allocation or descriptor;
- no double cleanup;
- valid initialized-prefix metadata;
- subsequent valid operations still work when promised.

### 7.5 Sanitizers and leak checks

Every milestone containing ownership or pointer manipulation must run:

- AddressSanitizer;
- UndefinedBehaviorSanitizer;
- leak detection where supported;
- generated C with warnings promoted to errors.

GC and regional tests also verify backend object/arena counters because a
generic leak detector cannot express their ownership policy.

### 7.6 Fuzzing

Add seed corpus entries for every grammar or data shape introduced. Fuzz:

- UTF-8 validation and scalar decoding;
- borrowed slicing and splitting;
- integer and float parsing;
- format specifications if a runtime specification syntax is approved;
- path component scanning;
- buffered-reader state transitions.

Each discovered crash becomes a minimized permanent regression input.

### 7.7 Performance checks

Performance checks are guardrails, not microbenchmark theater:

- borrowed text operations allocate zero bytes;
- fixed-buffer formatting allocates zero bytes;
- buffered line reading does not issue one syscall per byte;
- `String` append is amortized linear across a growing workload;
- `read_to_end` grows geometrically and checks all arithmetic;
- parsing is linear in consumed input for the supported grammar.

Generated C inspection or backend counters prove allocation/syscall claims.
Wall-clock thresholds are used only for coarse regression detection and are
not correctness gates on shared CI machines.

### 7.8 Required commands at each merge gate

At minimum:

```text
make test
make test-sanitize
make fuzz-smoke
bash docs/test.bash
bash editors/zed/test.bash
git diff --check
```

Milestone-specific targets should be introduced so a developer can run the
relevant layer quickly before the full suite.

## 8. M0: contracts and test harness

Status: M1-required portion complete. Reusable allocation-failure and
descriptor-fixture expansion remains active work for the owning and file
milestones that need it.

### Purpose

Prevent later modules from inventing incompatible error, backend, and fixture
patterns.

### Work

- establish tentative module paths for `text`, `string`, `format`, `parse`,
  buffered I/O, paths, files, process context, time, and randomness;
- define a private backend-fixture convention;
- add a reusable allocation-failure scenario runner;
- add descriptor/object counter fixtures;
- define test naming and Makefile aggregation rules;
- define how public API examples are promoted into documentation tests;
- inventory runtime UTF-8 primitives and prove their exact preconditions;
- audit `IoError` and `ParseError` for the later information requirements.

### Tests

- a deliberately failing fixture proves the harness notices leaks;
- a deliberately oversized fake-backend result is rejected;
- unsupported target samples fail during compilation;
- a no-stdlib or unused-module sample proves unused backends are not linked.

### Exit gate

- all design decisions needed by M1 are answered;
- fixture APIs are private and deterministic;
- the full current test suite remains green;
- no new application API is claimed as implemented.

## 9. M1: borrowed UTF-8 text

Status: implemented and tested (2026-07-29).

### Purpose

Make `str` useful without allocation and establish the indexing model that
every later text API depends on.

### Proposed capability groups

Observation:

- empty check;
- byte length;
- UTF-8 validity;
- scalar count.

Search:

- prefix and suffix checks;
- containment;
- first match with an explicitly documented index unit.

Views:

- checked byte-boundary substring;
- trim-start, trim-end, and trim;
- split-once;
- a split iterator or callback only if the current type system supports a
  clear allocation-free contract.

Scalar traversal:

- decode next;
- decode previous only if it is justified by a real consumer;
- scalar iterator/cursor.

### Invariants and behavior

- a language-created `str` is valid UTF-8;
- external byte storage must be validated before becoming safe text;
- every returned substring is a view into the original bytes;
- substring endpoints must be UTF-8 scalar boundaries;
- no API claims grapheme-cluster semantics;
- searching must identify whether results are byte offsets or scalar ordinals;
- no operation allocates;
- embedded NUL is ordinary text data.

### Implementation work

- create the approved public module;
- wrap existing runtime primitives where they are appropriate;
- implement simple scanning in Runes when no compiler/runtime privilege is
  required;
- keep byte-only algorithms in `std.bytes`;
- add a small cursor type if approved;
- document lifetime/provenance and invalid external-string construction.

### Test matrix

- empty, ASCII, two-byte, three-byte, and four-byte scalars;
- combining marks explicitly treated as separate scalars;
- embedded NUL;
- overlong encodings;
- surrogate encodings;
- truncated sequences;
- isolated continuation bytes;
- boundary indexes at 0, every scalar edge, inside every multibyte sequence,
  and at length;
- prefix/suffix longer than source;
- repeated and overlapping search patterns;
- trim with ASCII whitespace and the approved Unicode policy;
- property: iterating valid text and re-encoding scalars reproduces the bytes;
- property: every iterator position is a valid scalar boundary;
- generated C inspection proves no allocation.

### Exit gate

- [x] indexing and whitespace semantics are maintainer-approved;
- [x] public docs state byte/scalar behavior for every relevant operation;
- [x] malformed external input is rejected by the validation boundary without
  out-of-bounds access;
- [x] the module passes runtime vectors, property tests, sanitizer tests, fuzz
  smoke, and documentation examples.

## 10. M2: owning UTF-8 `String`

### Purpose

Provide realm-aware growable text using the proven `Vec<u8>` storage model.

### Proposed representation

The implementation should reuse the `Vec<u8>` growth and owner policy rather
than create a second allocator. The exact visible representation and whether
the type is a wrapper or a specialized container are maintainer decisions.

Approved representation:

```runes
pub type String = {
    bytes: Vec<u8>
}
```

`String` is a distinct owning wrapper, not an alias. Its `Vec<u8>` field is
temporarily visible only because field privacy is not implemented; application
code must treat it as library-private. The wrapper repeats the GC realm member
used by `Vec<u8>` so the compiler preserves persistent owner identity through
realm-specialized methods.

### Proposed capability groups

Construction:

- empty and with-capacity ordinary/recoverable pairs;
- validated copy from `str`;
- validated construction from bytes;
- no lossy byte conversion in M2. Invalid UTF-8 is rejected, and any future
  lossy API requires a separately approved replacement policy.

Observation and views:

- byte length and capacity;
- empty check;
- read-only `str` view;
- read-only byte view.

Mutation:

- reserve;
- append one Unicode scalar;
- append `str`;
- boundary-checked truncate;
- clear;
- optional append bytes only through validation.

Ownership:

- explicit `deinit`;
- no safe mutable byte view unless an API can revalidate before restoring the
  `String` invariant.

### Permanent invariant

Every live `String` contains exactly `length` initialized bytes and those bytes
are valid UTF-8. Spare capacity is not text and is not exposed as initialized
storage.

### Failure guarantees

- failed validation does not construct a live `String`;
- append preserves old text and length on failure;
- capacity may change if a later publication step fails, matching the approved
  basic guarantee;
- truncate rejects a nonboundary index without mutation;
- ordinary allocation failure follows the shared termination policy;
- recoverable `t` forms return `AllocationError`.

### Realm behavior

- dynamic, regional, and GC construction uses one source API;
- stack construction is rejected;
- regional growth preserves the persistent arena owner;
- `deinit` follows `Vec<u8>` realm release behavior;
- no runtime realm branch is emitted.

### Test matrix

- empty and requested zero capacity;
- ASCII and every UTF-8 width;
- repeated append across many reallocations;
- embedded NUL;
- maximum scalar and rejected surrogate/out-of-range values;
- valid and invalid byte construction;
- truncation at and inside multibyte sequences;
- allocation failure at construction, reserve, append, and publication;
- dynamic repeated `deinit`;
- nested regional owner rejection;
- GC collection across growth and clear;
- property: every successful public mutation leaves runtime validation true;
- ASan/UBSan/leak checks;
- generated-C realm specialization inspection.

### Exit gate

- [x] representation and lossy-conversion decisions are approved;
- [x] all construction and mutation paths preserve valid UTF-8;
- [x] the complete ownership contract is publicly documented;
- [x] formatting can target `String` without accessing its fields.

## 11. M3: formatting

Status: complete. Integer/bool/char/str/debug conversion, fixed buffers,
owning `String`, statically dispatched writers, realm crossing, failure
injection, and differential integer vectors are implemented and tested.

### Purpose

Convert values to text without relying on variadics, macros, or the compiler's
debug-only `print` builtin.

### Required destinations

- caller-provided fixed byte buffer;
- owning `String`;
- writer abstraction used later by terminal and file output.

### Initial value coverage

- all signed and unsigned integer widths;
- decimal and hexadecimal;
- `bool`;
- Unicode scalar;
- `str`;
- escaped/debug text;
- pointer formatting only if its security and portability policy is approved.

Floating-point formatting is a separate sub-gate because it requires a
specified rounding and spelling contract.

### Formatting options

- width;
- left/right alignment;
- padding;
- sign policy;
- base prefix;
- uppercase/lowercase hexadecimal.

No runtime format-string mini-language should be introduced without explicit
approval. Typed functions or a builder are acceptable candidates.

### Behavior

- fixed-buffer formatting never allocates;
- insufficient output space is represented, never overwritten;
- String formatting follows its allocation/failure convention;
- writer formatting handles partial writes through the writer contract;
- output is locale-independent;
- integer minimum values do not overflow during magnitude conversion;
- no formatter silently truncates.

### Test matrix

- zero, one, minus one, and min/max of every integer width;
- every base and prefix combination;
- width smaller/equal/larger than output;
- all alignment, padding, and sign combinations;
- empty text, embedded NUL, control characters, quotes, and backslashes;
- Unicode scalar escapes and invalid scalar rejection;
- every fixed-buffer capacity around the exact required size;
- allocation failure while growing `String`;
- short writer, interrupted writer, zero-progress writer, and writer failure;
- differential integer vectors against an independent formatter;
- generated C inspection for zero allocation on fixed buffers.

### Exit gate

- [x] API shape and float inclusion are approved;
- [x] one integer algorithm feeds fixed buffers and `String`;
- [x] no current destination-specific integer implementation has diverged;
- [x] writer integration drains the same cursor;
- [x] non-integer initial value coverage is complete;
- [x] realm, injected-failure, and differential tests pass;
- [x] short/zero-progress/interrupted writer tests pass;
- [x] current integer output vectors and failure behavior are documented and
  executable.

## 12. M4: parsing

Status: complete for the approved initial gate. Every integer width, `usize`,
lowercase bool, and exactly one Unicode scalar are implemented with strict
whole-input, allocation-free parsing and byte-positioned errors. Floating
point and partial/prefix parsing remain separate future contracts.

### Purpose

Turn external text into typed values with useful error positions.

### Initial coverage

- signed and unsigned integers with an explicit or approved default base;
- booleans;
- Unicode scalar;
- decimal floating point after its grammar is approved.

### Contract

- parsing returns consumed length when partial parsing is supported;
- indexes in diagnostics use one approved unit;
- invalid syntax, unexpected end, and overflow are distinct;
- leading/trailing whitespace policy is explicit;
- signs, prefixes, separators, exponent forms, infinities, and NaNs are each
  explicitly accepted or rejected;
- parsing is locale-independent;
- parsing allocates nothing.

### Test matrix

- empty, sign-only, prefix-only, valid minimum/maximum, and one-past-range;
- invalid digit at every position;
- every supported base;
- leading zero behavior;
- trailing junk and consumed-length behavior;
- ASCII and non-ASCII input;
- integer differential tests with independently generated big-integer
  expectations;
- float conformance vectors if floats enter the milestone;
- fuzz malformed input under sanitizers;
- linear-time regression inputs.

### Exit gate

- [x] grammar and error-position decisions are approved;
- [x] every included numeric primitive has boundary vectors;
- [x] parsing can support configuration and command-line input without hidden
  allocation.

## 13. M5: I/O contracts and buffering

Status: complete for the M5 byte-stream gate. The M7 `File` now exercises
`Closer`; owning sockets remain later work. The portable capability contract
is established here.

### Purpose

Generalize the proven terminal primitives into composable readers and writers
without exposing platform handles.

### Proposed abstractions

Approved and implemented as static generic interfaces plus concrete generic
adapters. Dispatch is monomorphized and carries no runtime vtable.

Required semantic operations:

- partial read;
- exact read;
- partial write;
- complete write;
- flush;
- seek where supported;
- close where supported.

Required adapters:

- stdin/stdout/stderr;
- immutable slice reader;
- mutable fixed-buffer writer;
- `String` writer;
- buffered reader;
- buffered writer.

### Buffered behavior

- buffer capacity is explicit or has a documented default;
- no one-syscall-per-byte behavior;
- EOF after buffered data is distinguishable from EOF before data;
- line reads have an explicit maximum;
- text line reads validate UTF-8;
- byte line reads do not;
- flush errors are observable;
- dropping/deinitializing a buffered writer does not silently discard an
  unreported error.

### Test matrix

- reads/writes of 0, 1, capacity-1, capacity, capacity+1, and many capacities;
- all partitions of a small logical operation into short backend operations;
- interruption before and after progress;
- zero-progress backend;
- EOF before data, after partial data, and exactly at boundary;
- line delimiters across buffer boundaries;
- overlong line;
- invalid UTF-8 line;
- flush success/failure and close success/failure;
- allocation failure for owned buffers (not applicable: M5 buffering is
  caller-owned and allocation-free; future owned convenience buffers must use
  the ordinary/`t` allocation convention);
- fake-backend scripted operation trace;
- real pipe integration; file integration landed with owning files in M7;
- syscall-count assertion for buffered workloads.

### Exit gate

- [x] abstraction mechanism is approved and works with ordinary Runes types;
- [x] fake and Linux standard-stream backends pass portable contract tests;
- [x] basic terminal APIs remain source-compatible;
- [x] buffering is borrowed, allocation-free, and has no hidden owned cleanup.

## 14. M6: paths

Status: complete for the M6 lexical path gate. M7 now supplies owning handles
and basic metadata/path operations. Filesystem canonicalization and directory
iteration remain follow-up work.

### Purpose

Separate lexical path manipulation from filesystem access.

### Required concepts

- borrowed `PathView`;
- owning `Path` only if it adds value beyond `String`/byte ownership;
- component iteration;
- filename and parent;
- absolute/relative detection;
- join;
- lexical normalization;
- conversion to the active platform's syscall representation.

Normalization must not claim to resolve symlinks or filesystem identity.

### Portability constraint

Linux paths may contain arbitrary non-NUL bytes, while a portable Unicode path
API would reject some valid Linux names. The approved representation preserves
arbitrary bytes. `PathView` and `Path` permit NUL because they are lexical;
`PlatformPath.tfrom_view` rejects embedded NUL and appends exactly one trailing
terminator without normalization.

### Test matrix

- empty path;
- root;
- repeated separators;
- `.` and `..`;
- trailing separator;
- hidden names;
- embedded NUL rejection at OS conversion;
- non-UTF-8 Linux bytes under the approved representation;
- checked allocation/capacity failure (a maximum-sized live slice cannot be
  safely materialized by an integration test);
- normalization never accesses the filesystem;
- round-trip platform conversion where representable;
- property: normalized output is idempotent.

### Exit gate

- [x] representation and normalization semantics are approved;
- [x] lexical code has no syscall dependency;
- [x] filesystem APIs can accept paths without application-level NUL conversion.

## 15. M7: files

Status: complete for the initial hosted Linux x86-64 gate. Directory iteration,
canonicalization, and additional hosted backends remain explicitly deferred.

### Purpose

Provide safe portable file operations over the private Linux descriptor layer.

### Proposed surface groups

Open options:

- read/write/append;
- create/create-new/truncate;
- platform-independent permissions policy where possible.

Operations:

- partial and exact read/write through M5 contracts;
- seek;
- flush/sync distinction;
- metadata;
- explicit close;
- bounded read-to-end;
- bounded read-to-string;
- create/remove/rename directories and files;
- directory iteration, possibly as a follow-up submilestone.

### Ownership

`File` owns exactly one kernel handle. Until move-only enforcement:

- copying is documented as invalid ownership duplication;
- successful close clears the handle;
- repeated close on the same cleared value is harmless if approved;
- close failure semantics state whether the handle remains owned;
- examples acquire then immediately schedule cleanup with `defer`.

### Error behavior

Portable errors need enough context to distinguish at least:

- not found;
- permission denied;
- already exists;
- invalid input/path;
- interrupted;
- would block;
- unsupported;
- other backend failure.

Whether errors carry operation, path, or native diagnostic detail is a
maintainer decision.

### Test matrix

- create, open, overwrite, append, seek, truncate, and remove;
- nonexistent, existing-with-create-new, permission denied;
- empty and embedded-NUL path;
- short reads/writes and interruption through fake backend;
- sparse/large offset where the host permits;
- EOF boundaries;
- bounded read limit reached;
- invalid UTF-8 in read-to-string;
- allocation failure during read growth;
- close failure and repeated close;
- descriptor leak count on every failure path;
- temporary-directory real Linux tests with explicit cleanup;
- ASan/UBSan plus descriptor-leak fixture.

### Exit gate

- file ownership and error-context decisions are approved;
- normal file use contains no `unsafe` or Linux import;
- fake backend and real Linux tests agree on portable behavior;
- every acquisition path has tested cleanup.

## 16. M8: arguments, environment, and process basics

### Purpose

Expose the process context required by real CLI programs.

### Required capabilities

- argument count and iteration;
- program name policy;
- environment lookup and iteration;
- portable exit status;
- process spawning/waiting only after argument and path ownership are stable;
- pipes and redirection as a later submilestone if spawning is included.

### Encoding constraint

Hosted Unix arguments and environment values may contain non-UTF-8 bytes.
Whether the portable API exposes bytes, validated text, both views, or a
fallible conversion is a maintainer decision.

### Test matrix

- no extra arguments;
- empty argument;
- spaces and shell-significant bytes passed without shell parsing;
- Unicode and non-UTF-8 bytes;
- embedded NUL impossibility documented at the OS boundary;
- missing, empty, and duplicate environment entries;
- deterministic injected startup block;
- exit success and failure observed by a parent fixture;
- if spawning is included: missing executable, child exit, signal termination,
  redirection, and cleanup on partial setup failure.

### Exit gate

- encoding and ownership policies are approved;
- a CLI can receive input without raw `argc`/`argv` declarations;
- tests do not depend on the developer's ambient environment.

## 17. M9: time and secure randomness

### Purpose

Supply deadlines, timestamps, sleeping, identifiers, and secure random bytes.

### Time concepts

- `Duration`;
- monotonic `Instant`;
- wall-clock timestamp;
- checked add/subtract;
- comparison;
- sleep with explicit interruption policy.

Wall time must never be used for elapsed deadlines.

### Randomness

- fill a caller-provided byte slice from the OS secure randomness facility;
- optionally provide integer helpers only after bias behavior is specified;
- never seed a security API from wall time;
- distinguish temporary interruption from unavailable entropy.

### Test matrix

- duration zero, arithmetic boundary, overflow, and underflow;
- monotonic nondecrease;
- fake clock exact advance;
- wall-clock conversion vectors if calendar conversion is included;
- sleep interruption and remaining-duration policy;
- random empty buffer;
- short/interrupted random backend reads;
- deterministic fake random stream;
- real Linux smoke without probabilistic assertions;
- unsupported target rejection.

### Exit gate

- time units, overflow, and interruption policies are approved;
- portable APIs hide Linux clock IDs and `getrandom` flags;
- deterministic fake tests cover all behavior.

## 18. M10: maintained regression CLI

### Purpose

Prove composition and expose ergonomic gaps before calling the foundation
ready.

### Required application

Build one nontrivial repository-owned CLI that:

- reads arguments and an environment value;
- opens an input file;
- performs buffered line or chunk reading;
- parses at least one value;
- stores data in `Vec<T>` and `String`;
- formats a result;
- writes output and diagnostics;
- uses monotonic timing or secure randomness;
- handles expected errors without panic/termination;
- cleans up every allocation and file handle;
- contains no `unsafe` and no `std.os` import.

The exact application is a maintainer decision. A small text statistics,
manifest inspection, or structured-data conversion tool would exercise the
foundation without requiring networking.

### Test matrix

- golden success cases;
- empty input;
- malformed input with stable diagnostic;
- missing file;
- permission failure where reliably injectable;
- allocation failure;
- short I/O through fake backend;
- deterministic output;
- repeated execution under sanitizers and leak checks;
- documentation walkthrough compiled as a test.

### Application-ready exit gate

The finish line is reached only when:

- the regression CLI is maintainable without privileged internals;
- all M0–M9 exit gates pass;
- public docs cover every shipped API and cleanup obligation;
- all public examples compile;
- `make test`, sanitizers, fuzz smoke, editor tests, docs tests, and diff checks
  pass from a clean build;
- generated C for the CLI compiles with `-Werror`;
- no Linux-specific type appears in its source or portable public signatures;
- known safety limitations are explicitly documented;
- the implementation-status and feature-matrix documents match reality.

## 19. Per-milestone deliverables

Every milestone produces:

1. approved design record;
2. public module source;
3. private backend source where applicable;
4. positive behavior sample;
5. expected-failure compiler samples;
6. deterministic failure fixture;
7. real-platform integration test where applicable;
8. sanitizer coverage;
9. fuzz seed and target where applicable;
10. public API reference;
11. internal implementation notes;
12. feature-matrix and implementation-status update;
13. executable documentation example;
14. clean full-suite result.

A milestone is not complete when only the happy-path source exists.

## 20. Review checklist for every public operation

Before accepting an operation, answer:

- What exact values does it accept?
- What does it return on empty input?
- Which index unit does it use?
- Does it allocate?
- Which realm owns the result?
- Who cleans it up?
- Can it copy an owning handle?
- What invalidates returned views?
- What is its time and space complexity?
- Which errors are ordinary domain errors?
- Which failures terminate, and which are recoverable?
- What state remains after failure?
- Can the backend make partial progress?
- How are interruption and zero progress handled?
- Is behavior target-dependent?
- Does it accept embedded NUL?
- Does it require valid UTF-8?
- Can arithmetic overflow?
- Can a GC safepoint observe uninitialized data?
- Is the behavior covered by a negative or adversarial test?
- Is the public documentation exact?

If any answer is unknown, implementation pauses and the issue goes to the
maintainer.

## 21. Initial decision register

Approved items are binding for the current implementation cycle. Unmarked
items remain open and must be resolved before their listed milestone.

| ID | Needed before | Decision |
|---|---|---|
| AF-001 | M1 | **Approved:** one `std.text` module |
| AF-002 | M1 | **Approved:** byte offsets; text slicing validates scalar boundaries; scalar count remains separate |
| AF-003 | M1 | **Approved:** explicitly named ASCII whitespace operations first; unqualified Unicode behavior remains reserved |
| AF-004 | M1 | **Approved:** prototype an allocation-free `ScalarCursor`; use the explicit text-plus-index function only if compiler safety blocks the cursor |
| AF-005 | M2 | **Approved:** distinct `String { bytes: Vec<u8> }` wrapper; field is temporarily visible but library-private by contract |
| AF-006 | M2 | **Approved:** no lossy construction in M2; invalid UTF-8 is rejected |
| AF-007 | M3 | **Approved by maintainer delegation:** typed functions plus explicit option structs; no runtime format-string language |
| AF-008 | M3 | **Approved by maintainer delegation:** deterministic float formatting is a later sub-gate after integer formatting |
| AF-009 | M3 | **Approved by maintainer delegation:** no pointer formatting in the initial API |
| AF-010 | M4 | **Approved:** strict whole-input parsing; no implicit whitespace; decimal default; explicit binary/octal/decimal/hex and automatic `0b`/`0o`/`0x`; no separators; optional sign with negative rejected for unsigned; lowercase bool; exactly one Unicode scalar; UTF-8 byte error offsets; no `tparse` prefix. Float, infinity, NaN, and partial parsing deferred |
| AF-011 | M3/M5 | **Approved:** static generic `Writer` interface with pointer receiver and monomorphized adapters; no runtime vtable, erased data pointer, realm tag, or dispatch allocation. Initial fixed-buffer and realm-derived String adapters land in M3; terminal/file adapters and `Reader` remain M5 work |
| AF-012 | M5 | **Approved by maintainer delegation:** partial `Reader.read` reports `Io(EndOfInput)`; exact reads report `UnexpectedEnd(completed)`; line reads distinguish EOF-before-data from a successful unterminated final line |
| AF-013 | M5 | **Approved by maintainer delegation:** caller-provided buffer capacity is explicit; zero capacity is a legal unbuffered pass-through; no hidden default allocation |
| AF-014 | M6 | **Approved by maintainer delegation:** canonical paths preserve arbitrary bytes; UTF-8 is an optional convenience; embedded NUL is rejected only at platform conversion |
| AF-015 | M6 | **Approved by maintainer delegation:** normalization is explicit and lexical; collapse separators, remove `.`, cancel prior normal components, preserve unmatched relative `..`, and discard above-root absolute `..`; never normalize implicitly before a syscall |
| AF-016 | M7 | **Approved by maintainer delegation:** `FsError` carries the operation plus a portable reason; embedded NUL carries its byte index; no hidden path copy/borrow or public raw errno |
| AF-017 | M7 | **Approved by maintainer delegation:** clear ownership before Linux close, never retry close, return `Ok(false)` on repeated close, and keep the value cleared even when close reports failure |
| AF-018 | M8 | Argument and environment encoding/ownership policy |
| AF-019 | M8 | Whether process spawning belongs before the first application-ready gate |
| AF-020 | M9 | Time unit representation, overflow policy, and sleep interruption behavior |
| AF-021 | M10 | Regression CLI application |

AF-001 through AF-004 were approved by maintainer delegation on 2026-07-29.
AF-005 and AF-006 were explicitly approved by the maintainer on 2026-07-29.
M0 through M2 may proceed. Later decisions should be discussed near their milestone
so present design is informed by the code and tests already completed.

## 22. M1 decision rationale

### AF-001: borrowed-text module organization

Options:

1. `std.text` with public functions and text traversal types.
2. Add methods directly to builtin `str`, if the language is intentionally
   extended to allow a standard module to define them.
3. Split immediately into several modules such as validation, search, and
   iteration.

Recommendation: begin with one `std.text` module. It works with current module
semantics, keeps the builtin type/compiler boundary small, and can split later
after real call patterns exist.

### AF-002: position units

Options:

1. All positions are byte offsets, and APIs that create text views require
   scalar-boundary byte offsets.
2. Search returns Unicode scalar ordinals and slicing converts or rescans.
3. Introduce distinct `ByteIndex` and `ScalarIndex` wrapper types immediately.

Recommendation: use byte offsets in the first API, name byte-sensitive
operations clearly, and validate scalar boundaries for text slicing. Scalar
count remains a separate O(n) operation. This preserves O(1) slicing and
matches the existing length-bearing runtime representation. Wrapper index
types can be reconsidered after usage demonstrates that their safety benefit
outweighs conversion friction.

### AF-003: whitespace

Options:

1. Make initial `trim` use ASCII whitespace.
2. Implement Unicode `White_Space` data before exporting `trim`.
3. Export explicitly named ASCII operations first, such as `trim_ascii`, and
   reserve unqualified `trim` for approved Unicode behavior.

Recommendation: choose option 3. It avoids making a misleading Unicode promise
and does not block configuration/CLI work. Unicode trimming can later use a
generated, versioned Unicode table.

### AF-004: scalar traversal

Options:

1. A non-owning `ScalarCursor` carrying the source view and current byte
   offset, with `next() -> Option<char>`.
2. A function taking the text plus a caller-owned byte-index pointer on every
   call.
3. A callback-based `for_each_scalar`.
4. Wait for a general iterator framework.

Recommendation: prototype option 1 as a compile-only and provenance test before
stabilizing it. It is the clearest application API and allocates nothing.
Choose option 2 as the fallback if the current aggregate-provenance or method
system cannot safely express the cursor. Do not block text work on a complete
iterator framework.

These four recommendations were approved by maintainer delegation on
2026-07-29. Implementation must now lock them with API, behavior, provenance,
and negative tests.

## 23. Recommended execution count

1. M0 contracts and harness.
2. M1 borrowed UTF-8 text.
3. M2 owning realm-aware `String`.
4. M3 formatting.
5. M4 parsing.
6. M5 I/O contracts and buffering.
7. M6 paths.
8. M7 files.
9. M8 arguments, environment, and selected process basics.
10. M9 time and secure randomness.
11. M10 maintained regression CLI and readiness audit.

Do not start the next milestone merely because the current code compiles.
Advance only when its design decisions, adversarial tests, ownership
documentation, and exit gate are complete.
