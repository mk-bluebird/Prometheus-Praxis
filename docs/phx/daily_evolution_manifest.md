<!-- filename: docs/phx/daily_evolution_manifest.md -->
<!-- destination: Eco-Fort/docs/phx/daily_evolution_manifest.md -->
<!-- description: Documentation for vphx_daily_evolution_manifest. -->

# Phoenix Daily Evolution Manifest

## Purpose

The `vphx_daily_evolution_manifest` view is a non-actuating, read-only composition layer for the Phoenix-AZ-US region. It produces one row per steward per day, combining:

- KER and Lyapunov residual metrics.
- ResponsibilityAxis and Risk-of-Harm (RoH) overlays.
- Lifeforce and biokarma healthcare deltas.
- EcoUnit issuance from StewardEcoWealthStatement and stake batches.
- Representation floors and daily ecowealth share.

This surface is designed for CI replay, governance audits, and AI-chat queries. It does not perform any actuation or modify state.

## Input Surfaces

The manifest joins the following existing tables and views:

- `shardinstance`, `vshardker`, `vshardkerviolation`, `kerresidualsnapshot`.
- `responsibilitymetric`, `portfoliodiversitymetric`.
- `rohshard`, `lifeforcetraitshard`.
- `StewardEcoWealthStatement`, `vecowealthview`.
- `econetstaketerminalbatch2026q2phx`, `veconetstakekarmadailyphx`.
- `regionrepresentation`.

All joins are filtered to `regioncode = 'Phoenix-AZ-US'`.

## Helper Views

Several helper views are defined in `dbphx_dailyevolutionmanifest.sql`:

- `vphx_steward_day_base`: per-steward, per-day index of Phoenix shard windows.
- `vphx_steward_day_ker`: daily aggregates of K, E, R, vtwithtopology, and reff.
- `vphx_steward_day_responsibility`: daily ResponsibilityAxis and RoH, plus a `roh_ok_day` flag.
- `vphx_steward_day_lifeforce`: daily lifeforce and biokarma deltas.
- `vphx_steward_day_ecounit`: daily EcoUnit aggregates and governance/dataquality factors.
- `vphx_steward_day_stake`: daily stake-based EcoUnit aggregates.
- `vphx_region_representation`: Phoenix region representation settings.
- `vphx_steward_day_representation`: daily ecowealth share vs region total.

These helpers keep the main manifest view compact and auditable.

## Manifest Columns

The `vphx_daily_evolution_manifest` view exposes:

- Steward and region identity:
  - `stewarddid`, `regioncode`, `day`.
- KER and residual:
  - `k_mean_day`, `k_min_day`, `k_max_day`,
  - `e_mean_day`, `e_min_day`, `e_max_day`,
  - `r_mean_day`, `r_max_day`,
  - `vt_max_day`, `reff_mean_day`.
- Responsibility and RoH:
  - `rresponsibility_day`, `rpharma_day`, `rtoxicity_day`, `roverride_day`,
  - `roh_max_day`, `roh_ok_day`.
- Lifeforce and biokarma:
  - `lifeforce_delta_day`, `biokarma_delta_day`.
- EcoUnit and governance:
  - `ecounit_final_day`,
  - `kmean_window_day`, `emean_window_day`, `rmean_window_day`,
  - `vtmaxwindow_day`,
  - `governancepenalty_day`, `dataqualityfactor_day`.
- Stake-based EcoUnits:
  - `ecounits_credited_day`, `ecounits_liquid_day`, `ecounits_restricted_day`.
- Representation:
  - `ecounit_region_day`, `ecowealth_share_day`,
  - `representationfloor`, `representation_ok_day`.

All numeric values are derived from existing kernels and windows. The view does not introduce new math.

## Filters and Invariants

The manifest enforces:

- `regioncode = 'Phoenix-AZ-US'`.
- `roh_ok_day = 1`, meaning `roh_max_day <= 0.30` or no RoH entries.
- No Lyapunov violations on contributing shards:
  - Excludes days where `vshardkerviolation.violateslyapunov = 1`.
- No non-deployable KER states:
  - Excludes days where any shardinstance row for that steward-day has `kerdeployable = 0`.

Representation floors are checked via:

- `representationfloor` from `regionrepresentation`.
- `ecowealth_share_day` from `vphx_steward_day_representation`.
- `representation_ok_day` set to 1 when `ecowealth_share_day >= representationfloor`.

These constraints keep the manifest aligned with existing safety and governance contracts.

## Definition Registry Binding

The manifest is registered in `definitionregistry` via:

- Logical name: `ecowealth.phx.daily_evolution_manifest.view.2026v1`.
- Scope: `PHX_EVOLUTION`.
- Linked table: `vphx_daily_evolution_manifest`.
- Linked ALN: `StewardEcoWealthStatement2026v1.aln`.
- Documentation: this file.

Once the SQL file is frozen, its hash must be computed and stored in `definitionregistry.hash`. This ensures that agents and CI can verify they are querying the canonical Phoenix daily evolution manifest.

## Usage Patterns

Examples:

- CI replay to confirm non-regression of KER and RoH across software updates, by diffing manifest rows between runs.
- Governance dashboards to inspect daily EcoUnit flows, responsibility trends, and representation floors.
- AI-chat queries to answer steward-centric questions such as:
  - “Show my last 30 days of EcoUnit issuance and RoH-safe healthcare activity.”
  - “Which Phoenix stewards are below representation floors despite passing KER thresholds?”

The view is intentionally non-actuating and uses only SELECT statements, making it safe to expose to read-only clients and agents.
