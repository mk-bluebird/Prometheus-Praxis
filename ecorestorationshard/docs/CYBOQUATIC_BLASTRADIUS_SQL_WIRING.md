# Cyboquatic Blast-Radius SQL Wiring

This document describes how the blast-radius schema from `20260724-g-blastradius` has been wired into the main SQLite spine used by Eco‑Fort/EcoNet tooling.

## Database File Used

**Target DB:** `/workspace/db/cyboquatic_core.db`

This is the canonical cyboquatic database in the monorepo root, already used by other cyboquatic indexing and governance tools.

> Note: The file at `data/constellation/econet_constellation_index.db` was found to be a text SQL export, not a live SQLite database. The actual `cyboquatic_core.db` was used instead.

## Tables Added

The following tables were created from `ecorestorationshard/cyboquaticprogress/20260724-g-blastradius/sql/canal_blastradius_schema.sql`:

| Table | Purpose |
|-------|---------|
| `canal_node` | Canal nodes with FOG region/channel IDs, energy envelopes, topo sensitivity |
| `ker_profile` | KER triad baselines (k, e, r) bound to canal nodes |
| `surcharge_event` | Non-actuating evidence of canal stress (surcharge depth, hydraulic head, diag energy) |
| `blast_radius_diag` | Computed blast-radius diagnostics per event: risk planes, Lyapunov residual, KER scores |

## Indices Created

| Index | Table | Purpose |
|-------|-------|---------|
| `idx_canal_node_fog` | `canal_node` | Fast lookup by FOG region/channel |
| `idx_ker_profile_node` | `ker_profile` | KER profile lookup by node |
| `idx_surcharge_event_node_ts` | `surcharge_event` | Event queries by node + timestamp |
| `idx_blast_radius_node_time` | `blast_radius_diag` | Diagnostic queries by FOG region + time |
| `idx_blast_radius_vt` | `blast_radius_diag` | Filter by Lyapunov residual magnitude |
| `idx_blast_radius_ker` | `blast_radius_diag` | Filter by KER score for governance gating |

## Triggers (Invariants)

Three triggers enforce data integrity:

1. **`trg_ker_profile_invariant`** – Ensures `ker_score = k * e - r` and `ker_score > 0`
2. **`trg_blast_radius_ker_invariant`** – Same KER invariant for diagnostic rows
3. **`trg_surcharge_energy_envelope`** – Ensures `diag_energy_j <= canal_node.max_diag_energy_j`

## How Other Tooling Should Reference These Tables

### Read-Only Diagnostics

These tables are **diagnostic only** and should be treated as read-only by most tooling:

- **Eco-Fort shard catalog**: Can query `blast_radius_diag` joined with `surcharge_event` and `canal_node` to display risk metrics per node.
- **Cyboquatic index queries**: Can correlate workload frames (`cybo_workload_frame`) with blast-radius events by `fogregionid`/`fogchannelid`.
- **Governance gates**: KER scores can be used as input to governance decisions, but never as direct actuation signals.

### Example Query (Read-Only)

```sql
SELECT 
    cn.canal_node_id,
    cn.name,
    se.ts_utc,
    brd.radius_m,
    brd.vt_residual,
    brd.ker_score
FROM blast_radius_diag brd
JOIN surcharge_event se ON brd.surcharge_event_id = se.surcharge_event_id
JOIN canal_node cn ON se.canal_node_id = cn.canal_node_id
WHERE brd.vt_residual > 0.5
ORDER BY brd.ts_utc DESC
LIMIT 10;
```

## Application Method

Schema was applied using Python's `sqlite3` module (no `sqlite3` CLI available):

```bash
cd /workspace/ecorestorationshard
python tools/apply_blastradius_schema.py
```

This script:
1. Reads `canal_blastradius_schema.sql`
2. Applies it to `/workspace/db/cyboquatic_core.db`
3. Verifies table and index creation

## Non-Actuating Guarantee

All tables in this schema are **diagnostic only**:
- No foreign keys point to actuator control tables
- Triggers enforce data quality, not physical changes
- KER scores are for governance evaluation, not direct automation

Future C++ kernels (e.g., `canal_blastradius_engine.cpp`) will write to these tables via Python/Java telemetry layers, never directly.
