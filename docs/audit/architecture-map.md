# Prometheus-Praxis Architecture Map

## Audit Date: 2026-08-10

This document maps the actual directory structure and wiring found in the repository checkout. Only paths that exist in the inventory are documented.

---

## 1. Root Manifests and Workspace Structure

| Path | Status | Description |
|------|--------|-------------|
| `Cargo.toml` | VERIFIED | Root workspace manifest declaring 120+ crate members, edition 2024, rust-version 1.85, MIT OR Apache-2.0 license |
| `Cargo.lock` | Present in inventory | Dependency lock file |
| `rust-toolchain.toml` | NOT FOUND in inventory | Toolchain pinning not present at root |
| `README.md` | VERIFIED | Root documentation with scope, principles, and structure |
| `LICENSE` | VERIFIED | MIT License file present (Apache-2.0 text not separately filed) |
| `CONTRIBUTING.md` | VERIFIED | Contributor guidelines present |
| `SECURITY.md` | NOT FOUND | Security policy missing |
| `CODE_OF_CONDUCT.md` | NOT FOUND | Code of conduct missing |

---

## 2. Rust Implementation Surfaces

### 2.1 Crates Directory (`crates/`)

The `crates/` directory contains 126 Rust crates with individual `Cargo.toml` manifests. See `crate-manifest-inventory.tsv` for full details.

**Summary of crate policy compliance:**
- Edition 2024: 114/126 compliant; 12 crates use edition 2021
- rust-version 1.85: 101/126 compliant; 25 crates missing declaration
- License "MIT OR Apache-2.0": 120/126 compliant; 6 crates use single license (MIT or Apache-2.0 only)

### 2.2 Other Rust Sources

| Path | Status | Notes |
|------|--------|-------|
| `src/*.rs` | VERIFIED | Core Rust modules including `ker_residual.rs`, `rtopology.rs`, `shard_evidence_verify.rs` |
| `eco_restoration_shard/` | VERIFIED | Primary eco-restoration shard crate |
| `eco_wealth/` | VERIFIED | Standalone wealth tracking crate |
| `ecorestorationshard/` | VERIFIED | Index/harness crate with binaries |
| `prometheus-praxis/` | VERIFIED | Core praxis implementation |
| `cybercore/prometheus_praxis/` | VERIFIED | Alternative prometheus_praxis location |

---

## 3. Formal Grammar and Governance (ALN)

| Path | File Count | Description |
|------|------------|-------------|
| `aln/` | 258 `.aln` files | ALN particles for governance, workload, health, cyboquatic definitions |
| `aln-specs/` | ~10 `.aln` files | Specification-level ALN including CosmWasm invariants, sensor trust planes |
| `aln-platform-ecosystem/` | Present | Platform ecosystem ALN |
| `aln_v2/` | Present | ALN version 2 experiments |
| `src/lanes/lanes.bnf` | VERIFIED | BNF grammar for lane definitions |

**ALN-to-SQL Binding:**
- One explicit binding found: `aln/cyboquatic/workload_corridor_2026_08_09.aln` declares `// sql-schema: sql/cyboquatic/workload_telemetry_2026_08_09.sql`
- Target SQL file exists and contains CHECK constraints for KER metrics, fog_confidence, and acceptance gates

---

## 4. Data Schemas and Configuration

| Path | File Count | Description |
|------|------------|-------------|
| `sql/` | 404 `.sql` files | SQL schemas including blast_radius, PFAS Lyapunov, cyboquatic telemetry, healthcare risk planes |
| `db/` | 3 `.db` files | Database artifacts |
| `config/` | Present | Configuration directory |
| `swarmnet/coin_config.json` | VERIFIED | Coin configuration |
| `.platform/continuity_anchor.json` | VERIFIED | Platform continuity anchor |

---

## 5. Multi-Language Interfaces

### 5.1 C++ (`cpp/`)

| Subdirectory | File Count | Description |
|--------------|------------|-------------|
| `cpp/eco_restoration/` | Present | Includes `cyboquatic_exergy_analysis.cpp` (contains blacklisted term "exergy") |
| `cpp/tools/` | Present | Includes `digital_twin_hex_rest_api_integration.cpp`, `edge_hex_cooling_wasm_trigger.cpp` (contain "digital twin" references) |
| `cpp/EcoNetCentralAZ/` | In workspace | C++ crate in workspace |

### 5.2 Java/Kotlin

| Path | Status | Description |
|------|--------|-------------|
| `java/` | Present | Java sources |
| `kotlin/` | Present | Kotlin sources |
| `src/main/java/` | 45 `.java` files | Including `AiDatacenterNodeAgent.java`, `MT6883RiskAuditShardTool.java` |
| `src/main/kotlin/` | 76 `.kt` files | Including `AlnParticleProcessor.kt`, `AlnToSqlCompiler.kt`, `CybowCodec.kt` |
| `android/`, `androidapp/` | Present | Android application directories |
| `tee/kotlin/` | Present | TEE Kotlin client code |

### 5.3 Lua

| Path | File Count | Description |
|------|------------|-------------|
| `lua/` | Present | Lua sources |
| `src/lua/` | 75 `.lua` files total in repo | Including `cyboquatic_fogrouter_mainlanes.lua` |
| `tools/*.lua` | Multiple | Including `aln_linter_sidecar/aln_linter.lua`, `lane_lyapunov_timed_automaton.lua` |

### 5.4 Go

| Path | File Count | Description |
|------|------------|-------------|
| `x/` | 5 `.go` files | Cosmos SDK-style modules: `aimodel/`, `evolutionwindow/`, `research/`, `sabotage/` |

---

## 6. Automation and Verification

### 6.1 GitHub Workflows (`.github/workflows/`)

| Workflow | Purpose |
|----------|---------|
| `aln_comprehensive_validation.yml` | ALN validation (also lists blacklist terms) |
| `aln_integrity_check.yml` | ALN integrity checks |
| `aln_sql_conformance.yml` | ALN-SQL conformance testing |
| `check_definition_registry.yml` | Definition registry validation |
| `ci-alncore.yml` | ALN core CI |
| `ci-cyboquatic-ecosafety.yml` | Cyboquatic ecosafety CI |
| `ci.yml` | General CI |
| `cpp_eco_ci.yml` | C++ eco CI |
| `econet-governance-summary-ci.yml` | Econet governance summary |
| `health-tcr-ci.yml` | Health TCR CI |
| `identity-did-domain-preflight.yml` | Identity/DID preflight |
| `kerci.yml` | KER CI |
| `lane_gates_per_crate.yml` | Lane gate verification per crate |
| `prometheus-praxis-core-ci.yml` | Prometheus-Praxis core CI |
| `sovereignty-check.yml` | Sovereignty validation |
| `update_repo_lastupdatedutc.yml` | Repository metadata update |

### 6.2 Scripts and Tools

| Path | Description |
|------|-------------|
| `scripts/` | Script directory (contents not enumerated) |
| `tools/` | 34 Python scripts, 11 shell scripts, multiple Lua tools |
| `.githooks/pre-commit` | Git pre-commit hook |

---

## 7. Public Documentation

| Path | Status |
|------|--------|
| `docs/` | Present with subdirectories |
| `README.md` | VERIFIED |
| `CONTRIBUTING.md` | VERIFIED |
| `CONTRIBUTING_AI_CHAT.md` | Present |
| `ARCHITECTURE.md` | Present |
| `SECURITY.md` | NOT FOUND |
| `CODE_OF_CONDUCT.md` | NOT FOUND |
| `LICENSE-MIT` | NOT FOUND (only combined LICENSE present) |
| `LICENSE-APACHE` | NOT FOUND |

---

## 8. Wiring Findings Table

| Surface | Existing Path | Entry Point / Manifest | Connected Schema/Grammar/Config | Evidence of Test/CI Coverage | Finding Status |
|---------|---------------|------------------------|--------------------------------|------------------------------|----------------|
| Root workspace | `Cargo.toml` | `[workspace]` section | Members list 120+ crates | CI workflows reference crates | VERIFIED |
| ALN grammar | `aln/`, `aln-specs/` | `.aln` particle files | SQL via `// sql-schema:` comment in one file | `aln_sql_conformance.yml` workflow | PARTIAL (only 1 explicit binding found) |
| Cyboquatic workloads | `aln/cyboquatic/`, `sql/cyboquatic/` | `workload_corridor_2026_08_09.aln` | `workload_telemetry_2026_08_09.sql` with CHECK constraints | `ci-cyboquatic-ecosafety.yml` | VERIFIED |
| C++ eco-restoration | `cpp/eco_restoration/` | `cyboquatic_exergy_analysis.cpp` | References pump-aeration telemetry struct | `cpp_eco_ci.yml` | PARTIAL (contains blacklisted term "exergy") |
| Digital twin references | `cpp/tools/` | `digital_twin_hex_rest_api_integration.cpp` | No connected schema found | None detected | UNRESOLVED (term appears in blacklist) |
| KER implementations | `src/ker*.rs`, `crates/ker*/` | Multiple crate manifests | ALN `KERComposition2026v1.aln` | `kerci.yml`, `ci.yml` | VERIFIED |
| Lane governance | `src/lanes/`, `crates/lane-governance*` | `lanes.bnf`, `lane_gate.rs` | ALN lane definitions | `lane_gates_per_crate.yml` | VERIFIED |
| EcoNet indexing | `econet_index/`, `.econet/` | Migration Rust code, SQL indices | ALN `econet.agent.function.catalog.v1.aln` | `econet-governance-summary-ci.yml` | VERIFIED |
| TEE enclaves | `tee/` | `ker_enclave/enclave_ker.cpp`, EDL file | Kotlin client in `tee/kotlin/` | No dedicated CI workflow found | PARTIAL |
| Go modules (Cosmos) | `x/` | `msg_server_*.go` files | Config in `swarmnet/` | No Go-specific CI found | UNRESOLVED |
| Android app | `android/`, `androidapp/` | No manifest found in quick scan | Kotlin sources in `src/main/kotlin/` | No Android-specific CI found | UNRESOLVED |
| Blacklist filter | `crates/blacklist_filter/` | `Cargo.toml` with `blake3 = "1.5"` dependency | `src/lib.rs` uses `blake3::Hasher` | Contradicts R5 principle | BLOCKER (violates own policy) |
| CyboVariant usage | `cybercore/prometheus_praxis/src/fog/` | `node_fog_routing.rs` enum `CyboVariant` | SQL `variantid` column in `db/db_eco_restoration_index.sql` | Smoke test `fog_routing_smoke.rs` | VERIFIED (but term flagged in blacklist scan) |

---

## 9. Aletheion Subsystem Directories

Seven Aletheion-named directories exist at root level:

| Directory | Contents | Purpose (from file names) |
|-----------|----------|---------------------------|
| `aletheion/` | `eco-machines/`, `erm/`, `health/`, `identity/`, `research/` | Core Aletheion subsystems |
| `aletheion_compliance/` | `core/` | Compliance logic |
| `aletheion_erm/` | `ecosafety/` | ERM ecosafety |
| `aletheion_governance/` | `smartchain/` | Smart chain governance |
| `aletheion_infra/` | `corridors/` | Infrastructure corridors |
| `aletheion_research/` | `eco/` | Research eco |
| `aletheion_rm/` | `materials/` | Risk materials |

See `aletheion-suitability.md` for public-presentability assessment.

---

## 10. Key Observations

1. **Workspace scale**: 120+ crates with inconsistent policy adherence
2. **Multi-language complexity**: Rust (588), C++ (395), SQL (404), ALN (258), Kotlin (76), Lua (75), Java (45)
3. **ALN-to-SQL wiring**: Only one explicit `// sql-schema:` binding found despite 258 ALN files
4. **Blacklist contradictions**: `crates/blacklist_filter/` depends on `blake3`; `cpp/eco_restoration/cyboquatic_exergy_analysis.cpp` uses "exergy"; multiple files reference "digital twin"
5. **Missing contributor documentation**: `SECURITY.md`, `CODE_OF_CONDUCT.md`, separate `LICENSE-MIT`, `LICENSE-APACHE` not present

---

*Generated from repository inventory without running Cargo or making assumptions about untracked content.*
