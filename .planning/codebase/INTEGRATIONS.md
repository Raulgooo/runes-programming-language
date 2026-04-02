# External Integrations

**Analysis Date:** 2026-04-02

## APIs & External Services

**Not applicable** - This is a compiler/language implementation project with no external API integrations. The Runes language itself is designed to support integrations at the application level, but the compiler does not depend on any external APIs.

## Data Storage

**Databases:**
- Not applicable - No database integration in compiler

**File Storage:**
- Local filesystem only
  - Reads `.runes` source files from disk
  - Outputs compiled binaries to local filesystem
  - No cloud storage integration

**Caching:**
- None - Single-pass compilation, no persistent caching layer

## Authentication & Identity

**Not applicable** - No authentication required for compiler operation. Runes language provides `extern` and `unsafe` mechanisms for applications to integrate with auth systems, but the compiler itself has no auth integration.

## Monitoring & Observability

**Error Tracking:**
- Not applicable - No error tracking service integration

**Logs:**
- Standard output (stdout) for normal operation
- Standard error (stderr) for error messages
- File-based error reporting to `err.txt` (see `.gitignore`)
- No structured logging or remote monitoring

## CI/CD & Deployment

**Hosting:**
- Not applicable - This is a compiler tool, not a hosted service
- Distributed as compiled binary executable

**CI Pipeline:**
- Not applicable - No CI service integration
- Manual build via `make` command
- Test execution via `make test` target

**Distribution:**
- Single self-contained executable binary
- No package managers (npm, cargo, pip) involved
- Direct compilation from source with `make`

## Environment Configuration

**Required env vars:**
- None - Compiler requires no environment variables
- All configuration via command-line flags

**Configuration files:**
- Compiler has no config files - purely CLI-driven
- VSCode extension has configuration via:
  - `runes-lang/package.json` - Extension metadata
  - `runes-lang/language-configuration.json` - Language server config

## Language Integration Points

The Runes language (not the compiler) supports external integration through:

**Assembly Interface:**
- `asm` keyword - Inline assembly for low-level operations
- Location: `src/lexer.h` TOKEN_ASM

**Foreign Function Interface:**
- `extern` keyword - Call external C/system functions
- Location: `src/lexer.h` TOKEN_EXTERN

**Unsafe Operations:**
- `unsafe` keyword - Low-level memory manipulation
- Location: `src/lexer.h` TOKEN_UNSAFE

**Standard Library (Planned):**
- Core library: `src/std/core.runes`
- OS library: `src/std/os.runes`
- Prelude: `src/std/prelude.runes`
- Modules system via `mod` and `use` keywords

## Compiler Integration Points

**VSCode Extension:**
- Package: `runes-lang` (version 0.0.1)
- VSIX distribution: `runes-lang/syntaxes/runes.tmLanguage.json`
- Language ID: `runes`
- File extension: `.runes`
- Features:
  - Syntax highlighting (TextMate grammar)
  - Comment styles (line: `--`, block: `--- ---`)
  - Bracket matching and auto-closing pairs
  - Attribute highlighting (`#[...]` syntax)
  - Keyword highlighting (memory strategies, control flow, systems keywords)

**Tools:**
- Lexer tool: `dump_tokens` - Tokenizes Runes source
- AST tool: `ast_tool` - Parses and analyzes abstract syntax trees
- Built-in test framework

## Webhooks & Callbacks

**Not applicable** - Compiler is a batch processing tool with no webhook or callback mechanisms.

---

*Integration audit: 2026-04-02*
