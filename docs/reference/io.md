# I/O

`std.io` is the portable, statically dispatched byte-stream layer. It keeps
file descriptors and platform error numbers below the public boundary. Hosted
Linux x86-64 is the first standard-stream backend.

## Contracts

```runes
interface Reader {
    flex f read(
        self: *Reader,
        bytes: []u8
    ) = outcome: Result<usize, ReadError>
}

interface Writer {
    flex f write(
        self: *Writer,
        bytes: []const u8
    ) = outcome: Result<usize, WriteError>
}
```

`Reader` and `Writer` are static generic constraints. Calls are
monomorphized; they do not allocate a trait object, inspect a realm tag, or
perform runtime interface dispatch.

A successful `read` or `write` may be short. For a nonempty request, a
successful count must be between one and the requested length. A count larger
than the request becomes `InvalidCount(count)`. `Ok(0)` is accepted at the
lowest-level writer contract so complete-write logic can report `NoProgress`;
reader complete-operation logic treats it as end-of-input. Empty requests
return `Ok(0)`.

The module also declares:

- `Flusher.flush() -> Result<usize, WriteError>`;
- `Seeker.seek(SeekFrom) -> Result<usize, IoError>`;
- `Closer.close() -> Result<bool, IoError>`.

An adapter implements only capabilities it actually supports. `SliceReader`
implements `Reader` and `Seeker`. `BufferedWriter<W>` implements `Writer` and
`Flusher`. `std.fs.File` provides the first resource-backed `Closer`
implementation. Owning sockets remain later work, and both APIs share this
contract.

## Complete operations

`write_all_to<W: Writer>(writer, bytes)` retries partial writes and
`Io(Interrupted)`. It rejects zero progress and impossible counts.

`read_exact_from<R: Reader>(reader, bytes)` fills the destination or returns
`ExactReadError.UnexpectedEnd(completed)`. The payload distinguishes EOF
before any data from EOF after a partial read. Other reader failures are
wrapped in `ExactReadError.Read`.

## Line input

`read_line_bytes_from<R: Reader>(reader, output)` consumes one LF-delimited
line. LF is consumed and excluded from the output. The result is:

```runes
type LineRead = {
    length: usize
    terminated: bool
    end_of_input: bool
}
```

- EOF before any byte is `LineReadError.EndOfInput`.
- EOF after bytes is a successful line with `terminated: false` and
  `end_of_input: true`.
- A consumed LF produces `terminated: true`.
- If another non-LF byte appears after `output` reaches capacity, the function
  returns `TooLong(output.len)`. The stored prefix and first excess byte have
  been consumed; truncation is never reported as success.

`read_line_from` has the same framing rules and returns a `str` borrowing the
caller-provided output. It additionally validates the complete line as UTF-8
and reports `InvalidUtf8`. The byte version does no encoding validation.

Use line helpers over `BufferedReader` for streams. Calling them directly on
an unbuffered operating-system reader is correct but may perform one backend
read per byte.

## Adapters

### Borrowed memory

`SliceReader.new(source)` reads an immutable byte slice and tracks a cursor.
`seek(Start(n))`, `seek(Current(delta))`, and `seek(End(delta))` reject
positions outside `0..source.len`.

`FixedBufferWriter.new(buffer)` writes into mutable caller storage.
`written()` returns the initialized prefix and `remaining()` returns unused
capacity.

`StringWriter(target: &value)` validates each byte chunk as UTF-8 and appends
it transactionally to a realm-owned `String`.

### Buffered streams

```runes
[4096]u8 staging_storage = []
[]u8 staging = staging_storage
buffered := BufferedReader<MyReader>.new(&source, staging)
```

`BufferedReader<R>` and `BufferedWriter<W>` borrow both their backend and
their staging slice. They allocate nothing, have identical storage behavior in
stack, dynamic, regional, and GC execution, and require no deinitialization.
Buffer capacity is always explicit. A zero-capacity buffer is legal and acts
as an unbuffered pass-through.

`BufferedReader` serves staged bytes before touching the backend and bypasses
staging for a sufficiently large destination. `BufferedWriter` accepts
partial writes into staging, bypasses copying for writes larger than an empty
buffer, and exposes `pending()`.

Call `flush()` or `flush_buffered()` explicitly before the backend or staging
storage becomes invalid. Runes has no implicit destructor hook, so abandoning
a buffered writer with pending bytes deliberately does not pretend the data
was written. A failed flush keeps only the unsent suffix; a later retry cannot
duplicate the already-written prefix.

## Standard streams

Hosted Linux x86-64 provides zero-sized `Stdin`, `Stdout`, and `Stderr`
adapters. They expose `Reader`, `Writer`, and no-op `Flusher` capabilities as
appropriate without exposing descriptors. The compatibility functions
`read`, `write`, `write_line`, `write_error`, and `write_error_line` remain
available.

Linux startup ignores `SIGPIPE`, so writing to a closed pipeline reports
`IoError.BrokenPipe` rather than terminating the process.

## Ownership and realm behavior

All fixed, slice, standard-stream, and borrowed buffered adapters are
allocation-free. Generic operations are specialized in the caller's effective
realm, but their byte behavior does not change by realm. `StringWriter` is the
exception only in storage policy: growth follows the target `String`'s
persistent owner realm.

## Current boundary

M5 supplies byte-stream contracts, standard streams, memory adapters,
buffering, exact operations, bounded line input, UTF-8 validation, seeking for
slices, and explicit flushing. M6 supplies lexical paths separately in
`std.path`. Owning files, file seeking/closing, socket adapters, and
asynchronous I/O are later library milestones.
