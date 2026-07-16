# File: aln/README.md
# Destination: Prometheus-Praxis/aln/README.md

# ALN Specs for KER Composition, Governance, and Ecosafety

This directory contains **ALN (Adaptive Layer Network) control documents** that define how **KER (Knowledge, Eco-impact, Risk)** evidence is represented, combined, and governed across the **Prometheus-Praxis ecosafety spine**.

All files in this directory are **non-actuating governance artifacts**: they define particles, functions, and invariants, but do not interact with hardware or actuators.

---

## Table of Contents

- [Core Specifications](#core-specifications)
- [Directory Structure](#directory-structure)
- [KER Algebra and Composition](#ker-algebra-and-composition)
- [Ecosafety Planes and Weights](#ecosafety-planes-and-weights)
- [Lane Governance](#lane-governance)
- [Identity and Neurorights](#identity-and-neurorights)
- [Health Data and Labor](#health-data-and-labor)
- [Integration Guidelines](#integration-guidelines)
- [Extension Guidelines](#extension-guidelines)

---

## Core Specifications

### Primary ALN Files

| File | Category | Role | Description |
|------|----------|------|-------------|
| `KERComposition2026v1.aln` | ECOSAFETY | GOVERNANCE | Base KER particle schema and compositional algebra |
| `KnowledgeFactorKernel2026v1.aln` | ECOSAFETY | GOVERNANCE | Knowledge factor kernel with evidential inputs |
| `EcoWealthKernel2026v1.aln` | ECOWEALTH | GOVERNANCE | Exponents α, β, γ for W_s(K_s,E_s,R_s) |
| `PlaneWeightsShard2026v1.aln` | ECOSAFETY_GOVERNANCE | SPINE | Lyapunov weights, non-compensation flags, uncertainty caps |
| `PrometheusPraxisCore.v1.aln` | ECOSAFETY | CORE | Core Prometheus-Praxis invariants and corridor definitions |
| `DataQualityPlane2026v1.aln` | ECOSAFETY_GOVERNANCE | SPINE | Risk coordinates for ingest quality (rcalib) and sensor uncertainty (rsigma) |
| `NonActuatingWorkloadPolicy2026v1.aln` | GOVERNANCE_CONTRACT | SPINE | Contract for workloads without actuator I/O |
| `DefinitionRegistry2026v1.aln` | GOVERNANCE | REGISTRY | Canonical registry particle for ALN definitions mirrored into SQL and Rust |
| `EcoRepoIndex2026v1.aln` | GOVERNANCE | INDEX | DID-anchored repo manifest for the EcoNet constellation |
| `LaneGovernanceTopology2026v1.aln` | LANE_TOPOLOGY | GOVERNANCE | Promotion/downgrade rules based on r_topology bands |

---

## Directory Structure

```
aln/
├── *.aln                          # Root-level ALN specifications
├── cyboquatic/
│   └── ALE-INF-CYBOQUATIC-INDEX-BIND-001.aln   # Cyboquatic asset bindings
├── eco/
│   ├── eco.beecorridor.envelope.v1.aln         # Bee corridor habitat envelope
│   ├── eco.beecorridor.evidencebundle.v1.aln   # Evidence bundle for bee corridors
│   └── eco.sunflower.beecorridor.ledger.v1.aln # Ledger for sunflower bee corridors
├── ecosafety/
│   └── SovereignPreDeployment2026v1.aln        # Pre-deployment sovereignty guarantees
├── governance/
│   └── AIChatEmergencyFetchPolicy2026v1.aln    # Emergency refetch rules for eco-wealth dashboards
├── health/
│   └── 2026/
│       └── HealthDataTCR2026v1.aln             # Health & AI-chat data TCR + QF grammar
├── healthcare/
│   └── HealthcareRiskPlane2026v1.aln           # Healthcare risk plane specification
├── identity/
│   ├── ALE-IDENTITY-BRAINDID-ALIASING-001.aln  # BrainDID aliasing policy
│   ├── ALE-IDENTITY-DID-DOMAIN-POLICY-001.aln  # DID domain separation policy
│   ├── PPX-Minimal-Continuity-Neurorights-001.aln
│   └── PPX-PsychContinuity-Neurorights-001.aln
├── particles/
│   └── eco.beecorridor.envelope.v1.aln         # Particle mirror
├── prometheus-role-bands.v1.aln                # Prometheus role band definitions
├── prometheus-shard-layout.v1.aln              # Shard layout specification
├── registry/
│   └── aln_particle_registry.aln               # Particle registry index
└── test-corpus/
    ├── cyboquatic-dao-deploy.aln
    ├── ecosafety.core.2026v1.aln
    └── phoenix-tray-pilot.aln
```

---

## KER Algebra and Composition

### Base KER Particle (`KERParticle2026v1`)

```aln
particle KERParticle2026v1 {
  field particle_id    string required true;
  field topic_id       string required true;
  field lane           string required true;      # RESEARCH, PILOT, PROD
  field K              float  required true;      # normalized [0,1]
  field E              float  required true;      # normalized [0,1]
  field R              float  required true;      # normalized [0,1]
  field evidence_hex   string required true;      # hex-encoded provenance
  field signing_hex    string required true;      # DID-bound signature
}
```

### Composition Operator: `ker_oplus_geom_min_max`

The composition operator combines two KER particles using conservative aggregation:

- **K_combined = sqrt(K1 * K2)** — geometric mean for knowledge
- **E_combined = min(E1, E2)** — worst-case for eco-impact (non-compensation)
- **R_combined = max(R1, R2)** — monotone risk (risk cannot decrease)

### Invariants

| Invariant | Description |
|-----------|-------------|
| `ker_combine_risk_cap` | If both parents satisfy R ≤ θ (θ=0.30), the composite must too |
| `ker_combine_K_E_bounds` | K_combined lies between parent Ks; E_combined ≤ each parent E |
| `ker_combine_R_monotone` | R_combined ≥ each parent R (risk monotonicity) |
| `ker_combine_provenance` | evidence_hex commits to parent IDs, evidence_hex values, K/E/R, and rule_id |
| `ker_combine_lane_safety` | PROD composites require PROD parents (no research leakage) |

---

## Ecosafety Planes and Weights

### PlaneWeightsShard2026v1

Defines authoritative weights and corridor bands for Lyapunov residual computation:

```
V_t = Σ_j w_j * r_j^2
```

| Plane | Weight | Non-offsettable | Soft Band | Hard Band | Uncertainty Cap |
|-------|--------|-----------------|-----------|-----------|-----------------|
| CARBON | 2.5 | true | 0.10 | 0.13 | 0.15 |
| BIODIVERSITY | 2.5 | true | 0.10 | 0.13 | 0.15 |
| RESTORATION | 2.0 | true | 0.10 | 0.13 | 0.20 |
| HYDROLOGYMAR | 1.8 | true | 0.10 | 0.13 | 0.20 |
| ENERGY | 1.0 | false | 0.10 | 0.20 | 0.30 |
| MATERIALS | 1.2 | false | 0.10 | 0.15 | 0.25 |
| DATAQUALITY | 1.0 | false | 0.05 | 0.10 | 0.30 |
| TOPOLOGY | 1.0 | false | 0.05 | 0.10 | 0.30 |

### Key Invariants

- **Non-negative weights**: All w_j ≥ 0
- **Non-offsettable strict caps**: uncertainty_cap ≤ 0.30 for non-offsettable planes
- **Soft < Hard**: soft_band < hard_band for all planes
- **Restoration alignment**: RESTORATION weight ≥ ENERGY weight

---

## Lane Governance

### Lane Thresholds (`SovereignPreDeployment2026v1`)

| Lane | K_min | E_min | R_max | r_calib_max | r_sigma_max |
|------|-------|-------|-------|-------------|-------------|
| RESEARCH | - | - | - | ≤ 0.50 | ≤ 0.50 |
| EXPPROD | ≥ 0.85 | ≥ 0.85 | ≤ 0.20 | ≤ 0.30 | ≤ 0.30 |
| PROD | ≥ 0.90 | ≥ 0.90 | ≤ 0.13 | ≤ 0.25 | ≤ 0.25 |

### Promotion/Downgrade Rules (`LaneGovernanceTopology2026v1`)

- **Promotion**: Require r_topology ≤ SAFE band for 4 consecutive windows
- **Downgrade**: Trigger when r_topology ≥ HARD band for 2 consecutive windows

### Additional Bars

- **Weaponization bar**: r_weaponization ≤ 0.15 && r_topology ≤ 0.20
- **Non-actuation**: nonactuating_only == 1 for production lanes
- **AI non-voting**: AI-chat DIDs cannot alone satisfy HARD governance class

---

## Identity and Neurorights

### DID Domain Separation (`ALE-IDENTITY-DID-DOMAIN-POLICY-001`)

Privacy domains enforced:
- HEALTH_DATA
- LABOR_DATA
- ECOSAFETY_TELEMETRY
- WATER_INFRASTRUCTURE
- CITY_BEHAVIORAL
- GOVERNANCE_METADATA
- TESTING_SANDBOX

**Key invariant**: A DID may have at most one primary privacy domain. Cross-domain linkage requires aliased DIDs.

### Neurorights Guarantees

- **No identity classification by health data**: HealthDataTCR cannot classify personhood
- **No downgrade for non-disclosure**: Refusal to provide data must not reduce Karma or access
- **Data as labor dignity**: Absence of labor must not reduce neurorights or baseline access

---

## Health Data and Labor

### HealthDataTCR2026v1

Defines curation, challenges, and quadratic funding for health datasets:

- **Dataset categories**: WEARABLE_VITALS, EEG_NEURO, EHR_SUMMARY, LAB_RESULTS_AGGREGATE, ENV_EXPOSURE, CLIMATE_HEALTH_LINKED
- **Labor sources**: HEALTH_DEVICE, CLINIC_EXPORT, ENV_SENSOR, AI_CHAT_SUMMARY, MANUAL_ENTRY
- **Curation statuses**: PENDING, ACCEPTED, REJECTED, SLASHED

### Quadratic Funding

```aln
function computeQFWeight(contributions) -> HealthQFWeights {
  sqrt_sum = Σ sqrt((boot + ecocredit) * (0.5 + 0.5 * responsibility_scalar))
  qf_weight = sqrt_sum^2
}
```

### Gas Discount Credits

Accepted datasets earn non-transferable gas discount credits:
- `gas_discount_bps`: basis points (0-10000)
- `non_transferable`: true (cannot be sold or transferred)

---

## Integration Guidelines

### Rust / SQLite Side

- **Rust crates** ingest `KERParticle2026v1` rows (from CSV shards or SQLite views)
- Implement `ker_oplus_geom_min_max` as a pure function consistent with ALN spec
- Composite KER rows must:
  - Use canonical member ordering for IDs (`members` field)
  - Recompute `evidence_hex` exactly as defined
  - Store `signing_hex` from secure enclave or TEE-bound signer

### CI / Governance

CI jobs load ALN specs to:
- Verify K, E, R invariants for new/modified composite rows
- Reject builds where R_combined exceeds corridor cap θ while parents claim safety
- Ensure `evidence_hex` matches parent hashes and composition rule
- Enforce lane admissibility (PROD requires k≥target, e≥target, r≤target)
- Validate non-compensation for non-offsettable planes (carbon, biodiversity)

### Lyapunov Residual Computation

```
V_t = Σ_j w_j * r_j^2
residualscore = min(1.0, V_t / V_ref)
```

Where V_ref is the contract-specific GOLD/HARD pivot from PlaneWeightsShard2026v1.

---

## Naming and Discoverability

### File Naming Conventions

| Pattern | Description |
|---------|-------------|
| `KER*.aln` | KER-related particles and invariants |
| `EcoCore*.aln` | Corridor and residual envelopes |
| `EcoWealth*.aln` | Eco-wealth kernels and policies |
| `KnowledgeFactor*.aln` | Knowledge kernels for K factors |
| `*Policy*.aln` | Policy specifications |
| `*Plane*.aln` | Plane definitions (data quality, topology, etc.) |
| `*Registry*.aln` | Registry and index specifications |

### Version Suffixes

All ALN specs use explicit version suffixes (e.g., `2026v1`) rather than modifying existing specs in place. This maintains backward compatibility by:
- Keeping old `rule_id` values stable
- Introducing new rules via new `rule_id`s and composition functions

---

## Extension Guidelines

When extending this directory:

1. **Add new ALN files** with explicit `version` suffixes (e.g., `KERComposition2030v1.aln`)
2. **Maintain backward compatibility**:
   - Keep old `rule_id` values stable
   - Introduce new rules via new `rule_id`s
3. **Update CI configurations** to:
   - Recognize new composition rules
   - Enforce invariants for all active versions
4. **Register new particles** in `registry/aln_particle_registry.aln`
5. **Mirror to SQL/Rust** as specified in `DefinitionRegistry2026v1.aln`

---

## Purpose Summary

These ALN specs provide a **formally defined, machine-checkable framework** for:

- **Risk monotonicity** — Risk of harm can never be reduced by composition
- **Non-compensation** — Eco-impact and knowledge cannot be inflated by combining strong and weak evidence
- **Provenance** — Every composite particle is hex-stamped and DID-signed
- **Lane governance** — Strict thresholds for RESEARCH → EXPPROD → PROD promotion
- **Neurorights protection** — Identity separation, no classification by health data
- **Data-as-labor** — Health and AI-chat data contributions earn non-transferable credits

The invariants ensure that any attempt to produce inconsistent or unsafe compositions fails validation at ALN-level and in CI.

---

## References

- **Prometheus-Praxis Core**: `PrometheusPraxisCore.v1.aln`
- **EcoNet Schema**: `EcoNetSchemaShard2026v1`
- **Plane Weights**: `PlaneWeightsShard2026v1`
- **Eco Repo Index**: `EcoRepoIndex2026v1`
- **Owner DID**: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`

---

*This directory is the canonical, repo-local source of truth for how KER evidence is structured, combined, and constrained across the Prometheus-Praxis ecosafety ecosystem.*
