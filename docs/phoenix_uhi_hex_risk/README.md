<!-- filename: docs/phoenix_uhi_hex_risk/README.md -->
<!-- destination: https://github.com/mk-bluebird/Prometheus-Praxis/docs/phoenix_uhi_hex_risk/README.md -->
<!-- license: MIT OR Apache-2.0 -->

# Phoenix UHI Hex Risk Constitution Slice

This directory documents the Phoenix Urban Heat Island (UHI) hex risk slice of the Prometheus‑Praxis KER‑Lyapunov constitution.[file:14][web:35]

## Components

- `crates/ecosafety-core-v2/src/uhi_triads.rs`
  - Non‑actuating Rust module that maps UHI observables (surface temperature, NDVI, canopy fraction, AQI) into normalized risk coordinates:
    - `r_t` – temperature risk.
    - `r_c` – vegetation/canopy risk.
    - `r_a` – air‑quality risk.
    - `r_thermal` – composite thermal risk scalar in \([0,1]\).[web:35][web:44]
  - Provides `build_uhi_triad` and `apply_uhi_to_riskvector`, wiring thermal risk into the constitutional `RiskVector` without touching other planes.[file:14]

- `crates/ecosafety-core-v2/src/bin/phoenix_uhi_hex_risk.rs`
  - Example binary:
    - Reads Phoenix hex UHI telemetry from JSON (`hex_id`, `surface_temp_c`, `ndvi`, `canopy_fraction`, `air_quality_index`).[web:38][web:44]
    - Builds `UhiTriadRisk` for each hex using calibrated bands (30–45 °C, NDVI/ canopy targets, AQI good/hazardous ranges).[web:35][web:39][web:46]
    - Emits one JSONL line per hex (`HexUhiRiskShard`) with triad coordinates and `r_thermal`, plus full `RiskVector` planes, ALN‑ready and non‑actuating.[file:14]

- `qpudatashards/phoenix.uhi.hex.risk.v1.aln`
  - ALN shard spec declaring the schema for `HexUhiRiskShard` and binding it to:
    - `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
    - Lane defaults `RESEARCH`, `EXPPROD`.[file:14]
  - Defines corridors for `r_t`, `r_c`, `r_a`, `r_thermal`, and other planes, plus non‑offsettable constraints for biodiversity, carbon, and neurorights.[file:14]
  - Provides CI rules:
    - All risk coordinates must be in \([0,1]\).
    - Shards violating non‑offsettable maxima must fail CI.[file:14]

- `db/dbrepoindexphoenix_uhi_hex_risk.sql`
  - Registers:
    - The ALN shard file.
    - The `ecosafety-core-v2` crate and `phoenix_uhi_hex_risk` binary.
    - An `agentsafecatalog` entry (`tool_id = 'phoenix_uhi_hex_risk'`) marking this binary as `READONLYSPINE` and AI‑safe.[file:14]
  - Adds an `econetrepoindex` manifest row tying this slice to the ecosystem’s KER‑Lyapunov constitution and non‑actuating governance spine.[file:14]

- `db/dbagentsqlpattern_phoenix_uhi_hex_risk.sql`
  - Registers an `agentsqlpattern`:
    - `pattern_id = 'phoenix_uhi_hex_risk_list'`.
    - SQL text: list UHI hex risk rows from `ecoshard_phoenix_uhi_hex_risk` for `region = 'Phoenix-AZ'`, ordered by `r_thermal` descending.
    - Lane scope `RESEARCH`, risk ceiling note referencing non‑offsettable enforcement at shard emission.[file:14]

## Intended Use

- **Data producers**:
  - Run `phoenix_uhi_hex_risk --json path/to/telemetry.json` to generate UHI risk shards for Phoenix hexes.
  - Load the resulting JSONL into a read‑only `ecoshard_phoenix_uhi_hex_risk` table or view.

- **Constitutional kernel**:
  - Use `uhi_triads::build_uhi_triad` and `apply_uhi_to_riskvector` to populate the thermal coordinate of `RiskVector` before calling Lyapunov invariant checks and FOG router predicates.[file:14]

- **AI‑chat platforms and agents**:
  - Discover the `phoenix_uhi_hex_risk` tool via `vagentsafecatalog` and `veconetrepomanifestagent` views.[file:14]
  - Fetch standard risk distributions using the `phoenix_uhi_hex_risk_list` pattern from `agentsqlpattern` instead of synthesizing SQL.[file:14]
  - Treat all outputs as diagnostic surfaces only; actuation remains outside this slice.

## Safety and Governance

- All UHI risk logic is **non‑actuating** and sits firmly inside the ecosafety spine:
  - No direct control of pumps, HVAC, routing, or hardware.
  - Outputs are advisory inputs to Lyapunov invariants, FOG predicates, and governance workflows.[file:14]

- Non‑offsettable corridors are enforced at shard level and CI:
  - Biodiversity (`r_biodiv`), carbon (`r_energy`), and neurorights‑adjacent (`r_ai`) planes have hard maxima.
  - AI tools cannot lower safety floors or widen corridors; evolution is monotone toward tighter bounds only.[file:14]

- This slice is designed to be:
  - Queryable by AI‑chat platforms.
  - Auditable via SQL and ALN.
  - Compatible with the broader Prometheus‑Praxis urban resilience constitution.

For extension, you can add:

- A hydraulic PDE → `r_hyd` normalizer bound to Phoenix canal telemetry.[web:35][web:38]
- A combined Lyapunov weights calibration shard that couples UHI, hydraulics, and AI workload planes under your minimax corridor strategy.[file:14]
