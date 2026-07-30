#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
GRAMMAR="$ROOT/editors/zed/tree-sitter-runes"
TEMP=$(mktemp -d "${TMPDIR:-/tmp}/runes-zed-test.XXXXXX")
trap 'rm -rf "$TEMP"' EXIT

(cd "$GRAMMAR" && tree-sitter generate)
printf '{"parser-directories":["%s/editors/zed"]}\n' "$ROOT" \
  > "$TEMP/config.json"

HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter parse --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/src/tests/samples/01_variables.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter parse --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/src/tests/samples/core_codegen_when_realm.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter parse --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/src/std/io.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter parse --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/src/tests/samples/core_codegen_associated_methods.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter query --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/editors/zed/languages/runes/highlights.scm" \
  "$ROOT/src/tests/samples/core_codegen_interfaces.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter query --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/editors/zed/languages/runes/brackets.scm" \
  "$ROOT/src/tests/samples/core_codegen_interfaces.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter query --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/editors/zed/languages/runes/indents.scm" \
  "$ROOT/src/tests/samples/core_codegen_interfaces.runes"
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter query \
  --config-path "$TEMP/config.json" \
  "$ROOT/editors/zed/languages/runes/outline.scm" \
  "$ROOT/src/tests/samples/core_codegen_interfaces.runes" \
  > "$TEMP/outline.txt"
grep -q 'text: `Value`' "$TEMP/outline.txt"
grep -q 'text: `main`' "$TEMP/outline.txt"

RUNES_ZED_PREFIX="$TEMP/install" RUNES_ZED_DATA_HOME="$TEMP/zed" \
  RUNES_ZED_SKIP_LSP_BUILD=1 \
  "$ROOT/editors/zed/install.bash" >/dev/null
test -f "$TEMP/install/runes-extension/extension.toml"
test -f "$TEMP/zed/extensions/installed/runes/grammars/runes.wasm"
jq -e '.extensions.runes.dev == false' \
  "$TEMP/zed/extensions/index.json" >/dev/null
jq -e '.extensions.runes.manifest.lib.kind == "Rust"' \
  "$TEMP/zed/extensions/index.json" >/dev/null
grep -q '^grammar = "runes"$' \
  "$TEMP/install/runes-extension/languages/runes/config.toml"
test -f "$TEMP/install/runes-extension/languages/runes/indents.scm"
test -f "$TEMP/install/runes-extension/languages/runes/outline.scm"
grep -q '^\[language_servers.runes-lsp\]$' \
  "$TEMP/install/runes-extension/extension.toml"
bash -n "$ROOT/editors/zed/install-icons.bash"
grep -q 'viewBox="0 0 24 24"' "$ROOT/editors/zed/icons/runes.svg"
grep -q 'fill="#FA1429"' "$ROOT/editors/zed/icons/runes.svg"
