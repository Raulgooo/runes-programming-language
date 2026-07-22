#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PREFIX=${RUNES_ZED_PREFIX:-"${XDG_DATA_HOME:-$HOME/.local/share}/runes/zed"}
ZED_DATA=${RUNES_ZED_DATA_HOME:-"${XDG_DATA_HOME:-$HOME/.local/share}/zed"}
INSTALLED="$ZED_DATA/extensions/installed/runes-icons"
INDEX="$ZED_DATA/extensions/index.json"
MATERIAL_REPOSITORY=https://github.com/zed-extensions/material-icon-theme.git
MATERIAL_REV=5ec848638409e4578d9e8c8478041fcab1df15f8
MATERIAL="$PREFIX/material-icon-theme"

if [[ ! -d "$MATERIAL/.git" ]]; then
  mkdir -p "$(dirname "$MATERIAL")"
  git clone -q "$MATERIAL_REPOSITORY" "$MATERIAL"
fi
git -C "$MATERIAL" fetch -q origin "$MATERIAL_REV"
git -C "$MATERIAL" checkout -q "$MATERIAL_REV"

mkdir -p "$INSTALLED/icon_themes" "$INSTALLED/icons"
cp "$ROOT/editors/zed/icon-extension.toml" "$INSTALLED/extension.toml"
cp -R "$MATERIAL/icons/." "$INSTALLED/icons/"
cp "$ROOT/editors/zed/icons/runes.svg" "$INSTALLED/icons/runes.svg"

jq '
  .name = "Runes Material Icons"
  | .author = "Zed Industries and Runes contributors"
  | .themes |= map(
      .name = "Runes Material Icon Theme"
      | .file_icons.runes = {path: "./icons/runes.svg"}
      | .file_suffixes.runes = "runes"
    )
' "$MATERIAL/icon_themes/material-icon-theme.json" \
  > "$INSTALLED/icon_themes/runes-icons.json"

mkdir -p "$(dirname "$INDEX")"
if [[ ! -f "$INDEX" ]]; then
  printf '{"extensions":{}}\n' > "$INDEX"
fi
index_temp=$(mktemp "${TMPDIR:-/tmp}/runes-zed-index.XXXXXX")
jq '
  .extensions["runes-icons"] = {
    manifest: {
      id: "runes-icons",
      name: "Runes Icons",
      version: "0.1.0",
      schema_version: 1,
      description: "Material file icons with a dedicated Runes source icon",
      repository: "https://github.com/Raulgooo/runes-programming-language",
      authors: ["Runes contributors"],
      themes: [],
      icon_themes: ["icon_themes/runes-icons.json"],
      languages: [],
      grammars: {},
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

printf 'Runes file icons installed for Zed at:\n%s\n' "$INSTALLED"
printf 'Select "Runes Material Icon Theme" in Zed, then restart if needed.\n'
