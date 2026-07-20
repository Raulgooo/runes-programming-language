# Runes for Zed

This local Zed extension provides `.runes` file detection, syntax highlighting,
comment toggling, and bracket matching.

Install it from the repository root:

```bash
make install-zed
```

The installer compiles the Tree-sitter grammar to WebAssembly and registers the
extension in Zed's local extension index. Restart Zed after installation if it
is already running.

The extension uses the checked-in Tree-sitter grammar. The installer stages it
as a local Git repository because Zed grammar manifests require a repository
URL and pinned revision. It requires `clang`, `wasm-ld`, `git`, and `jq`.

Validate the grammar, highlighting query, and installer with:

```bash
make test-zed
```
