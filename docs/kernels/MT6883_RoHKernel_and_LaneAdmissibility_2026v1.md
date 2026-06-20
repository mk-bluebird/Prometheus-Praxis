<!-- filename: docs/kernels/MT6883_RoHKernel_and_LaneAdmissibility_2026v1.md
     repo: mk-bluebird/eco_restoration_shard
     destination: Eco-Fort/docs/kernels/MT6883_RoHKernel_and_LaneAdmissibility_2026v1.md -->

# MT6883 RoHKernel and LaneAdmissibilityKernel 2026v1

## Purpose

- Make RoH ceilings for MT6883 healthcare corridors and Cyboquatic machinery explicit, canonical, and non-duplicated.
- Expose KER upgrade monotonicity, Lyapunov residual constraints, RoH ceilings, and Cyboquatic carbonnegativeok/restorationok flags through typed SQLite, ALN, and Rust guard functions.
- Ensure any new MT6883 diagnostic surface or Cyboquatic lane decision plugs into these kernels rather than re-implementing safety predicates.

## RoHKernelMT6883_2026v1

- Backed by `db/dbrohkernel_mt6883_2026v1.sql` and `aln/RoHKernelMT6883_2026v1.aln`.
- `neurocapability` catalog:
  - One row per MT6883 capability, with `rohceiling` and `rohkernelcode`.
  - RoH ceiling is a hard cap in `[0,1]`, typically `0.30` for healthcare corridors.
- `rohkernelmt6883` table:
  - Stores per-node, per-capability RoH values and ceilings, anchored by `evidencehex` and `signingdid`.
  - `rohceilingok = 1` exactly when `rohvalue <= rohceiling`.
- `vrohkernelmt6883` view:
  - Joins RoH readings with capability metadata for agent-friendly queries.

## PhoenixDailyEvolutionManifest2026v1

- Backed by `db/dbphoenix_daily_manifest_rohker_2026v1.sql` and `aln/PhoenixDailyEvolutionManifest2026v1.aln`.
- Extends `stewarddailystatephx` with:
  - `mt6883okday`, `neuroethicokday` as daily MT6883 safety flags.
- `vphoenixdailyevolutionfull`:
  - Full daily macro-state per steward, including K/E/R, Vt, EcoUnit, RoH, responsibility, representation, and MT6883 fields.
- `vphoenixdailyevolutionadmissible`:
  - Admissible days where:
    - `rohok = 1`,
    - `kerdeployableday = 1`,
    - `lyapunovokday = 1`,
    - `mt6883okday = 1`,
    - `neuroethicokday = 1`.

## LaneAdmissibilityKernel2026v1

- Defined in `aln/LaneAdmissibilityKernel2026v1.aln`.
- Binds the lane admissibility predicate to a canonical set of fields:
  - K, E, R, RoH, Vt before/after, RoH ceiling, monotonicity flags, Lyapunov flag, carbonnegativeok, restorationok.
- Intended to mirror the `vlaneadmissibility` view so that all lane decisions can be explained and audited in terms of the same kernel.

## Governance-guard Rust crate

- Implemented in `crates/governance-guard/src/lib.rs` with `Cargo.toml` in the same directory.
- Provides non-actuating guards:
  - `ker_upgrade_ok`:
    - Enforces `K_new >= K_old`, `E_new >= E_old`, `R_new <= R_old`, and `V_new <= V_old + epsilon`.
  - `lane_admissible`:
    - Applies lane-specific thresholds for `K/E/R/RoH`.
    - Enforces Lyapunov safety.
    - Requires `carbonnegativeok = 1` and `restorationok = 1` for Cyboquatic nodes in `EXPPROD` and `PROD` lanes.
  - `roh_ceiling_ok`:
    - Checks that `roh <= roh_ceiling` for MT6883 workloads.

## Integration pattern

- MT6883 diagnostics:
  - Write RoH readings into `rohkernelmt6883` and ensure `mt6883okday` in `stewarddailystatephx` reflects aggregated RoH ceilings.
  - Use `roh_ceiling_ok` and `LaneAdmissibilityKernel2026v1` to gate any MT6883-linked analytics or planning surfaces.
- Cyboquatic machinery:
  - Ensure `CyboquaticEcoPlot` and `CyboquaticRestorationSurface` surfaces provide `carbonnegativeok` and `restorationok`.
  - Use `lane_admissible` to gate `EXPPROD/PROD` lane assignments for Cyboquatic nodes.
- DefinitionRegistry:
  - `dbdefinitionregistry_mt6883_cyboquatic_kernels_2026v1.sql` registers all kernels and manifests under 2026v1 logical names, making them discoverable for CI and AI tools.
