#!/usr/bin/env bash
set -euo pipefail

runes_source=$(mktemp /tmp/runes-differential.XXXXXX.runes)
generated_c=$(mktemp /tmp/runes-differential-generated.XXXXXX.c)
runes_binary=$(mktemp /tmp/runes-differential-runes.XXXXXX)
reference_source=$(mktemp /tmp/runes-differential-reference.XXXXXX.c)
reference_binary=$(mktemp /tmp/runes-differential-reference.XXXXXX)
trap 'rm -f "$runes_source" "$generated_c" "$runes_binary" "$reference_source" "$reference_binary"' EXIT

cat >"$runes_source" <<'RUNES'
f calculate(seed: i64) = result: i64 {
    i64 value = seed
    i32 index = 0
    while index < 200 {
        value = (value * 17 + index as i64 * 31 + 7) % 1000003
        if (value & 1) == 0 {
            value = value / 2 + 19
        } else {
            value = value * 3 + 1
        }
        index = index + 1
    }
    result = value
}

f main() {
    print(calculate(1), " ", calculate(42), " ", calculate(99991))
}
RUNES

cat >"$reference_source" <<'C'
#include <stdint.h>
#include <stdio.h>

static int64_t calculate(int64_t seed) {
  int64_t value = seed;
  for (int32_t index = 0; index < 200; index++) {
    value = (value * 17 + (int64_t)index * 31 + 7) % 1000003;
    if ((value & 1) == 0)
      value = value / 2 + 19;
    else
      value = value * 3 + 1;
  }
  return value;
}

int main(void) {
  printf("%lld %lld %lld", (long long)calculate(1),
         (long long)calculate(42), (long long)calculate(99991));
  return 0;
}
C

./runes "$runes_source" --emit-c "$generated_c"
${CC:-gcc} -Isrc -std=c11 -Wall -Wextra -Werror "$generated_c" \
  src/runtime.c src/utils/arena.c -o "$runes_binary"
${CC:-gcc} -std=c11 -Wall -Wextra -Werror "$reference_source" \
  -o "$reference_binary"

test "$("$runes_binary")" = "$("$reference_binary")"
echo "generated-C differential test passed"
