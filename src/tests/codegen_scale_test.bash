#!/usr/bin/env bash
set -euo pipefail

platform_sources=()
if [[ $(uname -s) == Linux && $(uname -m) == x86_64 ]]; then
  platform_sources+=(src/platform/linux/x86_64/syscall.S)
else
  platform_sources+=(src/platform/unsupported/linux_syscall.c)
fi

source_file=$(mktemp /tmp/runes-codegen-scale.XXXXXX.runes)
c_file=$(mktemp /tmp/runes-codegen-scale.XXXXXX.c)
binary=$(mktemp /tmp/runes-codegen-scale.XXXXXX)
nested_source=$(mktemp /tmp/runes-codegen-nested.XXXXXX.runes)
nested_c=$(mktemp /tmp/runes-codegen-nested.XXXXXX.c)
nested_binary=$(mktemp /tmp/runes-codegen-nested.XXXXXX)
names_source=$(mktemp /tmp/runes-codegen-names.XXXXXX.runes)
names_c=$(mktemp /tmp/runes-codegen-names.XXXXXX.c)
names_binary=$(mktemp /tmp/runes-codegen-names.XXXXXX)
trap 'rm -f "$source_file" "$c_file" "$binary" "$nested_source" "$nested_c" "$nested_binary" "$names_source" "$names_c" "$names_binary"' EXIT

for i in $(seq 0 319); do
  printf 'f function_%03d() = result: i32 { result = %d }\n' "$i" "$i" \
    >>"$source_file"
done
printf 'f main() { print(function_319()) }\n' >>"$source_file"

./runes "$source_file" --emit-c "$c_file"
${CC:-gcc} -Isrc -std=c11 -w "$c_file" src/runtime.c src/utils/arena.c \
  "${platform_sources[@]}" \
  -o "$binary"
test "$("$binary")" = "319"

printf 'f main() {\n' >>"$nested_source"
for i in $(seq 0 47); do
  printf 'f n%d() {\n' "$i" >>"$nested_source"
done
printf 'print("deep")\n' >>"$nested_source"
for i in $(seq 47 -1 0); do
  printf '}\n' >>"$nested_source"
  printf 'n%d()\n' "$i" >>"$nested_source"
done
printf '}\n' >>"$nested_source"

./runes "$nested_source" --emit-c "$nested_c"
${CC:-gcc} -Isrc -std=c11 -w "$nested_c" src/runtime.c src/utils/arena.c \
  "${platform_sources[@]}" \
  -o "$nested_binary"
test "$("$nested_binary")" = "deep"

long_prefix=$(printf 'segment%.0s' $(seq 1 40))
first_module="${long_prefix}a"
second_module="${long_prefix}b"
printf 'mod %s { pub f value() = result: i32 { result = 1 } }\n' \
  "$first_module" >>"$names_source"
printf 'mod %s { pub f value() = result: i32 { result = 2 } }\n' \
  "$second_module" >>"$names_source"
printf 'f main() { print(%s.value(), %s.value()) }\n' \
  "$first_module" "$second_module" >>"$names_source"

./runes "$names_source" --emit-c "$names_c"
${CC:-gcc} -Isrc -std=c11 -w "$names_c" src/runtime.c src/utils/arena.c \
  "${platform_sources[@]}" \
  -o "$names_binary"
test "$("$names_binary")" = "12"
