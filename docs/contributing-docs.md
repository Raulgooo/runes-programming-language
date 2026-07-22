# Contributing Language Documentation

Language changes and documentation changes are one deliverable. A feature is
not complete when users must inspect compiler source or tests to discover its
syntax or restrictions.

## Required update checklist

For every user-visible language change, update each applicable item:

- lexer tokens or lexical rules;
- parser and AST forms;
- resolver, type, realm, and escape semantics;
- code generation and runtime behavior;
- positive executable test;
- negative diagnostic test;
- `docs/specv0_1.md` normative rule;
- `docs/reference/` exhaustive entry;
- tutorial chapter when beginners need the feature;
- `docs/feature-matrix.md` evidence row;
- editor grammars and highlighting;
- `docs/reference/implementation-status.md` for partial or target-dependent
  behavior.

## Example policy

Use a fenced `runes` block for syntax fragments. Put complete programs intended
for automated checking in `docs/examples/positive/`. Put examples intended to
fail in `docs/examples/negative/` with this first line:

```text
-- EXPECT FAIL: stable diagnostic substring
```

Prefer linking to a canonical example over copying a large program into several
chapters. Small duplicated fragments are acceptable when they make a rule
immediately understandable.

## Wording policy

- Use **implemented** only for behavior exercised by the current compiler.
- Use **partial** when parsing, checking, or backend support is incomplete.
- Use **planned** only for roadmap work; do not present planned APIs as usable.
- State whether a rule is compile-time, runtime, backend, platform, or library
  behavior.
- Describe traps and rejected programs explicitly.
- Do not call reserved `runes_` runtime contracts general standard-library APIs.

## Verification

Run:

```bash
make test-docs
make test
```

`test-docs` checks canonical examples, expected failures, internal Markdown
links, and selected inventories that are prone to documentation drift.

## Review questions

Before merging, reviewers should be able to answer yes to all of these:

1. Can a user find the exact syntax without reading a test?
2. Are type, lifetime, evaluation, and failure rules stated?
3. Is at least one accepted case executable?
4. Is the most important rejection tested and explained?
5. Are platform and backend limitations visible at the point of use?
6. Does the feature matrix point to the implementation and documentation?
