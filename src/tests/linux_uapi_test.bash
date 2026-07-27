#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
header=/usr/include/asm/unistd_64.h
table="$ROOT/src/std/os/linux/arch/x86_64/syscall_numbers.runes"

if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  printf '%s\n' 'linux UAPI test skipped: requires Linux x86-64'
  exit 0
fi

if [[ ! -f "$header" ]]; then
  printf '%s\n' "linux UAPI test skipped: $header is unavailable"
  exit 0
fi

temporary=$(mktemp -d "${TMPDIR:-/tmp}/runes-linux-uapi.XXXXXX")
trap 'rm -rf "$temporary"' EXIT

awk '
  /^#define __NR_[A-Za-z0-9_]+ [0-9]+$/ {
    name = $2
    sub(/^__NR_/, "", name)
    print "const usize SYS_" toupper(name) " = " $3
  }
' "$header" > "$temporary/expected"

grep -E '^const usize SYS_[A-Z0-9_]+ = [0-9]+$' \
  "$table" > "$temporary/actual"

diff -u "$temporary/expected" "$temporary/actual"
printf '%s\n' 'linux x86-64 syscall table matches installed UAPI'
