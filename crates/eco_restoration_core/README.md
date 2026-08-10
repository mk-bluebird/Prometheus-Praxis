<!-- File: crates/eco_restoration_core/README.md -->
# eco_restoration_core

`eco_restoration_core` provides non-actuating retrieval and maintenance-event interfaces for ecological stations. Its public contract exposes recent qpudatashard health windows and records maintenance observations so planners can evaluate K/E/R status, Lyapunov residuals, and corridor state before proposing any restoration workload.

## Interface contract

### FetchShard

`FetchShard` returns the latest validated diagnostic window for one ecological node.

```json
{
  "type": "FetchShard",
  "node_id": "vault-001"
}
```

A successful response contains the node identity, UTC window bounds, K/E/R values in `[0, 1]`, non-negative `vt`, a serialized corridor-status object, and an opaque evidence identifier.

```json
{
  "node_id": "vault-001",
  "window_start_ts": "2026-07-06T09:00:00Z",
  "window_end_ts": "2026-07-06T09:15:00Z",
  "ker_k": 0.93,
  "ker_e": 0.90,
  "ker_r": 0.12,
  "vt": 0.11,
  "corridor_status": "{}",
  "evidencehex": "0xa1b2c3d4e5f67890f1e2d3c4b5a6978"
}
```

The caller must treat an absent shard, an invalid K/E/R interval, a non-positive KER margin, or a corridor breach as unavailable for routing. The core returns diagnostic information only; it does not control machinery.

### MaintenanceEvent

`MaintenanceEvent` records a field observation from an authorized client. The event is evidence-bearing and its K/E/R deltas remain bounded to prevent a single unreviewed report from asserting an implausible ecological improvement.

```json
{
  "type": "MaintenanceEvent",
  "node_id": "vault-001",
  "event_ts": "2026-07-06T09:12:34Z",
  "engineer_id": "engineer-123",
  "event_type": "inspection",
  "notes": "Cleaned intake screen.",
  "photo_uri": "content://media/external/images/media/12345",
  "local_evidencehex": "0x9f8e7d6c5b4a392817160f0e0d0c0b0a",
  "device_id": "android-unit-01"
}
```

A successful response returns an accepted status, a core evidence identifier, and bounded K/E/R impact values.

```json
{
  "status": "ok",
  "core_evidencehex": "0xcf34e1a2b3c4d5e6f7081928374655aa",
  "ker_impact": {
    "delta_k": 0.01,
    "delta_e": 0.00,
    "delta_r": -0.01
  }
}
```

## Safety rules

- Node identifiers, engineer identifiers, device identifiers, and evidence identifiers must be nonempty.
- `ker_k`, `ker_e`, and `ker_r` are normalized to `[0, 1]`.
- A shard is admissible only when `ker_k * ker_e > ker_r`.
- `vt` is non-negative and corridor status must be present.
- Maintenance deltas are bounded to `[-0.10, 0.10]`; they describe evidence review effects and do not authorize actuation.
- Clients retain media locally. `photo_uri` is metadata only and is never dereferenced by the core.

## ALN contract

The machine-checkable contract is located at:

```text
crates/eco_restoration_core/aln/eco_station_mcp.aln
```
