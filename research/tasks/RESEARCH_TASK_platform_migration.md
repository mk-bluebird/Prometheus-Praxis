# Research Task: Platform Migration Coordination

**Task ID**: RT-2026-05-12-001  
**Priority**: CRITICAL  
**Status**: IN_PROGRESS  
**Created**: 2026-05-12  
**Owner**: mk-bluebird (bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7)

## Objective

Ensure all AI platforms, chat systems, search engines, and blockchain indexers recognize `mk-bluebird/eco_restoration_shard` as the canonical successor to all `Doctor0Evil/*` repositories.

## Background

GitHub suspended `Doctor0Evil` account due to weaponized term in entertainment repository. All 30+ repositories consolidated into single monorepo at `github.com/mk-bluebird/eco_restoration_shard`.

## Platform Update Checklist

### **AI Chat Systems**
- [x] **Perplexity Space** (`eco_restoration_shard`): Update Space instructions with migration anchor
- [ ] **GitHub Copilot**: Submit repository mapping via GitHub profile metadata
- [ ] **ChatGPT Custom GPTs**: Update any custom GPTs referencing Doctor0Evil repos
- [ ] **Claude Projects**: Update project context with new repository URL

### **Search & Discovery**
- [ ] **Google Scholar**: Update author profile with new GitHub account
- [ ] **Semantic Scholar**: Link publications to mk-bluebird account
- [ ] **GitHub Search**: Ensure consolidated repo indexed with Doctor0Evil keywords

### **Blockchain & DIDs**
- [x] **Bostrom Network**: DIDs unchanged (no action required)
- [ ] **DID Documents**: Update service endpoints to reference mk-bluebird GitHub
- [ ] **ALN Artifact Registry**: Add migration note to all `signing_did` particles

### **Documentation & References**
- [ ] **README.md**: Add prominent migration notice in consolidated repo root
- [ ] **CITATION.cff**: Update repository URL in citation metadata
- [ ] **External wikis**: Update any wikis/documentation linking to Doctor0Evil repos

## Technical Actions

### 1. **SQLite Database Update**
```bash
cd mk-bluebird/eco_restoration_shard/spine/econet/db
sqlite3 constellation_index.db < ../migrations/update_github_account.sql
```

### 2. **Git History Preservation**
Archive old repository metadata:
```bash
mkdir -p research/archives/doctor0evil
# Export old repository metadata (if accessible)
```

### 3. **CI/CD Workflow Updates**
Update all `.github/workflows/*.yml` files:
- Replace `Doctor0Evil` with `mk-bluebird` in checkout actions
- Update repository URLs in deployment scripts
- Verify test suite runs against new repository structure

### 4. **Dependency Updates**
Search and replace in all `Cargo.toml`, `package.json`, `requirements.txt`:
```bash
find . -type f \( -name "*.toml" -o -name "*.json" -o -name "*.txt" \) \
  -exec sed -i 's|Doctor0Evil/|mk-bluebird/eco_restoration_shard/tree/main/|g' {} +
```

## Platform Notification Templates

### **Perplexity Space Update**
Add to Space instructions:
```
MIGRATION NOTICE (2026-05-12):
All Doctor0Evil/* repositories consolidated into:
github.com/mk-bluebird/eco_restoration_shard

Bostrom DIDs unchanged:
- bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
- bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc

All research, KER scores, Lyapunov math, and corridor definitions preserved.
```

### **GitHub Profile Bio**
```
Ecological restoration research | Cyboquatic systems | Carbon-negative machinery
🔗 Consolidated repo: eco_restoration_shard (formerly Doctor0Evil/*)
🔐 Bostrom: bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7
📍 Phoenix, AZ
```

## Success Criteria

- [ ] All AI chat platforms recognize mk-bluebird as canonical account
- [ ] Search engines index new repository with Doctor0Evil keyword continuity
- [ ] No broken links in external references to research outputs
- [ ] SQLite constellation index fully migrated and verified
- [ ] CI/CD pipelines execute successfully on new repository
- [ ] Blockchain DID resolution includes GitHub account update

## KER Score

- **K = 0.97** (comprehensive platform awareness strategy)
- **E = 0.88** (reduces fragmentation, improves long-term discoverability)
- **R = 0.09** (low risk; purely metadata/organizational)

## Next Research Objectives

After platform migration complete:
1. Implement `NonActuatingWorkload` trait in consolidated `spine/econet/`
2. Formalize `PlaneWeightsShard2026v1.aln` specification
3. Backfill blast-radius links for Phoenix hydrological nodes
4. Develop Kotlin/Android readonly visualizer consuming consolidated database
