#!/usr/bin/env python3
"""Deterministic lexical-path vectors checked against an independent model."""

from __future__ import annotations

import pathlib
import random
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[2]


def normalize(value: str) -> str:
    absolute = value.startswith("/")
    output: list[str] = []
    for component in value.split("/"):
        if component == "" or component == ".":
            continue
        if component == "..":
            if output and output[-1] != "..":
                output.pop()
            elif not absolute:
                output.append(component)
        else:
            output.append(component)
    if absolute:
        return "/" + "/".join(output)
    return "/".join(output) or "."


def join(base: str, child: str) -> str:
    if child.startswith("/"):
        return normalize(child)
    separator = "" if not base or base.endswith("/") else "/"
    return normalize(base + separator + child)


def run() -> int:
    compiler = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "./runes")
    if not compiler.is_absolute():
        compiler = (ROOT / compiler).resolve()

    rng = random.Random(0x50415448)
    atoms = ["a", "b", "c", "name.ext", ".hidden", ".", "..", ""]
    paths = ["", "/", ".", "..", "a//b/../c", "../../a", "/../../a"]
    for _ in range(89):
        components = [rng.choice(atoms) for _ in range(rng.randrange(0, 9))]
        candidate = "/".join(components)
        if rng.getrandbits(1):
            candidate = "/" + candidate
        if rng.getrandbits(1):
            candidate += "/"
        paths.append(candidate)

    joins: list[tuple[str, str]] = []
    for _ in range(48):
        joins.append((rng.choice(paths), rng.choice(paths)))

    lines = [
        "use std.bytes.equal",
        "use std.path.Path",
        "use std.path.PathView",
        "dynamic f main() {",
        "    bool ok = true",
    ]
    for index, value in enumerate(paths):
        expected = normalize(value)
        lines.extend([
            f'    path_{index} := Path.normalize(PathView.from_str("{value}"))',
            f'    expected_{index} := PathView.from_str("{expected}")',
            f"    if !equal(path_{index}.as_view().bytes, "
            f"expected_{index}.bytes) {{ ok = false }}",
            f"    again_{index} := Path.normalize(path_{index}.as_view())",
            f"    if !equal(again_{index}.as_view().bytes, "
            f"expected_{index}.bytes) {{ ok = false }}",
            f"    again_{index}.deinit()",
            f"    path_{index}.deinit()",
        ])
    for index, (base, child) in enumerate(joins):
        expected = join(base, child)
        lines.extend([
            f'    joined_{index} := Path.join(',
            f'        PathView.from_str("{base}"),',
            f'        PathView.from_str("{child}")',
            "    )",
            f'    join_expected_{index} := PathView.from_str("{expected}")',
            f"    if !equal(joined_{index}.as_view().bytes, "
            f"join_expected_{index}.bytes) {{ ok = false }}",
            f"    joined_{index}.deinit()",
        ])
    lines.extend(["    print(ok)", "}"])

    with tempfile.TemporaryDirectory(prefix="runes-path-diff-") as directory:
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

    if actual != "true\n":
        print("path differential test failed", file=sys.stderr)
        return 1
    print(
        f"path differential test passed "
        f"({len(paths)} normalize/idempotence + {len(joins)} join vectors)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
