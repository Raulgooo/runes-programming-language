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
