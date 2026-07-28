# Executable Documentation Examples

Files under `positive/` must pass `runec check`. Files under `negative/` must
fail and place the expected stable diagnostic substring after `-- EXPECT FAIL:`
on their first line.

Run all documentation checks with:

```bash
make test-docs
```

These examples are canonical coverage for the reference. Tutorial code fences
may use shorter fragments, but should link here when they depend on substantial
surrounding declarations.

## Memory-realm examples

The five `positive/memory-*.runes` programs compare the allocation strategies:

| Program | `alloc()` behavior | Cleanup |
|---|---|---|
| `memory-stack.runes` | Direct call unavailable | Inline stack locals disappear on return |
| `memory-dynamic.runes` | Raw-owned allocation | Explicit `raw_free()` |
| `memory-regional.runes` | Current/child arena | Whole regional tree is destroyed together |
| `memory-gc.runes` | Traced GC object | Collector reclaims unreachable objects |
| `memory-flex.runes` | Specialized to dynamic, regional, or GC caller | Determined by the effective caller realm |
| `when-realm.runes` | Compile-time realm blocks | One source body, four pruned specializations |
| `realm-overloads.runes` | Exact definition, then shared fallback | Realm inferred at unchanged call sites |
| `realm-type-layouts.runes` | Hidden dynamic and GC type identities | Layout and ordinary function ABI specialize without call-site realm arguments |

`negative/stack-alloc.runes` proves that a direct `alloc()` call is rejected in
a stack function. `negative/flex-stack-alloc.runes` proves the same rule cannot
be bypassed through an allocating `flex f`: its demanded stack specialization
is rejected.
`negative/realm-overload-unavailable.runes` proves that an exact-only family is
an implicit allowlist and rejects a realm with no definition.
