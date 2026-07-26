# bostrom-cli

`bostrom-cli` is a non-actuating Rust CLI tool for Bostrom DID–aligned operations and blacklist-aware governance checks within the Prometheus-Praxis / EcoNet constellation. It provides a thin, inspectable command-line surface over the `bostrom_cli` library, suitable for CI, lane governance tooling, and AI-chat integration without any physical-world actuation. [file:33]

The crate lives under `crates/bostrom-cli` in the mono-repo and is designed to work alongside governance spine crates, Eco-Fort schemas, and blacklist-filter logic. [file:33]

---

## Goals

- Provide a stable CLI entrypoint for:
  - Basic identity/greeting checks (`hello()`).
  - Minimal JSON-RPC style chat tooling (`--chat` mode).
- Integrate with local `blacklist_filter` logic to guard Bostrom DID operations and evidence strings.
- Remain strictly non-actuating:
  - No network calls.
  - No hardware control.
  - Only stdin/stdout/stderr IO. [file:33]

---

## Features

- **Library + binary layout**:
  - Library: `bostrom_cli` (`src/lib.rs`) exposes `hello()` and can be extended with governance helpers.
  - Binary: `bostrom-cli` (`src/main.rs`) provides argument parsing and I/O wiring. [file:33]

- **Default mode (no arguments)**:
  - Prints the `hello()` string:
    - Useful for CI health checks and workspace validation. [file:33]

- **`--chat` mode**:
  - Reads a single JSON-RPC request from stdin.
  - Supports simple methods:
    - `ping` → `pong`.
    - `identify` → returns the `hello()` string (`"bostrom_cli"`).
    - Unknown methods → `unknown_method:<name>`.
  - Writes a single JSON-RPC response to stdout. [file:33]

- **Non-actuating governance alignment**:
  - Intended to integrate with SQLite-backed governance spines (e.g., Eco-Fort, EcoNet) via read-only crates.
  - Can be used as a front-door for AI agents that must only read governance state and respond, never actuate. [file:33][file:5]

---

## Installation

From the Prometheus-Praxis mono-repo root, ensure the workspace is set up with Rust `1.85` and edition `2024`. The crate’s `Cargo.toml` is defined under `crates/bostrom-cli/Cargo.toml` and expects a local `blacklist_filter` crate at `../blacklist_filter`. [file:33]

To build the binary:

```bash
cd crates/bostrom-cli
cargo build --bin bostrom-cli
```

To run tests:

```bash
cargo test -p bostrom-cli
```

---

## Usage

### 1. Health check (default mode)

Print the canonical CLI identifier string:

```bash
./target/debug/bostrom-cli
```

Example output:

```text
bostrom_cli
```

This is useful for:

- CI steps that verify the crate builds and runs.
- Simple “is this tool wired correctly?” checks. [file:33]

### 2. JSON-RPC chat mode

Invoke the CLI in `--chat` mode and send a single JSON-RPC request via stdin:

```bash
echo '{"jsonrpc":"2.0","method":"ping","params":[],"id":"1"}' \
  | ./target/debug/bostrom-cli --chat
```

Example response:

```text
{"jsonrpc":"2.0","result":"pong","id":"1"}
```

Identify the CLI:

```bash
echo '{"jsonrpc":"2.0","method":"identify","params":[],"id":"2"}' \
  | ./target/debug/bostrom-cli --chat
```

Response:

```text
{"jsonrpc":"2.0","result":"bostrom_cli","id":"2"}
```

Unsupported methods:

```bash
echo '{"jsonrpc":"2.0","method":"unknown_op","params":[],"id":"3"}' \
  | ./target/debug/bostrom-cli --chat
```

Response:

```text
{"jsonrpc":"2.0","result":"unknown_method:unknown_op","id":"3"}
```

In all cases:

- Input is read entirely from stdin as a UTF-8 string.
- Output is a single-line JSON response written to stdout.
- Errors (e.g., parse failures) are reported on stderr and cause a non-zero exit code. [file:33]

---

## JSON-RPC Contract

The current implementation supports a minimal subset of JSON-RPC 2.0 semantics:

- Expected fields in requests:
  - `jsonrpc`: `"2.0"` (treated as a string).
  - `method`: string.
  - `params`: any JSON value, treated as opaque; the CLI does not yet inspect it.
  - `id`: string or number; echoed back as a string. [file:33]

- Response fields:
  - `jsonrpc`: copies the request’s `jsonrpc` value.
  - `result`: method-specific string:
    - `"pong"` for `ping`.
    - `"bostrom_cli"` for `identify`.
    - `"unknown_method:<name>"` otherwise.
  - `id`: copies the request’s `id`. [file:33]

This conservative approach:

- Avoids adding a full JSON parser dependency.
- Keeps the wire format simple and inspectable.
- Still gives AI-chat frameworks a structured way to interact with the CLI. [file:33]

---

## Integration Patterns

### With blacklist_filter

The crate’s `Cargo.toml` declares:

```toml
[dependencies]
blacklist_filter = { path = "../blacklist_filter" }
```

You can:

- Extend `src/lib.rs` to expose Bostrom DID / evidence-string helpers that:
  - Call into `blacklist_filter` to validate hex-stamps or identifiers.
  - Return simple verdicts or normalized error messages for use in `--chat` responses. [file:33]

- Use `bostrom-cli` as a read-only governance tool that:
  - Answers “is this ID / hex-string allowed?” queries.
  - Surfaces blacklist decisions to other tools without exposing raw implementation details. [file:33]

### With Eco-Fort / EcoNet governance spines

Combined with SQLite-backed governance crates (e.g., `econetgovernancespine` and Eco-Fort DB schemas), `bostrom-cli` can act as:

- A narrow front-door CLI for:
  - Listing allowed operations for a Bostrom DID.
  - Printing lane status or KER residual summaries in a safe, read-only fashion.
- A tool surface for AI agents:
  - Tools call `bostrom-cli --chat` with a JSON-RPC request.
  - The CLI performs read-only queries via governance spines.
  - Returns JSON-RPC responses with human-readable summaries. [file:33][file:5]

The current implementation focuses on the core plumbing; more governance-specific commands can be added later without breaking the non-actuating contract. [file:33]

---

## Non-Actuating Guarantees

`bostrom-cli` is explicitly designed to be **non-actuating**:

- No functions in this crate:
  - Open network sockets.
  - Control hardware, valves, or motors.
  - Write to any DBs or files (beyond standard Rust logging / stderr messages). [file:33]

- All operations:
  - Are limited to local stdin/stdout/stderr.
  - Can be used safely in CI pipelines, governance checks, and AI-tooling contexts. [file:33]

This aligns with the Eco-Fort and EcoNet governance contracts, where:

- CLI tools serve as diagnostic and read-only surfaces.
- Actuation logic is gated by separate orchestrator repositories, not by this crate. [file:5]

---

## Development and Testing

- Rust toolchain:
  - `rust-version = "1.85"`.
  - `edition = "2024"`. [file:33]

- Profiles:
  - `panic = "abort"` in dev and release.
  - Overflow checks enabled to catch arithmetic issues early. [file:33]

- Kani integration:
  - The workspace metadata declares Kani flags (e.g., `default-unwind=4`).
  - You can add `cfg(kani)` harnesses in `src/lib.rs` or under `tests/` to:
    - Prove bounds on JSON-RPC parsing.
    - Verify that the CLI never emits malformed JSON responses. [file:5]

Run unit tests:

```bash
cargo test -p bostrom-cli
```

Add Kani proofs following the established patterns in Eco-Fort and related crates, ensuring that any extensions to `bostrom-cli` remain verifiable and safe. [file:5]

---

## License

`bostrom-cli` is dual-licensed:

- MIT
- Apache-2.0

This matches the broader Prometheus-Praxis / EcoNet licensing scheme and allows both open-source reuse and tightly governed deployment within ecological restoration projects. [file:5]
