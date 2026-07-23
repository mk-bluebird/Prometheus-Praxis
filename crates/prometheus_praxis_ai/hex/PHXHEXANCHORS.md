# Phoenix Hex Anchors for prometheus_praxis_ai
#
# This manifest statically binds ALN particles, Rust modules, C++ engine files,
# and SQL schemas for the prometheus_praxis_ai crate into the Phoenix Hex
# registry. All anchors are cryptographically associated with the Bostrom DID:
#   bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
#
# These entries MUST be mirrored in Eco-Fort/db/phoenixhexregistry.sql and kept
# in lockstep with repository changes to preserve provenance and grammar
# integrity.

## Root ecosafety spine anchor

- anchor_id: PHX_PROMETHEUS_PRAXIS_AI_ENGINE20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: SPINE
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:00:00Z
  files:
    - path: crates/prometheus_praxis_ai/Cargo.toml
      role: crate_manifest
    - path: crates/prometheus_praxis_ai/src/lib.rs
      role: ecosafety_spine
    - path: crates/prometheus_praxis_ai/src/always_improve.rs
      role: lane_corridor_controller
    - path: crates/prometheus_praxis_ai/src/engine/mod.rs
      role: numeric_engine_facade
    - path: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql
      role: sqlite_schema

## ALN particle anchors (canonical grammar)

- anchor_id: PHX_DRAINAGE_DECAY_KERNEL20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: ALN_DRAINAGE_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:01:00Z
  files:
    - path: crates/prometheus_praxis_ai/aln/DrainageDecayKernel2026v1.aln2
      role: aln_particle
    - path: crates/prometheus_praxis_ai/src/lib.rs
      role: rust_structs_drainage
    - path: crates/prometheus_praxis_ai/src/engine/mod.rs
      role: rust_kernel_drainage
    - path: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql
      role: sql_table_drainage_frame
    - path: crates/prometheus_praxis_ai/src/ffi/drainage.rs
      role: ffi_shim_drainage

- anchor_id: PHX_WORKLOAD_KERNEL20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: ALN_WORKLOAD_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:02:00Z
  files:
    - path: crates/prometheus_praxis_ai/aln/WorkloadKernel2026v1.aln2
      role: aln_particle
    - path: crates/prometheus_praxis_ai/src/lib.rs
      role: rust_structs_workload
    - path: crates/prometheus_praxis_ai/src/engine/mod.rs
      role: rust_kernel_workload
    - path: crates/prometheus_praxis_ai/src/always_improve.rs
      role: always_improve_workload_band
    - path: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql
      role: sql_table_workload_frame
    - path: crates/prometheus_praxis_ai/src/ffi/workload.rs
      role: ffi_shim_workload

- anchor_id: PHX_AI_NODE_KERNEL20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: ALN_AI_NODE_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:03:00Z
  files:
    - path: crates/prometheus_praxis_ai/aln/AiDatacenterNode2026v1.aln2
      role: aln_particle
    - path: crates/prometheus_praxis_ai/src/lib.rs
      role: rust_structs_ai_node
    - path: crates/prometheus_praxis_ai/src/engine/mod.rs
      role: rust_kernel_ai_node
    - path: crates/prometheus_praxis_ai/src/always_improve.rs
      role: always_improve_ai_node_band
    - path: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql
      role: sql_table_ai_node_frame
    - path: crates/prometheus_praxis_ai/src/ffi/ai_node.rs
      role: ffi_shim_ai_node

## C++ numeric engine anchors (non-actuating kernels)

- anchor_id: PHX_ECO_ENGINE_DRAINAGE20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: CPP_DRAINAGE_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:04:00Z
  files:
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_drainage.hpp
      role: cpp_header_drainage
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_drainage.cpp
      role: cpp_kernel_drainage
    - path: crates/prometheus_praxis_ai/src/ffi/drainage.rs
      role: rust_ffi_drainage

- anchor_id: PHX_ECO_ENGINE_WORKLOAD20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: CPP_WORKLOAD_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:05:00Z
  files:
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_workload.hpp
      role: cpp_header_workload
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_workload.cpp
      role: cpp_kernel_workload
    - path: crates/prometheus_praxis_ai/src/ffi/workload.rs
      role: rust_ffi_workload

- anchor_id: PHX_ECO_ENGINE_AI_NODE20260723
  domain: PROMETHEUS_PRAXIS_AI
  scope: CPP_AI_NODE_KERNEL
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:06:00Z
  files:
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_ai_node.hpp
      role: cpp_header_ai_node
    - path: crates/prometheus_praxis_ai/src/engine/cpp/eco_engine_ai_node.cpp
      role: cpp_kernel_ai_node
    - path: crates/prometheus_praxis_ai/src/ffi/ai_node.rs
      role: rust_ffi_ai_node

## Governance and audit anchors

- anchor_id: PHX_PROMETHEUS_PRAXIS_AI_AUDIT20260723
  domain: PROMETHEUS_PRAIS_AI
  scope: AUDIT_LOG_SCHEMA
  did_root: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
  created_utc: 2026-07-23T03:07:00Z
  files:
    - path: crates/prometheus_praxis_ai/sql/db_prometheus_praxis_ai.sql
      role: sql_audit_event_log
    - path: crates/prometheus_praxis_ai/src/always_improve.rs
      role: audit_rationale_source

## Notes

- All anchors above are static; they must be updated only via explicit commits
  and corresponding registry updates. No dynamic anchor generation is allowed.
- Hashes for each file (hash_hex field in phoenixhexfile) MUST be computed by
  the CI pipeline and verified against these anchors before promotion of any
  crate build or deployment.
- Any change to ALN particles, Rust structs, C++ kernels, or SQL tables must
  be accompanied by:
  - an updated hex anchor record,
  - regenerated Kani proofs for critical invariants (Lyapunov monotonicity,
    KER positivity, lane corridor constraints),
  - and a governance review bound to the same Bostrom DID.
