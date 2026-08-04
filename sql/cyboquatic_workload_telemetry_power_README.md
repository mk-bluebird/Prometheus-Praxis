# ALN → SQL Constraint Wiring and PFAS Power Analysis

## ALN v2 Invariants to SQL CHECK/Triggers

- The Kotlin tool `AlnToSqlConstraintGenerator.kt` reads `aln_v2/cyboquatic_workload_ker.aln`, extracts `invariant` blocks, and translates `assert` conditions into SQL trigger bodies for tables such as `cyboquatic_workload_telemetry` and `governance_particle`.
- Generated triggers use `RAISE(ABORT, ...)` to reject any inserts or updates that violate KER invariants (e.g., `ker_e <= 0`, `ker_k ∈ [0,1]`, `ker_r ∈ [0,1]`), ensuring the database enforces ALN-specified eco-restoration constraints.

## Information-Theoretic Lower Bound on Sampling Interval

- Under Poisson-noise-dominated PFAS measurements, baseline counts per sample follow Poisson(λ0).
- Detecting a 10% reduction (λ1 = 0.9 λ0) with high power hinges on the Kullback-Leibler divergence:

  - `D_KL = λ0 * log(λ0 / λ1) + λ1 - λ0`.

- The PFAS power-analysis utility `PFASPowerAnalysis.java`:

  - Computes `D_KL` between Poisson(λ0) and Poisson(λ1).
  - Uses a conservative bound `N ≥ c / D_KL` (with `c ≈ 10` for ~99% power) to estimate the minimal number of samples.
  - Derives the recommended sampling interval `Δt = horizonSeconds / N`.

- This utility links directly to the `cyboquatic_workload_telemetry` schema by informing how frequently telemetry rows for PFAS counts should be recorded to reliably detect modest concentration reductions, while still respecting energy and storage constraints in cyboquatic eco-restoration workloads.

Technical justification: The ALN→SQL generator provides a concrete bridge from formal invariants to DB-level enforcement, eliminating telemetry sequences that violate KER eco-restoration guarantees. The information-theoretic power analysis approximates a lower bound on sampling cadence needed to detect a 10% PFAS reduction with high power under Poisson noise, giving cyboquatic system designers a principled way to set `timestamp_s` spacing in `cyboquatic_workload_telemetry` for robust environmental monitoring.
