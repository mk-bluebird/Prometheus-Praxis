# ALN v2 → SQL Constraint Toolchain

This Kotlin script implements a simple code-generation pipeline:

1. Reads `aln_v2/cyboquatic_workload_ker.aln`.
2. Parses `invariant` blocks over entities such as `WorkloadTelemetry` and `GovernanceParticle`.
3. Emits SQL trigger definitions that enforce the asserted conditions (e.g., `ker_e <= 0.0`, `ker_k ∈ [0,1]`, `ker_r ∈ [0,1]`) on corresponding SQLite tables.

By deploying the generated SQL in the database, any insert or update that violates ALN invariants is rejected at the DB level via `RAISE(ABORT)`, aligning formal ALN v2 specifications with concrete storage guarantees for cyboquatic eco-restoration telemetry.
