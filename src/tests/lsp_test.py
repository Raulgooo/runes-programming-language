#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path


def send(process, message):
    payload = json.dumps(message, separators=(",", ":")).encode()
    process.stdin.write(f"Content-Length: {len(payload)}\r\n\r\n".encode())
    process.stdin.write(payload)
    process.stdin.flush()


def receive(process):
    length = None
    while True:
        line = process.stdout.readline()
        if not line:
            raise AssertionError("language server closed unexpectedly")
        if line in (b"\r\n", b"\n"):
            break
        name, value = line.decode().split(":", 1)
        if name.lower() == "content-length":
            length = int(value.strip())
    assert length is not None
    return json.loads(process.stdout.read(length))


server = subprocess.Popen(
    [sys.argv[1]], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
)

send(server, {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})
initialized = receive(server)
assert initialized["result"]["serverInfo"]["name"] == "runes-lsp"
assert initialized["result"]["capabilities"]["documentSymbolProvider"]

uri = "file:///tmp/lsp-test.runes"
source = """type Point = { x: i32, y: i32 }
dynamic f answer(value: i32) = result: i32 {
    when realm dynamic { result = value } else { result = 0 }
}
except(stack)
flex f selected() = result: i32 { result = 1 }
except(stack)
in gc f selected() = result: i32 { result = 2 }
"""
send(
    server,
    {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": uri,
                "languageId": "runes",
                "version": 1,
                "text": source,
            }
        },
    },
)
diagnostics = receive(server)
assert diagnostics["method"] == "textDocument/publishDiagnostics"
assert diagnostics["params"]["diagnostics"] == []

send(
    server,
    {
        "jsonrpc": "2.0",
        "id": 2,
        "method": "textDocument/documentSymbol",
        "params": {"textDocument": {"uri": uri}},
    },
)
symbols = receive(server)["result"]
assert [symbol["name"] for symbol in symbols] == [
    "Point",
    "answer",
    "selected",
    "selected",
]
assert [child["name"] for child in symbols[0]["children"]] == ["x", "y"]

send(
    server,
    {
        "jsonrpc": "2.0",
        "id": 3,
        "method": "textDocument/hover",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 1, "character": 11},
        },
    },
)
assert "Runes function" in receive(server)["result"]["contents"]["value"]

send(
    server,
    {
        "jsonrpc": "2.0",
        "id": 4,
        "method": "textDocument/definition",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 1, "character": 11},
        },
    },
)
assert receive(server)["result"]["uri"] == uri

send(
    server,
    {
        "jsonrpc": "2.0",
        "id": 5,
        "method": "textDocument/completion",
        "params": {
            "textDocument": {"uri": uri},
            "position": {"line": 1, "character": 4},
        },
    },
)
completion_labels = {item["label"] for item in receive(server)["result"]}
assert {"when", "realm", "in", "except"}.issubset(completion_labels)

send(
    server,
    {
        "jsonrpc": "2.0",
        "method": "textDocument/didChange",
        "params": {
            "textDocument": {"uri": uri, "version": 2},
            "contentChanges": [{"text": "f broken( {"}],
        },
    },
)
assert receive(server)["params"]["diagnostics"]

io_uri = Path("src/std/io.runes").resolve().as_uri()
io_source = Path("src/std/io.runes").read_text()
send(
    server,
    {
        "jsonrpc": "2.0",
        "method": "textDocument/didOpen",
        "params": {
            "textDocument": {
                "uri": io_uri,
                "languageId": "runes",
                "version": 1,
                "text": io_source,
            }
        },
    },
)
io_diagnostics = receive(server)
assert io_diagnostics["method"] == "textDocument/publishDiagnostics"
assert io_diagnostics["params"]["diagnostics"] == [], io_diagnostics

send(server, {"jsonrpc": "2.0", "id": 6, "method": "shutdown", "params": None})
assert receive(server)["result"] is None
send(server, {"jsonrpc": "2.0", "method": "exit", "params": None})
assert server.wait(timeout=5) == 0
