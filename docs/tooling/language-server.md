# Runes editor support and language server

Runes has a native Language Server Protocol implementation in `runes-lsp`.
It uses the compiler's lexer, parser, and AST instead of maintaining a separate
editor parser.

## Build and test

From the repository root:

```bash
make runes-lsp
make test-lsp
make test-zed
```

`make` builds both `runes` and `runes-lsp`.

## Current capabilities

| Capability | Current behavior |
|---|---|
| Synchronization | Full document updates |
| Diagnostics | Live lexer/parser syntax errors |
| Document symbols | Functions, types, fields, variants, interfaces, modules, externs, variables, and parameters from the compiler AST |
| Hover | Declaration category for symbols declared in the current document |
| Go to definition | Declarations in the current document |
| Completion | Runes keywords, realms, primitive types, and constants |

The server deliberately starts with parser-backed features. Resolver and type
checker diagnostics, cross-file navigation, signature help, semantic tokens,
rename, references, and formatting remain future milestones. Adding these
should expose structured compiler diagnostics and query APIs rather than
reimplementing semantic analysis inside the server.

## Visual Studio Code

The extension source is under `runes-lang/`. Install its dependencies and
package it with:

```bash
cd runes-lang
npm install
npx @vscode/vsce package
```

The extension starts `runes-lsp` from `PATH` by default. An explicit executable
can be configured with `runes.languageServer.path`.

VS Code receives indentation, auto-closing, surrounding-pair, and Enter-key
rules from `language-configuration.json`. Bracket-pair colorization is enabled
by default for Runes files.

## Zed

The Zed integration lives under `editors/zed/`. Its Tree-sitter grammar keeps
braces, brackets, and parentheses as nested nodes. `brackets.scm` identifies
pairs, `indents.scm` drives automatic indentation, and `outline.scm` supplies
the declarations displayed in Zed's Outline panel. The outline panel is a
Tree-sitter feature in Zed; it does not consume the LSP's `documentSymbol`
response.

Build `runes-lsp`, ensure it is on `PATH`, and run:

```bash
make install-zed
```

The Zed adapter also honors `lsp.runes-lsp.binary.path`. See
[`editors/zed/README.md`](../../editors/zed/README.md) for the complete settings
example and build requirements.

Rainbow brackets are an editor presentation setting. The extension supplies
the bracket structure, while `colorize_brackets` controls whether Zed displays
the colors.

## Protocol design

`runes-lsp` communicates over standard input/output using JSON-RPC framing. It
keeps the latest text for each open URI and reparses changed documents in a
fresh arena. Parser diagnostics are delivered through a callback added to the
compiler parser, preserving the compiler's existing command-line diagnostics
when no callback is installed.

This separation is intentional:

- Tree-sitter handles fast editor structure, indentation, and base highlighting.
- The compiler AST supplies language-aware document structure.
- Later compiler resolver and type-checker APIs will supply semantic behavior.
