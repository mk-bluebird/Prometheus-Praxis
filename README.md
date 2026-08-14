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
- Orchestrate **eco_restoration_shard** and satellite repos (Cyboquatics, Cybercore, Augmented‑Citizen, Nano-Tree, Data_Lake, BLE‑Code) into a coherent city‑OS tree.
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

- `mk-bluebird/Prometheus-Praxis`
- `mk-bluebird/Cyboquatics`
- `mk-bluebird/Cybercore`
- `mk-bluebird/Skynet`
- `mk-bluebird/Nano-Tree`
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

---

## 12. Getting Started for AI-Chat Assistants

This section explains how AI assistants should interact with this repository.

### Preferred Entry Points

AI agents should start with these files and directories:

| Entry Point | Purpose | Safe to Modify? |
|-------------|---------|-----------------|
| `docs/ALN-SPECS.md` | Index of all ALN specs and their consumers | Yes (add entries) |
| `CONTRIBUTING.md` | Contribution guidelines and checklists | No (read-only) |
| `python/*.py` | Python utilities for diagnostics and metrics | Yes (with tests) |
| `tools/*.py` | Validation and quality tools | Yes (extend functionality) |
| `examples/` | Example scenarios demonstrating KER/corridor logic | Yes (add examples) |
| `tests/python/` | Unit tests for Python tools | Yes (add tests) |

### Naming Conventions to Respect

- **ALN files**: `domain_purpose.v1.aln` or `domain-purpose-v1.aln`
- **Python modules**: `snake_case.py` with module-level docstring
- **Rust crates**: `snake_case` under `crates/`
- **Lua scripts**: `snake_case.lua` under `tools/` or `lua/`

### AI-Chat Safety Checklist

Before making any changes, verify:

- [ ] **Respect ALN invariants**: Never weaken safety constraints or governance gates
- [ ] **Avoid blacklisted primitives**: No Argon2, BLAKE, SHA3-256, "digital twins"
- [ ] **Use native tools only**: No `pip install`, no cargo for simple tasks
- [ ] **Read ALN specs first**: Understand governance before modifying code
- [ ] **Add docstrings**: All public functions/classes must have docstrings
- [ ] **Run validations**: `python tools/repo_quality_check.py` passes
- [ ] **Update docs**: Add entries to `docs/ALN-SPECS.md` for new specs

### Safe Modification Patterns

**Safe operations for AI assistants:**

```bash
# Run tests to verify changes
python -m unittest discover tests/python

# Check for quality issues
python tools/repo_quality_check.py

# Find documentation gaps
python tools/docstring_check.py

# Run example scenarios
python examples/ker/compute_ker_scores.py
```

**Avoid these operations:**

- Modifying core ALN specs in `aln/` without understanding all consumers
- Changing Rust crate APIs without updating dependent code
- Installing new Python packages (use stdlib only for tools)
- Modifying database schemas in `db/` without migration paths

### Data Flow Understanding

To understand how data flows through the system:

1. Start at `docs/ALN-SPECS.md` to find spec definitions
2. Check "Consumed by" section to see which modules use each spec
3. Read Python/Lua/Rust code that references the spec
4. Trace inputs → processing → outputs

For more details, see `CONTRIBUTING.md` and `docs/MAINTENANCE_SESSION.md`.

---

## Prometheus-Praxis: Eco-Governance Monorepo Overview

Prometheus-Praxis is the consolidated, mono-repository for EcoFort-aligned, Phoenix-anchored ecological restoration research and governance tooling. It unifies prior EcoNet constellation work into a single, contract-driven codebase focused on non-actuating diagnostics, carbon-negative machinery planning, and Rust-anchored data sovereignty.

### Core principles

- Contract-first design using ALN v2 particles for all governance-relevant data structures.
- Non-actuating numeric kernels (C++ and Rust) that never directly control hardware.
- EcoFort/Phoenix governance semantics embedded into schemas, triggers, and proofs.
- Bostrom DID and Phoenix hex anchors for authorship, provenance, and discoverability.
- Carbon-negative orientation: workloads and diagnostics must align with restoration corridors.

---

## Repository structure and major bands

### Root-level bands

- `aln/`
  - ALN v2 governance particles for cyboquatic workloads, drainage-decay, blast-radius, and energy/ecoperJoule restoration.
  - JSON machine-readable contracts (e.g., `aln_particles_cyboquatic.json`) used as a single source of truth for codegen.
- `src/cpp/`
  - Non-actuating C++ numeric engines for cyboquatic domains.
  - Generated headers in `src/cpp/generated/` derived from ALN contracts.
- `crates/`
  - Rust crates providing:
    - ALN-driven code generation.
    - Lyapunov/KER guard helpers and Kani proofs.
    - Governance and ecosafety utilities.
- `db/`
  - SQLite schema files for daily progress, drainage-decay, blast-radius, and energy/ecoperJoule restoration indices.
  - Generated DDL under `db/generated/` mirroring ALN particle structures.
- `docs/`
  - Governance, placement, and binding documentation:
    - Engine placement and Phoenix anchors.
    - ALN↔SQL bindings, AI-safe entrypoints, and governance validation.

### Cyboquatic band (hydraulic/ecological diagnostics)

Cyboquatic artifacts implement non-actuating diagnostics for hydraulic corridors, canal nodes, and eco-restoration workloads:

- Workload energetics: energy, duty cycle, Lyapunov residuals, K,E,R triads.
- Drainage-decay: BOD/TSS/CEC frames, corridor-normalized residuals.
- Blast-radius: surcharge breach diagnostics, radius metrics, RoH, and KER planes.
- Energy/ecoperJoule/restoration: explicit carbon-negative flags and eco-efficiency metrics.

---

## ALN v2 governance particles

Prometheus-Praxis uses ALN v2 particles as canonical governance contracts. Key cyboquatic particles include:

- `cyboquatic.workload.kernel`
  - Domain: `CYBOQUATIC`
  - Subdomain: `WORKLOADENERGYDV`
  - Fields: node/window identifiers, `energyReqJ`, `dutyCycle`, `vtCurrent`, `vtNext`, `vtDelta`, `k`, `e`, `r`, `kerScore`, `lane`, `safeToPromote`, `evidenceHex`, `signingDid`.
  - Invariants:
    - \(0 \leq k,e,r \leq 1\).
    - \(\text{kerScore} \approx k \cdot e - r\).
    - \(\Delta V_t \leq 0\) (non-increasing Lyapunov residual).

- `cyboquatic.drainagedecay.kernel`
  - Domain: `HYDRO`
  - Subdomain: `DRAINAGEDECAY`
  - Fields: drainage frame IDs, canal node and ker profile IDs, `bodMgL`, `tssMgL`, `cecCmolPerKg`, `frameEnergyJ`, `deltaVtMps`, K,E,R triad, `kerScore`, FOG region/channel, governance/evidence hexes, DID.
  - Invariants:
    - Corridor bounds for BOD/TSS/CEC.
    - K,E,R and KER score consistency.

- `cyboquatic.blastradius.governance`
  - Domain: `GOV`
  - Subdomain: `CYBOQUATIC`
  - Fields: blast index IDs, corridor and lane IDs, hydraulic metrics, radius metrics, K,E,R triads, residual KER, RoH coordinates, governance flags, provenance.
  - Invariants:
    - Normalized radius and RoH in `[0,1]`.
    - Residual KER ≥ 0 and K,E,R bounds.

- `cyboquatic.energy.ecoperjoule.restoration`
  - Domain: `CYBOQUATIC`
  - Subdomain: `ENERGYRESTORATION`
  - Fields: energy requirements, ecoperJoule, restoration flags, carbon-negative status, K,E,R triad, KER score, evidence/DID.
  - Invariants:
    - ecoperJoule within defined eco-corridors.
    - `carbonNegativeOk` must be true for admissible frames.

These particles are the authority for field names, types, and core invariants, and drive all downstream struct and schema generation.

---

## Non-actuating numeric engines (C++)

The `src/cpp` directory contains pure numeric kernels for cyboquatic diagnostics. Engines operate exclusively on data structures and do not interact with hardware, networks, or devices.

### Workload engine

- File: `src/cpp/cyboquatic_workload_engine.cpp`
- Input:
  - Workload telemetry (`energyReqJ`, `headM`, `throughputM3`, `dutyCycle`, uncertainty factors).
  - Node and window identifiers.
- Output:
  - `cyboquatic_workload_kernel_struct` populated with:
    - Normalized risk coordinates (`r_energy`, `r_hydraulics`, `r_uncertainty`).
    - Lyapunov residuals (`vtCurrent`, `vtNext`, `vtDelta`).
    - K,E,R triad and KER score.
    - Governance lane and `safeToPromote`.
    - Evidence hex and DID provenance.

### Drainage-decay engine

- File: `src/cpp/cyboquatic_drainagedecay_engine.cpp`
- Input:
  - Drainage frame telemetry (`bodMgL`, `tssMgL`, `cecCmolPerKg`, `deltaVtMps`).
- Output:
  - `cyboquatic_drainagedecay_kernel_struct` with:
    - Normalized corridor coordinates.
    - Lyapunov hints.
    - K,E,R triad, KER score, FOG bindings.
    - Governance and evidence hexes.

### Blast-radius engine

- File: `src/cpp/cyboquatic_blastradius_engine.cpp`
- Input:
  - Hydraulic and surcharge metrics (`surchargeLevelM`, `inflowM3s`, `durationS`, `hydraulicHeadM`).
- Output:
  - `cyboquatic_blastradius_governance_struct` expressing:
    - Raw and normalized radius.
    - K,E,R triad, KER score, residual KER, RoH coordinate.
    - Corridor and lane flags for governance.
    - Provenance fields.

### Energy/ecoperJoule restoration tooling

- Uses `cyboquatic_energy_ecoperjoule_restoration_struct` to:
  - Bind workload energy frames to `ecoperJoule`.
  - Track restoration and carbon-negative flags in tandem with K,E,R.

All engines share the following properties:

- No hardware or device APIs.
- No network sockets or external IO beyond data struct handling.
- Designed for FFI integration with Rust/Java governance crates.

---

## ALN-driven code generation pipeline

Prometheus-Praxis includes a schema-driven codegen pipeline to prevent manual drift between ALN contracts and implementation artifacts.

### Contract source

- File: `aln/aln_particles_cyboquatic.json`
  - Machine-readable representation of cyboquatic ALN particles.
  - Captures `id`, `name`, `domain`, `subdomain`, and `fields` with `kind`.

### Codegen crate

- Crate: `crates/aln-cyboquatic-codegen`
- Role:
  - Parse JSON contracts.
  - Emit C++ headers in `src/cpp/generated/`.
  - Emit SQL DDL in `db/generated/`.

### Generated artifacts

- C++ headers:
  - `src/cpp/generated/cyboquatic_workload_kernel_struct.hpp`
  - `src/cpp/generated/cyboquatic_drainagedecay_kernel_struct.hpp`
  - `src/cpp/generated/cyboquatic_blastradius_governance_struct.hpp`
  - `src/cpp/generated/cyboquatic_energy_ecoperjoule_restoration_struct.hpp`

- SQL DDL:
  - `db/generated/cyboquatic_workload_kernel.sql`
  - `db/generated/cyboquatic_drainagedecay_kernel.sql`
  - `db/generated/cyboquatic_blastradius_governance.sql`
  - `db/generated/cyboquatic_energy_ecoperjoule_restoration.sql`

### Typical invocation

From the repo root:

```bash
cargo run --manifest-path crates/aln-cyboquatic-codegen/Cargo.toml -- \
  aln/aln_particles_cyboquatic.json \
  src/cpp/generated \
  db/generated
```

Developers modify ALN contracts (and export them to JSON), then regenerate structs and schemas. Hand-authored code and triggers build on these generated artifacts.

---

## SQLite governance schemas and triggers

The `db/` directory provides governance-aligned SQLite schemas and triggers for cyboquatic indices.

### Daily progress

- File: `db/dbcyboquaticdailyprogress.sql`
- Table: `cyboquatic_daily_progress`
- Purpose:
  - Consolidate per-day domain shards into a single daily progress index.
  - Persist K,E,R triads and Lyapunov residuals per node and window.
- Governance:
  - CHECK constraints for K,E,R bounds.
  - Trigger `trg_daily_progress_ker_lyapunov` to enforce:
    - KER score consistency: \(|k \cdot e - r - \text{ker_score}| \leq 10^{-6}\).
    - Non-increasing residual: `vt_delta <= 0`.

### Drainage-decay index

- File: `db/dbcyboquaticdrainagedecayindex.sql`
- Table: `cyboquatic_drainagedecay_index`
- Purpose:
  - Long-lived index for BOD/TSS/CEC frames.
  - Corridor-normalized windows with K,E,R, Vt, and evidence hex bindings.
- Governance:
  - Bounds for environmental parameters.
  - Triggers enforcing positive KER scores and consistency with K,E,R.

### Blast-radius index

- File: `db/dbcyboquaticblastradiusindex.sql`
- Table: `cyboquatic_blast_radius_index`
- View: `v_cyboquatic_blast_radius_facade`
- Purpose:
  - Diagnostic blast-radius index for surcharge breaches.
  - Non-actuating governance spines providing radius, K,E,R, RoH, and lane flags.
- Governance:
  - `radius_norm` and `roh_coordinate` bounded in `[0,1]`.
  - `residual_ker` non-negative.
  - KER consistency enforced via triggers.

### Energy/ecoperJoule restoration

- File: `db/dbcyboquaticenergyecoperjoulerestoration.sql`
- Table: `cyboquatic_energy_ecoperjoule_restoration`
- Purpose:
  - Bind workload energy frames to ecoperJoule and restoration flags.
  - Encode `carbonNegativeOk` as a hard governance requirement.
- Governance:
  - Trigger ensuring KER consistency.
  - Trigger rejecting frames where `carbonNegativeOk = 0`.

---

## Lyapunov and KER guard crate (Rust)

To further enforce and prove governance invariants, Prometheus-Praxis provides a Rust crate for Lyapunov and KER checks.

### Crate: `prometheus-praxis-lyapunov-guard`

- Functions:
  - `lyapunov_non_increasing(vt_current: f64, vt_next: f64) -> bool`
    - Ensures \(\Delta V_t = vt\_next - vt\_current \leq 0\) within epsilon.
  - `ker_band_and_consistency(k: f64, e: f64, r: f64, ker_score: f64) -> bool`
    - Validates \(0 \leq k,e,r \leq 1\) and \(|k \cdot e - r - \text{ker_score}| \leq 10^{-6}\).

- Kani proofs (when built with `cfg(kani)`):
  - Prove that `lyapunov_non_increasing` holds for all `vt_next <= vt_current`.
  - Prove that `ker_band_and_consistency` holds when `ker_score` is defined as `k * e - r` and K,E,R are within `[0,1]`.

Governance crates wrapping C++ kernels can use these helpers to verify outputs before persisting them.

---

## AI-safe entrypoints and diagnostics

Prometheus-Praxis is designed to be friendly to AI-assisted research while maintaining strict safety boundaries.

### AI-safe surfaces

- ALN specs in `aln/`:
  - Read-only contracts describing fields, bounds, and invariants.
- DB schemas and views in `db/`:
  - Read-only SQLite connections to inspect diagnostic frames and governance flags.
- Generated JSON/CSV diagnostics:
  - Produced by Java/Kotlin reporters purely for analysis (no actuation).

### Guidelines for AI agents

- Only read, never actuate:
  - Reason over ALN, C++, Rust guards, and DB data.
  - Do not propose or generate code that touches hardware APIs, pumps, or control systems.
- Respect governance invariants:
  - Preserve K,E,R bounds, Lyapunov non-increase, RoH ceilings, and carbon-negative requirements.
- Maintain provenance:
  - Carry `evidenceHex` and `signingDid` fields through transformations.
  - Avoid modifying hex anchors or DIDs in ways that break auditability.

Documentation such as `docs/CYBOQUATICAIENTRYPOINTS.md` enumerates recommended entrypoints and usage patterns.

---

## Extending Prometheus-Praxis

Prometheus-Praxis is designed to be extensible for new eco-restoration bands and research topics.

### Adding a new band

1. Define ALN particle(s) for the band under `aln/`.
2. Export the particle definitions to JSON and merge into `aln/aln_particles_cyboquatic.json` or a band-specific JSON file.
3. Run the codegen pipeline to produce C++ structs and SQL schemas.
4. Implement non-actuating C++ kernels and Rust guards that compute K,E,R and Lyapunov metrics.
5. Add DB triggers, views, and documentation to align with EcoFort/Phoenix governance.

### Research and upgrade pathways

- Add cross-band views and triggers to enforce global invariants (e.g., linking workload, drainage, blast-radius, and energy/restoration data for a node).
- Expand Kani proof suites to cover more complex Lyapunov models and corridor definitions.
- Introduce new AI-facing diagnostics and documentation to support exploratory research while maintaining strict safety boundaries.

Prometheus-Praxis is the foundational study and tooling space for making a real-world difference "just by researching it": every new diagnostic, contract, and proof is designed to advance ecological restoration, protect augmented citizens and data sovereignty, and strengthen governance of carbon-negative machinery.

---

## Task ↔ EcoNet / eco_restoration_shard Mapping Table

Below is a compact mapping table showing, for each task, which eco_restoration_shard / EcoNet artifacts it should bind to, and which K/E/R band it is primarily meant to improve.[file:2][file:22]

| Task ID        | Primary EcoNet / eco_restoration_shard Artifacts                                           | Main K/E/R Band Focus |
|----------------|---------------------------------------------------------------------------------------------|------------------------|
| PPX-TASK-0001  | `ecorestoration_shard/.econet/econetrepoindex.sql`, `Eco-Fort/db/ecoconstellationindex.sql` | K (knowledge, topology) and R (risk visibility) |
| PPX-TASK-0002  | `Eco-Fort/db/planeweightsschema.sql`, `Eco-Fort/db/blastradiusspine.sql`                    | R (blast-radius), E (ecosafety envelope), K (policy clarity) |
| PPX-TASK-0003  | `ppx.function.meta.v1.aln`, `Eco-Fort/db/repostatussemantics.sql`                           | K (governance metadata) |
| PPX-TASK-0004  | `Eco-Fort/db/virtasysgovernance.db`, `ppx.function.meta.v1.aln`                             | R (operational risk gating) |
| PPX-TASK-0005  | KER/Lyapunov crates in `eco_restoration_shard/crates/*`, `Eco-Fort/db/corridordefinitionschema.sql` | E (ecosafety envelope) and K (multi-plane coupling) |
| PPX-TASK-0006  | `Eco-Fort/db/econetrepoindexecosafetybinding.sql`                                          | R (actuation risk) |
| PPX-TASK-0007  | `Eco-Fort/db/contributionresumebostrom.sql`, `Eco-Fort/db/ecoperjoulepolicyenergy.sql`, `Eco-Fort/db/psychriskengine.db` | E (reward eco-alignment), R (psych/neurorights), K (sovereignty grammar) |
| PPX-TASK-0008  | `ppx.function.meta.v1.aln`, ALN spec index in `docs/ALN-SPECS.md`                           | K (spec discoverability) |
| PPX-TASK-0009  | `Eco-Fort/db/phoenixheatcorridor.db`, `Eco-Fort/db/psychriskengine.db`, `Eco-Fort/db/contributionresumebostrom.sql` | E (eco-health coupling), R (cross-corridor risk), K (cross-domain knowledge) |
| PPX-TASK-0010  | `Eco-Fort/db/virtaupgradeledger.sql`, `PrometheusPraxisCodingTaskList2026v1.aln`           | K (upgrade ledger), R (monotone safety) |

This shard and mapping are designed to be **directly usable**: you can drop the ALN block into `workspacealn/alnPrometheusPraxisCodingTaskList2026v1.aln` and start instantiating tasks, while the table guides which EcoNet and eco_restoration_shard artifacts each task should touch and which K/E/R band it is intended to strengthen.[file:21][file:22]
