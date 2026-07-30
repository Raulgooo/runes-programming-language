#!/usr/bin/env python3
"""Deterministic differential vectors for allocation-free integer parsing."""

from __future__ import annotations

import pathlib
import random
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]


def encoded(value: int, radix: int, automatic: bool) -> str:
    negative = value < 0
    magnitude = -value if negative else value
    digits = {
        2: format(magnitude, "b"),
        8: format(magnitude, "o"),
        10: str(magnitude),
        16: format(magnitude, "x"),
    }[radix]
    prefix = ""
    if automatic and radix != 10:
        prefix = {2: "0b", 8: "0o", 16: "0x"}[radix]
    return ("-" if negative else "") + prefix + digits


def run() -> int:
    compiler = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "./runes")
    if not compiler.is_absolute():
        compiler = (ROOT / compiler).resolve()

    rng = random.Random(0x5041525345)
    vectors: list[tuple[int, bool, int, bool]] = [
        (0, False, 10, False),
        (2**64 - 1, False, 10, False),
        (2**63 - 1, True, 16, True),
        (-(2**63), True, 2, True),
    ]
    for _ in range(124):
        signed = bool(rng.getrandbits(1))
        value = (rng.randrange(-(2**63), 2**63) if signed
                 else rng.randrange(0, 2**64))
        radix = rng.choice([2, 8, 10, 16])
        automatic = radix != 10 and bool(rng.getrandbits(1))
        vectors.append((value, signed, radix, automatic))

    lines = [
        "use std.core.Result",
        "use std.parse.IntegerParse",
        "use std.parse.parse_i64",
        "use std.parse.parse_i64_with",
        "use std.parse.parse_i8",
        "use std.parse.parse_i16",
        "use std.parse.parse_i32",
        "use std.parse.parse_u8",
        "use std.parse.parse_u16",
        "use std.parse.parse_u32",
        "use std.parse.parse_u64",
        "use std.parse.parse_u64_with",
        "f main() {",
    ]
    expected: list[str] = []
    constructor = {2: "binary", 8: "octal", 10: "decimal", 16: "hexadecimal"}
    for value, signed, radix, automatic in vectors:
        text = encoded(value, radix, automatic)
        if radix == 10 and not automatic:
            call = f'parse_{"i64" if signed else "u64"}("{text}")'
        else:
            option = "auto" if automatic else constructor[radix]
            call = (
                f'parse_{"i64" if signed else "u64"}_with('
                f'"{text}", IntegerParse.{option}())'
            )
        lines.extend([
            f"    match {call} {{",
            "        Ok(value) -> { print(value) }",
            '        Err(_) -> { print("ERROR") }',
            "    }",
        ])
        expected.append(str(value))

    for position in range(32):
        malformed = "0" * position + "_"
        lines.extend([
            f'    match parse_u64("{malformed}") {{',
            '        Ok(_) -> { print("ERROR") }',
            "        Err(failure) -> {",
            "            match failure {",
            "                InvalidSyntax(index) -> { print(index) }",
            '                _ -> { print("ERROR") }',
            "            }",
            "        }",
            "    }",
        ])
        expected.append(str(position))

    overflow_cases = [
        ("parse_u8", "256"),
        ("parse_u16", "65536"),
        ("parse_u32", "4294967296"),
        ("parse_u64", "18446744073709551616"),
        ("parse_i8", "128"),
        ("parse_i8", "-129"),
        ("parse_i16", "32768"),
        ("parse_i16", "-32769"),
        ("parse_i32", "2147483648"),
        ("parse_i32", "-2147483649"),
        ("parse_i64", "9223372036854775808"),
        ("parse_i64", "-9223372036854775809"),
    ]
    for function, text in overflow_cases:
        lines.extend([
            f'    match {function}("{text}") {{',
            '        Ok(_) -> { print("ERROR") }',
            "        Err(failure) -> {",
            "            match failure {",
            "                OutOfRange(index) -> { print(index) }",
            '                _ -> { print("ERROR") }',
            "            }",
            "        }",
            "    }",
        ])
        expected.append(str(len(text) - 1))

    long_zero_prefix = "0" * 16384 + "1"
    lines.extend([
        f'    match parse_u64("{long_zero_prefix}") {{',
        "        Ok(value) -> { print(value) }",
        '        Err(_) -> { print("ERROR") }',
        "    }",
    ])
    expected.append("1")
    lines.append("}")

    with tempfile.TemporaryDirectory(prefix="runes-parse-diff-") as directory:
        temp = pathlib.Path(directory)
        source = temp / "vectors.runes"
        generated = temp / "vectors.c"
        binary = temp / "vectors"
        source.write_text("\n".join(lines) + "\n", encoding="utf-8")
        subprocess.run(
            [str(compiler), str(source), "--emit-c", str(generated)],
            cwd=ROOT,
            check=True,
        )
        command = [
            "gcc", "-Isrc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            str(generated), "src/runtime.c", "src/utils/arena.c",
        ]
        platform = ROOT / "src/platform/linux/x86_64/syscall.S"
        if platform.exists():
            command.append(str(platform))
        command.extend(["-o", str(binary)])
        subprocess.run(command, cwd=ROOT, check=True)
        actual = subprocess.check_output([str(binary)], cwd=ROOT, text=True)

    wanted = "".join(line + "\n" for line in expected)
    if actual != wanted:
        actual_lines = actual.splitlines()
        for index, wanted_line in enumerate(expected):
            got = actual_lines[index] if index < len(actual_lines) else "<missing>"
            if got != wanted_line:
                print(
                    f"parse vector {index} mismatch: "
                    f"expected {wanted_line!r}, got {got!r}",
                    file=sys.stderr,
                )
                break
        return 1
    print(f"parse differential test passed ({len(expected)} checks)")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
