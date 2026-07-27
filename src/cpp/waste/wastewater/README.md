# Wastewater CPP Layer (`src/cpp/waste/wastewater`)

Non‑actuating C++ layer for wastewater telemetry modeling, corridor checks, and governance‑bound accountability in the Prometheus‑Praxis mono‑repo.[file:6][file:8]

## Scope and Role

- Models wastewater pumps, screens, and corridors as pure numeric/telemetry structures in C++.[file:8]
- Performs corridor and KER/ROH window evaluation and emits evidence payloads for Rust/ALN governance crates, without owning actuators.[file:8]
- Bridges Phoenix EcoNet / Eco‑Restoration layers so wastewater decisions are logged as accountability rows in SQLite and hex‑anchored registries.[file:8]

## Design Principles

- Non‑actuating by design: no fieldbus, PLC, or direct hardware control; actuation lives in separately governed stacks.[file:6][file:8]
- Governance‑first: every start/stop or lane decision passes through KER, Lyapunov, RoH, and corridor predicates before any external system acts.[file:8]
- Evidence‑centric: all corridor‑checked decisions are serialized and handed to EcoNet/Eco‑Restoration FFI for write‑once, DID‑bound logging.[file:8]

## Key Responsibilities

- Ingest pump and screen telemetry windows (turbidity, DO, flow, head, energy) and normalize them into bounded scalars.[file:8]
- Classify telemetry into bands and compute KER/ROH slices consistent with Prometheus‑Praxis governance crates.[file:8]
- Serialize accountability records to JSON or C‑POD structs and call the deployment accountability FFI, which persists rows into SQLite cores.[file:8]

## Directory Overview

- `deployment_accountability_adapter.cpp` – JSON/C‑struct adapter that turns pump/screen KER/ROH windows plus lane decisions into governance payloads and calls the EcoNet / Eco‑Restoration FFI; never issues actuator commands.[file:8]
- `deployment_accountability_adapter.hpp` – C++ header declaring telemetry structs, KER/ROH corridor types, lane decisions, and adapter APIs for upstream Rust.[file:8]
- Additional wastewater engine files (e.g., telemetry normalization, corridor math) are expected to sit alongside the adapter and follow the same non‑actuating, evidence‑only grammar.[file:8]

## Telemetry and Corridor Types

- Pump telemetry structures carry identifiers (asset, corridor, window) and physical metrics (turbidity, dissolved oxygen, flow, head, energy).[file:8]
- Screen telemetry structures capture influent turbidity, delta‑head across screens, energy, and corridor tags.[file:8]
- KER/ROH window types encode knowledge, eco‑impact, risk‑of‑harm, and RoH ceilings for each decision window, aligned with Rust kernels.[file:8]

## Accountability Flow

- Upstream Rust governance code computes KER/ROH windows and lane decisions per pump/screen window and passes them into this C++ adapter.[file:8]
- The adapter:
  - Validates and normalizes telemetry.
  - Classifies bands (e.g., turbidity/DO) for governance reporting.
  - Packages telemetry, bands, KER, ROH, corridor flags, and lane decision into a `PumpAccountabilityRecord`.[file:8]
- `PumpAccountabilityRecord` is serialized to JSON and sent to the EcoNet/Eco‑Restoration FFI (`deployment_accountability_core_ingest_json_payload`), which writes a durable evidence row; actuation decisions remain elsewhere.[file:8]

## Governance and Hex Anchoring

- All wastewater evidence rows are expected to be hex‑stamped and bound to Phoenix Eco‑Fort registries and ALN particles via Rust/SQL layers; this C++ layer only prepares the payloads.[file:6][file:8]
- DID provenance and corridor semantics are enforced upstream in Rust/ALN; this layer preserves identifiers and window metadata needed for those bindings.[file:8]

## Non‑Actuation Guarantees

- No functions in this directory open devices, send hardware commands, or manipulate pumps/screens directly.[file:6][file:8]
- Adapter APIs are limited to:
  - Constructing records from telemetry and KER/ROH inputs.
  - Serializing and forwarding records to the accountability FFI.[file:8]
- Any future additions must keep this invariant: wastewater C++ code is numeric and governance‑bound only, never a control surface.[file:6][file:8]

## Integration Notes

- Rust crates in `Prometheus‑Praxis` call into this adapter via `extern "C"` FFI, passing KER/ROH windows and lane decisions and receiving no actuation feedback.[file:8]
- SQLite schemas and ALN particles for wastewater accountability live in the EcoNet / Eco‑Restoration stack; they consume the JSON/C‑struct payloads produced here.[file:6][file:8]
- New C++ modules under `src/cpp/waste/wastewater` should:
  - Declare pure POD structs and functions.
  - Be wired to Rust via explicit FFI shims.
  - Register their evidence paths and particles via the Phoenix hex registry and ALN, outside this directory.[file:6][file:8]
