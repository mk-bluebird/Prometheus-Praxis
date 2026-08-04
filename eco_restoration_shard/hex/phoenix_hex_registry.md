---
title: "Phoenix Hex-Anchor Registry"
description: "Registry of H3 hex cells for Phoenix, AZ heat-island recovery and cyboquatic eco-restoration nodes."
version: "1.0.0"
city: "Phoenix, AZ"
coordinate_system: "WGS84"
h3_resolution: 9
governance_did: "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7"
did_governance_tag_scheme: "KER-tag-v1"
generated_at_utc: "2026-08-04T06:00:00Z"
registry_sql_path: "Eco-Fort/db/phoenix_hex_registry.sql"
manifest_path: "eco_restoration_shard/hex/PHX_HEX_ANCHORS.md"
---

# Phoenix Hex-Anchor Registry

This registry defines Phoenix H3 hex anchors for urban heat-island recovery and cyboquatic eco-restoration workloads. Each entry binds a hex to:

- Center coordinates (latitude, longitude).
- Typical land-surface temperature (LST) anomaly relative to rural baseline.
- Assigned cyboquatic node identifier.
- DID-governance tag derived from `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` via a KER-tag scheme.
- Canonical anchor metadata in the SQLite registry.

## DID Governance Tagging Scheme

For each hex cell, the `did_governance_tag` is derived as:

- Base DID: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`.
- Hex-specific tag: `KER-<short_h3>-<ker_profile>` where:
  - `short_h3` is the first 6 characters of the H3 index.
  - `ker_profile` encodes the governance KER triad category, e.g. `K0.90-E-1.00-R0.20`.

This tag is descriptive only and uses no cryptographic hashing primitives.

## Hex Cells

### Hex Cell phoenix-hex-01

- **H3 Index:** `8a2a1072bffffff`
- **Center Latitude:** 33.450123
- **Center Longitude:** -112.074321
- **Typical LST Anomaly (K):** 4.5
- **Assigned Cyboquatic Node:** `cybo-node-downtown-01`
- **DID Governance Tag:** `KER-8a2a10-K0.90-E-1.00-R0.20`
- **Anchor Logical Name:** `PHX_WORKLOAD_ENERGY_DV_20260709`
- **Evidence Hex:** `0x20260709PHX3345NWorkloadEnergyDeltaVt`
- **Domain / Subdomain:** `CYBOQUATIC / WORKLOAD_ENERGY_DV`
- **Region Code:** `PHX-CAZ-CEIM`
- **Planes:** `ENERGY,HYDRAULICS,DATA`
- **Default Relpath:** `eco_restoration_shard/cyboquatic_progress/20260709`

### Hex Cell phoenix-hex-02

- **H3 Index:** `8a2a1072cffffff`
- **Center Latitude:** 33.456789
- **Center Longitude:** -112.068765
- **Typical LST Anomaly (K):** 3.8
- **Assigned Cyboquatic Node:** `cybo-node-midtown-01`
- **DID Governance Tag:** `KER-8a2a10-K0.90-E-0.80-R0.25`
- **Anchor Logical Name:** `PHX_DRAINAGE_DECAY_20260708`
- **Evidence Hex:** `0x20260708PHX3345NDrainageDecayBODTSSCEC`
- **Domain / Subdomain:** `HYDRO / DRAINAGE_DECAY`
- **Region Code:** `PHX-CAZ-CEIM`
- **Planes:** `HYDRAULICS,ENERGY,DATA`
- **Default Relpath:** `eco_restoration_shard/cyboquatic_progress/20260708`

### Hex Cell phoenix-hex-03

- **H3 Index:** `8a2a1072dffffff`
- **Center Latitude:** 33.462345
- **Center Longitude:** -112.061234
- **Typical LST Anomaly (K):** 5.2
- **Assigned Cyboquatic Node:** `cybo-node-canal-01`
- **DID Governance Tag:** `KER-8a2a10-K0.95-E-1.20-R0.30`
- **Anchor Logical Name:** `PHX_HEX_REGISTRY_CORE_2026`
- **Evidence Hex:** `0xPHXHEXREGISTRYCORE2026`
- **Domain / Subdomain:** `GOV / HEX_REGISTRY`
- **Region Code:** `PHX-CAZ-CEIM`
- **Planes:** `TOPOLOGY,GOV,DATA`
- **Default Relpath:** `Eco-Fort/db`

## Placement and Registry Integration

- The canonical SQLite registry schema resides at `Eco-Fort/db/phoenix_hex_registry.sql`.
- Each hex anchor is registered as a row in `phoenix_hex_anchor`, with:
  - `logical_name`, `evidence_hex`, `domain`, `subdomain`, `region_code`, `planes`, `yyyymmdd`, `signing_did`, `summary`, `file_class`, `default_relpath`, `created_utc`.
- Files associated with a hex anchor are recorded in `phoenix_hex_file`, with:
  - `relpath`, `filename`, `file_type`, `file_hash_hex`, `scope`, `created_utc`.

When adding new eco-restoration files:

- Select or register a `phoenix_hex_anchor` entry.
- Place files under the anchor’s `default_relpath`, with further nesting by language (`sql/`, `aln/`, `cpp/`, `java/`, `kotlin/`, `lua/`, `doc/`).
- Register each file in `phoenix_hex_file` to maintain discoverability.

## Usage Patterns

- Cyboquatic workload models use H3 indices and `default_relpath` to map telemetry and simulations to hex cells.
- FOG-router micro-services consume `phoenix_hex_registry.md` and the SQLite view `v_phx_hex_registry` to resolve anchors and file locations.
- Governance audits use the DID governance tags and KER profiles to check carbon-negative compliance and risk bounds per hex.

All updates to this registry are append-only, preserve existing anchors, and maintain DID binding and KER semantics across Eco-Fort, EcoNet, and Prometheus-Praxis eco-restoration shards.
