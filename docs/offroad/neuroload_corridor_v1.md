# NeuroLoad Corridor v1

## Role

NeuroLoad Corridor v1 defines a sovereign, off-road risk coordinate for normalized neuro load in augmented-citizen and smart-city contexts. It is designed to plug into Aletheion and Prometheus-Praxis ecosafety grammars without interfering with existing corridors and capitals.

The goal is to give augmented citizens and system designers a clear, auditable way to express how much neural load a node is carrying, and to bind that load to safety corridors, K/E/R factors, and data-as-labor policies.

## ALN shard

The ALN file `aln/offroad/neuroload_corridor_v1.aln` introduces:

- `NeuroLoadCoord.v1`: a risk coordinate with fields for id, node_id, value, minsafe, maxsafe, status, timestamp_ns, K/E/R factors, author_did, corridor_id, and notes.
- `NeuroLoadCorridorBinding.v1`: default corridor bounds and author metadata for a given node.
- `NeuroLoadCorridor.v1` schema: relation between a coordinate and its corridor binding.
- `NeuroLoadSovereignty.v1`: a policy wrapper for knowledge objects under data-as-labor.
- Rules for status updates, hard-stop escalation, and K/E/R factor transitions.

These shards are self-describing and versioned, so future off-road extensions can add new rules or fields without breaking existing workflows.

## Rust module

The Rust file `src/offroad/neuroload_corridor_v1.rs` provides:

- `NeuroLoadStatus`: enum for "OK", "SoftViolation", "HardViolation".
- `NeuroLoadCoord`: struct aligned with `NeuroLoadCoord.v1`, including invariants and step logic.
- `NeuroLoadCorridorBinding`: struct for default bounds per node and corridor.
- `NeuroLoadKnowledgeObject`: wrapper that embeds a coordinate in a data-as-labor policy.
- Validator functions for invariants, status updates, hard-stop, K/E/R updates.
- Utility functions for tick-based stepping and Lyapunov-style residual comparison.

The module forbids unsafe code and is structured for reuse in corridor engines, ecosafety guards, and governance layers.

## Wiring into existing architecture

NeuroLoad can be wired into existing Aletheion and Prometheus-Praxis components:

- **Ecosafety spine**: treat NeuroLoad as an additional risk coordinate channel, similar to thermal, EMF, or chemical risk. Controllers can require that `NeuroLoadCoord.is_inside_corridor()` remains true during actuation.
- **Lyapunov residuals**: the `residual()` method gives a simple quadratic term that can be aggregated into higher-level Lyapunov functions for urban heat, neurobiome, or augmentation safety.
- **SMART-chains and governance**: `NeuroLoadKnowledgeObject` can be bound to SMART-chain decisions, ensuring that any use of neuro load metrics remains consistent with data-as-labor and sovereignty policies.
- **Data-as-labor**: the KO fields associate each coordinate with an author DID, allowed roles, allowed actions, and CHAT cost units. This makes it possible to track, reward, and constrain use of contributed neuro load data across agents and workflows.

## CI and safety

For production readiness:

- Add unit tests that construct valid and invalid `NeuroLoadCoord` instances and assert that invariants and status transitions behave as expected.
- Integrate the module into existing ecosafety CI pipelines to ensure that any added NeuroLoad corridors do not bypass existing "no corridor, no build" rules.
- Use the ALN shard as the single source of truth for schema evolution; any Rust changes must reflect ALN changes and vice versa.

NeuroLoad Corridor v1 is intentionally minimal and self-contained. It can be extended with richer models (e.g., multi-band load, time-window averages, multi-node aggregates) while preserving the sovereignty and data-as-labor principles that govern all off-road vocabulary in the mono-repo.
