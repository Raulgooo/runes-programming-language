# Runes for Visual Studio Code

The extension provides Runes syntax highlighting, automatic delimiter
insertion and indentation, bracket-pair colors and guides, and an LSP client.

Build the server from the repository root:

```bash
make runes-lsp
```

Add the repository root to `PATH`, or set `runes.languageServer.path` to the
absolute path of the resulting `runes-lsp` executable. Opening a `.runes` file
then starts the server automatically.

The current language server provides live parser diagnostics, AST document
symbols, declaration hover and go-to-definition within the current document,
and completions for language keywords and built-in types.
