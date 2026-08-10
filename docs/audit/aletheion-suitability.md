# Aletheion Suitability Assessment

## Audit Date: 2026-08-10

This document assesses the public-presentability of Aletheion-related content found in the repository checkout. Assessment is based solely on evidence from actual files, not assumptions.

---

## Inventory Summary

Seven Aletheion-named directories exist at the root level:

| Directory | File Count (approx.) | Content Types |
|-----------|---------------------|---------------|
| `aletheion/` | ~17 files | ALN particles, Rust sources, test vectors |
| `aletheion_compliance/` | Minimal | Core subdirectory |
| `aletheion_erm/` | Minimal | Ecosafety subdirectory |
| `aletheion_governance/` | Minimal | Smartchain subdirectory |
| `aletheion_infra/` | Minimal | Corridors subdirectory |
| `aletheion_research/` | Minimal | Eco subdirectory |
| `aletheion_rm/` | Minimal | Materials subdirectory |

Total Aletheion-related paths identified: 7 directories (see `aletheion-path-inventory.txt`)

---

## Detailed Assessment by Directory

### 1. `aletheion/` - Core Aletheion Subsystems

**Exact relative path:** `aletheion/`

**Subdirectories:**
- `eco-machines/sunflower/` - Contains `ALE-ERM-SUNFLOWER-PROFILE-2026V1.aln`
- `erm/` - ERM (Eco Restoration Management) with `qf/`, `funding/`, `health_tcr/`
- `health/` - Healthcare risk planes with SQL schema
- `identity/` - Data labor and identity classification
- `research/` - Research agendas for eco and health augmentation

**Current purpose (from file headers and content):**
- ALN namespace `ALE.ERM.QF.ATTENUATION.V1` defines quadratic funding attenuation policies
- Rust code implements domain attestation, QF domain parameters, health TCR logic
- Test vectors in JSON format for Health TCR 2026v1
- ALN particle `HealthcareRiskPlane2026v1.aln` defines healthcare risk governance
- Identity work includes "no identity classification" policy (PPX-NO-IDENTITY-CLASSIFICATION-001.aln)

**Content categories:**
- [x] Internal implementation (Rust sources under `erm/funding/src/`, `erm/health_tcr/src/`, `identity/ppx_no_identity_classification/src/`)
- [x] Governance definitions (ALN policy files)
- [x] Test fixtures (`test_vectors/HEALTH-TCR-2026V1-001.json`, `test_seed.rs`, `prop_health_credits.rs`)
- [ ] Public-facing material (no standalone README or user documentation found)
- [ ] Experimental research (research agenda ALN files present)
- [ ] Generated output (none detected)
- [ ] Credentials/telemetry examples (one `fpic_token_id` reference in test vector - not a real credential)
- [ ] Governance definitions (yes, ALN policies)

**Public-presentability: CONDITIONAL**

**Required preparation before public presentation:**
1. Add README.md to `aletheion/` explaining scope and relationship to Prometheus-Praxis
2. Review test vector `HEALTH-TCR-2026V1-001.json` to confirm `fpic_token_id` is clearly fictional/test data
3. Document the ALN namespace convention (`ALE.ERM.*`) for external contributors
4. Clarify whether `erm/`, `health/`, `identity/` are stable APIs or internal implementation
5. Ensure all Rust modules have crate-level documentation comments

**Evidence notes:**
- No localhost/device identifiers found
- No private paths exposed
- No claims of live deployment
- License inherited from workspace (MIT OR Apache-2.0) but not restated locally

---

### 2. `aletheion_compliance/`

**Exact relative path:** `aletheion_compliance/`

**Contents:** Single subdirectory `core/` (contents not enumerated in detail)

**Current purpose:** Unclear from directory structure alone; name suggests compliance logic

**Public-presentability: NO**

**Required preparation:**
1. Populate with actual compliance definitions or merge into `aletheion/governance/`
2. Add README explaining purpose
3. If empty placeholder, consider removing to reduce confusion

---

### 3. `aletheion_erm/`

**Exact relative path:** `aletheion_erm/`

**Contents:** Single subdirectory `ecosafety/`

**Current purpose:** Unclear; likely ERM ecosafety policies but no files visible at top level

**Public-presentability: NO**

**Required preparation:**
1. Populate with ecosafety ALN particles or merge into `aletheion/erm/`
2. Add README or remove if redundant

---

### 4. `aletheion_governance/`

**Exact relative path:** `aletheion_governance/`

**Contents:** Single subdirectory `smartchain/`

**Current purpose:** Unclear; likely smart contract governance but no files visible

**Public-presentability: NO**

**Required preparation:**
1. Populate with governance ALN or smart contract code
2. Add README explaining smartchain concept
3. Consider consolidating with `aletheion/` governance work

---

### 5. `aletheion_infra/`

**Exact relative path:** `aletheion_infra/`

**Contents:** Single subdirectory `corridors/`

**Current purpose:** Unclear; likely infrastructure corridor definitions

**Public-presentability: NO**

**Required preparation:**
1. Populate with corridor ALN or infrastructure specs
2. Add README
3. Consider consolidating with existing corridor work in `crates/` or `aln/`

---

### 6. `aletheion_research/`

**Exact relative path:** `aletheion_research/`

**Contents:** Single subdirectory `eco/`

**Current purpose:** Unclear; `aletheion/research/eco/` already contains research agenda ALN

**Public-presentability: NO**

**Required preparation:**
1. Determine if this duplicates `aletheion/research/`
2. Populate or remove to avoid confusion
3. Add README if distinct purpose exists

---

### 7. `aletheion_rm/`

**Exact relative path:** `aletheion_rm/`

**Contents:** Single subdirectory `materials/`

**Current purpose:** Unclear; possibly risk materials or restoration materials

**Public-presentability: NO**

**Required preparation:**
1. Populate with materials science ALN or documentation
2. Add README explaining "rm" abbreviation
3. Consider clearer naming (e.g., `aletheion_materials/`)

---

## Cross-Cutting Findings

### Naming Inconsistency

Two parallel structures exist:
- `aletheion/` with subdirs `erm/`, `health/`, `identity/`, `research/`, `eco-machines/`
- Separate roots: `aletheion_compliance/`, `aletheion_erm/`, `aletheion_governance/`, `aletheion_infra/`, `aletheion_research/`, `aletheion_rm/`

**Issue:** This creates ambiguity. Is `aletheion_erm/` different from `aletheion/erm/`? Contributors may be confused.

**Recommendation:** Consolidate into single `aletheion/` tree or clearly document distinction.

### Documentation Gaps

None of the seven Aletheion directories contain:
- Top-level README.md explaining Aletheion's role
- CONTRIBUTING.md for Aletheion-specific contribution guidance
- Clear boundary documentation between Aletheion and Prometheus-Praxis proper

### Credential/Privacy Scan

Scan of `aletheion/` directory found:
- One `fpic_token_id` in test vector - appears to be fictional identifier (format: "FPIC-TOKEN-001")
- No localhost, device IDs, private paths, or real credentials detected
- No deployment claims found

### License Status

- Aletheion content inherits workspace license (MIT OR Apache-2.0) from root `Cargo.toml`
- No local LICENSE files in Aletheion directories
- ALN files do not declare license explicitly

---

## Public-Presentability Gates Assessment

| Gate | Status | Notes |
|------|--------|-------|
| Clear scope and non-fictional purpose | PARTIAL | `aletheion/` has clear purpose; six satellite directories do not |
| No credentials, private paths, local device identifiers | VERIFIED | No sensitive data found in scan |
| No unredacted personal data | VERIFIED | No personal data detected |
| No claims of live deployment without evidence | VERIFIED | No deployment claims found |
| Clear safety boundary (diagnostic vs actuation) | PARTIAL | ALN files imply governance role but actuation status not explicit |
| License and contribution expectations discoverable | PARTIAL | Workspace license applies but not restated; no CONTRIBUTING.md |
| Architecture diagrams/terminology consistent with repo structure | PARTIAL | Naming inconsistency between `aletheion/` and `aletheion_*/` |
| Aletheion subordinate to Prometheus-Praxis identity | VERIFIED | All content under Prometheus-Praxis root; no separate repo implied |

---

## Recommendations

### Immediate Actions (Before Public Presentation)

1. **Consolidate or clarify structure:**
   - Either merge `aletheion_compliance/`, `aletheion_erm/`, etc. into `aletheion/` as subdirectories
   - Or add README to each explaining why they are separate

2. **Add top-level documentation:**
   - Create `aletheion/README.md` explaining:
     - What Aletheion is (compliance/governance subsystem)
     - Relationship to Prometheus-Praxis
     - Which directories are stable vs experimental

3. **Review test data:**
   - Confirm all token IDs, node names, and identifiers in test vectors are clearly fictional
   - Add comment headers to JSON test files stating "TEST DATA ONLY - NOT FOR PRODUCTION USE"

4. **Document safety boundaries:**
   - Add explicit statement to ALN files: "This ALN particle is NON-ACTUATING / DIAGNOSTIC ONLY" or "ACTUATION-CAPABLE - requires steward approval"

### Longer-Term Improvements

5. **License clarity:**
   - Add LICENSE file to `aletheion/` or reference root license in README
   - Consider adding license headers to ALN files

6. **Contributor guidance:**
   - Add `aletheion/CONTRIBUTING.md` with ALN authoring conventions
   - Document namespace conventions (`ALE.*`)

7. **Architecture documentation:**
   - Create diagram showing Aletheion's relationship to:
     - Prometheus-Praxis core
     - ALN grammar
     - SQL enforcement layer
     - Multi-language interfaces

---

## Final Presentability Verdict

| Directory | Present Now? | Effort Level | Priority |
|-----------|-------------|--------------|----------|
| `aletheion/` | CONDITIONAL | Low (add README, clarify structure) | HIGH |
| `aletheion_compliance/` | NO | Medium (populate or remove) | LOW |
| `aletheion_erm/` | NO | Medium (populate or merge) | LOW |
| `aletheion_governance/` | NO | Medium (populate or merge) | LOW |
| `aletheion_infra/` | NO | Medium (populate or merge) | LOW |
| `aletheion_research/` | NO | Medium (populate or merge) | LOW |
| `aletheion_rm/` | NO | Medium (populate or rename) | LOW |

**Overall recommendation:** Focus on `aletheion/` core directory for initial public presentation. Address satellite directories only if they contain unique, non-duplicated content worth preserving separately.

---

## Statement of Limitations

This assessment was conducted using:
- File inventory from `find` command
- Content sampling via `head` and `grep`
- No execution of Rust, ALN parsers, or build tools
- No assumptions about unexamined files

A full presentability review should include:
- Complete reading of all ALN particles
- Review of all Rust source files for documentation quality
- Verification that no additional sensitive data exists in deeper directories
- Legal review of license compatibility if Aletheion is presented separately

---

*Assessment generated from repository evidence without modifying production files.*
