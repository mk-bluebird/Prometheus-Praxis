# CI Lane Semantics (2026v1)

This document describes the CI job structure ("lanes") for the `eco_restoration_shard` workspace.

## Overview

CI jobs are organized into lanes that run in sequence. Each lane has a specific purpose and gates subsequent lanes.

## Lane Structure

### Lane 1: Workspace Health and Formatting

**Purpose**: Validate workspace topology and code style before any builds.

**Jobs**:
1. `cargo fmt --all --check` - Verify code formatting
2. `cargo clippy --workspace --all-targets -- -D warnings` - Lint all workspace members
3. `cargo metadata --no-deps` - Validate workspace manifest resolution
4. `tools/check_workspace_members.sh` - Verify all members have valid Cargo.toml and targets

**Gate**: All subsequent lanes depend on Lane 1 success.

### Lane 2: Core Spine Builds

**Purpose**: Build core infrastructure packages to verify structural integrity.

**Jobs** (parallel matrix):
- `cargo build -p eco_restoration_shard_root --all-targets`
- `cargo build -p eco_restoration_shard --all-targets`
- `cargo build -p eco_restoration_shard_core --all-targets`

**Gate**: Lane 3 depends on Lane 2 success.

### Lane 3: Eco Kernels

**Purpose**: Build specialized ecosystem kernel crates.

**Jobs** (parallel matrix):
- `cargo build -p <kernel-crate> --all-targets` for each kernel crate

**Note**: Currently no additional kernel crates beyond core spine.

## Non-Actuating Enforcement

All workspace members are research-band, non-actuating crates. This is enforced via:

### Clippy Configuration

Two Clippy configuration files:

1. **`clippy.toml`** - Standard development linting
2. **`clippy_research_ci.toml`** - CI-specific restrictions for research-band crates

### Restricted APIs

The following are denied in non-actuating crates (unless explicitly whitelisted):

- `std::process::Command` - Process spawning
- `std::fs::write`, `std::fs::remove_file` - Filesystem mutation
- Network APIs (`std::net`, `tokio::net`) - Network access
- External FFI calls

### Feature Flag

All crates must enable the `non_actuating` feature by default:

```toml
[features]
default = ["non_actuating"]
non_actuating = []
```

## Adding New CI Jobs

When adding new packages or build configurations:

1. **New Package**: Add to Lane 2 or Lane 3 matrix depending on category
2. **New Lane**: Only add if existing lanes cannot accommodate the check
3. **Feature Gates**: Ensure new packages respect non-actuating constraints

## Failure Isolation

Each package build runs as a separate job so that:
- Failures identify the specific broken package
- Other packages continue building in parallel
- CI provides actionable feedback immediately

## Local Verification

Before pushing changes, run locally:

```bash
# Lane 1 checks
cargo fmt --all --check
cargo clippy --workspace --all-targets -- -D warnings
cargo metadata --no-deps
./tools/check_workspace_members.sh

# Lane 2 + 3 checks
cargo build --workspace --all-targets
```

## CI Configuration Location

CI workflows are defined in `.github/workflows/` (or equivalent CI platform config).
