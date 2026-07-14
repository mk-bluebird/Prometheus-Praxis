# ker_triad Functional Definition

## 1. Role in Prometheus‑Praxis

- ker_triad is the energy‑sovereignty scoring crate that:
  - Wraps the pure KER engine (Knowledge K, Eco‑impact E, Risk‑of‑harm R) from `prometheuspraxisker`.
  - Exposes an Energy Sovereignty Scalar (ESS) and ESS state vectors for governance kernels.
  - Provides ESS distribution utilities and a Kolmogorov–Smirnov (KS) gate that enforces first‑order stochastic dominance between releases.

- The crate is non‑actuating: it computes numeric kernels only and does not touch hardware, networking, or IO, consistent with the governance spine design. [file:92][file:78]

## 2. Functional Components

- KER integration:
  - Uses `KnowledgeEvidence`, `EcoImpactEvidence`, and `RiskEvidence` from `prometheuspraxisker` to compute `KerOutput` in \([0,1]\) per axis.
  - Provides `compute_ker_and_ess` as a convenience function that returns both KER and a minimal ESS scalar placeholder.
  - Ensures KER values remain bounded and traceable to evidence bundles, aligning with KER triad safety invariants. [file:92]

- ESS representation:
  - Defines `EssSnapshot` (ESS scalar plus KER) and `EssState9D` (9‑D ESS state vector) to model sovereignty space coordinates.
  - Offers `ess_homotopy_path_stub` to construct a trivial 9‑D path; in full implementations this becomes a corridor‑respecting homotopy in ESS space. [file:92]

- ESS distribution sampling:
  - Provides `EssSample` and stub functions `ess_sample_previous_stub` and `ess_sample_current_stub` that generate ESS arrays for KS checks.
  - In production, these functions replay a fixed, hex‑stamped scenario set (qpudatashard windows, eco‑restoration episodes, city objects) for reproducible distributions. [file:91][file:78]

- KS stochastic dominance gate:
  - Implements `ess_ks_dominance_stub`:
    - Computes empirical CDFs for previous and current ESS samples.
    - Calculates the KS statistic \(D = \sup_x |F_\text{prev}(x) - F_\text{curr}(x)|\).
    - Checks the first‑order dominance condition \(F_\text{curr}(x) \le F_\text{prev}(x)\) for all grid points (current ESS is stochastically higher). [file:78]
  - Exposes `EssKsVerdict { dominates, ks_statistic, p_value, note }` for audit and CI.
  - Provides `ess_ks_gate_passes(sample_size, alpha)` as a pure gate that callers use to decide whether a release is “safe to publish”.

- Kani harness stub:
  - Declares `ess_ks_dominance_harness_stub` as a Kani‑friendly function that asserts basic numeric sanity on KS outputs.
  - Cargo metadata binds this harness name, preparing the crate for full formal proofs that dominance holds under bounded sample and scenario sets. [file:78]

## 3. Safety and Sovereignty Guarantees

- Non‑regression in ESS:
  - A version of ker_triad is considered “safe to publish” only if the ESS distribution of the new version first‑order dominates the previous version, meaning the probability of low ESS values does not increase for any threshold. [file:78]
  - KS statistics and p‑values are recorded as explicit artifacts for regulatory and internal audits.

- Integration with KER and governance:
  - ESS is treated as a derived sovereignty scalar over the same scenario sets used for KER and Lyapunov residuals.
  - Governance kernels use ESS (and its 9‑D states) as additional coordinates when evaluating macro‑actions against RoH ceilings, neurorights envelopes, and Lyapunov non‑increase. [file:92][file:78]

- Formal verification path:
  - The crate is structured for Kani:
    - `#![forbid(unsafe_code)]`, pure functions, typed inputs.
    - Clear separation between numeric kernels and external IO.
  - Future harnesses will prove:
    - That ESS KS gates cannot be bypassed in CI.
    - That any tagged ker_triad version satisfies dominance constraints and does not weaken sovereignty guarantees compared to its predecessor. [file:78]
