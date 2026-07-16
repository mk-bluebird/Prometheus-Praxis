# steward_identity

`steward_identity` is a non‑actuating Rust crate that centralizes steward identity primitives for the Prometheus‑Praxis / EcoNet / Cyboquatics ecosystem.[file:15][file:91]

It provides a single, canonical way to bind:

- A Bostrom DID (e.g. `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`)  
- A human‑readable alias (e.g. `wallet_fetch18sd2uj`)  
- A stable UUID (e.g. `87cb8e02-c918-4b2a-aa40-36a8efa37e52`)  

into one `StewardIdentity` object that can be reused across qpudatashards, governance spine tables, and ERSI / response shards.[file:15][file:91]

No actuation logic is exposed: this crate only validates and serializes identity metadata, keeping it aligned with your existing KER and Lyapunov grammar.[file:91][file:92]

---

## Features

- `StewardIdentity` struct:
  - `signinghex`: canonical Bostrom DID string.
  - `steward_id`: human‑facing label (e.g. wallet / fetch alias).
  - `steward_uuid`: stable UUID for joins in SQLite governance spines.
  - `role`: logical role (e.g. `STEWARD`, `OPERATOR`, `AUDITOR`).
  - `lane`: governance lane (`RESEARCH`, `EXP`, `PROD`, etc.).[file:91]

- Validation helpers:
  - `StewardIdentity::new(...)` rejects empty fields so malformed identities fail fast.
  - `StewardIdentity::assert_bostrom_prefix(...)` checks that `signinghex` starts with a required Bostrom prefix (e.g. `bostrom18sd2uj`), mirroring your DID grammar.[file:91]

- `EnergyMassWindow` shard struct:
  - A Rust mirror of your hydrological / PFAS qpudatashards, extended with an embedded `StewardIdentity`.[file:91]
  - Designed for ALN ↔ Rust round‑trips via `serde`, so AI nodes, MAR vaults, and AI data centers can share the same identity pattern.[file:91][file:92]

- JSON / ALN compatibility:
  - All types derive `Serialize` / `Deserialize` and can be embedded in qpudatashard schemas and Phoenix Hex‑anchored artifacts.[file:15][file:91]

---

## Installation

Add the crate to your workspace:

```toml
# Cargo.toml (workspace root)

[workspace]
members = [
  "crates/steward_identity",
  # ... other crates
]
```

The crate itself declares:

```toml
[package]
name = "steward_identity"
version = "0.1.0"
edition = "2024"
rust-version = "1.85"
license = "MIT OR Apache-2.0"
repository = "https://github.com/mk-bluebird/Prometheus-Praxis"

[dependencies]
serde = { version = "1.0.203", features = ["derive"] }
serde_json = { version = "1.0.120" }
thiserror = "1.0.64"
time = { version = "0.3.36", features = ["formatting", "parsing", "macros"] }
```

These dependencies match your existing ecosystem constraints and stay non‑actuating.[file:15]

---

## Usage

### 1. Constructing a steward identity

```rust
use steward_identity::StewardIdentity;

const BOSTROM_DID: &str = "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";
const STEWARD_ID: &str = "wallet_fetch18sd2uj";
const STEWARD_UUID: &str = "87cb8e02-c918-4b2a-aa40-36a8efa37e52";

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let steward = StewardIdentity::new(
        BOSTROM_DID,
        STEWARD_ID,
        STEWARD_UUID,
        "STEWARD",
        "RESEARCH",
    )?;

    steward.assert_bostrom_prefix("bostrom18sd2uj")?;

    println!("Steward: {}", steward.steward_id);
    Ok(())
}
```

- This pattern ensures that every shard and governance row uses the same DID + alias + UUID triple, which can be joined safely in your SQLite spines.[file:15][file:91]

### 2. Embedding in an EnergyMassWindow shard

```rust
use steward_identity::{EnergyMassWindow, StewardIdentity};

fn example_shard() -> EnergyMassWindow {
    let steward = StewardIdentity::new(
        "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7",
        "wallet_fetch18sd2uj",
        "87cb8e02-c918-4b2a-aa40-36a8efa37e52",
        "STEWARD",
        "RESEARCH",
    )
    .expect("valid steward identity");

    EnergyMassWindow::new(
        "Node-Gila-001",
        "Phoenix-AZ",
        "water",
        "PFBS",
        "2026-07-16T00:00:00Z",
        "2026-07-16T06:00:00Z",
        3.9e-9,
        3.9e-10,
        0.5,
        1200.0,
        2.592e7,
        1.0,
        2.592e7,
        0.93,
        0.91,
        0.13,
        0.45,
        true,
        true,
        "0xa1b2c3d4e5f67890",
        steward,
    )
}
```

- In your qpudatashards, this struct can be flattened into RFC4180 columns (`signinghex`, `steward_id`, `steward_uuid`, `role`, `lane`) while Rust keeps them grouped as a `StewardIdentity`.[file:91]

---

## Integration surfaces

You can reuse `steward_identity` anywhere a steward identity is needed:

- **qpudatashards**:
  - Water quality and MAR nodes (PFBS, E. coli, salinity).
  - AI data center nodes in `AI-ENERGY`, `AI-CARBON`, and `AI-ECO-RATIO` planes.[file:91]

- **Governance spine**:
  - `steward_registry` table in SQLite binds `steward_uuid`, `signinghex`, and `steward_id` to lanes and KER tolerances.[file:91][file:92]
  - ALN governance particles can embed `StewardIdentity` fields directly.

- **ERSI / response shards**:
  - Extend your ERSI logger rows with `steward_uuid` and `steward_id`, then reuse this crate’s type for serialization and validation.[file:15]

This keeps identity treatment consistent across research and production lanes, and makes it structurally hard to misattribute shards or weaken steward bindings.[file:91][file:92]

---

## Design constraints

- Non‑actuating:
  - No actuator control, network calls, or hardware bindings.
  - Safe to use in CI, offline governance tools, and Phoenix Hex registry utilities.[file:15]

- Grammar‑aligned:
  - Matches your existing ALN / qpudatashard grammar for `signinghex`, `evidencehex`, and steward roles.
  - Ready to be referenced from Phoenix Hex Registry entries as the steward of a given `evidence_hex`.[file:91]

- Future‑ready:
  - Can be extended with additional governance fields (e.g. `karmatier`, `neurorights_status`) without breaking existing shards.[file:92]

---

## License

Dual‑licensed under:

- MIT  
- Apache‑2.0  

consistent with the Prometheus‑Praxis mono‑repo policy.[file:15]
