#!/bin/bash
set -u

PASS=0
FAIL=0
OUT_DIR="/tmp/runes-codegen-inventory.$$"
mkdir -p "$OUT_DIR"
trap 'rm -rf "$OUT_DIR"' EXIT

echo "=== EXECUTABLE GENERATED C INVENTORY ==="
for source in src/tests/samples/*.runes; do
  name=$(basename "$source")
  case "$name" in
    core_codegen_*.runes|core_print_builtin.runes|core_string_ordering.runes)
      ;;
    *)
      continue
      ;;
  esac
  if head -1 "$source" | grep -q '^-- EXPECT FAIL: '; then
    continue
  fi

  context=()
  if ./runes "$source" >/dev/null 2>&1; then
    context=("$source")
  else
    context=(src/tests/fixtures/sample_prelude.runes "$source")
  fi

  c_file="$OUT_DIR/${name%.runes}.c"
  log_file="$OUT_DIR/${name%.runes}.log"
  if ! ./runes "${context[@]}" --emit-c "$c_file" >"$log_file" 2>&1; then
    echo "FAIL $name (C emission)"
    sed -n '1,6p' "$log_file" | sed 's/^/  /'
    FAIL=$((FAIL + 1))
    continue
  fi
  if ! ${CC:-gcc} -std=c11 -Wall -Wextra -Werror -c "$c_file" \
      -o "$OUT_DIR/${name%.runes}.o" >"$log_file" 2>&1; then
    echo "FAIL $name (C compilation)"
    grep -m 6 -E 'error:|warning:' "$log_file" | sed 's/^/  /'
    FAIL=$((FAIL + 1))
    continue
  fi
  echo "PASS $name"
  PASS=$((PASS + 1))
done

echo
echo "Generated C compiled: $PASS"
echo "Failed: $FAIL"
test "$FAIL" -eq 0
