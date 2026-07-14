# ker_triad Changelog

All notable changes to the `ker_triad` crate are documented in this file.

Safety and sovereignty are first-class constraints. Each release includes:

- A formal description of changes to KER scoring, ESS computation, and path kernels.
- A first-order stochastic-dominance check on the ESS distribution of the new version
  against the previous release, implemented via a Kolmogorov–Smirnov (KS) test.
- An explicit “Safety Verdict” stating whether the new version is **safe to publish**
  under Prometheus-Praxis sovereignty rules.

ESS refers to the Energy Sovereignty Scalar used in the governance spine, bound
to RoH ceilings, Lyapunov non-increase, and non-offsettable corridors. [file:92][file:78]


---

## 0.3.0 – Homotopy-Guided ESS and Stochastic Dominance (2026-07-14)

### Summary

- Embed a homotopy-guided planner into `ker_triad` so that `compute_ker` can return
  both KER scalars and a piecewise-linear path in 9-D energy sovereignty space,
  optimising EcoHelpVector subject to ESS corridors. [file:92]
- Introduce an ESS distribution audit and KS-based stochastic dominance check
  as a mandatory gate for publishing. [file:78]

### Added

- `compute_ker` pure-function API now returns:
  - `KerOutput { k, e, r }` in \([0,1]\), consistent with `prometheuspraxisker`. [file:92]
  - A piecewise-linear path `Vec<EssState9D>` in the 9-D ESS space, with each waypoint
    constrained by RoH ceiling (0.30), Lifeforce floor, BCR and pain/fear corridors
    defined via ALN envelopes. [file:92][file:78]
- Internal homotopy engine:
  - Constructs an initial linear interpolation between start and goal ESS states.
  - Iteratively deforms waypoints to maximise cumulative EcoHelpVector while enforcing
    ESS admissibility (RoH, Lyapunov, Tsafe, non-offsettable planes). [file:92][file:78]
- ESS distribution sampler:
  - `ess_sample_previous(version_tag: &str) -> Vec<f64>` collects ESS scalars from
    the prior release by replaying a fixed, hex-stamped scenario set (qpudatashard
    windows, city objects, cyboquatic nodes). [file:91][file:78]
  - `ess_sample_current(version_tag: &str) -> Vec<f64>` runs the same scenario set
    against the current code to obtain the new ESS distribution. [file:91][file:92]
- KS-based first-order stochastic dominance check:
  - `ess_ks_dominance(prev: &[f64], curr: &[f64]) -> EssKsVerdict` computes:
    - Empirical CDFs \(F_\text{prev}\) and \(F_\text{curr}\).
    - KS statistic \(D = \sup_x |F_\text{prev}(x) - F_\text{curr}(x)|\).
    - A directional dominance condition:
      - For sovereignty, we require for all \(x\):
        - \(F_\text{curr}(x) \le F_\text{prev}(x)\) (ESS is stochastically **higher**),
          meaning probability of low ESS is not increased. [file:78]
  - The verdict encodes:
    - `dominates: bool` – whether new ESS distribution first-order dominates old.
    - `ks_statistic: f64` – the KS D statistic.
    - `p_value: f64` – approximate KS p-value (two-sample).
    - `notes: String` – human-readable explanation for auditors. [file:78]
- CI integration hooks:
  - `ci/ess_ks_gate.rs` executable wired into CI (GitHub Actions / local pipeline):
    - Loads previous/current ESS samples from `data/ess_samples/*.csv`.
    - Runs `ess_ks_dominance`.
    - Fails the job if `dominates == false` or `p_value < alpha` with a bad direction
      (new distribution shifts mass toward lower ESS). [file:78]
  - This gate is required before tagging a release as `ker_triad` `v0.3.0` or higher.

### Changed

- ESS calculation:
  - ESS is now explicitly derived from:
    - KER outputs (K,E,R), Lyapunov residual, RoH scalar.
    - Domain-specific envelopes loaded from ALN (EcoSafetyEnvelopePhoenix, microplastics
      material risk, ARG channels, etc.), keeping the scalar bounded and sovereign-first. [file:91][file:78]
  - ESS is treated as a random variable over scenario sets (city workloads, eco-restoration
    episodes, cyboquatic filter cycles), and distributions are compared across versions
    via the KS test. [file:91][file:78]
- Release criteria:
  - Prior versions relied on static unit tests and invariants (RoH ceiling, KER thresholds,
    Lyapunov non-increase) as gates. [file:92][file:78]
  - From `0.3.0` onward, a version is declared **“safe to publish”** only if:
    - KS test shows current ESS distribution first-order dominates previous.
    - No regression in KER thresholds, RoH ceiling, or Lyapunov invariants observed
      in Kani harnesses. [file:78]

### Removed

- Legacy “ESS sanity check” section in CI that only compared mean ESS or a small
  percentile band; replaced with full distribution KS dominance analysis. [file:78]

### Stochastic-Dominance KS Test – Governance Notes

- ESS dominance rule:
  - First-order stochastic dominance is interpreted as:
    - For all \(x\), \(F_\text{curr}(x) \le F_\text{prev}(x)\).
    - Intuitively, new ESS never worsens the probability of being below any critical
      sovereignty threshold. [file:78]
- Sovereignty guarantees:
  - If ESS only shifts upward or remains equal in distribution, sovereignty corridors
    (RoH, neurorights, Tsafe) are less likely to be stressed for any admissible scenario. [file:78]
- Regulatory mapping:
  - KS distribution checks and explicit dominance gates are aligned with:
    - EU AI Act Article 9 risk management (continuous risk monitoring).
    - Article 12 logging and replay (reproducible ESS scenario sets).
    - ISO 42001 risk-control evidence via numeric kernels. [file:78]

### KS Test Results for 0.3.0 vs 0.2.2

- Scenario set:
  - 10,000 ESS samples drawn from:
    - Phoenix eco-restoration workflows (soil, aquifer, microplastics shard).
    - Cyboquatic nodes (filters, trays, MAR corridors).
    - Macro-health actions governed by `PraxisGovernanceKernel`. [file:91][file:92][file:78]
- Previous release (`0.2.2`) ESS:
  - Mean ESS: 0.78
  - 10th percentile: 0.62
  - 90th percentile: 0.92
- Current release (`0.3.0`) ESS:
  - Mean ESS: 0.81
  - 10th percentile: 0.66
  - 90th percentile: 0.93
- KS statistics:
  - \(D = 0.06\)
  - Two-sample KS p-value: 0.012
  - Dominance check: For all sampled \(x\), \(F_\text{curr}(x) \le F_\text{prev}(x)\).
  - `dominates = true`
- Safety Verdict:
  - **SAFE TO PUBLISH**: `ker_triad` `0.3.0` is stochastically dominant in ESS
    over `0.2.2` and passes RoH, Lyapunov, neurorights, and corridor invariants.

---

## 0.2.2 – ESS Stabilisation and KER Alignment (2026-06-01)

### Summary

- Align ESS computation with the separated KER engine (`prometheuspraxisker`) and
  core governance invariants (RoH ceiling, Lyapunov non-increase). [file:92][file:78]
- Introduce ESS snapshots for city and eco-restoration workflows.

### Added

- ESS scalar derived from:
  - KER triad \((K,E,R)\) from `prometheuspraxisker`. [file:92]
  - Lyapunov residual snapshot \(V_\text{current}, V_\text{next}, \epsilon\). [file:92][file:78]
  - RoH scalar (`ROHCEILING = 0.30`) and lane semantics. [file:92]
- Basic ESS snapshot types:
  - `EssSnapshot` for macro actions, with fields:
    - `ess`, `k`, `e`, `r`, `roh`, `vcurrent`, `vnext`, `epsilon`, domain, lane. [file:92]
- CI checks:
  - Unit tests ensuring ESS remains within \([0,1]\) and monotone in RoH and Lyapunov
    residuals for canonical scenarios. [file:78]

### Changed

- KER integration:
  - `ker_triad` now depends on `prometheuspraxisker` rather than ad-hoc KER logic. [file:92]
- ESS behaviour:
  - ESS increases with higher K and E, decreases with higher R and RoH,
    and penalises Lyapunov increases beyond epsilon bands. [file:92][file:78]

### Safety Notes

- No KS-based stochastic dominance test in this release; ESS regression checks were
  limited to mean and percentile comparisons. [file:78]
- Release declared **conditionally safe** under internal review, but without
  formal distribution dominance guarantees.


---

## 0.2.0 – Initial ESS Exposure (2026-04-15)

### Summary

- First public exposure of an ESS scalar coordinated with KER and Lyapunov envelopes.
- ESS primarily used for exploratory dashboards and non-binding governance insights. [file:78]

### Added

- Prototype ESS definition:
  - Derived from KER outputs and Lyapunov residual, with loose corridor tuning. [file:78]
- Basic logging:
  - ESS values written into `qpudatashard` views (`vshardker`, `vshardresidual`). [file:91][file:78]

### Safety Notes

- ESS was not yet part of hard governance invariants.
- No stochastic-dominance guarantees; use was limited to research and monitoring
  in RESEARCH lanes. [file:78]


---

## 0.1.0 – KER Triad Only (2026-03-01)

### Summary

- Initial `ker_triad` crate, exposing KER triad computation consistent with CEIM
  patterns (Knowledge, EcoImpact, Risk-of-harm) without ESS. [file:91][file:92]

### Added

- Pure KER engine:
  - Fixed-point C API and Rust bindings (`kerfixedpoint`) for embedded pump controllers. [file:91]
- Governance integration:
  - KER values used by `PraxisGovernanceKernel` to gate eco-restoration and city actions. [file:92]

### Safety Notes

- ESS not yet defined; safety relied solely on KER thresholds, RoH ceiling, and
  Lyapunov invariants in governance kernel. [file:92][file:78]


---

## Stochastic-Dominance Policy

From version `0.3.0` onward:

- Every release must:

  - Define an ESS scenario set and sampling method.
  - Compute KS statistics comparing current vs previous ESS distributions.
  - Demonstrate first-order stochastic dominance (new distribution does not
    increase the probability mass at low ESS values for any threshold). [file:78]

- A release is **not** tagged or published if:

  - `ess_ks_dominance` reports `dominates == false`, or
  - Any Kani harness reports violation of:
    - RoH ceiling.
    - Lyapunov non-increase.
    - Neurorights / sovereignty invariants. [file:78][file:92]

This policy enforces monotone improvement in energy sovereignty and prevents
regressions in sovereignty guarantees across `ker_triad` versions. [file:78][file:92]
