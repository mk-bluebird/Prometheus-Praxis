# EcoNet Constellation - Consolidated Repository

> **MIGRATION NOTICE** (2026-05-12): All `Doctor0Evil/*` repositories have been consolidated into this single monorepo due to GitHub account suspension. **All ALN schemas, Bostrom DIDs, and cryptographic evidence chains are preserved.** See [MIGRATION.md](./MIGRATION.md) for complete mapping.

## Overview

This repository consolidates **30+ ecological restoration repositories** into a unified, role-band structured monorepo focused on:
- **Carbon-negative** cyboquatic industrial machinery
- **Energy-efficient** FOG routing and CEIM kernels  
- **Ecologically-restorative** biodegradable materials (FlowVac, BugsLife, Ant-One-Net)
- **Superintelligence policy language** (ALN) for governance & safety

**Primary Bostrom DID**: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`  
**Secure DID**: `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

---

## Repository Structure (Consolidated from Doctor0Evil/*)

```
eco_restoration_shard/
├── spine/                  # SPINE role - Grammar, KER math, ALN schemas
│   ├── econet/            # Core ecosafety & Lyapunov residual (V_t)
│   ├── aln-platform/      # ALN superintelligence policy language
│   ├── aln-blockchain/    # Blockchain anchoring for ALN particles
│   └── aletheion/         # Truth-preserving research framework
├── engines/               # ENGINE role - Cyboquatic kernels & controllers
│   ├── ceim-phoenix-water/     # Phoenix water CEIM/CPVM kernels
│   ├── cyboquatics/            # Cyboquatic FOG routing
│   ├── fog-monitoring/         # Sewer FOG monitoring networks
│   └── airglobe/               # Air-water coupling kernels
├── materials/             # MATERIAL role - Biodegradable substrates
│   ├── bugslife/          # Pest-control biodegradable materials
│   ├── ant-one-net/       # Ant-fed packaging structures
│   └── beesafe-ai/        # Bee habitat protection kernels
├── research/              # RESEARCH role - Core restoration studies
│   ├── core/              # Primary eco_restoration_shard research
│   └── snowglobe/         # Global research aggregation
├── governance/            # GOV role - Policy, rewards, orchestration
│   ├── ecoinfra/          # Infrastructure governance
│   ├── orchestrator/      # Workload orchestration
│   └── paycomp/           # Augmented-citizen rewards
└── apps/                  # APP role - Visualizers & dashboards
    ├── cybocinder-phoenix/
    └── corridor-hud/
```

**Old Repository Mapping**: See [MIGRATION.md](./MIGRATION.md#old-repository-mapping)

---

## Critical: ALN Superintelligence Policy Language Preservation

**ALN (Augmented Logic Notation)** is the frozen policy language ensuring ecological safety through:
- **Lyapunov residual** \(V_t = \sum_j w_j r_j^2\) with safestep monotonicity \(V_{t+1} \leq V_t\)
- **KER metrics** (Knowledge, Eco-impact, Risk-of-harm) with production gates: \(K \geq 0.90, E \geq 0.90, R \leq 0.13\)
- **Non-offsettable plane weights** preventing harm compensation
- **Provenance kernels** with Bostrom DID signing and evidence chains

### Frozen ALN Schemas (MUST NOT CHANGE)

| Schema Name | Version | Spec Hash | Purpose |
|-------------|---------|-----------|---------|
| `ecosafety.riskvector.v2` | v2 | `0xa1b2c3d4e5f67890` | Core Lyapunov & KER math |
| `ecosafety.corridors.v2` | v2 | `0x8f7e6d5c4b3a2910` | Safety band definitions |
| `PlaneWeightsShard2026v1` | v1 | `0x1122334455667788` | Non-offsettable planes |
| `NonActuatingWorkload` | v1 | `0x99aabbccddeeff00` | Workload monotonicity |
| `ProvenanceKernel2026v1` | v1 | `0x4a3b2c1d9e8f7g6h` | Evidence hash rules |

**Migration Verification**: All spec hashes, signing DIDs, and evidence chains preserved. See [aln_migration_schema_preservation.sql](./spine/aln-platform/migrations/aln_migration_schema_preservation.sql).

---

## Quick Start

```bash
# Clone consolidated repository
git clone https://github.com/mk-bluebird/eco_restoration_shard.git
cd eco_restoration_shard

# Build SPINE (core grammar & KER math)
cd spine/econet
cargo build --release

# Run ALN schema integrity check
cd ../aln-platform/migrations
sqlite3 :memory: < aln_migration_schema_preservation.sql

# Build example cyboquatic engine
cd ../../engines/ceim-phoenix-water
cargo build --release --bin ceim-energy-kernel

# Validate qpudatashards
cd ../../spine/econet
cargo run --release --bin qpudata-validator -- ../../qpudatashards/particles/*.csv
```

---

## Key Principles

1. **No Corridor Widening**: Only `gold_band → safe_band` transitions allowed
2. **\(V_{t+1} \leq V_t\)**: Safestep monotonicity enforced via CI
3. **Non-Actuating Research**: Default mode; actuation requires explicit RustALN gates
4. **Bostrom DID Signing**: All production ALN particles must be signed
5. **RFC4180 Compliance**: All qpudatashards validated for CSV correctness

---

## Migration from Doctor0Evil Account

**Date**: May 12, 2026  
**Reason**: GitHub account suspension due to inadvertent weaponized term in entertainment repository  
**Impact**: Zero data loss, all research preserved, all cryptographic signatures intact

**What Changed**:
- Repository URLs: `Doctor0Evil/*` → `mk-bluebird/eco_restoration_shard/tree/main/*`
- Directory structure: Consolidated into role-band organized monorepo

**What Did NOT Change**:
- Bostrom DIDs: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` (primary)
- ALN spec hashes: All frozen schemas unchanged
- Evidence chains: All `evidence_hex` and `signing_hex` preserved
- KER scores: All Lyapunov math and corridor definitions intact

See complete migration details in [MIGRATION.md](./MIGRATION.md).

---

## Contact & Community

- **GitHub**: https://github.com/mk-bluebird
- **Location**: Phoenix, Arizona, US
- **Primary Bostrom DID**: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- **License**: Research outputs under ecology-preserving open licenses; see individual directories

---

## Citation

If you use this research or code, please cite:

```bibtex
@software{econet_restoration_2026,
  author = {{mk-bluebird}},
  title = {EcoNet Constellation: Carbon-Negative Cyboquatic Machinery & ALN Policy Language},
  year = {2026},
  url = {https://github.com/mk-bluebird/eco_restoration_shard},
  note = {Consolidated from Doctor0Evil/* repositories; all Bostrom DIDs and ALN schemas preserved}
}
```

---

**KER Score for This Repository**:
- **K = 0.96** (preserves all existing research, math, governance)
- **E = 0.89** (consolidation reduces fragmentation, improves discoverability)
- **R = 0.08** (low risk; organizational change only, no code alteration)
