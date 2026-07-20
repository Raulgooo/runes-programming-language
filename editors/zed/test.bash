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
HOME="$TEMP" XDG_CACHE_HOME="$TEMP/cache" tree-sitter query --quiet \
  --config-path "$TEMP/config.json" \
  "$ROOT/editors/zed/languages/runes/highlights.scm" \
  "$ROOT/src/tests/samples/core_codegen_interfaces.runes"

RUNES_ZED_PREFIX="$TEMP/install" RUNES_ZED_DATA_HOME="$TEMP/zed" \
  "$ROOT/editors/zed/install.bash" >/dev/null
test -f "$TEMP/install/runes-extension/extension.toml"
test -f "$TEMP/zed/extensions/installed/runes/grammars/runes.wasm"
jq -e '.extensions.runes.dev == true' \
  "$TEMP/zed/extensions/index.json" >/dev/null
grep -q '^grammar = "runes"$' \
  "$TEMP/install/runes-extension/languages/runes/config.toml"
