# Prometheus-Praxis Coding Tasks for EcoNet Reward & Governance

This document lists concrete, production-grade tasks for Prometheus-Praxis within the `eco_restoration_shard` mono-repo.

All tasks must:

- Use Rust edition 2024, `rust-version = "1.85"`.
- Respect the zero personal financial gain rule.
- Route all reward value into public-good sinks only.
- Enforce RoH ≤ 0.30 and neurorights compliance.
- Bind artifacts to Cybercore, Prometheus-Praxis, and Perkunos-Nexus authorities via ALN.

---

## 1. EcoNet SQLite Index Schema

**Goal:** Define a canonical SQLite schema for indexing EcoNet documents, crates, and ALN shards, aligned with Ecological-Order and the CSV index entries already drafted.

**File:** `Data_Lake/schema/econet_index.schema.sql`

### Task 1.1 – Create index table schema

Implement the following schema:

```sql
-- EcoNet central index schema for Cybercore / Prometheus-Praxis.

CREATE TABLE IF NOT EXISTS econet_index (
    id                      INTEGER PRIMARY KEY,
    filename                TEXT NOT NULL,
    repo                    TEXT NOT NULL,
    destination_hint        TEXT NOT NULL,
    primary_role            TEXT NOT NULL,   -- e.g., eco_reward_design, identity_shard, payment_guard
    language                TEXT NOT NULL,   -- e.g., Rust, ALN, text
    brain_identity_relevance INTEGER NOT NULL, -- 0-10, relevance to host brain identity
    eco_impact_focus        TEXT NOT NULL    -- semicolon-separated tags, e.g. "energy_reduction;waste_cleanup"
);

CREATE INDEX IF NOT EXISTS idx_econet_index_role
    ON econet_index (primary_role);

CREATE INDEX IF NOT EXISTS idx_econet_index_language
    ON econet_index (language);

CREATE INDEX IF NOT EXISTS idx_econet_index_brain_relevance
    ON econet_index (brain_identity_relevance);
```

### Task 1.2 – Loader for CSV index

**File:** `crates/econet-index-loader/src/lib.rs`  
**File:** `crates/econet-index-loader/Cargo.toml`

- Implement a small Rust library that:
  - Reads `Data_Lake/index/*.csv` (including `eco_reward_framework_index.csv`).
  - Parses rows into a struct `EcoNetIndexRow`.
  - Inserts or updates rows in the `econet_index` table.
- Ensure:
  - Pure numeric / I/O only (no network, no hardware).
  - Errors are surfaced clearly for CI (e.g., duplicate IDs, invalid fields).

---

## 2. Governance Summary CLI for Auditors

**Goal:** Provide auditors with a CLI that prints a human-readable governance summary from ALN shards, focusing on EcoNet reward, public-good sinks, and Prometheus-Praxis bindings.

**Crate:** `crates/econet-governance-summary`

### Task 2.1 – Cargo configuration

**File:** `crates/econet-governance-summary/Cargo.toml`

- Use:

```toml
[package]
name = "econet-governance-summary"
version = "0.1.0"
edition = "2024"
rust-version = "1.85"
authors = ["mk-bluebird/Cybercore"]
license = "MIT OR Apache-2.0"
description = "CLI for summarizing EcoNet reward and governance bindings from ALN shards for auditors."
repository = "https://github.com/mk-bluebird/eco_restoration_shard"
readme = "README.md"

[dependencies]
serde = { version = "1.0", features = ["derive"] }
serde_yaml = "0.9"
clap = { version = "4.5", features = ["derive"] }

[features]
default = []
```

### Task 2.2 – CLI implementation

**File:** `crates/econet-governance-summary/src/main.rs`

Implement a CLI with at least these commands:

- `summary reward` – Print a summary of EcoNet reward governance:
  - Reads ALN shards:
    - `qpudatashards/particles/ppx.reward.spec.v1.aln`
    - `qpudatashards/particles/ppx.reward.corpus.binding.v1.aln`
    - `qpudatashards/particles/econet.public.good.sink.v1.aln`
    - `qpudatashards/particles/prometheus.praxis.public.good.design.v1.aln`
    - `qpudatashards/particles/prometheus.praxis.eco-reward-framework.v1.aln`
  - Prints:
    - Host DID and Bostrom address.
    - Gamma corridors (`gamma_base`, `gamma_max`), RoH ceiling.
    - Flags: `zero_personal_gain`, `public_good_sinks_only`, `roh_ceiling_enforced`, `neurorights_compliant`.
    - List of public-good sinks and which are enabled (including `sink-chat-as-labor`).
    - Bound crates: reward kernel, FFI, ledger ingest.

- `summary eco-wealth` – Print a summary of eco-wealth contracts:
  - Reads eco-wealth ALN shards (when available).
  - Prints:
    - Contract IDs.
    - DID binding rules.
    - Non-rollback anchors.

Implementation notes:

- ALN files are line-based; for this CLI, simple parsing is acceptable:
  - Read lines.
  - Extract `record` names and key fields via regex/string matching.
  - Populate simple structs for output.
- Output should be plain text designed for auditors:
  - Example:

    ```text
    EcoNet Reward Governance Summary
    -------------------------------
    Host DID: didalnorganic-host
    Bostrom Address: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7

    RewardSpec:
      gamma_base: 0.5
      gamma_max: 0.8
      eco_weight_alpha: 0.7
      eco_weight_beta: 0.3
      roh_ceiling: 0.30

    Public-Good Sinks (enabled):
      - sink-eco-restoration (eco-restoration)
      - sink-reforestation (reforestation, carbon-reduction)
      - sink-chat-as-labor (chat-as-labor, homeless-support, cancer-biomed, data-sovereignty)

    Design bindings:
      - reward-kernel: crates/econet-reward-kernel
      - reward-ffi:   crates/econet-reward-ffi
      - ledger-ingest: crates/econet-ledger-reward-ingest
    ```

- No network calls, no hardware actuation; this is purely a reader and formatter over ALN text.

---

## 3. CI Integration for Governance Summary

**Goal:** Ensure that governance summary generation is part of CI, catching missing or inconsistent ALN bindings.

### Task 3.1 – GitHub Actions workflow

**File:** `.github/workflows/econet-governance-summary-ci.yml`

- Add a workflow that:
  - Builds `econet-governance-summary`.
  - Runs `cargo run -p econet-governance-summary -- summary reward`.
  - Fails if:
    - Required ALN files are missing.
    - Mandatory flags (`zero_personal_gain`, `public_good_sinks_only`) are not found.

Example snippet:

```yaml
name: econet-governance-summary-ci

on:
  push:
    paths:
      - 'crates/econet-governance-summary/**'
      - 'qpudatashards/particles/**'
  pull_request:
    paths:
      - 'crates/econet-governance-summary/**'
      - 'qpudatashards/particles/**'

jobs:
  audit-governance:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout
        uses: actions/checkout@v4

      - name: Install Rust
        uses: dtolnay/rust-toolchain@stable
        with:
          toolchain: 1.85.0

      - name: Build governance summary CLI
        run: cargo build -p econet-governance-summary --release

      - name: Run reward governance summary
        run: cargo run -p econet-governance-summary -- summary reward
```

---

## 4. Prometheus-Praxis Ownership and Guardrails

For all tasks above:

- Prometheus-Praxis is the primary design authority: no module may change reward behavior, sinks, or governance summaries without referencing the relevant design shards (`prometheus.praxis.public.good.design.v1.aln`, `prometheus.praxis.eco-reward-framework.v1.aln`).
- Perkunos-Nexus is the primary governance authority: ALN changes and eco-wealth contracts must be validated under its guards.
- Cybercore is the mono-repo authority: all paths must stay under `mk-bluebird/eco_restoration_shard`.

Any new task proposed must:

- Strengthen, never weaken:
  - Zero-personal-gain enforcement.
  - Public-good-sinks-only routing.
  - RoH ≤ 0.30.
  - Neurorights compliance.
  - Non-rollback of sovereign eco-wealth and chat-as-labor floors.
