# Platform Boundary and Portable I/O Plan

This document defines how raw OS support becomes a portable `std.io` without
making application code detect the operating system at runtime.

The complete ordered library sequence is in the
[application-readiness plan](app-readiness-plan.md).

## The rule

Platform selection is a compile-time build decision.

An application should write:

```runes
use std.io
```

It should not call `uname`, inspect environment variables, or branch between
Linux and another OS every time it writes a byte. The selected executable has
one target ABI, so its standard library should contain one matching backend.

## Layering

```text
application
    |
    v
std.io                    portable behavior and errors
    |
    v
internal platform contract
    |
    +-- std.os.linux      Linux kernel ABI
    +-- std.os.runes      future Runes OS ABI
    +-- std.os.windows    possible hosted backend
    |
    v
architecture boundary    x86-64, AArch64, ...
```

`std.io` owns portable concepts such as complete writes, end of input,
interruption policy, buffering, and text encoding. `std.os.linux` owns Linux
descriptors, errno, syscall arguments, and mapping flags. Architecture modules
own register conventions and syscall numbers.

The portable layer must not expose `LinuxResult`, Linux errno numbers, or raw
syscall constants in its public signatures.

## Current bootstrap state

The first implemented backend is hosted Linux x86-64:

- `syscall.S` translates the System V function ABI into the Linux syscall ABI;
- `std.os.linux.syscall` provides `call0` through `call6`;
- `std.os.linux.result` preserves exact errno numbers;
- `std.os.linux.fd` wraps descriptor operations and complete text writes;
- `std.os.linux.memory` wraps mapping primitives;
- the x86-64 syscall table is checked against the installed Linux UAPI.

`runec` now resolves an explicit CLI/manifest/default target and chooses its
platform link behavior from that target rather than `uname`.

The standard namespace materializes referenced top-level modules, and the
compiler performs reachability-based C emission. Unused Linux functions and
their foreign syscall symbol are absent, so applications that do not use the
backend link without `syscall.S`.

## Implemented target model

The compiler and command driver need one explicit target description:

```text
architecture-vendor-operating_system-environment
```

Accepted targets are:

```text
x86_64-unknown-linux-gnu
x86_64-unknown-linux-none
x86_64-unknown-runes-none
```

The exact spelling can be finalized with the freestanding build work. The
important properties are available independently:

- `target_arch`, initially `x86_64`;
- `target_os`, initially `linux` or `runes`;
- `target_env`, initially `gnu` or `none`;
- pointer width and endianness;
- hosted versus freestanding runtime profile.

`runec --target TRIPLE` overrides a manifest target. With neither present, the
driver infers a supported host target for convenience. The resolved
target must be passed to parsing/module loading, code generation, C compiler
flags, platform link inputs, and documentation emitted by `runec project`.

Cross-compiling must never silently fall back to host detection.

## Conditional standard-library selection

The compiler provides a built-in configuration attribute:

```runes
#[cfg(target_os = "linux")]
use std.os.linux.fd as platform_fd
```

Disabled declarations are removed before external-module loading, name resolution, and
monomorphization. That prevents references to unavailable modules and avoids
emitting or linking the wrong backend.

The first supported predicates only need to be:

- `target_os`;
- `target_arch`;
- `target_env`;
- `hosted`;
- `freestanding`;
- `all(...)`, `any(...)`, and `not(...)`.

This mechanism should select private imports inside `std.io`. Applications
normally should not need platform conditions unless they intentionally use a
target-specific API.

## Portable I/O contract

The initial public layer should stay small:

```runes
std.io.write(text)
std.io.write_line(text)
std.io.write_error(text)
std.io.read(buffer)
```

The portable implementation should:

- write all requested bytes or return a portable error;
- retry an interrupted operation where retrying is correct;
- distinguish end of input from failure;
- never expose a descriptor as part of ordinary terminal output;
- preserve the original OS error in an optional raw diagnostic payload;
- perform no allocation for the basic byte and text operations.

Linux `write_all_text` already implements the partial-write and `EINTR` loop
needed by this contract.

Do not design buffered streams, formatting, paths, or owning files into the
first I/O milestone. Those build on the byte-level contract.

## Portable errors

`std.io` can now use the implemented `Result<T, E>` and `IoError` from
`std.core`.
It should map backend errors into a small semantic set such as:

```text
interrupted
would_block
permission_denied
not_found
invalid_input
broken_pipe
unsupported
other(raw_code)
```

Linux errno remains available through `std.os.linux`; it does not become the
portable error identity. A future Runes OS backend maps its own kernel status
codes into the same semantic errors.

## Freestanding and the future Runes OS

Freestanding does not mean Linux without libc. A Runes OS has a different
kernel ABI and therefore a different `std.os.runes` backend.

The portable `std.io` contract stays the same while the backend changes:

```text
Linux program: std.io -> std.os.linux -> syscall
Runes OS app:   std.io -> std.os.runes -> kernel IPC/syscall
Kernel code:    no std.io unless a kernel console backend is selected
```

This separation allows ordinary applications to be ported without teaching
them Linux syscall numbers or the future kernel's driver protocol.

## Implementation milestones

1. Finish and stabilize the Linux byte, descriptor, and mapping boundary. (done)
2. Implement `pub const` so ABI constants stop being duplicated. (compiler done; library migration pending)
3. Add explicit target triples to compiler configuration and `runec`. (done)
4. Add `#[cfg]` pruning before resolution. (done)
5. Define portable `IoError`, `IoResult`, and the minimal `std.io` functions.
6. Bind the Linux backend through private conditional imports.
7. Add a fake backend for deterministic portable-I/O unit tests.
8. Add `std.os.runes` when the Runes OS userspace ABI exists.
9. Add buffering, formatting, files, paths, networking, and async only after
   the byte-level contract is stable.

## Acceptance gates

The portable layer is ready when:

- application source contains no Linux import;
- the same application compiles against two test backends;
- Linux output handles partial writes and `EINTR`;
- unsupported targets fail during compilation, not at first I/O call;
- cross-target builds do not inspect the host to select an ABI;
- unused target backends are neither emitted nor linked;
- freestanding builds do not acquire a libc dependency through `std.io`.
