# Runes for Zed

The local Zed integration provides `.runes` file detection, syntax highlighting,
comment toggling, bracket matching, and a Runes file icon. The icon installer
builds a local Runes variant of Zed's Material Icon Theme so that icons for
other file types continue to work.

Install it from the repository root:

```bash
make install-zed
```

The installer compiles the Tree-sitter grammar to WebAssembly and registers the
language and icon extensions in Zed's local extension index. Select **Runes
Material Icon Theme** with Zed's `icon theme selector: toggle` command, then
restart Zed if the project panel does not refresh immediately.

The language extension uses the checked-in Tree-sitter grammar. The installer
stages it as a local Git repository because Zed grammar manifests require a
repository URL and pinned revision. The icon extension uses a pinned release of
the Material Icon Theme and adds the checked-in Runes glyph. Installation
requires `clang`, `wasm-ld`, `git`, and `jq` plus network access for the icon
theme's first installation.

Validate the grammar, highlighting query, and installer with:

```bash
make test-zed
```
