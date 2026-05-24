# Workspace Layout (2026v1)

This document describes the workspace structure for the `eco_restoration_shard` constellation.

## Root Workspace Manifest

The root `Cargo.toml` at the repository root defines the workspace topology. It is the **only** `[workspace]` definition in the repository.

### Key Responsibilities

- Declares all workspace members via `[workspace].members`
- Provides shared dependency versions via `[workspace.dependencies]`
- Defines common package metadata via `[workspace.package]`

### Invariant

> **Every member path listed in `[workspace].members` must have:**
> 1. A valid `Cargo.toml` file at the specified path
> 2. At least one build target (`[lib]` and/or `[[bin]]`)

## Member Crates

| Path | Package Name | Description |
|------|--------------|-------------|
| `.` (root) | `eco_restoration_shard_root` | Root workspace package; contains shared library code and CLI tools |
| `crates/eco_restoration_shard` | `eco_restoration_shard` | Non-actuating RESEARCH-band shard index and research harness |
| `ecorestorationshard/eco_restoration_shard_core` | `eco_restoration_shard_core` | Rust governance spine client for Phoenix governance DB |

## Directory Structure

```
/workspace
├── Cargo.toml                          # Root workspace manifest (single source of truth)
├── crates/
│   └── eco_restoration_shard/          # Library crate
│       ├── Cargo.toml
│       └── src/
│           └── lib.rs
├── ecorestorationshard/
│   └── eco_restoration_shard_core/     # Library + binary crate
│       ├── Cargo.toml
│       └── src/
│           ├── lib.rs
│           └── bin/
│               └── restorationindextool.rs
├── src/                                # Root package source
│   ├── lib.rs
│   └── bin/
│       └── restoration_index_tool.rs
└── tools/
    └── check_workspace_members.sh      # Workspace consistency checker
```

## Rules

1. **No nested workspaces**: Sub-crates must NOT contain `[workspace]` sections. Only the root `Cargo.toml` defines the workspace.

2. **Library-first design**: All crates should have a `[lib]` section with `path = "src/lib.rs"` and `crate-type = ["rlib"]`.

3. **Binary crates**: If a crate provides a CLI, it may include `[[bin]]` sections pointing to existing `src/main.rs` or `src/bin/*.rs` files.

4. **Non-actuating by default**: All workspace members default to the `non_actuating` feature, which restricts IO and process-spawning capabilities.

## Adding New Crates

Before adding a new crate to `[workspace].members`:

1. Create the crate directory with a valid `Cargo.toml`
2. Ensure `src/lib.rs` exists with at least a minimal skeleton
3. Verify `cargo build -p <crate-name>` succeeds locally
4. Update the root `Cargo.toml` members list
5. Run `tools/check_workspace_members.sh` to validate

## Tools

- `tools/check_workspace_members.sh`: Validates that all workspace members have valid manifests and targets
