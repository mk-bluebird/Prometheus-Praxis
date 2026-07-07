# Prometheus‑Praxis

Prometheus‑Praxis is the augmented execution‑layer for the **eco_restoration_shard** constellation, focused on turning verified research into deployable code, materials, and governance logic for eco‑restoration, healthcare, cybernetics, and urban operations in and around Phoenix, AZ.

This repository is **not** a generic framework and **not** a toy. It is a live city‑OS execution tree that must respect ecosafety, neurorights, data sovereignty, and multi‑stakeholder governance at every layer.

---

## 1. Scope and Role

Prometheus‑Praxis exists to:

- Convert research and ALN specs into:
  - Rust 2024 crates (non‑actuating by default),
  - SQLite and SQL governance spines,
  - ALN governance and particle definitions,
  - CI/validation pipelines and MCP wiring.
- Orchestrate **eco_restoration_shard** and satellite repos (Cyboquatics, Cybercore, Augmented‑Citizen, nanorobotics, Data_Lake, BLE‑Code) into a coherent city‑OS tree.
- Provide **diagnostic and planning superpowers**, never unilateral control:
  - Ecosafety envelopes and Lyapunov residuals for water, heat, waste, and air.
  - Reward and KER scoring that remain non‑fungible and tied to public benefit.
  - Dev‑tunnel and MCP verb surfaces that are pre‑gated by governance.

Prometheus‑Praxis is **custom‑first** and sovereignty‑preserving:

- No external governance models are imported wholesale.
- New concepts are introduced as custom rules, ALN particles, or Rust crates that respect:
  - Augmented‑citizen sovereignty;
  - Non‑augmented citizen rights and consent;
  - Ecological corridors and KER scoring;
  - Neurorights corridors for any BCI, biosignal, or psych‑risk surface.

---

## 2. Core Principles

Prometheus‑Praxis follows these hard constraints:

- **R1 – No fiction, no toy examples**
  - All assets must be real‑world feasible and non‑harmful.
  - No “illustrative” snippets that are not deployable in principle.
- **R2 – Non‑actuating by default**
  - Planning, diagnostics, and governance logic live here.
  - Any actuation surfaces are pushed to outer, separately‑governed stacks and must pass ecosafety, neurorights, and sovereignty gates.
- **R3 – Dual‑license (MIT OR Apache‑2.0)**
  - All Rust crates and ALN shards adopt MIT OR Apache‑2.0.
- **R4 – Rust edition 2024, rust‑version 1.85**
  - All crates specify `edition = "2024"` and `rust-version = "1.85"`.
  - `kani-verifier = "0.67"` is mandatory where formal properties are defined.
- **R5 – No blacklisted primitives**
  - No Argon2, BLAKE, SHA3‑256, “digital twins”, or other explicitly banned items.
  - When stronger cryptography is needed, code must explicitly state the limitation and avoid simulating blacklisted primitives.
- **R6 – Monotone safety evolution**
  - No hidden downgrades, no rollbacks that weaken invariants.
  - No hidden control panels, greed tactics, or unilateral superpowers.

---

## 3. Repository Structure

Prometheus‑Praxis is designed as a **tree of planes** inside the global mono‑repo:

- `ecorestoration_shard/`
  - Canonical mono‑repo for ecosafety, Cyboquatics, energy, and city Lyapunov.
  - Prometheus‑Praxis assets bind to this root via ALN and Rust.
- `Prometheus-Praxis/`
  - This repository: execution layer, MCP wiring, CI, and governance logic.
  - Contains:
    - Function/meta ALN shards (`ppx.function.meta.*.aln`),
    - Governance helpers for blast‑radius and gate binding,
    - Tooling glue for MCP, dev‑tunnels, and CI.

A typical layout inside this repo looks like:

- `README.md`  
  High‑level documentation (this file).
- `ppx.function.meta.v1.aln`  
  ALN records describing MCP/AI‑exposed functions and their governance bindings.
- `tools/`
  - `src/governance_flag.rs`  
    Rust helpers for blast‑radius and governance flags (`B_f`, `H_f`, `G_f`).
  - Future helpers for:
    - MCP server wiring,
    - Dev‑tunnel filters,
    - CI rule checkers.

Prometheus‑Praxis assumes the following sibling/related repos are present and aligned:

- `mk-bluebird/eco_restoration_shard`
- `mk-bluebird/Cyboquatics`
- `mk-bluebird/Cybercore`
- `mk-bluebird/Augmented-Citizen`
- `mk-bluebird/nanorobotics`
- `mk-bluebird/Data_Lake`
- `mk-bluebird/BLE-Code`

All new files must be placed under these trees with semantically meaningful paths and unique filenames.

---

## 4. Functional Domains

Prometheus‑Praxis operates across four primary domains:

1. **Eco‑Restoration**
   - Cyboquatic nodes (canals, MAR vaults, pumps, soft robots).
   - Lyapunov envelopes and ecosafety corridors (`V_t`, risk coordinates).
   - Blast‑radius and workload windows for water, heat, and waste missions.

2. **Healthcare and Cybernetics**
   - Non‑fungible eco‑credits for public benefit.
   - Biosignal and BCI surfaces that respect neurorights and consent.
   - Homomorphic mappings into symbol grammars via human‑safe libraries.

3. **Urban Operations**
   - FOG flood channels, canyon wind‑nets, trash routing, pest‑deterrent logic.
   - City maintenance, waste, and mobility corridors bound by ecosafety.
   - Cyboquatic energy and material ledgers, coupled to heat and carbon.

4. **Governance and Identity**
   - DID‑bound brain‑identity and Eco‑Fort grammar:
     - `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
     - `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`
   - ALN governance shards for:
     - Absolute data sovereignty,
     - Contribution ledgers,
     - Neurorights and ecosafety gates.

---

## 5. Governance Model

Prometheus‑Praxis encodes governance in three main ways:

- **ALN Particles and Shards**
  - Function meta and gate bindings (`ppx.function.meta.v1.aln`).
  - Ecosafety, reward, and neurorights policies stored in versioned ALN files.
- **Rust Governance Helpers**
  - Static, non‑actuating Rust functions that:
    - Compute blast‑radius indicators (`B_f`),
    - Identify healthcare/cybernetics/nanoswarm reward paths (`H_f`),
    - Decide whether governance is required (`G_f = max(B_f, H_f)`).
- **CI and MCP Wiring**
  - CI checks that:
    - All high‑blast‑radius or reward‑bearing functions are bound to gates.
    - No new functions bypass ecosafety, neurorights, or sovereignty invariants.
  - MCP server configuration that:
    - Exposes only pre‑vetted, non‑actuating functions,
    - Enforces gates before any action verbs are accessible to AI agents.

**Superpowers** (e.g., nanoswarm routing, citywide Lyapunov modifications) are:

- Represented as **governance‑gated functions**, not raw APIs.
- Bound to:
  - KER scores (Knowledge, Eco‑impact, Risk‑of‑Harm),
  - Multi‑stakeholder approvals,
  - Time‑locks and audit trails.

No single human or augmented citizen can unilaterally operate these functions.

---

## 6. Rust and ALN Conventions

All Rust code in Prometheus‑Praxis and connected stacks must follow:

- **Rust Tooling**
  - `edition = "2024"`
  - `rust-version = "1.85"`
  - `kani-verifier = "0.67"` for formal verification crates.
- **Crate Policies**
  - `#![forbid(unsafe_code)]` at the crate root.
  - Clippy lints:
    - `#![deny(missing_docs)]`
    - `#![deny(clippy::unwrap_used)]`
    - `#![deny(clippy::expect_used)]`
    - `#![deny(clippy::panic)]`
- **Non‑Actuating Design**
  - Crates in Prometheus‑Praxis and eco_restoration_shard:
    - must not open sockets, serial ports, or device handles,
    - must not use embedded HALs or OS‑specific APIs,
    - must focus on diagnostics, planning, and governance math.
  - Any actuation layer must live in dedicated, governance‑bound stacks and use explicit gate surfaces.

**ALN**:

- All ALN files:
  - Use small, composable records and instances.
  - Declare KER scores and corridors where relevant.
  - Carry explicit compliance and sovereignty fields where data touches biosignals, BCI, or personally identifiable information.

---

## 7. MCP / Tooling Model

Prometheus‑Praxis treats MCP tools and AI‑callable functions as first‑class objects:

- **Function Meta (ALN)**
  - `ppx.function.meta.v1.aln` defines:
    - Function IDs and names.
    - Domains and corridors (e.g., `water`, `heat`, `governance`).
    - Capitals touched (water, thermal, biotic, somatic, neurobiome).
    - Whether ecosafety is required, and whether the function is actuating.
    - Policy tags such as `DATA_DIAGNOSTIC_MULTI_DOMAIN` or `GATE`.

- **Governance Bindings**
  - Each function can declare:
    - `REQUIRES_GATE` relations to gate IDs (e.g., `ecosafety.governance.gate.v1`).
  - CI enforces that:
    - Any function with `G_f = 1` must have a gate binding.
    - Functions with non‑diagnostic surfaces are not exposed without explicit approval.

- **Dev‑Tunnels**
  - All MCP and agent interactions are assumed to run through dev‑tunnels that:
    - Authenticate callers and hosts.
    - Limit verbs to a safe subset.
    - Log all calls for ecosafety and neurorights analysis.

---

## 8. Contribution Guidelines

Prometheus‑Praxis has strict contribution rules to keep the city‑OS safe, coherent, and composable.

### 8.1. General Rules

- No fictional scenarios, speculative examples, or “toy” code.
- Every addition must:
  - Be tied to a real mechanism, device, or governance pattern.
  - Be suitable for offline use (no hard external dependencies).
  - Respect existing blacklists and governance constraints.

### 8.2. Rust Code

- Place new code under a **new path** that does not conflict with existing files.
- Ensure:
  - Crate root uses `#![forbid(unsafe_code)]`.
  - Clippy warnings are treated as errors in CI.
  - No network, filesystem, or device IO in **diagnostic** crates.
- When adding Kani verification:
  - Use precise `kani-verifier = "0.67"`.
  - Focus on:
    - Type‑level safety (no raw brainstate crossing boundaries).
    - Reward semantics (no fungible tokens from eco/health flows).
    - Policy invariants mirrored from SQL/ALN.

### 8.3. ALN Shards

- Add new ALN files under descriptive, versioned filenames:
  - `ppx.function.meta.v1.aln`
  - `ppx.ecosafety.policy.v1.aln`
- Include:
  - Particle definitions (records).
  - Instances that tie particles to:
    - Functions, nodes, corridors, and KER scores.
- Keep shards:
  - Small, composable, and auditable.
  - Self‑documented through field names and comments.

### 8.4. Governance and Safety

- All new functions that:
  - Touch health, nanoswarms, cybernetics, BCI, or eco rewards
  - Or operate across many nodes/corridors
  - Must be classified with `B_f`, `H_f`, and `G_f` and bound to gates.
- If in doubt:
  - Treat new functionality as high‑blast‑radius until proven otherwise.
  - Add explicit CI checks to prevent accidental exposure.

---

## 9. Getting Started

### 9.1. Prerequisites

- Rust 1.85 (with `cargo`, `clippy`, `rustfmt`).
- Kani verifier (`kani-verifier = "0.67"`) for crates that include formal proofs.
- Access to:
  - `eco_restoration_shard` (mono‑repo),
  - `Prometheus-Praxis` (this repo),
  - Optional satellite repos: Cyboquatics, Cybercore, Augmented‑Citizen, etc.

### 9.2. Basic Workflow

1. **Clone the mono‑repo constellation**
   - Ensure `eco_restoration_shard` and `Prometheus-Praxis` are in your workspace.
2. **Read the ALN shards**
   - Start with `ppx.function.meta.v1.aln` and ecosafety policy ALN files in `eco_restoration_shard`.
3. **Build Rust crates**
   - For eco_restoration_shard:
     ```bash
     cargo build --workspace --all-targets
     ```
   - For Prometheus‑Praxis tools:
     ```bash
     cargo build -p prometheus-praxis-tools
     ```
4. **Run CI locally**
   - Use the same commands as the GitHub CI to check:
     - Build, test, clippy, and format.
     - Audit and deny checks for dependencies.
5. **Add new functions and gates**
   - Extend `ppx.function.meta.v1.aln` with new function meta and gate bindings.
   - Implement governance helpers and CI checks to enforce invariants.

---

## 10. Roadmap

Prometheus‑Praxis will evolve toward:

- **Richer ecosafety pipelines**
  - Multi‑frame pipelines for Lyapunov, biodiversity, and risk‑of‑harm.
  - More tightly coupled KER scoring and reward logic.
- **Deeper MCP integration**
  - Stronger dev‑tunnel enforcement and per‑function gating.
  - Tool discovery and documentation aligned with ALN shards.
- **Cross‑domain coupling**
  - Linking eco_restoration_shard, Cybercore, and Augmented‑Citizen into a single city‑OS plane.
  - Extending ecosafety grammar from water/heat to social, neural, and economic corridors.

All of this must remain:

- Non‑actuating by default,
- Eco‑restorative,
- Sovereignty‑preserving,
- And auditable by math, behavior, and open documentation.

---

## 11. Contact and Identity

Prometheus‑Praxis and eco_restoration_shard are aligned with:

- DID and Eco‑Fort identities:
  - `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
  - `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

Attribution and rewards for contributions are intended to bind to these identities through:

- ALN contribution ledgers,
- Governance shards,
- And KER‑based scoring of sustained, ecosafe improvements.

Please open issues or proposals in this repo only after reading this README and the relevant ALN shards, and align all new work with the principles and constraints described above.
