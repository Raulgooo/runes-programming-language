# Runes for Zed

The local Zed integration provides `.runes` file detection, nested syntax
highlighting, automatic indentation, rainbow-bracket metadata, the Runes
language server, symbol outlines, comment toggling, bracket matching, and a
Runes file icon.

Build and install it from the repository root:

```bash
make
make install-zed
```

The installer compiles the Tree-sitter grammar and Zed LSP adapter to
WebAssembly, then registers the language and icon extensions in Zed's local
extension index. Select **Runes Material Icon Theme** with Zed's
`icon theme selector: toggle` command, then restart Zed.

The language extension uses the checked-in Tree-sitter grammar. The installer
stages it as a local Git repository because Zed grammar manifests require a
repository URL and pinned revision. The icon extension uses a pinned release of
the Material Icon Theme and adds the checked-in Runes glyph.

Installation requires `clang`, `wasm-ld`, `git`, `jq`, Cargo,
`cargo-component`, and Rust's `wasm32-wasip1` standard library plus network
access for the first build. Install the component packager with
`cargo install cargo-component --locked`. With rustup, install the target using
`rustup target add wasm32-wasip1`; Arch Linux's system Rust provides it through
the `rust-wasm` package.

`runes-lsp` must be on `PATH`. Alternatively, set its explicit path and enable
rainbow brackets in Zed settings:

```json
{
  "languages": {
    "Runes": {
      "colorize_brackets": true
    }
  },
  "lsp": {
    "runes-lsp": {
      "binary": {
        "path": "/absolute/path/to/runes/runes-lsp"
      }
    }
  }
}
```

Validate the grammar, highlighting, bracket, indentation, and outline queries,
and the local installer with:

```bash
make test-zed
```
