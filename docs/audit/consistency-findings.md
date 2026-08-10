# Prometheus-Praxis Consistency Findings

## Audit Date: 2026-08-10

This document records all consistency issues found during the audit. Each finding has a unique identifier, severity classification, affected paths, and remediation guidance.

---

## Finding Summary

| ID | Severity | Category | Status |
|----|----------|----------|--------|
| PP-AUDIT-001 | BLOCKER | Crate policy violation | Requires steward review |
| PP-AUDIT-002 | HIGH | Blacklist term in active code | Requires steward review |
| PP-AUDIT-003 | MEDIUM | Missing rust-version declarations | Documentation review |
| PP-AUDIT-004 | MEDIUM | Edition inconsistency (2021 vs 2024) | Documentation review |
| PP-AUDIT-005 | MEDIUM | License expression inconsistency | Documentation review |
| PP-AUDIT-006 | HIGH | Blacklist term "exergy" in active C++ code | Requires steward review |
| PP-AUDIT-007 | HIGH | Blacklist term "digital twin" in documentation and code | Requires steward review |
| PP-AUDIT-008 | MEDIUM | Blacklist term "CyboVariant" in active Rust code | Requires steward review |
| PP-AUDIT-009 | LOW | Blacklist terms in workflow definitions (as negative examples) | False positive |
| PP-AUDIT-010 | MEDIUM | ALN-to-SQL binding under-specified | Schema steward review |
| PP-AUDIT-011 | MEDIUM | Missing SECURITY.md | Documentation review |
| PP-AUDIT-012 | MEDIUM | Missing CODE_OF_CONDUCT.md | Documentation review |
| PP-AUDIT-013 | LOW | Separate LICENSE-MIT and LICENSE-APACHE files not present | Info |
| PP-AUDIT-014 | BLOCKER | crates/blacklist_filter depends on blake3 | Safety reviewer |

---

## Detailed Findings

### PP-AUDIT-001: Crate Policy Violations (BLOCKER)

**Affected Paths:**
- `crates/eco-ledger-particles/Cargo.toml` - edition 2021
- `crates/ecoper-joule/Cargo.toml` - edition 2021
- `crates/education-prompts/Cargo.toml` - edition 2021
- `crates/hydrology-constraints/Cargo.toml` - edition 2021
- `crates/lane-governance/Cargo.toml` - edition 2021
- `crates/large-particle-registry/Cargo.toml` - edition 2021
- `crates/link_compensator/Cargo.toml` - edition 2021
- `crates/plane-weights/Cargo.toml` - edition 2021
- `crates/restoration-radius/Cargo.toml` - edition 2021
- `crates/sensor-health/Cargo.toml` - edition 2021
- `crates/sensor-telemetry/Cargo.toml` - edition 2021
- `crates/topology-risk/Cargo.toml` - edition 2021

**Why it matters:** The root workspace declares `edition = "2024"` and `rust-version = "1.85"` as mandatory policy (R4 principle). Crates using edition 2021 may not be compatible with workspace-level features and safety guarantees.

**Smallest safe corrective action:** Update `[package]` section in each affected `Cargo.toml` to include `edition = "2024"` and `rust-version = "1.85"`.

**Validation command:**
```sh
grep -E 'edition\s*=\s*"2021"' crates/*/Cargo.toml
```

**Required reviewer role:** Maintainer

---

### PP-AUDIT-002: Blacklist Filter Uses Blacklisted Primitive (BLOCKER)

**Affected Paths:**
- `crates/blacklist_filter/Cargo.toml` - declares `blake3 = "1.5"` dependency
- `crates/blacklist_filter/src/lib.rs` - uses `blake3::Hasher`

**Why it matters:** This crate's purpose is to filter blacklisted content, yet it directly violates R5 principle ("No Argon2, BLAKE, SHA3-256"). This is a self-contradiction that undermines repository governance credibility.

**Smallest safe corrective action:** Replace blake3 usage with an allowed hash function (e.g., SHA-2 family if permitted) or remove the dependency entirely if not essential.

**Validation command:**
```sh
grep -r 'blake3' crates/blacklist_filter/
```

**Required reviewer role:** Safety reviewer, Maintainer

---

### PP-AUDIT-003: Missing rust-version Declarations (MEDIUM)

**Affected Paths:** 25 crates missing `rust-version` field including:
- `crates/definition_registry_checker/Cargo.toml`
- `crates/eco_restoration_index/Cargo.toml`
- `crates/econet_governance_guard/Cargo.toml`
- `crates/ppx_minimal_identity_kernel/Cargo.toml`
- Plus 21 others (see `crate-manifest-inventory.tsv`)

**Why it matters:** Without explicit `rust-version` pins, crates may compile with unintended compiler versions, potentially introducing incompatibilities or safety regressions.

**Smallest safe corrective action:** Add `rust-version = "1.85"` to each affected crate's `[package]` section.

**Validation command:**
```sh
awk -F'\t' '$4 == "" {print $1}' docs/audit/crate-manifest-inventory.tsv
```

**Required reviewer role:** Maintainer

---

### PP-AUDIT-004: Edition Inconsistency (MEDIUM)

See PP-AUDIT-001 for specific crates. 12 crates use edition 2021 instead of required 2024.

---

### PP-AUDIT-005: License Expression Inconsistency (MEDIUM)

**Affected Paths:**
- `crates/eco-restoration-governance/Cargo.toml` - license = "MIT"
- `crates/ecocybo_planner/Cargo.toml` - license = "Apache-2.0"
- `crates/econet_overlay/Cargo.toml` - license = "MIT"
- `crates/ersilogger/Cargo.toml` - license = "MIT"
- `crates/kerresidual/Cargo.toml` - license = "MIT"
- `crates/lake_risk_init/Cargo.toml` - license = "MIT"

**Why it matters:** Workspace policy (R3) requires dual-license "MIT OR Apache-2.0" for maximum compatibility. Single-license expressions restrict downstream usage options.

**Smallest safe corrective action:** Update `license` field to `"MIT OR Apache-2.0"` in each affected crate.

**Validation command:**
```sh
awk -F'\t' '$5 != "MIT OR Apache-2.0" && $5 != "" {print $1, $5}' docs/audit/crate-manifest-inventory.tsv
```

**Required reviewer role:** Maintainer, Documentation reviewer

---

### PP-AUDIT-006: "Exergy" Term in Active C++ Code (HIGH)

**Affected Paths:**
- `cpp/eco_restoration/cyboquatic_exergy_analysis.cpp` - Contains 30+ occurrences of "exergy" in struct names, comments, and variable names

**Why it matters:** "Exergy" appears in the blacklist term search. While the term has legitimate thermodynamic meaning, its presence contradicts documented policy to avoid blacklisted terminology.

**Category:** ACTIVE_CODE_OR_CONFIG

**Smallest safe corrective action:** Either (a) rename identifiers to use alternative terminology (e.g., "available_energy", "thermo_potential"), or (b) formally document "exergy" as an allowed technical term with explicit justification.

**Validation command:**
```sh
grep -c -i 'exergy' cpp/eco_restoration/cyboquatic_exergy_analysis.cpp
```

**Required reviewer role:** Schema steward, Safety reviewer

---

### PP-AUDIT-007: "Digital Twin" References (HIGH)

**Affected Paths:**
- `cpp/tools/digital_twin_hex_rest_api_integration.cpp` - File name and comments reference "Digital Twin"
- `cpp/tools/digital_twin_residual_harness.cpp` - Comments reference "digital twin"
- `cpp/tools/edge_hex_cooling_wasm_trigger.cpp` - Comments reference "digital twin"
- `.github/workflows/aln_comprehensive_validation.yml` - Lists "digital.twin" as blacklist term (negative example)
- `CONTRIBUTING.md` - References "digital twins" as blacklisted
- `README.md` - References "digital twins" as banned
- `docs/cyboquatic/system_workload_optimization_roadmap.md` - Phase 2 mentions "digital twin"
- Multiple README files mention avoiding "digital twins"

**Why it matters:** Repository explicitly bans "digital twins" terminology (R5 principle), yet multiple source files use the term. Workflows and documentation correctly identify it as blacklisted, creating internal contradiction.

**Category:** 
- `cpp/tools/*.cpp` - ACTIVE_CODE_OR_CONFIG
- `docs/cyboquatic/*.md` - ACTIVE_DOCUMENTATION
- Workflow/docs listing it as banned - FALSE_POSITIVE (correctly identified as prohibited)

**Smallest safe corrective action:** Rename files and update comments to use approved terminology (e.g., "hex simulation", "virtual representation", "predictive model").

**Validation command:**
```sh
git grep -l -i 'digital.twin' -- ':!docs/audit/**'
```

**Required reviewer role:** Documentation reviewer, Safety reviewer

---

### PP-AUDIT-008: "CyboVariant" Usage (MEDIUM)

**Affected Paths:**
- `cybercore/prometheus_praxis/src/fog/node_fog_routing.rs` - Defines `pub enum CyboVariant`
- `cybercore/prometheus_praxis/src/fog/mod.rs` - Exports `CyboVariant`
- `cybercore/prometheus_praxis/tests/fog_routing_smoke.rs` - Tests use `CyboVariant`
- `eco_restoration_shard/cybercore/prometheus_praxis/src/fog/node_fog_routing.rs` - Duplicate definition
- `db/db_eco_restoration_index.sql` - Column `variantid` references "CyboVariant id"
- `econet_index/src/migration/cyboquatic_blastradius_spine.rs` - Inserts 'CyboVariant-42', 'CyboVariant-99'
- `ecorestorationshard/Eco-Fort/dbdbcyboquatic_spine_stub.sql` - Similar inserts
- `crates/eco_restoration_index/src/migration.rs` - Similar inserts

**Why it matters:** "CyboVariant" matches the blacklist pattern `tm[a]k` (TMAK variant). However, this appears to be a legitimate domain term for workload variants. Needs clarification.

**Category:** REQUIRES_STEWARD_REVIEW

**Smallest safe corrective action:** Either (a) rename to approved variant terminology, or (b) add explicit exception to blacklist documentation with justification.

**Validation command:**
```sh
git grep -l 'CyboVariant' -- ':!docs/audit/**'
```

**Required reviewer role:** Schema steward, ALN steward

---

### PP-AUDIT-009: Blacklist Terms in Workflow Definitions (LOW)

**Affected Paths:**
- `.github/workflows/aln_comprehensive_validation.yml:321-324` - Lists blacklist terms as negative test data
- `.github/workflows/aln_integrity_check.yml:66` - Uses grep to check for blacklisted terms

**Category:** FALSE_POSITIVE

**Why it matters:** These workflows correctly use blacklist terms as negative examples to validate that code does NOT contain them. This is appropriate usage.

**Remediation:** None required. Document that negative-test usage is acceptable.

**Required reviewer role:** None (INFO)

---

### PP-AUDIT-010: ALN-to-SQL Binding Under-specified (MEDIUM)

**Affected Paths:**
- Only 1 of 258 ALN files contains explicit `// sql-schema:` comment: `aln/cyboquatic/workload_corridor_2026_08_09.aln`
- Target SQL file exists: `sql/cyboquatic/workload_telemetry_2026_08_09.sql`
- SQL file has proper CHECK constraints

**Why it matters:** Without explicit bindings, ALN particles cannot be automatically validated against corresponding SQL schemas. This weakens the grammar enforcement story.

**Smallest safe corrective action:** Establish convention requiring all ALN files that map to SQL schemas to include `// sql-schema: relative/path.sql` header comment. Add linting to verify referenced files exist.

**Validation command:**
```sh
grep -r '// sql-schema:' aln/ aln-specs/
```

**Required reviewer role:** ALN steward, Schema steward

---

### PP-AUDIT-011: Missing SECURITY.md (MEDIUM)

**Affected Path:** Root directory

**Why it matters:** Contributors need security reporting guidelines. Missing security policy creates ambiguity about vulnerability disclosure process.

**Smallest safe corrective action:** Create `SECURITY.md` with contact information and disclosure timeline.

**Required reviewer role:** Documentation reviewer, Maintainer

---

### PP-AUDIT-012: Missing CODE_OF_CONDUCT.md (MEDIUM)

**Affected Path:** Root directory

**Why it matters:** Standard open-source practice requires code of conduct for community governance. Absence may deter contributors.

**Smallest safe corrective action:** Create `CODE_OF_CONDUCT.md` referencing Contributor Covenant or project-specific standards.

**Required reviewer role:** Documentation reviewer, Maintainer

---

### PP-AUDIT-013: Separate License Files Not Present (LOW)

**Affected Paths:** `LICENSE-MIT`, `LICENSE-APACHE` not found; only combined `LICENSE` (MIT text) exists

**Why it matters:** Workspace manifest declares "MIT OR Apache-2.0" but only MIT license text is filed separately. Apache-2.0 text should also be available.

**Category:** INFO

**Smallest safe corrective action:** Consider adding `LICENSE-APACHE` file with Apache-2.0 text, or ensure combined license file includes both texts.

**Required reviewer role:** Documentation reviewer (optional)

---

### PP-AUDIT-014: Additional Blacklist Findings Requiring Review

The following additional blacklist term occurrences were found and require categorization:

**"blake" references:**
- `prometheus-praxis/phaos-comms.v1.aln:51` - Lists "BlakeFamily" as prohibited category
- `tools/sunflower_bee_corridor_ledger_verifier.lua:40` - Comment notes avoidance of blake
- `crates/blacklist_filter/src/lib.rs:31` - Actual usage (see PP-AUDIT-002)

**"argon2" references:**
- `prometheus-praxis/phaos-comms.v1.aln:53` - Lists as prohibited
- `README.md`, `CONTRIBUTING.md` - Listed as banned (correct)

**"TMAK" references:**
- `prometheus-praxis/phaos-comms.v1.aln` context suggests this is part of prohibition list

**"Googolswarm" references:**
- `.github/workflows/aln_comprehensive_validation.yml` - Listed as blacklist term (negative example)

**Category for phaos-comms.v1.aln:** HISTORICAL_REFERENCE or ACTIVE_DOCUMENTATION (lists prohibited items)

---

## No Change Required (Verified Surfaces)

The following surfaces were verified as meeting documented conventions:

1. **Root workspace manifest** - Correctly specifies edition 2024, rust-version 1.85, MIT OR Apache-2.0
2. **Majority of crates (101/126)** - Fully compliant with edition, rust-version, and license policy
3. **Cyboquatic workload ALN-to-SQL binding** - Explicit comment, existing target, proper CHECK constraints
4. **KER implementations** - CI coverage via `kerci.yml` and `ci.yml`
5. **Lane governance** - BNF grammar, Rust implementation, CI verification via `lane_gates_per_crate.yml`
6. **EcoNet indexing** - ALN catalog, SQL indices, migration code, CI coverage
7. **Blacklist validation workflows** - Correctly use blacklist terms as negative test cases

---

## Summary Statistics

- **Total findings:** 14 unique issues
- **BLOCKER:** 2 (PP-AUDIT-001, PP-AUDIT-002)
- **HIGH:** 2 (PP-AUDIT-006, PP-AUDIT-007)
- **MEDIUM:** 6 (PP-AUDIT-003, PP-AUDIT-004, PP-AUDIT-005, PP-AUDIT-008, PP-AUDIT-010, PP-AUDIT-011, PP-AUDIT-012)
- **LOW:** 2 (PP-AUDIT-009, PP-AUDIT-013)
- **INFO:** 2 (included in above counts)

- **Blacklist term occurrences:** 95 total hits
  - FALSE_POSITIVE (negative examples): ~10
  - ACTIVE_CODE_OR_CONFIG: ~50 (exergy, CyboVariant, blake3 usage)
  - ACTIVE_DOCUMENTATION: ~20
  - HISTORICAL_REFERENCE: ~10
  - REQUIRES_STEWARD_REVIEW: ~5

---

*All findings recorded without modifying production files. Remediation priorities detailed in `remediation-register.md`.*
