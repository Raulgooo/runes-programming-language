# Runes Standard Library and Ecosystem Roadmap

Status: design and implementation roadmap. None of the modules described here
are part of the compiler-required runtime unless explicitly identified in
`v0.1-runtime-requirements.md`.

The objective is an ergonomic library ecosystem capable of supporting compiler
bootstrapping, command-line tools, servers, desktop applications, games,
graphics, numerical computing, and machine learning without hiding Runes'
memory realms or systems-level control.

## 1. Library boundaries

Runes should use four clearly separated layers:

1. **Compiler runtime**: traps, arenas, promotion, GC mechanics, closure support,
   and low-level string primitives required by generated code.
2. **Standard library**: portable containers, text, formatting, I/O interfaces,
   algorithms, concurrency primitives, and stable platform abstractions.
3. **Official ecosystem libraries**: networking protocols, TLS integration,
   graphics, databases, numerical computing, ML, audio, and UI frameworks.
4. **Application libraries**: domain-specific code and product policy.

Only layer 1 is linked automatically by `runec`. Everything else should be
normal Runes source or an explicit native dependency.

## 2. Ergonomic design requirements

Every official API should follow these rules:

- expose ownership and lifetime in the type or constructor name;
- accept slices and string views for borrowed input;
- return owned values only when ownership is necessary;
- provide regional, dynamic, and GC-friendly construction where meaningful;
- never silently promote between realms;
- use nominal errors and preserve OS/native error codes;
- make fallible operations return `!T` instead of sentinel values;
- provide explicit `close`, `deinit`, or scoped helpers for resources;
- use iterators or callbacks without forcing intermediate allocations;
- support dependency injection for allocators, clocks, randomness, and I/O;
- keep deterministic behavior available for tests and reproducible builds;
- document thread safety and blocking behavior on every relevant type;
- avoid ambient global state except for process-level facilities;
- keep unsafe and FFI details behind small audited modules;
- provide useful defaults without removing lower-level control.

The library should adopt consistent naming:

- `new` creates an empty/default value;
- `with_capacity` reserves storage;
- `from_*` converts and takes or copies as documented;
- `as_*` returns a borrowed view;
- `into_*` consumes ownership;
- `get` is checked/fallible, while explicitly unchecked access is unsafe;
- `read`, `write`, and `update` may perform partial operations;
- `read_all`, `write_all`, and `update_all` guarantee completion or error;
- `parse` validates external text or bytes;
- `encode` and `decode` transform representations;
- `close` releases external handles, and `deinit` releases owned memory.

## 3. Foundation modules

These modules are prerequisites for nearly every other library.

### `core.option`

- `Option<T>` with `None` and `Some(T)`;
- `is_some`, `is_none`, `unwrap`, `unwrap_or`, `map`, `and_then`, and `take`;
- conversion between nullable pointers and options where representation permits;
- iteration over zero or one value.

### `core.result`

Runes already has `!T`; helpers should add:

- mapping success and error values;
- contextual error wrapping;
- collection of iterators of fallible values;
- explicit conversion between nominal error sets;
- stable error-chain rendering for diagnostics.

### `core.compare` and `core.hash`

- equality and ordering interfaces;
- total ordering wrappers for floating-point values;
- stable and randomized hash interfaces;
- hash combiners for tuples, slices, strings, and aggregates;
- explicit distinction between persistent hashes and denial-of-service-resistant
  process hashes.

### `core.iter`

- iterator and double-ended iterator interfaces;
- `map`, `filter`, `flat_map`, `enumerate`, `zip`, `take`, `skip`, and `chain`;
- `fold`, `reduce`, `find`, `position`, `all`, `any`, `min`, and `max`;
- fallible iteration and collection;
- zero-allocation iteration over arrays, slices, strings, maps, files, and
  directory entries.

### `core.range`

- checked integer ranges and inclusive ranges;
- range intersection, containment, clamping, and splitting;
- byte, text, and collection range validation helpers.

## 4. Allocation and ownership

The stdlib must not assume one universal heap.

### Allocator interface

Provide a typed allocator capability supporting:

- allocate, resize, and free with size and alignment;
- fallible allocation for library code;
- raw/dynamic, regional, and GC implementations;
- allocation statistics and failure injection for tests;
- adapters for C allocators and application-provided pools.

### Owning container policy

Recommended families:

- `Vec<T>`: raw/dynamic ownership with explicit `deinit`;
- `ArenaVec<T>`: regional growth, abandoning replaced buffers until arena exit;
- `GcVec<T>`: traced GC ownership where automatic reclamation is appropriate;
- `Box<T>`: one dynamically owned value;
- `Gc<T>`: an explicit managed reference if a library-level wrapper improves
  readability;
- `Rc<T>` and `Arc<T>` only if reference counting is needed independently of
  the scoped GC, with cycle behavior documented.

All owning containers need checked capacity arithmetic and must expose slices
without transferring ownership.

## 5. Collections

### Sequential collections

- `Vec<T>` with reserve, append, extend, insert, remove, swap-remove, pop,
  truncate, clear, retain, sort, deduplicate, and slice views;
- `Deque<T>` as a ring buffer;
- `SmallVec<T, N>` after const generics exist, or fixed-capacity alternatives;
- singly/doubly linked lists only for workloads where node stability matters;
- priority queue/binary heap;
- bit set and compact bit vector;
- fixed-capacity stack and queue for freestanding or deterministic workloads.

### Associative collections

- `HashMap<K, V>` and `HashSet<T>` with pluggable hash/equality;
- ordered `TreeMap<K, V>` and `TreeSet<T>`;
- insertion-ordered map/set where deterministic iteration matters;
- multi-map and bidirectional-map utilities as ecosystem modules;
- entry APIs to avoid duplicate lookups;
- borrowed-key lookup for strings and slices;
- collision and adversarial-input testing.

### Algorithms

- stable and unstable sorting;
- binary search and partitioning;
- copy, move, fill, rotate, reverse, and permutation helpers;
- set operations and merge algorithms;
- graph traversal, topological sorting, and shortest paths in a separate
  algorithms package.

## 6. Text, bytes, and formatting

### `text.string`

- owning UTF-8 `String` and `StringBuilder`;
- validated construction from bytes and explicit lossy conversion;
- append scalar/string/bytes, reserve, truncate, clear, and consume to bytes;
- byte length, scalar iteration, grapheme iteration through an optional Unicode
  package, and boundary-safe slicing;
- explicit C-string conversion with embedded-NUL rejection;
- case conversion, normalization, and Unicode categories in generated Unicode
  data modules rather than the compiler runtime.

### `bytes`

- immutable and mutable byte buffers;
- cursors/readers over slices;
- endian-aware integer and floating-point loads/stores;
- hex, base64, and percent encoding;
- constant-time equality for secret material;
- efficient search, split, join, and streaming pattern matching.

### `fmt`

- integer bases, padding, alignment, width, and sign control;
- deterministic float formatting and parsing;
- string/character escaping and debug rendering;
- user-defined display/debug interfaces;
- formatting into strings, files, sockets, logs, and fixed buffers;
- compile-time checked format strings when macro/compile-time support exists;
- a builder API until format-string validation is available.

### `parse`

- integers in explicit bases with overflow diagnostics;
- decimal and scientific floating-point parsing;
- booleans, Unicode scalars, durations, sizes, and network addresses;
- whitespace trimming and tokenization;
- parsers that report consumed input and detailed error positions.

## 7. I/O and platform abstraction

### Common I/O interfaces

- `Reader`, `Writer`, `Seeker`, `Flusher`, and `Closer`;
- partial and complete read/write operations;
- buffered reader/writer types;
- byte and line iteration with explicit maximum sizes;
- in-memory and counting/null streams;
- timeout and cancellation-aware variants;
- adapters between slices, files, sockets, compression, hashing, and TLS.

### Standard input and safe foreign-I/O boundary

The POSIX-style `read(fd, *u8, count)` binding must remain unsafe: its raw
pointer and independent byte count cannot express that the destination is
writable for the requested length. Do not mark this binding `#[safe]` merely
to make application calls convenient. The standard library must contain the
unsafe operation and expose checked Runes APIs:

- `read_into(fd, buffer: []u8) -> !usize` as the allocation-free primitive;
- pass exactly `buffer.ptr` and `buffer.len` to the foreign call inside the
  library's audited `unsafe` block;
- translate negative results and platform error state into a typed `IoError`;
- retry interrupted operations where appropriate and preserve partial reads;
- defensively reject a foreign result larger than the supplied buffer;
- represent end-of-file distinctly from an ordinary nonempty read.

Text input builds on the byte primitive:

- `read_line(reader, maximum) -> !ReadLine` with a distinct EOF variant;
- a realm-aware owning `String` or byte builder for buffers that must grow;
- explicit maximum sizes so untrusted input cannot allocate without limit;
- UTF-8 validation with a typed invalid-encoding error;
- a documented policy for retaining or removing line terminators;
- buffered operation so line input does not issue one syscall per byte;
- `input(prompt) -> !String` as a convenience over standard input/output;
- non-newline output and `flush` before blocking for an interactive prompt;
- password input as a separate terminal API with echo restoration on every
  success and error path.

Calling a normal Runes wrapper does not require callers to enter `unsafe` just
because its implementation contains a lexical unsafe block. This is the
intended trusted-boundary model.

The compiler currently exposes every extern declaration across its containing
module. Before treating `std.io` as a clean capability boundary, add private
extern bindings so the raw platform symbol can remain an implementation detail.
Visibility must also be made consistent for globals and methods; see the
language guide for the currently implemented visibility rules.

### `fs` and `path`

- open, create, read, write, append, seek, truncate, flush, close, and sync;
- metadata, permissions, links, rename, copy, remove, and atomic replacement;
- directory creation and iteration;
- recursive walking with cycle/link policy;
- path components, joining, normalization, absolute resolution, and extension
  manipulation;
- temporary files/directories and secure creation;
- memory mapping and advisory file locking where supported;
- Linux path bytes must not be silently treated as guaranteed UTF-8.

### `process` and `env`

- command-line arguments and environment access;
- subprocess creation, pipes, redirection, wait, terminate, and exit status;
- current directory and executable path;
- signals through an explicit platform module;
- shell-free argument passing by default;
- process builder with inherited or sanitized environment.

### `time`

- monotonic and wall clocks;
- `Duration`, `Instant`, and checked arithmetic;
- sleep and deadline helpers;
- calendar/date/time-zone support as a separate data-backed package;
- injectable clocks for tests.

### `os`

- errno/native error preservation;
- file descriptors and native handles;
- virtual memory mapping/protection;
- terminal modes and capability detection;
- dynamic library loading;
- platform feature detection without leaking preprocessor details into normal
  Runes code.

## 8. Diagnostics, logging, and testing

### Diagnostics

- source files, spans, line maps, labels, notes, and suggestions;
- colored terminal output with automatic capability detection;
- plain and machine-readable JSON/SARIF output;
- error context chains and backtraces where available;
- deterministic rendering for compiler golden tests.

### Logging and tracing

- levels, structured fields, targets, timestamps, and spans;
- sinks for stderr, files, rotation, syslog, and user-defined writers;
- compile-time and runtime filtering;
- tracing correlation IDs and metrics hooks;
- no allocation on disabled hot paths.

### Testing

- unit and integration test discovery;
- assertions with useful value diffs;
- parameterized, property, fuzz, snapshot, and golden tests;
- temporary filesystem and fake clock/random/network helpers;
- benchmarks with warmup and statistical summaries;
- sanitizer and leak-check integration;
- deterministic random seeds and reproducible failure artifacts.

## 9. Concurrency and asynchronous I/O

This layer must respect the v0.1 rule that GC references cannot cross OS
threads.

### Threads and synchronization

- thread creation, join, naming, affinity, and stack sizing;
- mutex, read/write lock, condition variable, semaphore, barrier, and once;
- atomics with explicit memory ordering;
- thread-local storage;
- bounded and unbounded channels;
- raw/regional/dynamic thread-safe data only in v0.1 unless the GC model is
  expanded;
- scoped threads that cannot outlive borrowed inputs.

### Async runtime

After language-level async support is designed:

- futures/tasks, executor, reactor, timers, cancellation, and structured
  concurrency;
- Linux `epoll` initially, with `io_uring` as an optional backend;
- async files, sockets, DNS, subprocess pipes, and synchronization;
- bounded work queues and blocking-task isolation;
- no hidden global executor requirement.

Before native async syntax, networking can use blocking APIs, explicit event
loops, or callback/state-machine libraries.

## 10. Networking

### `net`

- IPv4/IPv6 addresses, ports, socket addresses, and CIDR networks;
- TCP listener/stream with connect, accept, shutdown, timeouts, keepalive, and
  no-delay options;
- UDP sockets, multicast, broadcast, and packet metadata;
- Unix-domain sockets;
- DNS resolution with sync and eventual async APIs;
- nonblocking mode and poller integration;
- scatter/gather I/O and zero-copy facilities where supported;
- explicit partial I/O and cancellation semantics.

### Protocol foundations

- incremental codecs over byte buffers;
- framed transports and backpressure;
- URL and URI parsing;
- MIME/media types;
- HTTP date/header/cookie utilities;
- proxy configuration and connection pooling.

### HTTP ecosystem

- HTTP/1.1 parser and serializer with strict limits;
- HTTP/2 through a proven implementation or carefully audited package;
- client with redirects, decompression, pooling, proxies, and streaming bodies;
- server with routing, middleware, request limits, graceful shutdown, and
  streaming responses;
- WebSocket client/server;
- multipart forms, static file serving, and range requests;
- optional HTTP/3/QUIC through a vetted native dependency initially.

### TLS and cryptography

Do not invent production cryptographic primitives in the stdlib. Begin with
audited bindings to OpenSSL, BoringSSL, LibreSSL, rustls C bindings, or another
maintained provider. The wrapper needs:

- certificate and hostname verification enabled by default;
- system and custom trust stores;
- client/server configuration and ALPN;
- streaming TLS over generic transports;
- secure random bytes;
- explicit secret-zeroing and constant-time operations;
- provider/version reporting and actionable errors.

Pure Runes cryptography can be added only with dedicated review, known-answer
tests, fuzzing, side-channel analysis, and external interoperability tests.

## 11. Serialization and data formats

- JSON parser/writer with streaming and DOM APIs, limits, exact number policy,
  and useful locations;
- CSV with dialect configuration and streaming rows;
- TOML and YAML as ecosystem packages;
- binary codecs for MessagePack, CBOR, and Protocol Buffers;
- schema validation and versioning;
- derive/code-generation support after macros or compiler plugins exist;
- zero-copy borrowed decoding where input lifetime permits;
- limits against malicious nesting, lengths, and allocation requests.

Compression/archive packages should cover gzip/zlib, zstd, brotli, ZIP, and tar
through vetted libraries first, exposed as `Reader`/`Writer` adapters.

## 12. Databases and persistence

- SQLite as the first embedded database binding;
- PostgreSQL and MySQL clients with prepared statements and typed parameters;
- connection pools, transactions, migrations, cancellation, and timeouts;
- row decoding without mandatory reflection;
- key/value and embedded storage interfaces;
- memory-mapped and append-only data structures for systems workloads;
- serialization/versioning and corruption recovery policy.

An ORM should remain a separate optional package. The base database APIs should
stay explicit and usable without code generation.

## 13. Math and numerical computing

### Scalar math

- complete `f32`/`f64` elementary functions;
- finite/NaN/infinity classification;
- rounding and conversion policy;
- checked, wrapping, saturating, and overflow-result integer helpers;
- complex numbers, rational numbers, and big integers as packages;
- numerical tolerance and approximate-comparison utilities.

### Randomness

- cryptographic OS randomness separated from deterministic PRNGs;
- seedable fast generators for simulation and ML;
- uniform integer/float sampling without modulo bias;
- normal, Bernoulli, categorical, and common statistical distributions;
- shuffle and weighted sampling;
- reproducible generator state and independent streams.

### Linear algebra

- vectors, matrices, and N-dimensional strided views;
- owned and borrowed storage with shape/stride metadata;
- checked shape arithmetic and bounds;
- elementwise operations, reductions, matrix multiplication, decompositions,
  and solvers;
- contiguous-layout conversion only when required;
- BLAS/LAPACK bindings with a pure Runes correctness fallback;
- explicit dtype, device, layout, and aliasing rules;
- parallel kernels that do not move GC references between threads.

### Scientific modules

- statistics and online moments;
- distributions and hypothesis tests;
- interpolation, integration, optimization, and root finding;
- FFT and signal-processing packages;
- sparse matrices and graph/numerical algorithms;
- image and audio numerical transforms.

## 14. Machine learning

The ML stack should be an official ecosystem package built on the numerical
layer, not compiler magic.

### Tensor core

- `Tensor<T>` with owned storage plus borrowed `TensorView<T>`;
- rank, shape, strides, offset, dtype, device, and contiguity;
- slicing, reshape, transpose, broadcast, concatenate, gather, and scatter;
- checked shape multiplication and allocation;
- deterministic initialization and serialization;
- `f32`, `f64`, integer, boolean, and eventually quantized dtypes;
- CPU kernels first, with SIMD and threaded execution behind stable operations.

### Autograd

- reverse-mode computation graph;
- gradient ownership and accumulation rules;
- no-gradient scopes and detached tensors;
- saved-tensor lifetime management;
- custom operations and gradient checking;
- graph release after backward unless explicitly retained;
- anomaly detection with operation/source context.

### Neural-network modules

- parameter/module abstraction and state dictionaries;
- dense/linear, embedding, normalization, dropout, convolution, pooling, and
  attention layers;
- activations and numerically stable losses;
- train/eval modes and recursive parameter traversal;
- weight initialization and checkpoint loading;
- mixed precision only after correctness and overflow handling are explicit.

### Optimization and training

- SGD, momentum, Adam/AdamW, RMSProp, learning-rate schedules, and gradient
  clipping;
- batching, shuffling, data loaders, and prefetching;
- metrics, validation loops, early stopping, and checkpointing;
- deterministic seeds and reproducibility reports;
- profiler hooks and operation timing;
- distributed training deferred until concurrency and device models mature.

### Classical ML

- linear/logistic regression;
- least squares, regularization, and gradient-based alternatives;
- k-nearest neighbors, naive Bayes, decision trees, and clustering;
- preprocessing, normalization, categorical encoding, and train/test splits;
- common metrics and cross-validation.

This layer is enough to train linear regression and small CPU neural/language
models once tensors, matrix multiplication, autograd, tokenization, and data
loading exist. The language core already provides the required generics,
slices, floats, modules, closures, raw memory, arenas, and optional GC; the
algorithms and optimized kernels still need implementation.

### Language-model support

- Unicode-aware tokenization plus byte/BPE/WordPiece tokenizers;
- memory-mapped datasets and streaming samples;
- embedding, attention, masking, rotary/positional encoding, and transformer
  blocks;
- stable softmax, cross-entropy, sampling, top-k/top-p, and temperature;
- quantized inference formats and model checkpoints;
- ONNX or another interchange format before inventing a native format;
- CPU inference with SIMD/BLAS, then optional GPU backends.

## 15. Graphics, UI, games, and media

### Windowing and input

- windows, monitors, DPI, fullscreen, clipboard, cursor, keyboard, mouse,
  touch, and game controllers;
- event loop with explicit thread requirements;
- initial bindings to SDL3 or GLFW for portability;
- headless surfaces for tests and servers.

### Images and color

- pixel formats, color spaces, alpha modes, and conversion;
- image buffers and non-owning views;
- PNG/JPEG/WebP/EXR loading through audited codecs;
- resize, crop, rotate, composite, filters, and mipmaps;
- ICC/color-management integration when needed.

### 2D graphics

- paths, strokes, fills, transforms, clipping, gradients, text, and images;
- CPU raster backend and optional GPU backend;
- font discovery, shaping through HarfBuzz, and rasterization through FreeType;
- retained and immediate drawing APIs;
- SVG parsing/rendering as an ecosystem package.

### 3D and GPU

- vectors, matrices, quaternions, transforms, cameras, meshes, materials, and
  scenes;
- Vulkan first for explicit Linux systems access, with portability layers or
  WebGPU bindings for broader reach;
- resource lifetime, command buffers, synchronization, shaders, pipelines,
  descriptors, render passes, and compute;
- shader compilation/reflection through established tools;
- asset loading such as glTF;
- frame graphs and higher-level renderers as separate packages.

### UI toolkit

- layout, constraints, text, focus, accessibility, input, scrolling, and
  selection;
- state/model separation and predictable event propagation;
- theme and style system;
- native accessibility bridges;
- testable headless rendering and event simulation;
- no requirement that application state use GC, though GC-backed UI graphs may
  be convenient on the owning thread.

### Games

- timing, fixed-step loop, input mapping, assets, scenes, animation, audio,
  collision, and physics bindings;
- entity-component-system as an optional package, not a language requirement;
- deterministic simulation options;
- hot reload/tooling only after resource lifetime behavior is reliable.

### Audio and video

- device enumeration and streaming audio;
- sample formats, mixing, resampling, codecs, and spatial audio;
- bindings to PipeWire/ALSA and portable libraries;
- FFmpeg bindings for video/container support before native codec work.

## 16. Application ergonomics

### CLI applications

- declarative argument parser with subcommands, defaults, validation, and help;
- shell completion generation;
- terminal colors, progress bars, prompts, tables, and password input;
- configuration layering from defaults, files, environment, and flags;
- structured exit codes and diagnostics.

### Configuration and secrets

- typed config loading with source attribution;
- environment and file providers;
- redacted secret values;
- reload/watch support;
- explicit precedence and validation.

### Web applications

- router, middleware, cookies, sessions, forms, uploads, static files, and
  templates;
- request IDs, logging, metrics, tracing, limits, and graceful shutdown;
- authentication primitives and vetted password hashing;
- database pools and migrations;
- production TLS deployment guidance.

### Desktop applications

- windowing, UI, clipboard, file dialogs, notifications, drag/drop, and system
  integration;
- packaging and resource embedding;
- accessibility and internationalization;
- crash reports and update mechanisms as optional packages.

## 17. Internationalization

- locale identifiers and negotiation;
- Unicode normalization, segmentation, width, and case mapping;
- message catalogs and plural rules;
- date/time/number/currency formatting;
- bidirectional text and shaping integration;
- generated CLDR/Unicode data with version reporting.

Core compiler diagnostics can remain English initially, but library APIs should
avoid designs that make localization impossible.

## 18. Package, build, and documentation tooling

Maximum ergonomics eventually requires more than APIs:

The v0.1 bootstrap already provides a strict `runes.toml`, project entry
discovery, multiple module roots, an installed `std` search path, and named
local path dependencies. See [projects-and-modules.md](projects-and-modules.md).
The remaining package work includes:

- lockfiles and a versioned expansion of the current package manifest;
- semantic versions and reproducible dependency resolution;
- source registries plus Git/path dependencies;
- build scripts with controlled capabilities;
- native dependency discovery and linker configuration;
- target triples, feature flags, build profiles, and cross-compilation;
- package documentation generation with searchable API pages;
- formatter, linter, language server, debugger integration, and code coverage;
- vulnerability/advisory database and dependency auditing;
- vendoring and offline builds;
- workspace/monorepo support.

The package manager should not make the compiler runtime depend on networking.
Dependency fetching belongs in tooling, and fully vendored builds must work.

## 19. Platform support tiers

Recommended rollout:

- **Tier 1:** Linux x86-64, GCC and Clang C backend, fully tested.
- **Tier 2:** Linux AArch64 after target layout and ABI validation.
- **Tier 2:** macOS x86-64/AArch64 and Windows x86-64 after platform modules.
- **Tier 3:** BSDs, WebAssembly, embedded, and freestanding targets.

Portable modules should depend on narrow platform interfaces. Unsupported
operations must fail at build time or return a documented platform error; they
must not silently degrade correctness.

## 20. Implementation order

### P0: compiler bootstrap

1. `Option`, iterators, comparison, and hashing interfaces.
2. Allocator interface, `Vec`, `HashMap`, owned `String`, and `StringBuilder`.
3. Formatting, byte buffers, integer/float parsing.
4. Files, paths, buffered I/O, environment, and process execution.
5. Diagnostics, source spans, logging, testing, and CLI parsing.

Completing P0 makes a self-hosted Runes compiler practical.

### P1: general systems and applications

1. Time, randomness, OS handles, memory mapping, and dynamic libraries.
2. Threads, atomics, locks, scoped concurrency, and channels.
3. Serialization, compression, SQLite, and configuration.
4. Blocking TCP/UDP/Unix sockets, DNS, TLS bindings, HTTP client/server.
5. Package/build tooling and generated API documentation.

### P2: numerical and ML

1. Scalar math, deterministic PRNGs, statistics.
2. Matrix/tensor storage and views.
3. BLAS/LAPACK bindings and pure correctness kernels.
4. Autograd, neural-network modules, optimizers, datasets, and checkpoints.
5. Classical ML, tokenizers, transformer operations, and CPU inference.

### P3: graphics and media

1. SDL3/GLFW window/input bindings and image codecs.
2. Math types, 2D drawing, fonts, and text shaping.
3. Vulkan/WebGPU abstraction, shaders, assets, and 3D renderer foundations.
4. UI toolkit, audio, video bindings, and game-oriented packages.

### P4: ecosystem maturity

1. Async language/runtime design and async networking.
2. Additional databases, HTTP/2/3, distributed systems, and observability.
3. GPU compute and ML acceleration.
4. Cross-platform installers, application packaging, and broader target tiers.

## 21. Quality gates

Every official module should require:

- unit, integration, property, and malformed-input tests;
- fuzzing for parsers, codecs, protocols, and unsafe boundaries;
- ASan/UBSan and leak checks for native/FFI code;
- thread and race testing where applicable;
- GCC and Clang builds on supported targets;
- API documentation with runnable examples;
- benchmarks for performance-sensitive operations;
- allocation-count and failure-injection tests;
- interoperability tests against established implementations;
- security limits for untrusted sizes, nesting, time, and resource use;
- a statement of realm behavior, ownership, thread safety, and blocking.

## 22. Definition of ergonomic completeness

The ecosystem is broadly ergonomic when a user can build each of these without
writing platform C wrappers directly:

- a self-hosted compiler and build driver;
- a polished CLI application;
- a concurrent file-processing service;
- a secure HTTP client and production web server;
- a database-backed application;
- a desktop UI application;
- a 2D game and basic 3D renderer;
- numerical simulations and data analysis;
- CPU linear regression, classical ML, small neural networks, and small
  language-model training/inference;
- low-level Linux systems software with explicit unsafe/FFI isolation.

This is an ecosystem roadmap, not the v0.1 compiler completion boundary. The
compiler remains responsible only for language semantics and its minimal linked
runtime; the libraries can evolve independently through normal Runes packages.
