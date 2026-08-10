# Prometheus-Praxis Repository Baseline Audit

## Audit Metadata

- **Audit Date**: 2026-08-10
- **UTC Timestamp**: Generated during audit session
- **Commit Identifier**: `6a314edbf247606dcee3661d5a87585068754528`
- **Commit Date**: 2026-08-10T09:36:10-07:00
- **Commit Message**: Update workload_corridor_2026_08_09.aln
- **Branch**: qwen-code-37ee4cc1-98d2-45b4-86f1-53f811930b6d
- **Repository Status**: Clean (no uncommitted changes at audit time)

## File Inventory Summary

Total files inventoried: 2591 (excluding `.git/`, `target/`, `**/build/`, `**/.gradle/`)

### File Extension Counts

| Extension | Count |
|-----------|-------|
| rs (Rust) | 588 |
| cpp (C++) | 395 |
| sql (SQL) | 404 |
| aln (ALN particles) | 258 |
| aln2 | 16 |
| kt (Kotlin) | 76 |
| java | 45 |
| lua | 75 |
| md (Markdown) | 182 |
| toml | 166 |
| yaml/yml | 41 |
| json | 30 |
| csv | 36 |
| h/hpp (C/C++ headers) | 91 |
| js | 49 |
| py | 34 |
| go | 5 |
| proto | 18 |
| tla | 1 |
| bnf/ebnf | 6 |
| Other | See `file-extension-counts.txt` |

## Top-Level Directories Present

The following top-level directories exist in this checkout (non-exhaustive, excluding build artifacts):

- `.ai-chat/` - AI chat context and analysis tools
- `.econet/` - Econet indexing schemas
- `.githooks/` - Git hooks
- `.github/` - GitHub Actions workflows
- `.platform/` - Platform configuration
- `Data_Lake/` - Data lake schemas and indices
- `Eco-Fort/`, `EcoNet/` - Eco-system components
- `aletheion/`, `aletheion_compliance/`, `aletheion_erm/`, `aletheion_governance/`, `aletheion_infra/`, `aletheion_research/`, `aletheion_rm/` - Aletheion subsystem directories
- `aln/`, `aln-specs/`, `aln-platform-ecosystem/`, `aln_v2/` - ALN grammar and specification files
- `android/`, `androidapp/` - Android application code
- `api/`, `app/` - Application entry points
- `automation/`, `bee-corridor-evidence/` - Automation and evidence
- `c_kernel/`, `cpp/` - C and C++ implementation
- `city_os/`, `communications/`, `config/`, `contracts/`, `core/`, `core_aln/`
- `corridor-econet-blastradius/`, `crates/` - Rust crate workspace
- `cybercore/`, `cyboquatic_progress/`, `cyboquatic_workload_*` - Cyboquatic workload implementations
- `data/`, `data-fragments/`, `dataconstellation/`, `db/` - Data and database schemas
- `diagrams/`, `doc/`, `docker/`, `docs/` - Documentation
- `eco/`, `eco-fort/`, `eco_restoration_shard/`, `eco_wealth/`, `ecofort/`, `ecological-orchestrator/`, `econet*`, `ecosafety*`, `ecoshard/`
- `edge/`, `education-prompts/`, `eval/`, `examples/`, `firmware/`, `fog-*`, `fragments/`, `fw/`
- `gateway/`, `governance/`, `health/`, `include/`, `infra/`, `java/`, `ker/`, `kernels/`, `knowledge/`, `kotlin/`
- `lane/`, `lua/`, `mk-bluebird/`, `nonactuating/`, `nonactuating-processor/`, `output/`, `particles/`, `plane/`, `policies/`, `policy/`
- `ppx_minimal_identity_kernel/`, `praxiskernel/`, `prometheus-praxis/`, `prometheus-role-bands-processor/`, `proto/`, `public/`, `python/`
- `qpudatashards/`, `research/`, `roh-kernel-mt6883/`, `runtime/`, `runtimelua/`, `rust/`, `scenarios/`, `schema/`, `schemas/`, `scripts/`, `services/`, `shards/`, `spec/`, `specs/`, `spine/`, `sql/`, `sqlspine/`, `src/`, `swarmnet/`, `tee/`, `tests/`, `titan-net/`, `tla/`, `tools/`, `workflows/`, `workspacePrometheus-Praxis/`, `x/`

## Workspace Manifest Structure

Root `Cargo.toml` declares:
- **Workspace Edition**: 2024
- **Rust Version**: 1.85
- **License**: MIT OR Apache-2.0
- **Resolver**: 2
- **Members**: 120+ crates under `crates/`, `eco_restoration_shard/`, `eco_wealth/`, `ecorestorationshard/`, `contracts/`, `prometheus-praxis/`, `src/urban_climate/evaluation/`, `eval/`, `health/`, `city_os/phoenix/heat_island/`, and `cpp/EcoNetCentralAZ/`

## Statement on Excluded Directories

The following directories were explicitly excluded from the file inventory as they represent generated build output or dependency caches:
- `.git/` - Git internals
- `target/` - Cargo build output
- `**/build/` - Generic build directories
- `**/.gradle/` - Gradle build cache

These exclusions ensure the inventory reflects only source-controlled, human-authored artifacts.

---

*This baseline was generated using only shell tools (`find`, `awk`, `sort`, `git`) without invoking any Rust build tooling.*
