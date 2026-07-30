# Runes Standard-Library Internal Documentation

These documents are the internal design and implementation workspace for the
Runes standard library. They are not the public API reference.

Read them in this order:

1. [Understanding Runes by building its standard library](building-guide.md)
   explains the language and library-design reasoning.
2. [Ordinary and recoverable `t` operations](fallible-operation-naming.md)
   records the implemented naming and failure-policy convention.
3. [Application-readiness plan](app-readiness-plan.md) is the active ordered
   plan for I/O, text, containers, files, processes, networking, and graphical
   application foundations.
4. [Application foundation execution plan](application-foundation-execution-plan.md)
   turns the CLI-application portion into exit-gated implementation and
   testing work, with explicit maintainer decision points.
5. [Standard-library and ecosystem roadmap](roadmap.md) records the broader
   long-term library surface.
6. [Platform boundary and portable I/O plan](platform-io-plan.md) defines how
   `std.io` will select an OS backend without runtime platform checks.

Public language documentation remains in the main `docs/guide/` and
`docs/reference/` trees. The APIs that work today are documented in the
[public standard-library reference](../../reference/standard-library.md).

## Current implementation checkpoint

- `std.core` implements and tests `Option<T>`, `Result<T, E>`, their initial
  methods, `UnwrapError`, and the portable `IoError`, `ParseError`, and
  `AllocationError` vocabulary;
- `std.bytes` implements and tests `fill`, exact `copy`, `equal`, `find`, and
  `starts_with`;
- `std.text` implements allocation-free borrowed UTF-8 observation, search,
  checked substring views, explicit ASCII trimming, split-once, and scalar
  cursor traversal;
- `std.os.linux` implements the initial Linux x86-64 syscall, descriptor, and
  virtual-memory boundary;
- `std.io` implements the M5 static `Reader`/`Writer` capability layer,
  exact/complete operations, slice/fixed/String/standard-stream adapters,
  borrowed buffering, bounded byte and UTF-8 line input, explicit flushing,
  and slice seeking. Fake short-operation backends, Linux pipes, realm
  specialization, zero allocation, and sanitizers are covered;
- `std.allocation` implements paired ordinary and recoverable `t` operations,
  initialized `allocate<T>(value)`, typed array storage, initialized-prefix
  publication, owner-sensitive resize, and realm-correct release for dynamic,
  regional, and GC storage;
- `std.vec` implements public realm-aware `Vec<T>` with concise ordinary
  operations, recoverable `tnew`/`tpush`/related forms, checked growth, access,
  slices, mutation, shrinking, explicit deinitialization, and pointer-bearing
  forced-GC coverage;
- `std.string` implements public realm-aware owning UTF-8 text over `Vec<u8>`,
  validated construction, transactional append, checked truncation, borrowed
  views, and explicit deinitialization;
- `std.format` has shared integer and Unicode-scalar cursor engines covering
  every integer width, bool, char, str, and escaped/debug text for fixed
  buffers, owning `String`, and statically dispatched `Writer`
  implementations. Partial/interrupted writes, invalid counts, zero progress,
  malformed UTF-8, and dynamic/regional/GC String writers are tested;
- `std.parse` strictly parses every integer width, `usize`, lowercase bool,
  and one Unicode scalar without allocation. Decimal and explicit/automatic
  bases, byte-positioned failures, all extrema, malformed input, differential
  vectors, and all execution realms are tested;
- `std.path` implements M6 arbitrary-byte `PathView`, realm-owning `Path`,
  allocation-free component traversal, explicit lexical normalization/join,
  and checked exact-byte `PlatformPath` conversion. Realm, allocation-failure,
  non-UTF-8, embedded-NUL, generated-C, and sanitizer behavior are tested;
- owning hosted Linux x86-64 files are implemented; sockets, directory
  iteration, canonicalization, and additional OS backends remain future
  milestones.

Generic declarations can now be imported directly or aliased. Foundation code
can use `use std.core.Option` or `use std.core.Option as Maybe` without changing
the underlying generic type identity.
