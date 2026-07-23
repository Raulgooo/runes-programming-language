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
| `memory-flex.runes` | Inherits caller; otherwise raw | Determined by the effective caller realm |

`negative/stack-alloc.runes` proves that a direct `alloc()` call is rejected in
a stack function. Notice that stack code can currently call an allocating
`flex f`; without an arena or GC context, that flex allocation is raw-owned and
must be freed explicitly. Whether stack-to-allocating-flex calls should remain
legal is a separate language-design question.
