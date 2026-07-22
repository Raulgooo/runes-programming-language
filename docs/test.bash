#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"

for file in docs/examples/positive/*.runes; do
  ./runec check "$file" >/dev/null
  example_name=$(basename "$file" .runes)
  executable="/tmp/runes_docs_$example_name"
  ./runec build "$file" -o "$executable" >/dev/null
  expected=$(sed -n 's/^-- EXPECT OUTPUT: //p' "$file")
  if [[ -n "$expected" ]]; then
    actual=$($executable)
    if [[ "$actual" != "$expected" ]]; then
      echo "documentation example output mismatch: $file" >&2
      echo "expected: $expected" >&2
      echo "actual:   $actual" >&2
      exit 1
    fi
  fi
done

for file in docs/examples/negative/*.runes; do
  expected=$(sed -n 's/^-- EXPECT FAIL: //p' "$file")
  if [[ -z "$expected" ]]; then
    echo "missing EXPECT FAIL marker: $file" >&2
    exit 1
  fi
  if ./runec check "$file" >/tmp/runes_docs_negative.out 2>&1; then
    echo "documentation example unexpectedly passed: $file" >&2
    exit 1
  fi
  if ! grep -Fq "$expected" /tmp/runes_docs_negative.out; then
    echo "documentation example produced the wrong diagnostic: $file" >&2
    echo "expected: $expected" >&2
    sed -n '1,12p' /tmp/runes_docs_negative.out >&2
    exit 1
  fi
done

required_reference=(
  docs/reference/README.md
  docs/reference/syntax.md
  docs/reference/semantics.md
  docs/reference/memory-and-unsafe.md
  docs/reference/modules-ffi-tooling.md
  docs/reference/implementation-status.md
  docs/feature-matrix.md
  docs/contributing-docs.md
)

for file in "${required_reference[@]}"; do
  [[ -s "$file" ]] || { echo "missing reference document: $file" >&2; exit 1; }
done

if rg -n 'u32 mask = 0b101010' docs --glob '*.md' >/tmp/runes_docs_stale.out; then
  echo "stale binary-literal example found in documentation:" >&2
  cat /tmp/runes_docs_stale.out >&2
  exit 1
fi

for term in 'return expression' 'Pointer capture' 'identity<i32>' \
            'Point(x: 0, y)' 'raw_alloc_aligned'; do
  if ! rg -Fq "$term" docs/feature-matrix.md docs/reference docs/guide; then
    echo "missing required feature coverage: $term" >&2
    exit 1
  fi
done

while IFS= read -r markdown; do
  while IFS= read -r raw_target; do
    target=${raw_target#](}
    target=${target%%#*}
    target=${target%% *}
    [[ -z "$target" ]] && continue
    case "$target" in
      http://*|https://*|mailto:*|/*) continue ;;
    esac
    if [[ ! -e "$(dirname "$markdown")/$target" ]]; then
      echo "broken Markdown link in $markdown: $raw_target" >&2
      exit 1
    fi
  done < <(grep -oE '\]\([^)]+' "$markdown" || true)
done < <(find docs -type f -name '*.md' | sort)

echo "documentation checks passed"
