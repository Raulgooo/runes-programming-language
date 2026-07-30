#!/usr/bin/env python3
"""Differential integer vectors for std.format."""

from __future__ import annotations

import pathlib
import random
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]


def expected(value: int, signed: bool, base: str, prefix: bool,
             sign: str, width: int, padding: str, left: bool) -> str:
    negative = signed and value < 0
    magnitude = -value if negative else value
    if base == "decimal":
        digits = str(magnitude)
        marker = ""
    elif base == "lower":
        digits = format(magnitude, "x")
        marker = "0x" if prefix else ""
    else:
        digits = format(magnitude, "X")
        marker = "0X" if prefix else ""

    if negative:
        header = "-"
    elif sign == "always":
        header = "+"
    elif sign == "space":
        header = " "
    else:
        header = ""
    header += marker
    core = header + digits
    count = max(0, width - len(core.encode("utf-8")))
    if left:
        return core + padding * count
    if padding == "0":
        return header + padding * count + digits
    return padding * count + core


def run() -> int:
    compiler = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "./runes")
    if not compiler.is_absolute():
        compiler = (ROOT / compiler).resolve()

    rng = random.Random(0x52554E45)
    vectors: list[tuple[int, bool, str, bool, str, int, str, bool]] = [
        (0, False, "decimal", False, "negative", 0, " ", False),
        (1, False, "lower", True, "always", 12, "0", False),
        (-1, True, "upper", True, "negative", 12, ".", True),
        (2**64 - 1, False, "decimal", True, "space", 20, " ", False),
        (-(2**63), True, "lower", True, "negative", 0, " ", False),
        (2**63 - 1, True, "upper", False, "always", 24, "0", False),
    ]
    bases = ["decimal", "lower", "upper"]
    signs = ["negative", "always", "space"]
    pads = [" ", "0", "."]
    for _ in range(90):
        signed = bool(rng.getrandbits(1))
        value = (rng.randrange(-(2**63), 2**63) if signed
                 else rng.randrange(0, 2**64))
        vectors.append((
            value,
            signed,
            rng.choice(bases),
            bool(rng.getrandbits(1)),
            rng.choice(signs),
            rng.randrange(0, 29),
            rng.choice(pads),
            bool(rng.getrandbits(1)),
        ))

    lines = [
        "use std.core.Result",
        "use std.format.Alignment",
        "use std.format.FormatError",
        "use std.format.IntegerFormat",
        "use std.format.SignPolicy",
        "use std.format.write_i64",
        "use std.format.write_u64",
        "#[safe]",
        "extern f runes_str_view(value: *const u8, length: usize) = result: str",
        "f main() {",
        "    [64]u8 bytes = []",
        "    []u8 output = bytes",
    ]
    expected_lines: list[str] = []
    for index, vector in enumerate(vectors):
        value, signed, base, prefix, sign, width, padding, left = vector
        constructor = {
            "decimal": "decimal",
            "lower": "hex_lower",
            "upper": "hex_upper",
        }[base]
        lines.append(f"    options{index} := IntegerFormat.{constructor}()")
        lines.append(f"    options{index}.prefix = {'true' if prefix else 'false'}")
        lines.append(f"    options{index}.width = {width}")
        lines.append(f"    options{index}.padding = {ord(padding)}")
        if sign != "negative":
            variant = "Always" if sign == "always" else "Space"
            lines.append(f"    options{index}.sign = SignPolicy.{variant}()")
        if left:
            lines.append(f"    options{index}.alignment = Alignment.Left()")
        function = "write_i64" if signed else "write_u64"
        if value == -(2**63):
            lines.append(f"    i64 minimum{index} = -9223372036854775807")
            lines.append(f"    minimum{index} = minimum{index} - 1")
            expression = f"minimum{index}"
        else:
            expression = str(value)
        lines.extend([
            f"    match {function}(output, {expression}, options{index}) {{",
            "        Ok(length) -> {",
            "            print(runes_str_view(&bytes[0], length))",
            "        }",
            '        Err(_) -> { print("ERROR") }',
            "    }",
        ])
        expected_lines.append(expected(*vector))
    lines.append("}")

    with tempfile.TemporaryDirectory(prefix="runes-format-diff-") as directory:
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

    wanted = "".join(line + "\n" for line in expected_lines)
    if actual != wanted:
        actual_lines = actual.splitlines()
        for index, wanted_line in enumerate(expected_lines):
            got = actual_lines[index] if index < len(actual_lines) else "<missing>"
            if got != wanted_line:
                print(
                    f"format vector {index} mismatch: "
                    f"expected {wanted_line!r}, got {got!r}",
                    file=sys.stderr,
                )
                print(
                    f"actual neighborhood: "
                    f"{actual_lines[max(0, index - 2):index + 3]!r}",
                    file=sys.stderr,
                )
                break
        return 1
    print(f"format differential test passed ({len(vectors)} vectors)")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
