# Internal Language-Design Notes

This directory records open design questions that affect the Runes language,
compiler, ABI, and standard library. These notes are not normative language
documentation and must not be treated as accepted syntax or semantics.

Current notes:

1. [Value passing, zero-copy matching, and safe pointers](value-passing-and-safe-pointers.md)
   separates the `Option<T>` copying problem from the much larger question of
   pointer safety.
2. [Import ergonomics: current contract and next steps](import-ergonomics-plan.md)
   records implemented aliases and the ordered plan for canonical module
   identity, grouped imports, re-exports, and generic inference.

When a decision becomes stable, update the specification, reference,
implementation-status document, feature matrix, compiler tests, and relevant
editor tooling. Do not leave the final rule solely in this directory.
