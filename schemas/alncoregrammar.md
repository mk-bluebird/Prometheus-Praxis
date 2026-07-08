# ALN Core Grammar Specification (2026v1)

This document defines the canonical grammar for ALN (Artifact Linkage Notation) files used in EcoNet governance.

## Lexical Rules

### Comments
- Line comments start with `#` or `--` and extend to end of line
- Empty lines are ignored

### Block Headers
- Record blocks: `record <Name>` ... `endrecord` or `end`
- Section blocks: `section <name>` ... `endsection` or `end`
- Particle blocks: `particle <Name>` ... `end`

### Key-Value Syntax
- Inside record blocks: `<key> <value>`
- Keys are case-insensitive
- Values can be:
  - Strings: unquoted text until whitespace or comment
  - Float32/Float64: numeric literals (e.g., `0.5`, `1e-9`)
  - Bool: `true`/`false`, `yes`/`no`, `1`/`0`
  - DateTime: ISO 8601 format (e.g., `2026-05-12T00:00:00Z`)

### Numeric Formats
- Integers: `[+-]?[0-9]+`
- Floats: `[+-]?[0-9]*\.?[0-9]+([eE][+-]?[0-9]+)?`
- Range validation is performed during semantic analysis

## Block Definitions

### AlnDocument2026v1
Top-level document metadata:
- `docid` / `doc_id`: string - Unique document identifier
- `schemaname` / `schema_name`: string - Schema reference
- `versiontag` / `version_tag`: string - Version tag (e.g., "2026v1")
- `category`: string - Document category
- `region`: string (optional) - Geographic region code
- `roleband` / `role_band`: string - Role band (RESEARCH, EXPPROD, PROD, GOVERNANCE)
- `ownerdid` / `owner_did`: string - Owner's Bostrom DID
- `evidencehex` / `evidence_hex`: string - Hex evidence pointer
- `signinghex` / `signing_hex`: string - Hex signing pointer
- `parentevidencehex` / `parent_evidence_hex`: string (optional) - Parent evidence

### SafeStepRule2026v1
Lyapunov-based safety step rule:
- `ruleid` / `rule_id`: string - Unique rule identifier
- `description`: string - Human-readable description
- `vtceiling` / `vt_ceiling`: float64 (optional) - Maximum Vt ceiling
- `epsilon`: float64 - Maximum allowed step increase (must be >= 0)
- `lyapchannel` / `lyap_channel`: string - Lyapunov channel name

**Invariants:**
- INV-SAFESTEP-1: epsilon >= 0
- INV-SAFESTEP-2: V_{t+1} <= V_t + epsilon

### DeployDecisionKernel2026v1
KER-based deployment decision kernel:
- `kernelid` / `kernel_id`: string - Unique kernel identifier
- `description`: string - Human-readable description
- `kmin` / `k_min`: float32 [0,1] - Minimum knowledge threshold
- `emin` / `e_min`: float32 [0,1] - Minimum eco-impact threshold
- `rmax` / `r_max`: float32 [0,1] - Maximum risk ceiling
- `lanescope` / `lane_scope`: string - Lane scope (RESEARCH, EXP, SIM, PROD, EXPPROD, ALL)

**Invariants:**
- INV-KER-1: k_min in [0, 1]
- INV-KER-2: e_min in [0, 1]
- INV-KER-3: r_max in [0, 1]

### KerSnapshot2026v1
KER state snapshot:
- `k`: float32 [0,1] - Knowledge metric
- `e`: float32 [0,1] - Eco-impact metric
- `r`: float32 [0,1] - Risk metric
- `vt`: float32 - Lyapunov residual
- `lane`: enum (RESEARCH, EXP, SIM, PROD)
- `completeness`: enum (MEASURED, SIMULATED, MIXED, CONCEPTUAL)
- `is_speculative`: bool - Whether this is a speculative snapshot

### RepoManifest2026v1
Repository manifest for EcoRestorationShard binding:
- `reponame` / `repo_name`: string - Repository name
- `githubslug` / `github_slug`: string - GitHub organization/repo
- `roleband` / `role_band`: string - Role band
- `lanedefault` / `lane_default`: string - Default lane
- `ecosafetybinding` / `ecosafety_binding`: string - Ecosafety binding reference
- `shardprotocol` / `shard_protocol`: string - Shard protocol version
- `kertargetk` / `ker_target_k`: float64 - Target K value
- `kertargete` / `ker_target_e`: float64 - Target E value
- `kertargetr` / `ker_target_r`: float64 - Target R bound
- `nonactuatingonly` / `non_actuating_only`: bool - Non-actuating flag
- `ownerdid` / `owner_did`: string - Owner DID
- `evidencehex` / `evidence_hex`: string - Evidence hex
- `signinghex` / `signing_hex`: string - Signing hex

**Invariants:**
- INV-REPO-1: For ecorestorationshard, roleband == "RESEARCH" AND non_actuating_only == true
- INV-REPO-2: owner_did must be in allowed Bostrom DID set

### OverridePolicy2026v1
Override policy for emergency situations:
- `policyid` / `policy_id`: string - Unique policy identifier
- `description`: string - Human-readable description
- `allowtightenonly` / `allow_tighten_only`: bool - Allow tightening only
- `allowemergencyderate` / `allow_emergency_derate`: bool - Allow emergency derating
- `forbidsafetyloosen` / `forbid_safety_loosen`: bool - Forbid safety loosening (must be true)

**Invariants:**
- INV-OVERRIDE-1: forbid_safety_loosen must be true

## Document-Level Invariants

### INV-ALNDOC-1
For non-RESEARCH roleband documents:
- `evidence_hex` must be non-empty
- `signing_hex` must be non-empty

### INV-SAFESTEP-2 (Evaluation Rule)
A step is valid if:
```
V_{t+1} <= V_t + epsilon
```
And optionally:
```
V_{t+1} <= vt_ceiling (if defined)
```

### INV-DEPLOY (Evaluation Rule)
A deployment is admissible if:
```
k_clamped >= k_min AND
e_clamped >= e_min AND
r_clamped <= r_max
```
Where clamping ensures values are in [0, 1].

## Lane Scope Matching

Kernels apply to snapshots based on lane scope:
- RESEARCH scope matches only RESEARCH lane
- EXP scope matches EXP lane
- SIM scope matches SIM lane
- PROD scope matches PROD lane
- EXPPROD scope matches both EXP and PROD lanes
- ALL scope (or empty) matches all lanes

## Usage Examples

See `crates/alncore/examples/` for working examples:
- `parse_ecosafety.rs` - Parse ecosafety ALN files
- `parse_phoenix_tray.rs` - Parse Phoenix tray pilot ALN
- `parse_cyboquatic_dao_deploy.rs` - Parse Cyboquatic DAO deploy ALN

## Cross-Language Conformance

The canonical JSON representation (via `to_canonical_json()`) serves as the ground truth for cross-language parser equivalence testing. See `.github/workflows/ci-alncore.yml` for CI configuration.
