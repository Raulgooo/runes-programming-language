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
  docs/reference/standard-library.md
  docs/reference/text.md
  docs/reference/string.md
  docs/reference/vec.md
  docs/reference/format.md
  docs/reference/parse.md
  docs/reference/io.md
  docs/reference/path.md
  docs/reference/fs.md
  docs/reference/implementation-status.md
  docs/feature-matrix.md
  docs/contributing-docs.md
  docs/internal/stdlib/application-foundation-execution-plan.md
)

for file in "${required_reference[@]}"; do
  [[ -s "$file" ]] || { echo "missing reference document: $file" >&2; exit 1; }
done

while IFS= read -r function_name; do
  if ! rg -Fq "$function_name(" docs/reference/text.md; then
    echo "public std.text function missing from text reference: $function_name" >&2
    exit 1
  fi
done < <(
  sed -n 's/^pub f \([a-z_][a-z0-9_]*\).*/\1/p' src/std/text.runes |
    sort -u
)

while IFS= read -r method_name; do
  if ! rg -Fq "$method_name(" docs/reference/text.md; then
    echo "public ScalarCursor method missing from text reference: $method_name" >&2
    exit 1
  fi
done < <(
  sed -n '/^pub method ScalarCursor {/,/^}/p' src/std/text.runes |
    sed -n 's/^[[:space:]]*f \([a-z_][a-z0-9_]*\).*/\1/p' |
    sort -u
)

for term in 'Vec<T>.new()' 'Vec<T>.tnew()' 'reserve(additional)' \
            'treserve(additional)' 'push(value)' 'tpush(value)' \
            'pop()' 'tpop()' 'truncate(n)' 'ttruncate(n)' \
            'clear()' 'tclear()' 'as_slice()' 'as_mut_slice()' \
            'deinit()' 'length <= reserved' 'OwnerUnavailable'; do
  if ! rg -Fq "$term" docs/reference/vec.md; then
    echo "missing Vec contract coverage: $term" >&2
    exit 1
  fi
done

while IFS= read -r method_name; do
  if ! rg -Fq "$method_name(" docs/reference/vec.md; then
    echo "public Vec method missing from Vec reference: $method_name" >&2
    exit 1
  fi
done < <(
  sed -n '/^pub method Vec<T> {/,/^}/p' src/std/vec.runes |
    sed -n 's/^[[:space:]]*flex f \([a-z_][a-z_]*\).*/\1/p' |
    sort -u
)

for term in 'String.new()' 'String.tnew()' 'String.from_str' \
            'String.tfrom_str' 'String.from_bytes' 'push(scalar' \
            'tpush(scalar' 'push_str(value' 'tpush_str(value' \
            'ttruncate' 'InvalidUtf8' 'InvalidBoundary' \
            'as_str()' 'as_bytes()' 'deinit' 'OwnerUnavailable'; do
  if ! rg -Fq "$term" docs/reference/string.md; then
    echo "missing String contract coverage: $term" >&2
    exit 1
  fi
done

while IFS= read -r method_name; do
  if ! rg -Fq "$method_name(" docs/reference/string.md; then
    echo "public String method missing from String reference: $method_name" >&2
    exit 1
  fi
done < <(
  sed -n '/^pub method String {/,/^}/p' src/std/string.runes |
    sed -n 's/^[[:space:]]*flex f \([a-z_][a-z_]*\).*/\1/p' |
    sort -u
)

for term in \
  'FsOperation' 'FsError' 'FileReadError' 'FileType' 'Metadata' \
  'OpenOptions' 'File' 'open(' 'metadata(' 'create_directory(' \
  'remove_file(' 'remove_directory(' 'rename(' 'read_to_end(' \
  'read_to_string(' 'sync_data(' 'sync_all(' 'set_length(' 'close('; do
  if ! rg -Fq "$term" docs/reference/fs.md; then
    echo "public std.fs surface missing from filesystem reference: $term" >&2
    exit 1
  fi
done

while IFS= read -r function_name; do
  if ! rg -Fq "$function_name(" docs/reference/parse.md; then
    echo "public std.parse function missing from parsing reference: $function_name" >&2
    exit 1
  fi
done < <(
  sed -n 's/^pub f \([a-z_][a-z0-9_]*\).*/\1/p' src/std/parse.runes |
    sort -u
)

while IFS= read -r method_name; do
  if ! rg -Fq "$method_name()" docs/reference/parse.md; then
    echo "public IntegerParse method missing from parsing reference: $method_name" >&2
    exit 1
  fi
done < <(
  sed -n '/^pub method IntegerParse {/,/^}/p' src/std/parse.runes |
    sed -n 's/^[[:space:]]*f \([a-z_][a-z0-9_]*\).*/\1/p' |
    sort -u
)

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
