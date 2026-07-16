# ai_node_shard

`ai_node_shard` is a non‑actuating Rust crate that models AI data centers as first‑class Cyboquatic nodes within the EcoNet / Prometheus‑Praxis governance grammar.[file:91][file:92]

It provides:

- A typed `AINodeShard` struct capturing the ten‑axis measurement stack (energy, carbon, power, bandwidth, utilization, heat reuse, eco‑task ratio, water, materials, and derived KER/strength metrics) for AI facilities treated as Cyboquatic nodes.[file:91]
- A `PhoenixHexAnchor` schema and SQLite bindings for the **Phoenix Hex Registry**, allowing every AI shard and governance artifact to be hex‑stamped, chained, and verified in CI.[file:91][file:15]
- Integration points for the shared `StewardIdentity` crate, so each shard is bound to a Bostrom DID, human alias, and UUID steward.[file:91][file:15]

No actuation logic is included; this crate only processes telemetry and registry metadata and is safe for CI, analysis, and governance tooling.[file:91]

---

## Features

### AINodeShard

`AINodeShard` represents a single AI data center node over a measurement window (e.g. 1 hour or 1 day), aligned with your Cyboquatic / KER grammar.[file:91][file:92]

Key fields:

- Identity and governance:
  - `nodeid`: logical node name (e.g. `AI-PHX-001`).
  - `region`: geospatial/administrative region (e.g. `Phoenix-AZ`).
  - `lane`: governance lane (`RESEARCH`, `EXP`, `PROD`, etc.).
  - `steward`: embedded `StewardIdentity` (Bostrom DID + alias + UUID).[file:91][file:15]

- Measurement window:
  - `twindow_start`, `twindow_end`: ISO‑8601 timestamps delimiting the telemetry window.[file:91]

- Energy and efficiency:
  - `core_energy_kwh_per_workload`: kWh per workload (e.g. per 10^6 tokens or per inference).
  - `joules_per_inference`: joules per useful inference.
  - `pue`: Power Usage Effectiveness.
  - `cue_kg_co2_per_kwh`: Carbon Usage Effectiveness (kg CO₂eq/kWh).
  - `eco_per_joule`: normalized eco‑benefit per joule.[file:91]

- Bandwidth and utilization:
  - `throughput_tokens_per_s`: tokens per second.
  - `throughput_inferences_per_s`: inferences per second.
  - `utilization_pct`: IT utilization percentage (0–100).[file:91]

- Heat reuse and eco ratio:
  - `ere`: Energy Reuse Effectiveness.
  - `eco_task_ratio_pct`: percentage of energy spent on KER‑verified eco‑restorative workloads.[file:91]

- Water and materials:
  - `wue_l_per_kwh`: Water Usage Effectiveness (L/kWh).
  - `embodied_kg_co2eq`: amortized embodied carbon per window.[file:91]

- Derived KER metrics:
  - `k`, `e`, `r`: Knowledge, Eco‑impact, Risk‑of‑harm scalars in \([0,1]\) derived from the Lyapunov residual and corridors.[file:91][file:92]
  - `vt`: Lyapunov residual \(V_t\) over AI planes.

- Strength index:
  - `strength_index_s`: composite eco‑strength index \(S\) for ranking and routing, not gating.[file:91][file:92]

- Evidence:
  - `evidencehex`: Phoenix hex anchor for this shard.
  - `signinghex`: steward’s Bostrom DID (usually matches `steward.signinghex`).[file:91][file:15]

Validation:

- `AINodeShard::validate()` enforces basic sanity checks (non‑negative energy metrics, K/E/R in [0,1], utilization in [0,100]) before Lyapunov normalization or corridor checks in `kercore`.[file:91]

---

### PhoenixHexAnchor and registry bindings

`PhoenixHexAnchor` mirrors the **Phoenix Hex Registry** entry for any evidence_hex.[file:91][file:15]

Fields:

- `hex_id`: unique hex identifier (evidence_hex).
- `kind`: logical type (`TELEMETRY`, `SHARD`, `GOV_CORRIDOR`, etc.).
- `logical_name`: human‑meaningful anchor (e.g. `PHX_AI_ENERGY_DV_20260716`).
- `path`: repo‑relative path (e.g. `qpudatashards/particles/AINodePhoenix2026v1.csv`).
- `steward_uuid`: steward UUID from `StewardIdentity`.
- `created_utc`: ISO‑8601 UTC timestamp.
- `prior_anchor_id`: optional previous hex in the same chain, enforcing forward‑only progress.[file:91]

Registry helpers:

- `init_phoenix_hex_schema(conn: &Connection)`: initializes the `phoenix_hex_anchor` table and relevant indices in SQLite.[file:91]
- `insert_phoenix_hex_anchor(conn: &Connection, anchor: &PhoenixHexAnchor)`: appends a new anchor (no overwrites).
- `latest_anchor_for_logical(conn: &Connection, logical_name: &str)`: fetches the newest anchor for a logical name by `created_utc`.[file:91]

These functions provide the non‑actuating backbone needed to ensure that every AI shard and corridor file is registered and chained, as described in your Phoenix Hex Registry design.[file:91][file:15]

---

## Installation

Add the crate to your workspace:

```toml
# Cargo.toml (workspace root)

[workspace]
members = [
  "crates/steward_identity",
  "crates/ai_node_shard",
  # ... other crates
]
```

`ai_node_shard`’s own `Cargo.toml` declares:

```toml
[package]
name = "ai_node_shard"
version = "0.1.0"
edition = "2024"
rust-version = "1.85"
license = "MIT OR Apache-2.0"
repository = "https://github.com/mk-bluebird/Prometheus-Praxis"

[dependencies]
serde = { version = "1.0.203", features = ["derive"] }
serde_json = { version = "1.0.120" }
serde_with = "3.11.0"
thiserror = "1.0.64"
time = { version = "0.3.36", features = ["formatting", "parsing", "macros"] }
rusqlite = { version = "0.31.0", features = ["bundled"] }
steward_identity = { path = "../steward_identity" }
```

All dependencies are non‑actuating and compatible with your existing governance crates.[file:15]

---

## Usage

### 1. Constructing an AI node shard

```rust
use ai_node_shard::AINodeShard;
use steward_identity::StewardIdentity;

const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
const STEWARD_ID: &str = "wallet_fetch18sd2uj";
const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

fn make_shard() -> Result<AINodeShard, Box<dyn std::error::Error>> {
    let steward = StewardIdentity::new(
        BOSTROM_DID,
        STEWARD_ID,
        STEWARD_UUID,
        "STEWARD",
        "RESEARCH",
    )?;
    steward.assert_bostrom_prefix("bostrom18sd2uj")?;

    let shard = AINodeShard {
        nodeid: "AI-PHX-001".into(),
        region: "Phoenix-AZ".into(),
        lane: "RESEARCH".into(),
        steward,
        twindow_start: "2026-07-16T00:00:00Z".into(),
        twindow_end: "2026-07-16T01:00:00Z".into(),
        core_energy_kwh_per_workload: 0.25,
        joules_per_inference: 3.5,
        pue: 1.15,
        cue_kg_co2_per_kwh: 0.20,
        eco_per_joule: 0.75,
        throughput_tokens_per_s: 1200.0,
        throughput_inferences_per_s: 35.0,
        utilization_pct: 76.0,
        ere: 0.10,
        eco_task_ratio_pct: 65.0,
        wue_l_per_kwh: 0.9,
        embodied_kg_co2eq: 15.0,
        k: 0.93,
        e: 0.91,
        r: 0.13,
        vt: 0.42,
        strength_index_s: 0.88,
        evidencehex: "0xa3f5c7e9b1d20468c7e4a9d2b5f81357".into(),
        signinghex: BOSTROM_DID.into(),
    };

    shard.validate()?; // basic sanity checks
    Ok(shard)
}
```

- This pattern lets you ingest telemetry from AI data centers, bind it to a steward, and emit a KER‑aligned shard ready for Lyapunov normalization in `kercore`.[file:91][file:92]

### 2. Registering a Phoenix Hex anchor

```rust
use ai_node_shard::{
    init_phoenix_hex_schema, insert_phoenix_hex_anchor, PhoenixHexAnchor,
};
use rusqlite::Connection;

fn register_anchor() -> Result<(), Box<dyn std::error::Error>> {
    let conn = Connection::open("phoenix_hex_registry.sqlite3")?;
    init_phoenix_hex_schema(&conn)?;

    let anchor = PhoenixHexAnchor::new(
        "0xa3f5c7e9b1d20468c7e4a9d2b5f81357",
        "SHARD",
        "PHX_AI_ENERGY_DV_20260716",
        "qpudatashards/particles/AINodePhoenix2026v1.csv",
        "87cb8e02-c918-4b2a-aa40-36a8efa37e52",
        None,
    )?;

    insert_phoenix_hex_anchor(&conn, &anchor)?;
    Ok(())
}
```

- CI can be wired to reject any shard or corridor file whose `evidencehex` is not present in this registry, enforcing your forward‑only governance rule.[file:91][file:15]

---

## Integration surfaces

`ai_node_shard` is intended to plug into several existing components:

- **kercore / Lyapunov engine**:
  - Use `AINodeShard` as the raw telemetry container.
  - Compute normalized risk coordinates \(r_j\), residual \(V_t\), and KER using your shared Lyapunov residual and corridor tables.[file:91][file:92]

- **ERSI / response shards**:
  - Log AI node KER and strength index values alongside traditional Cyboquatic nodes, using the same steward identities and Phoenix hex anchors.[file:15]

- **Phoenix Hex Registry tools**:
  - Use `PhoenixHexAnchor` and its functions inside CLI tools or CI steps that manage evidence registration and hex‑chain validation.[file:91][file:15]

This keeps AI data centers fully inside the EcoNet / Cyboquatic grammar, as “AI‑as‑environmental‑machine,” without creating a parallel governance system.[file:91][file:92]

---

## Design constraints

- Non‑actuating:
  - No hardware control, no network clients, no actuator bindings.
  - Safe for use in CI, offline analysis, and governance pipelines.[file:91][file:15]

- Grammar aligned:
  - Fields and semantics match your existing qpudatashards, Phoenix Hex Registry, and KER grammar (Lyapunov residual, K,E,R triad, lanes).[file:91][file:92]

- Future‑extensible:
  - Additional AI‑specific planes (e.g. `AI-HEAT-LATENT`, `AI-BIODIVERSITY`) can be added as fields and normalized into new risk coordinates without breaking existing shards, as long as corridor tables and KER weights are updated consistently.[file:91]

---

## License

Dual‑licensed under:

- MIT  
- Apache‑2.0  

consistent with the Prometheus‑Praxis mono‑repo policy.[file:15]
