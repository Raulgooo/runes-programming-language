# Runes Standard-Library Internal Documentation

These documents are the internal design and implementation workspace for the
Runes standard library. They are not the public API reference.

Read them in this order:

1. [Understanding Runes by building its standard library](building-guide.md)
   explains the language and library-design reasoning.
2. [Application-readiness plan](app-readiness-plan.md) is the active ordered
   plan for I/O, text, containers, files, processes, networking, and graphical
   application foundations.
3. [Standard-library and ecosystem roadmap](roadmap.md) records the broader
   long-term library surface.
4. [Platform boundary and portable I/O plan](platform-io-plan.md) defines how
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
- `std.os.linux` implements the initial Linux x86-64 syscall, descriptor, and
  virtual-memory boundary;
- `std.io` exists only as an empty, unexported placeholder;
- portable safe I/O, owning containers, and owning text remain future
  milestones.

Generic declarations can now be imported directly or aliased. Foundation code
can use `use std.core.Option` or `use std.core.Option as Maybe` without changing
the underlying generic type identity.
