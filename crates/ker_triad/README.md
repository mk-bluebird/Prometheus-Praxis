# ker_triad

`ker_triad` is the Prometheus-Praxis KER triad + ESS scalar and KS-based stochastic dominance gate crate, bound to the `eco_restoration_shard` mono-repo and governed by non-actuation, sovereignty-first invariants. [file:92][file:78]

## 1. Role and scope

- Implements the KER triad axes: **knowledge**, **eco_impact**, and **risk_of_harm** as pure numeric kernels over typed evidence bundles.
- Derives an ESS scalar and 9-D ESS state vectors used for corridor gating and Lyapunov-based evolution checks.
- Provides a KS-first-order stochastic dominance gate for ESS distributions, enforcing “safe corridor” selection across candidate scenarios.
- Is strictly non-actuating: it never drives hardware, nanoswarms, or city actuators; it only computes scalars and dominance verdicts. [file:92][file:78]

## 2. Alignment with Cargo.toml metadata

- `edition = "2024"`, `rust-version = "1.85"`: matches the space-wide Rust requirements for all governance kernels. [file:78]
- `license = "MIT OR Apache-2.0"`: crate is dual-licensed under the default Prometheus-Praxis license set. [file:78]
- `[package.metadata.ker]` declares:
  - `axes = ["knowledge", "eco_impact", "risk_of_harm"]`.
  - `ess_scalar = true`, `ess_state_dimensions = 9`.
  - `stochastic_dominance_gate = "KS-first-order"`.
  - `roh_ceiling_global = "0.30"`, mirroring the global RoH ceiling invariant. [file:92]
- `[package.metadata.prometheuspraxis]` declares:
  - `role = "KER triad + ESS scalar and stochastic dominance gate"`.
  - `domain = "eco_restoration_shard"`.
  - `non_actuating = true`, `sovereignty_first = true`. [file:92][file:78]
- `[package.metadata.kani]` declares:
  - `version = "0.67"`.
  - `enable = true`.
  - `harnesses = ["ess_ks_dominance_harness_stub"]`. [file:92]

These metadata entries are the single source of truth for CI, governance, and audit tooling. [file:92][file:78]

## 3. Library contents (expected modules)

- `ker_axes.rs`
  - Defines KER axes and scalar types (e.g., `KerTriad`, `KerAxis` enums, bounded `Decimal` scalars).
  - Exposes pure functions to compute K, E, R from evidence, aligned with the `prometheuspraxisker` crate’s triad engine. [file:92]
- `ess_state.rs`
  - Defines ESS state vectors (9-D) and the ESS scalar aggregation kernel.
  - Encodes Lyapunov-compatible ESS evolution functions that preserve non-increase in the global Lyapunov residual. [file:78]
- `ess_distribution.rs`
  - Provides types for ESS scenario sets and empirical distributions.
  - Computes KS statistics and issues a dominance verdict for candidate ESS corridors. [file:92]
- `ess_ks_gate.rs`
  - Implements the KS-first-order stochastic dominance gate over ESS distributions.
  - Integrates with lane/lifecycle metadata so ESS selections respect KER thresholds and RoH ceilings. [file:92][file:78]

All modules must remain non-actuating and depend only on numeric and serialization crates declared in `Cargo.toml`. [file:92]

## 4. CI consumption of Cargo metadata

CI jobs should treat `Cargo.toml` as a configuration contract and enforce the following:

- Kani integration:
  - Read `[package.metadata.kani]`:
    - Assert `enable = true` for this crate.
    - Assert `version = "0.67"` to match workspace toolchain.
    - Assert that each harness listed in `harnesses` exists and is compiled in `tests` or `kani` modules. [file:92][file:78]
  - Run `cargo kani` on:
    - `ess_ks_dominance_harness_stub` (and successors) to prove:
      - If ESS_A is declared dominant over ESS_B under KS-first-order, no counterexample path exists where KS rejects dominance for the same inputs.
      - ESS selection never violates RoH ceiling or Lyapunov non-increase constraints inherited from the governance kernel. [file:78][file:92]

- Non-regression invariants:
  - Use `[package.metadata.ker]` to configure invariants enforced by CI:
    - `roh_ceiling_global = "0.30"`:
      - CI should scan tests/harnesses and require a proof that any ESS state or KER triad with `risk_of_harm > 0.30` is rejected or flagged as inadmissible.
    - `ess_state_dimensions = 9`:
      - CI should fail if the ESS state vector dimension in code drifts from 9 without a corresponding metadata and ALN shard update. [file:92]
    - `stochastic_dominance_gate = "KS-first-order"`:
      - CI must verify that the gate implementation still uses KS-first-order; switching to another method requires a metadata change and associated proofs. [file:92]

- Prometheus-Praxis binding:
  - Read `[package.metadata.prometheuspraxis]`:
    - Assert `non_actuating = true` by static analysis or `cargo geiger`-like checks that forbid `unsafe` and direct hardware/IO in this crate. [file:78]
    - Assert `sovereignty_first = true` by checking that all public APIs are pure functions over typed inputs and do not alter authority bindings. [file:78]

## 5. Required Kani harness properties

The Kani harnesses referenced in `Cargo.toml` must cover at least these properties:

- ESS KS dominance correctness:
  - If `ess_A` and `ess_B` are ESS distributions with `ess_A` declared dominant under KS-first-order, the gate:
    - Must not accept a scenario where `ess_B` actually has strictly lower risk-of-harm across all quantiles.
    - Must reject any configuration where KS statistic or p-value contradicts the declared dominance. [file:92]

- RoH ceiling and Lyapunov invariants:
  - For any ESS state and KER triad satisfying:
    - K and E above lane thresholds.
    - R below its lane-specific maximum.
    - RoH below `roh_ceiling_global`.
    - Lyapunov residual non-increase (within epsilon).
  - The gate and associated kernels:
    - Must not produce a decision that increases RoH beyond 0.30 or Lyapunov beyond its allowed slack.
    - Must never “weaken” existing corridor protections (non-regression). [file:78][file:92]

CI should treat any Kani counterexample as a hard failure and block merges or releases until invariants are restored. [file:78]

## 6. How to extend README and CI as research evolves

When you add new research (e.g., higher-order KS gates, new ESS planes, vulnerable-impact envelopes):

- Update `[features]` and `[package.metadata.*]` in `Cargo.toml`:
  - Examples: `ess_corridor_envelopes`, `vulnerable_impact`, ALN shard names for new envelopes. [file:92][file:78]
- Extend this README with:
  - New sections for added metadata fields (e.g., additional ESS dimensions, new dominance methods).
  - Explicit CI rules describing how those fields must be enforced by Kani and static checks. [file:78]
- Add matching Kani harnesses:
  - Ensure every new safety claim has at least one harness proving non-regression and correct application of KER, RoH, ESS, and Lyapunov invariants. [file:78][file:92]

This README should remain the human-facing mirror of `Cargo.toml` and the canonical description of how CI enforces ESS KS dominance and safety invariants across releases. [file:92][file:78]
