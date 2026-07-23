# ALN Specifications Index

This document lists all key `.aln` files in the repository, their purpose, inputs/outputs, and which modules consume them.

## Table of Contents

- [Identity Specs](#identity-specs)
- [Corridor Specs](#corridor-specs)
- [KER Specs](#ker-specs)
- [Shard Layout Specs](#shard-layout-specs)
- [Blast Radius Specs](#blast-radius-specs)
- [Governance Specs](#governance-specs)

---

## Identity Specs

### `knowledge/identity_bostrom_primary.aln`

**Purpose**: Primary DID identity binding for Bostrom identity in the eco-system.

**Inputs**: 
- DID identifiers
- Domain separation policies

**Outputs**:
- Verified identity bindings
- Alias mappings

**Consumed by**:
- Python: `scripts/identity/check_did_domain_separation.py`
- Rust: `crates/bostrom-signer`, `crates/steward_identity`

---

### `aln/identity/ALE-IDENTITY-DID-DOMAIN-POLICY-001.aln`

**Purpose**: Policy for DID domain separation to prevent identity collisions across contexts.

**Inputs**:
- Domain definitions
- Separation rules

**Outputs**:
- Validated domain bindings

**Consumed by**:
- Python: `scripts/identity/check_did_domain_separation.py`
- Lua: `lua/chat_kermap.lua`

---

### `aln/identity/PPX-Minimal-Continuity-Neurorights-001.aln`

**Purpose**: Minimal continuity policy respecting neurorights for cognitive interfaces.

**Inputs**:
- Neurorights constraints
- Continuity requirements

**Outputs**:
- Validated continuity envelopes

**Consumed by**:
- Rust: `crates/ppx_minimal_identity_kernel`
- Python: `python/ppx_agent.py`

---

## Corridor Specs

### `aln/corridor_econet_blastradius_2026v1.aln`

**Purpose**: Defines ecosafety corridors for blast radius calculations across the econet.

**Inputs**:
- Node positions
- Impact metrics
- Risk vectors

**Outputs**:
- Corridor bounds
- Safety margins

**Consumed by**:
- Lua: `tools/sunflower_bee_corridor_ledger_verifier.lua`, `lua/econet_blastradius_inspect.lua`
- Rust: `crates/blast_radius_kernel`, `crates/econet_blastradius_spine`, `crates/cyboquatic_blastradius_spine`

---

### `aln/eco/eco.beecorridor.envelope.v1.aln`

**Purpose**: Envelope definition for bee corridor ecosafety bounds.

**Inputs**:
- Bee corridor geometry
- Environmental thresholds

**Outputs**:
- Envelope constraints
- Violation alerts

**Consumed by**:
- Lua: `tools/sunflower_bee_corridor_ledger_verifier.lua`
- Rust: `crates/eco-bee-corridor-envelope`, `crates/beecorridor-ledger`

---

### `aln/eco/eco.sunflower.beecorridor.ledger.v1.aln`

**Purpose**: Ledger entries for sunflower bee corridor evidence bundles with Merkle roots.

**Inputs**:
- Evidence leaves
- Corridor IDs
- Epoch timestamps

**Outputs**:
- Merkle root hashes
- Ledger entries

**Consumed by**:
- Lua: `tools/sunflower_bee_corridor_ledger_verifier.lua`
- Rust: `crates/beecorridor-ledger`

---

## KER Specs

### `aln/KERComposition2026v1.aln`

**Purpose**: Core KER (Knowledge/Eco-impact/Risk-of-Harm) composition policy.

**Inputs**:
- Knowledge factors
- Eco-impact scores
- Risk assessments

**Outputs**:
- Composite KER scores
- Deployment decisions

**Consumed by**:
- Lua: `crates/ker-composition/lua/ker_composition_validator.lua`
- Rust: `crates/ker-composition`, `crates/ker-score-lab`, `crates/federated-ker`

---

### `knowledge/eco_lake_ker_governance_v1.aln`

**Purpose**: KER governance policy for eco-lake risk assessment.

**Inputs**:
- Lake risk metrics
- Governance thresholds

**Outputs**:
- KER-gated decisions

**Consumed by**:
- Rust: `crates/lake_risk_init`
- Python: `python/ppx_metrics.py`

---

### `knowledge/eco_recovery_az_lake_risk_v1.aln`

**Purpose**: Arizona lake recovery risk assessment with KER scoring.

**Inputs**:
- Water quality data
- Recovery targets

**Outputs**:
- Risk coordinates
- Recovery recommendations

**Consumed by**:
- Lua: `tools/cyboquatic_microplastic_risk.lua`
- Rust: `crates/ecosafety-core`

---

## Shard Layout Specs

### `aln/prometheus-shard-layout.v1.aln`

**Purpose**: Master shard layout defining zones, roles, and dependencies for Prometheus-Praxis.

**Inputs**:
- Zone definitions
- Role assignments
- Dependency graph

**Outputs**:
- Shard topology
- PlantUML diagram (generated)

**Consumed by**:
- Lua: `tools/prometheus_shard_layout_plantuml.lua`
- Rust: `crates/cyboquatic_spine`, `crates/eco_restoration_shard_config`

---

### `core/aln/ecosafety-nanoswarm-urban-core/src/nanoswarm_urban_shards.aln`

**Purpose**: Shard configuration for nanoswarm urban core operations.

**Inputs**:
- Urban zone boundaries
- Nanoswarm deployment rules

**Outputs**:
- Sharded deployment plans

**Consumed by**:
- Rust: `crates/ecosafety-nanoswarm-urban-core`, `crates/school_zone_nanodefense`

---

### `crates/city-pass/src/aln_city_pass_shards.aln`

**Purpose**: City-pass shard configuration for access control and revocation.

**Inputs**:
- City zone definitions
- Pass validity rules

**Outputs**:
- Shard-specific pass states

**Consumed by**:
- Rust: `crates/city-pass`

---

### `aln/RegionRepresentationShard2026v1.aln`

**Purpose**: Regional representation shard for geographic data partitioning.

**Inputs**:
- Geographic boundaries
- Representation weights

**Outputs**:
- Regional shard assignments

**Consumed by**:
- Lua: `tools/region_representation_shard_visualizer.lua`
- Rust: `crates/eco_restoration_shard`

---

## Blast Radius Specs

### `Eco-Fort/aln/LaneAdmissibilityKernel2026v1.aln`

**Purpose**: Lane admissibility kernel with blast radius constraints.

**Inputs**:
- Lane status
- Admissibility criteria

**Outputs**:
- Admission decisions
- Blast radius flags

**Consumed by**:
- Rust: `crates/lane-governance`, `crates/governance-guard`

---

### `aln/MT6883RiskAuditShard2026v1.aln`

**Purpose**: Risk audit shard for MT6883 component with blast radius tracking.

**Inputs**:
- Component telemetry
- Risk thresholds

**Outputs**:
- Audit reports
- Risk coordinates

**Consumed by**:
- Rust: `crates/topology-risk`, `crates/blastradius`

---

## Governance Specs

### `praxiskernel/aln/PPX-CONTINUITY-GOVERNANCE-ENVELOPE-001.aln`

**Purpose**: Continuity governance envelope for Prometheus-Praxis kernel.

**Inputs**:
- Continuity metrics
- Governance gates

**Outputs**:
- Envelope compliance status

**Consumed by**:
- Rust: `crates/ppx_continuity_kernel`
- Python: `python/ppx_config.py`

---

### `aln/PrometheusPraxisCore.v1.aln`

**Purpose**: Core governance spec for Prometheus-Praxis execution layer.

**Inputs**:
- Execution parameters
- Governance policies

**Outputs**:
- Validated execution plans

**Consumed by**:
- All Prometheus-Praxis crates
- Lua: `runtime/lua/prometheus_praxis/*.lua`

---

### `aln-specs/AbsoluteDataSovereigntyPolicy2026v1.aln`

**Purpose**: Absolute data sovereignty policy enforcing data ownership rights.

**Inputs**:
- Data provenance records
- Sovereignty claims

**Outputs**:
- Validated sovereignty bindings

**Consumed by**:
- Rust: `crates/deployment_accountability_core`
- Python: `aln_migration.py`

---

### `aln/governance/AIChatEmergencyFetchPolicy2026v1.aln`

**Purpose**: Emergency fetch policy for AI chat interactions with governance overrides.

**Inputs**:
- Emergency conditions
- Fetch permissions

**Outputs**:
- Emergency access grants

**Consumed by**:
- Python: `python/ppx_agent.py`
- Rust: `crates/eco-chat-ffi`

---

## Data Flow Summary

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   ALN Specs     │────▶│  Python Tools    │────▶│  Metrics/Output │
│   (.aln files)  │     │  (parsers,       │     │  (reports,      │
│                 │     │   validators)    │     │   diagrams)     │
└────────┬────────┘     └──────────────────┘     └─────────────────┘
         │                                              ▲
         │                                              │
         ▼                                              │
┌─────────────────┐     ┌──────────────────┐           │
│   Rust Crates   │────▶│   Lua Scripts    │───────────┘
│  (consumers,    │     │  (visualizers,   │
│   enforcers)    │     │   verifiers)     │
└─────────────────┘     └──────────────────┘
```

## Adding New ALN Specs

When adding a new ALN spec:

1. Place it in the appropriate directory based on category
2. Update this document with purpose, inputs, outputs, and consumers
3. Ensure at least one tool or crate references the spec
4. Run `python tools/repo_quality_check.py` to verify no orphans
