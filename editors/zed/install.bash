#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PREFIX=${RUNES_ZED_PREFIX:-"${XDG_DATA_HOME:-$HOME/.local/share}/runes/zed"}
ZED_DATA=${RUNES_ZED_DATA_HOME:-"${XDG_DATA_HOME:-$HOME/.local/share}/zed"}
GRAMMAR="$PREFIX/tree-sitter-runes"
EXTENSION="$PREFIX/runes-extension"
INSTALLED="$ZED_DATA/extensions/installed/runes"
INDEX="$ZED_DATA/extensions/index.json"

mkdir -p "$GRAMMAR" "$EXTENSION/languages/runes"
cp "$ROOT/editors/zed/tree-sitter-runes/grammar.js" "$GRAMMAR/grammar.js"
cp "$ROOT/editors/zed/tree-sitter-runes/package.json" "$GRAMMAR/package.json"
cp "$ROOT/editors/zed/tree-sitter-runes/tree-sitter.json" \
  "$GRAMMAR/tree-sitter.json"
if [[ -d "$ROOT/editors/zed/tree-sitter-runes/src" ]]; then
  mkdir -p "$GRAMMAR/src"
  cp -R "$ROOT/editors/zed/tree-sitter-runes/src/." "$GRAMMAR/src/"
fi

git -C "$GRAMMAR" init -q
git -C "$GRAMMAR" add grammar.js package.json tree-sitter.json src
git -C "$GRAMMAR" -c user.name=Runes -c user.email=runes@local \
  commit -q --allow-empty -m "Stage Runes grammar" || true
rev=$(git -C "$GRAMMAR" rev-parse HEAD)

cp "$ROOT/editors/zed/languages/runes/"*.toml \
  "$ROOT/editors/zed/languages/runes/"*.scm "$EXTENSION/languages/runes/"
sed -e "s|@GRAMMAR_REPOSITORY@|file://$GRAMMAR|" \
    -e "s|@GRAMMAR_REV@|$rev|" \
    "$ROOT/editors/zed/extension.toml.in" > "$EXTENSION/extension.toml"

mkdir -p "$INSTALLED/grammars" "$INSTALLED/languages/runes"
clang --target=wasm32 -Os -fPIC -nostdlib \
  -I "$ROOT/editors/zed/tree-sitter-runes/wasm-include" \
  -I "$ROOT/editors/zed/tree-sitter-runes/src" \
  -c "$ROOT/editors/zed/tree-sitter-runes/src/parser.c" \
  -o "$PREFIX/tree-sitter-runes.o"
wasm-ld --no-entry --shared --export=tree_sitter_runes --strip-all \
  -o "$INSTALLED/grammars/runes.wasm" "$PREFIX/tree-sitter-runes.o"
cp "$EXTENSION/extension.toml" "$INSTALLED/extension.toml"
cp "$EXTENSION/languages/runes/"* "$INSTALLED/languages/runes/"

mkdir -p "$(dirname "$INDEX")"
if [[ ! -f "$INDEX" ]]; then
  printf '{"extensions":{}}\n' > "$INDEX"
fi
index_temp=$(mktemp "${TMPDIR:-/tmp}/runes-zed-index.XXXXXX")
jq --arg repository "file://$GRAMMAR" --arg rev "$rev" '
  .extensions.runes = {
    manifest: {
      id: "runes",
      name: "Runes",
      version: "0.1.0",
      schema_version: 1,
      description: "Runes language syntax highlighting",
      repository: "https://github.com/Raulgooo/runes-programming-language",
      authors: ["Runes contributors"],
      themes: [],
      icon_themes: [],
      languages: ["languages/runes"],
      grammars: {
        runes: {repository: $repository, rev: $rev, path: null}
      },
      language_servers: {},
      context_servers: {},
      slash_commands: {},
      snippets: null,
      capabilities: []
    },
    dev: true
  }
' "$INDEX" > "$index_temp"
mv "$index_temp" "$INDEX"

printf 'Runes syntax highlighting installed for Zed at:\n%s\n' "$INSTALLED"
printf 'Restart Zed if it is currently running.\n'
