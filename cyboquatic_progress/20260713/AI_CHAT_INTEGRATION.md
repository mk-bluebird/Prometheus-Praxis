# Cyboquatic Workload - AI Chat Integration Guide

## Overview

This directory contains a multi-language implementation of the Cyboquatic workload diagnostic system for Phoenix canal nodes. The system computes energy tailwind corridors, Lyapunov residuals (ΔVt), and K,E,R triad scores for non-actuating diagnostic workloads.

## Quick Start for AI Assistants

### Python API (Recommended for AI Agents)

```python
from cyboquatic_workload import evaluate_workload, make_sample

# Basic evaluation
result = evaluate_workload(
    energy_req_j=1000.0,
    energy_surplus_j=1200.0,
    hydraulic_risk=0.2,
    uncertainty_risk=0.3,
    vt_before=0.1
)
print(result)  # JSON-serializable dict

# Extended evaluation with velocity and sensor health
result = evaluate_workload(
    energy_req_j=1000.0,
    energy_surplus_j=1200.0,
    hydraulic_risk=0.2,
    uncertainty_risk=0.3,
    vt_before=0.1,
    canal_velocity_mps=1.5,
    sensor_health=0.95,
    include_extended=True
)

# Create full sample object
sample = make_sample(
    sample_id="SAMPLE-001",
    node_id="PHX-NODE-01",
    timestamp_utc="2026-07-13T120000Z",
    energy_req_j=1000.0,
    energy_surplus_j=1200.0,
    hydraulic_risk=0.2,
    uncertainty_risk=0.3
)
```

### CLI Commands

```bash
# Evaluate workload
python cyboquatic_workload.py eval <energy_req_j> <energy_surplus_j> <hydraulic_risk> <uncertainty_risk> <vt_before>

# Evaluate with extended metrics
python cyboquatic_workload.py eval-extended <energy_req_j> <energy_surplus_j> <hydraulic_risk> <uncertainty_risk> <vt_before> <canal_velocity_mps> <sensor_health>

# Insert into database
python cyboquatic_workload.py insert <db_path> <node_id> <sample_id> [options...]

# Query database
python cyboquatic_workload.py query <db_path> [--date YYYYMMDD] [--node NODE_ID]
```

## Core Concepts

### Energy Tailwind Corridor

The system uses an **energy tailwind ratio** to determine safe operating corridors:

| Ratio (surplus/req) | Energy Risk (renergy) | Interpretation |
|---------------------|----------------------|----------------|
| ≥ 1.2               | 0.0                  | Safe tailwind (≥20% surplus) |
| 0.0 - 1.2           | Linear interpolation | Marginal corridor |
| ≤ 0.0               | 1.0                  | Severe shortfall |

### Lyapunov Residual (Vt)

```
Vt = Σ w_j * r_j²
   = 0.8 * renergy² + 1.0 * rhydraulic² + 0.6 * runcertainty²
   [+ 0.7 * rvelocity² + 0.5 * rsensor_health²]  (extended mode)
```

### K,E,R Triad

| Factor | Formula | Meaning |
|--------|---------|---------|
| **K** (Knowledge) | `0.95 - 0.4 * max_risk [- 0.25 if ΔVt > 0]` | High when risks are low and residual doesn't increase |
| **E** (Eco-impact) | `0.95 - Vt [- 0.3 if ΔVt > 0]` | High when residual is small and stable |
| **R** (Risk-of-harm) | `Vt [+ ΔVt if ΔVt > 0]` | Derived from residual, increases with worsening state |

### Safety Thresholds

A workload is considered **safe for production coupling** when:
- K ≥ 0.9
- E ≥ 0.9  
- R ≤ 0.15
- ΔVt ≤ 0.0 (non-worsening residual)

## Available Implementations

| Language | File | Purpose |
|----------|------|---------|
| **Python** | `python/cyboquatic_workload.py` | Full implementation with CLI, SQLite, JSON output |
| **C++** | `cpp/cyboquatic_workload_energyreq_dvt.cpp` | Native diagnostic helper with SQLite integration |
| **Java** | `java/CyboquaticWorkloadEnergyReqDvt.java` | JVM API for telemetry services |
| **Kotlin** | `kotlin/CyboquaticWorkloadEnergyTelemetry.kt` | Kotlin/Android telemetry with JDBC support |
| **Lua** | `lua/cyboquatic_workload_energyreq_dvt.lua` | Lightweight edge device module |
| **SQL** | `sql/cyboquatic_workload_energyreq_dvt.sql` | Direct SQLite schema and sample data |
| **ALN** | `aln/workload_energyreq_dvt_ker_shard.aln` | Governance particle with Bostrom DID binding |

## Database Schema

### daily_progress Table

```sql
CREATE TABLE daily_progress (
    progress_id       INTEGER PRIMARY KEY,
    yyyymmdd          TEXT NOT NULL,
    domain            TEXT NOT NULL,
    subtask_id        TEXT NOT NULL,
    node_id           TEXT NOT NULL,
    sample_id         TEXT NOT NULL,
    timestamp_utc     TEXT NOT NULL,
    energy_req_j      REAL NOT NULL,
    energy_surplus_j  REAL NOT NULL,
    hydraulic_risk    REAL NOT NULL,
    uncertainty_risk  REAL NOT NULL,
    canal_velocity_mps REAL DEFAULT 0.0,
    sensor_health     REAL DEFAULT 1.0,
    renergy           REAL NOT NULL,
    rhydraulic        REAL NOT NULL,
    runcertainty      REAL NOT NULL,
    rvelocity         REAL DEFAULT 0.0,
    rsensor_health    REAL DEFAULT 0.0,
    vt_before         REAL NOT NULL,
    vt_after          REAL NOT NULL,
    delta_vt          REAL NOT NULL,
    k_factor          REAL NOT NULL,
    e_factor          REAL NOT NULL,
    r_factor          REAL NOT NULL,
    phoenix_hex       TEXT NOT NULL,
    prior_pointer     TEXT NOT NULL
);
```

### Useful Views

- `v_cybo_workload_window`: Per-node workload summary aggregated by date
- `v_safe_workload_candidates`: Samples meeting safety thresholds (K≥0.9, E≥0.9, R≤0.15, ΔVt≤0)

## Example Outputs

### Successful Evaluation (Safe Tailwind)

```json
{
  "risk": {
    "renergy": 0.0,
    "rhydraulic": 0.15,
    "runcertainty": 0.25,
    "rvelocity": 0.0,
    "rsensor_health": 0.0
  },
  "vt_before": 0.1,
  "vt_after": 0.085,
  "delta_vt": -0.015,
  "ker": {
    "vt": 0.085,
    "delta_vt": -0.015,
    "k": 0.89,
    "e": 0.865,
    "r": 0.085
  },
  "is_safe": false
}
```

Note: `is_safe=false` because K < 0.9 (0.89 < 0.9). The workload has acceptable risk but doesn't meet the strict knowledge threshold.

## Next-Step Research Questions

1. **Calibrate ENERGY_TAILWIND_SAFE_RATIO**: Use Phoenix grid carbon-intensity and node surplus traces to harden `renergy` computation.

2. **Extend residual dimensions**: Add canal-velocity (`rvelocity`) and sensor health (`rcalib`, `rsigma`) planes for PFAS fate corridors.

3. **Validate K,E,R gating rules**: Replay historical cyboquatic workloads to validate thresholds (K≥0.9, E≥0.9, R≤0.15) before production coupling.

4. **CI guards for production**: Wire CI so that any new machinery shard entering EXPPROD or PROD must show recent, Lyapunov-safe evidence rows in `daily_progress`.

## Governance

- **DID Owner**: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- **Phoenix Hex Pattern**: `0x20260713PHX3345NWorkloadEnergyDeltaVt*`
- **Prior Pointer**: `20260709/workload_energy_dvt_rust`
- **Domain**: `workload_energy_dvt`
- **Subtask ID**: `PHX-CANAL-WL-2026-07-13`

## Non-Actuating Guarantee

All implementations in this directory are **strictly non-actuating**. They:
- Compute diagnostic metrics only
- Do not issue hardware control commands
- Do not modify canal operations
- Serve as evidence for future routing decisions

This maintains the safety fence while building the machinery spine for future always-improve routing.
