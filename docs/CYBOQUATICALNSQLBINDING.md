# CYBOQUATICALNSQLBINDING.md

## Overview

Cyboquatic ALN v2 particles define the canonical schema for workload, drainage-decay, and blast-radius diagnostics. SQLite DBs in `db/` implement these schemas as tables and views, ensuring K,E,R and Lyapunov fields are consistent across C++ engines, Rust governance crates, and EcoNet spines.

This document binds key ALN specs to their DB tables and maps K,E,R,Vt fields explicitly.

## Key ALN specs and DB tables

### Workload kernel

- ALN spec:
  - `aln/alnCyboquaticWorkloadKernel2026v1.aln2`
  - Particle: `cyboquatic.workload.kernel`
- DB table:
  - `db/dbcyboquaticdailyprogress.sql`
  - Table: `cyboquatic_daily_progress`
- Field mapping:
  - ALN `nodeId` → SQL `node_id`
  - ALN `dateUtc` → SQL `date_utc`
  - ALN `windowStartUtc` → SQL `window_start_utc`
  - ALN `windowEndUtc` → SQL `window_end_utc`
  - ALN `energyReqJ` → SQL `energy_req_j` (if stored separately)
  - ALN `vtCurrent` → SQL `vt_current`
  - ALN `vtNext` → SQL `vt_next`
  - ALN `vtDelta` → SQL `vt_delta`
  - ALN `k` → SQL `k_knowledge`
  - ALN `e` → SQL `e_ecoimpact`
  - ALN `r` → SQL `r_risk`
  - ALN `kerScore` → SQL `ker_score`
  - ALN `lane` → SQL `lane`
  - ALN `safeToPromote` → SQL `safetopromote_ok`
  - ALN `evidenceHex` → SQL `evidence_hex`
  - ALN `signingDid` → SQL `signing_did`

The trigger `trg_daily_progress_ker_lyapunov` enforces KER and Lyapunov invariants consistent with ALN rules.

### Drainage-decay kernel

- ALN spec:
  - `aln/alnCyboquaticDrainageDecayKernel2026v1.aln2`
  - Particle: `cyboquatic.drainagedecay.kernel`
- DB table:
  - `db/dbcyboquaticdrainagedecayindex.sql`
  - Table: `cyboquatic_drainagedecay_index`
- Field mapping:
  - ALN `frameId` → SQL `frameid`
  - ALN `canalNodeId` → SQL `canalnodeid`
  - ALN `kerProfileId` → SQL `kerprofileid`
  - ALN `timestampUtc` → SQL `timestamputc`
  - ALN `bodMgL` → SQL `bodmgl`
  - ALN `tssMgL` → SQL `tssmgl`
  - ALN `cecCmolPerKg` → SQL `ceccmolperkg`
  - ALN `frameEnergyJ` → SQL `frameenergyj`
  - ALN `deltaVtMps` → SQL `deltavtmps`
  - ALN `k` → SQL `kknowledgefactor`
  - ALN `e` → SQL `eecoimpact`
  - ALN `r` → SQL `rriskfactor`
  - ALN `kerScore` → SQL `kerscore`
  - ALN `fogRegionId` → SQL `fogregionid`
  - ALN `fogChannelId` → SQL `fogchannelid`
  - ALN `governanceParticleHex` → SQL `governanceparticlehex`
  - ALN `evidenceHex` → SQL `evidence_hex`
  - ALN `signingDid` → SQL `signing_did`

The trigger `trg_drainagedecay_ker_invariant` checks `kerscore` matches `k e - r` and remains positive, reflecting ALN corridor constraints.

### Blast-radius governance

- ALN spec:
  - `aln/alnCyboquaticBlastRadiusGovernance2026v1.aln2`
  - Particle: `cyboquatic.blastradius.governance`
- DB table:
  - `db/dbcyboquaticblastradiusindex.sql`
  - Table: `cyboquatic_blast_radius_index`
- Field mapping:
  - ALN `blastIndexId` → SQL `blast_index_id`
  - ALN `nodeId` → SQL `node_id`
  - ALN `eventId` → SQL `event_id`
  - ALN `corridorId` → SQL `corridor_id`
  - ALN `laneId` → SQL `lane_id`
  - ALN `alnShardName` → SQL `aln_shard_name`
  - ALN `alnVersion` → SQL `aln_version`
  - ALN `didRoot` → SQL `did_root`
  - ALN `eventTimestampUtc` → SQL `event_timestamp_utc`
  - ALN `windowStartUtc` → SQL `window_start_utc`
  - ALN `windowEndUtc` → SQL `window_end_utc`
  - ALN `surchargeLevelM` → SQL `surcharge_level_m`
  - ALN `hydraulicHeadM` → SQL `hydraulic_head_m`
  - ALN `inflowM3s` → SQL `inflow_m3_s`
  - ALN `durationS` → SQL `duration_s`
  - ALN `radiusM` → SQL `radius_m`
  - ALN `radiusNorm` → SQL `radius_norm`
  - ALN `k` → SQL `k_knowledge`
  - ALN `e` → SQL `e_ecoimpact`
  - ALN `r` → SQL `r_risk`
  - ALN `kerScore` → SQL `ker_score`
  - ALN `residualKer` → SQL `residual_ker`
  - ALN `rohCoordinate` → SQL `roh_coordinate`
  - ALN `radiusWithinLimit` → SQL `radius_within_limit`
  - ALN `kerWithinLimit` → SQL `ker_within_limit`
  - ALN `laneAdmissibleOk` → SQL `lane_admissible_ok`
  - ALN `safeToPromoteOk` → SQL `safetopromote_ok`
  - ALN `sourceDbPath` → SQL `source_db_path`
  - ALN `sourceViewName` → SQL `source_view_name`
  - ALN `evidenceHex` → SQL `evidence_hex`
  - ALN `createdAtUtc` → SQL `created_at_utc`
  - ALN `lastUpdatedUtc` → SQL `last_updated_utc`

The trigger `trg_blast_radius_ker_invariants` enforces KER consistency and radius/ker bounds, matching ALN governance rules.

## Worked example: daily progress → main index

Consider a cyboquatic workload frame generated in a dated progress shard and promoted into the main daily progress DB:

1. Engine output (C++):
   - `WorkloadInput` and `WorkloadOutput` from `cyboquatic_workload_engine.cpp` produce:
     - `node_id = "CANAL-001"`
     - `window_start_utc = "2026-07-22T00:00:00Z"`
     - `window_end_utc = "2026-07-22T00:15:00Z"`
     - `k = 0.92`, `e = 0.91`, `r = 0.10`, `ker_score = 0.92 * 0.91 - 0.10`
     - `vt_current = 0.48`, `vt_next = 0.47`, `vt_delta = -0.01`
     - `lane = "PRODUCTION"`, `safeToPromote = true`
     - `evidence_hex = 0x20260722PHX3345NWorkloadEnergyVt`
2. ALN instantiation:
   - A `cyboquatic.workload.kernel` record is instantiated in `alnCyboquaticWorkloadKernel2026v1.aln2` with these values.
   - ALN `kerScore` requirement enforces `kerScore ≈ k e - r`.
   - ALN `vtDelta` requirement enforces `vtDelta ≈ vtNext - vtCurrent ≤ 0`.
3. SQLite insertion:
   - Row written into `cyboquatic_daily_progress` in `db/dbcyboquaticdailyprogress.sql`:
     - `node_id = "CANAL-001"`
     - `date_utc = "2026-07-22"`
     - `window_start_utc`, `window_end_utc` as above.
     - `k_knowledge = 0.92`, `e_ecoimpact = 0.91`, `r_risk = 0.10`
     - `ker_score` computed and checked by `trg_daily_progress_ker_lyapunov`.
     - `vt_current`, `vt_next`, `vt_delta = -0.01` verified as non-increasing.
     - `lane = "PRODUCTION"`, `safetopromote_ok = 1`
     - `evidence_hex` and `signing_did` bound to Phoenix registry and Bostrom DID.
4. EcoNet and Prometheus-Praxis usage:
   - EcoNet views aggregate the daily progress rows into KER windows per node/lane.
   - Prometheus-Praxis governance crates read these rows to gate lane promotions and schedule future workloads.

This example shows the end-to-end binding:

- C++ engine → ALN particle → SQLite row → EcoNet view → governance decision,

all governed by the same K,E,R, Vt, and evidence hex semantics under EcoFort/Phoenix grammar.
