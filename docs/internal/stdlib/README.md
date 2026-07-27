# Runes Standard-Library Internal Documentation

These documents are the internal design and implementation workspace for the
Runes standard library. They are not the public API reference.

Read them in this order:

1. [Understanding Runes by building its standard library](building-guide.md)
   explains the language and library-design reasoning.
2. [Standard-library implementation plan](implementation-plan.md) defines the
   concrete milestones, acceptance criteria, and test requirements.
3. [Standard-library and ecosystem roadmap](roadmap.md) records the broader
   long-term library surface.

Public language documentation remains in the main `docs/guide/` and
`docs/reference/` trees. The APIs that work today are documented in the
[public standard-library reference](../../reference/standard-library.md).

## Current implementation checkpoint

- `std.core` implements and tests `Option<T>`, `UnwrapError`, `is_some`,
  `is_none`, `unwrap_or`, `unwrap`, and `map`;
- `std.bytes` implements and tests `fill`, exact `copy`, `equal`, `find`, and
  `starts_with`;
- `std.os` is exported but empty;
- `std.io` exists only as an empty, unexported placeholder;
- raw Linux I/O, safe I/O, owning containers, and owning text remain future
  milestones.

The current compiler still requires module-qualified generic use in this
layer. Code uses `use std.core` and `core.Option<T>` until imported generic
aliases are handled before monomorphization.
