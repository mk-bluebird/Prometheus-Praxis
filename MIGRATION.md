# EcoNet Constellation Repository Migration

## Migration Event

- **Date**: May 12, 2026, 5:15 PM MST
- **Old Account**: `Doctor0Evil` (suspended)
- **New Account**: `mk-bluebird`
- **Consolidated Repository**: `eco_restoration_shard`
- **URL**: https://github.com/mk-bluebird/eco_restoration_shard
- **Reason**: GitHub account suspension; all repositories consolidated into single monorepo

## Account Continuity

**Bostrom DIDs** (unchanged):
- Primary: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- Secure: `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

**ERC-20 Address**: `0x519fC0eB4111323Cac44b70e1aE31c30e405802D`

## Old Repository Mapping

All previous `Doctor0Evil/*` repositories are now consolidated into `mk-bluebird/eco_restoration_shard` with the following directory structure:

### **SPINE** (Governance, Grammar, KER Math)
- `Doctor0Evil/EcoNet` → `spine/econet/`
- `Doctor0Evil/aln-platform-ecosystem` → `spine/aln-platform/`
- `Doctor0Evil/ALN-Blockchain` → `spine/aln-blockchain/`
- `Doctor0Evil/Aletheion` → `spine/aletheion/`

### **ENGINE** (Cyboquatic Kernels, Industrial Machinery)
- `Doctor0Evil/EcoNet-CEIM-PhoenixWater` → `engines/ceim-phoenix-water/`
- `Doctor0Evil/Cyboquatics` → `engines/cyboquatics/`
- `Doctor0Evil/Eco-Sys` → `engines/eco-sys/`
- `Doctor0Evil/Sewer-FOG-Monitoring-Network` → `engines/fog-monitoring/`
- `Doctor0Evil/AirGlobeEcoKernel` → `engines/airglobe/`
- `Doctor0Evil/EcoNetHumanoidEcoCore` → `engines/humanoid-ecocore/`
- `Doctor0Evil/PhoenixMicroEcoNodesCEIM` → `engines/phoenix-micro-nodes/`
- `Doctor0Evil/PhoenixCorridorEcoHUDServer` → `engines/corridor-hud/`
- `Doctor0Evil/CEIM-EcoDeviceScore` → `engines/eco-device-score/`
- `Doctor0Evil/EcoNetPhoenix` → `engines/econet-phoenix/`
- `Doctor0Evil/Phoenix-AWP-Gila-EcoBridge` → `engines/gila-ecobridge/`

### **MATERIAL** (Biodegradable Substrates, FlowVac)
- `Doctor0Evil/BugsLife` → `materials/bugslife/`
- `Doctor0Evil/Ant-One-Net` → `materials/ant-one-net/`
- `Doctor0Evil/EcoNet-BeeSafeAI` → `materials/beesafe-ai/`

### **RESEARCH** (Core Studies, Documentation)
- `Doctor0Evil/eco_restoration_shard` → `research/core/`
- `Doctor0Evil/SnowGlobe` → `research/snowglobe/`
- `Doctor0Evil/EcoNet-CERG` → `research/cerg/`

### **GOV** (Governance, Orchestration, Rewards)
- `Doctor0Evil/ecoinfra-governance` → `governance/ecoinfra/`
- `Doctor0Evil/ecological-orchestrator` → `governance/orchestrator/`
- `Doctor0Evil/Paycomp` → `governance/paycomp/`
- `Doctor0Evil/Ocu-Trust` → `governance/ocu-trust/`
- `Doctor0Evil/Globe` → `governance/globe/`

### **APP** (Visualizers, Dashboards, UIs)
- `Doctor0Evil/EcoNetCybocinderPhoenix` → `apps/cybocinder-phoenix/`
- `Doctor0Evil/EcoBuild` → `apps/ecobuild/`
- `Doctor0Evil/Windminer` → `apps/windminer/`
- `Doctor0Evil/Swarm-x` → `apps/swarm-x/`

## Updated Repository References

**All code, documentation, and ALN schemas** must now reference:
```
github.com/mk-bluebird/eco_restoration_shard
```

**SQLite Constellation Index**: `spine/econet/db/constellation_index.sql`

**Master Repo Index**: `.econet/econet_repo_index.sql`

## Migration Checklist

- [x] Consolidate all repositories into monorepo structure
- [ ] Update SQLite `repo` table to reflect new GitHub account
- [ ] Merge all `.econet/econet_repo_index.sql` shards into master index
- [ ] Update Bostrom DID signing contexts in all ALN particles
- [ ] Update CI/CD workflows (GitHub Actions, test scripts)
- [ ] Archive old Doctor0Evil metadata in `research/archives/doctor0evil/`
- [ ] Update platform continuity anchors (AI chat systems, Perplexity Spaces)
- [ ] Create redirect documentation for external references

## Platform Continuity Anchors

This migration event is recorded in:
- **Perplexity Space**: `eco_restoration_shard` (this Space)
- **Blockchain**: Bostrom network (DIDs unchanged)
- **ALN Schemas**: All particles retain original `signing_did` with migration note

## Contact & Support

- **GitHub**: https://github.com/mk-bluebird
- **Primary Email**: [Associated with mk-bluebird account]
- **Location**: Phoenix, Arizona, US

---

**KER Score for Migration**:
- **K = 0.96** (preserves all existing research, math, and governance semantics)
- **E = 0.89** (consolidation reduces fragmentation, improves discoverability)
- **R = 0.08** (low risk; purely organizational change, no code alteration)
