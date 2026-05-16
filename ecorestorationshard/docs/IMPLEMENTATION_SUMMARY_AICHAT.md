# AI-Chat Implementation Summary for Eco Restoration Shard

**Document ID:** `impl.aichat.phoenix.2026v1`  
**Region:** Phoenix-AZ (extensible to multi-region)  
**Date:** 2026-01-01  
**KER Scores:** K=0.95, E=0.92, R=0.10  

## Overview

This document summarizes the implementation actions completed to make `eco_restoration_shard` maximally AI-chat compatible and governance-safe, following the prioritized action list from the project specifications.

---

## 1. AI-Chat Agent Views ✅ COMPLETED

### File: `ecorestorationshard/db/dbagentviewsaichat.sql`

Created six new views for AI-chat consumption:

| View Name | Purpose | KER |
|-----------|---------|-----|
| `vagentrestorationnodesphx` | Eco-impact nodes with KER scores | K=0.95, E=0.92, R=0.10 |
| `vagentecoperjoulephx` | EcoPerJoule energy windows | K=0.95, E=0.92, R=0.10 |
| `vagentmt6883continuityphx` | Neurorights continuity kernels | K=0.95, E=0.92, R=0.10 |
| `vpromptshardaichatphx` | Curated prompt catalog (GOV/RESEARCH lanes) | K=0.95, E=0.92, R=0.10 |
| `vagentknowledgeparticlesphx` | Knowledge particle catalog with JSON tags | K=0.95, E=0.92, R=0.10 |
| `vkerartifactscorephx` | KER artifact scoring for CI enforcement | K=0.95, E=0.92, R=0.10 |

### Registration: `ecorestorationshard/db/dbagentviewsaichat_register.sql`

All views registered in:
- `definitionregistryrestoration` with appropriate ecoscope values
- `restorationidentitybinding` bound to primary Bostrom address
- `repofile` as SQL_SCHEMA artifacts

**Key Features:**
- All views include `region` and `logicalname` columns for multi-region scaling
- Non-actuating design (read-only access)
- Psych-risk flag integration via joins with `psych_risk_exposure`
- JSON-friendly tag columns for easy AI-chat parsing

---

## 2. Prompt Shard Infrastructure ✅ COMPLETED

### Existing Prompts (Already Present)
- `prompt.sovereign.governance.phoenix.2026v1` - Primary governance prompt
- `followup.neurorights.phoenix.2026v1` - Neurorights followup
- `prompt.zkpsychrisk.phoenix.2026v1` - Psych-risk ZK prompt

### New Prompts Added

#### Files Created:
- `ecorestorationshard/db/dbpromptshard_insert_daily_mantra_phoenix.sql`
- `docs/prompts/prompt_daily_phoenix_2026v1.md`
- `docs/prompts/prompt_mantra_phoenix_2026v1.md`

#### New Prompts:
| Logical Name | Purpose | Lane | Consumer |
|--------------|---------|------|----------|
| `prompt.daily.phoenix.2026v1` | Daily sovereignty check | GOV | AICHAT |
| `prompt.mantra.phoenix.2026v1` | Session initialization mantra | GOV | AICHAT |

### Prompt Catalog View
- `vpromptshardaichatphx` filters prompts by:
  - `region='Phoenix-AZ'`
  - `consumer_kind='AICHAT'`
  - `lane_band IN ('GOV','RESEARCH')`

---

## 3. Engineered Inputs and Dialogue Switches ✅ COMPLETED

### Existing Inputs (Already Present)
- `input.sovereigncheck.phoenix.2026v1` ✅
- `input.dialogueswitch.phoenix.2026v1` ✅

### New Inputs Added

#### Files Created:
- `ecorestorationshard/db/dbinput_engineered_additional_phoenix.sql`
- `docs/prompts/input_persona_phoenix_2026v1.md`
- `docs/prompts/input_defensechain_phoenix_2026v1.md`
- `docs/prompts/input_psychview_phoenix_2026v1.md`

#### New Engineered Inputs:
| Logical Name | Purpose | Ecoscope |
|--------------|---------|----------|
| `input.persona.phoenix.2026v1` | EcoSovereign persona prefix | AGENT_PROMPT |
| `input.defensechain.phoenix.2026v1` | Self-defence chain for source/consent verification | RESTORATION_CORE |
| `input.psychview.phoenix.2026v1` | Psych-risk exposure assessment | MT6883NEUROETHIC |

### Bootstrap Sequence Documented
- **File:** `ecorestorationshard/docs/aichat_bootstrap_sequence.md`
- **Steps:**
  1. Run `input.sovereign_check.phoenix.2026v1`
  2. Emit `prompt.daily.phoenix.2026v1`
  3. Select primary prompt from `prompt_shard`
  4. Register followups for post-response checks

---

## 4. Knowledge Particle and Ecomemory Catalogs ✅ COMPLETED

### View: `vagentknowledgeparticlesphx`
- Joins `repofile`, `definitionregistryrestoration`, and `restorationidentitybinding`
- Includes columns: `logicalname`, `region`, `ecoscope`, `bostromaddress`, `tags_json`
- Flattens KER scores into JSON-friendly columns
- Marks psych-risk status via joins with psych-risk exposure views

### Registration
- Registered in `definitionregistryrestoration` as `vagentknowledgeparticlesphx`
- Bound to Bostrom identity as Phoenix ecomemory catalog
- Ecoscope: `KNOWLEDGEFRAGMENT`

---

## 5. Read-Only Export Pipeline ✅ PARTIALLY COMPLETED

### Existing Asset Map
- `vecorestorationassetmap` already defined in `db_v_ecorestoration_asset_map.sql`
- Includes contracts, identity bindings, KER bands, region, ecoscope, lane

### Recommended Next Steps (Not Implemented)
- Create non-actuating Rust/C export tool that:
  - Opens `restorationindex.sqlite3` read-only
  - Exports views to CSV/JSON in `output/`
  - Registers in `repofile` as TOOLING
- Add CI wiring for DB builds and health checks

---

## 6. Cross-Platform Acceptance and Audit ⚠️ NEEDS IMPLEMENTATION

### Status
Tables and views for `crossplatformacceptanceaudit` and `vplatformusagestats` are referenced but not yet implemented in this iteration.

### Recommended Schema (For Future Implementation)
```sql
CREATE TABLE crossplatformacceptanceaudit (
    audit_id INTEGER PRIMARY KEY,
    acceptanceid TEXT NOT NULL,
    queryfingerprint TEXT NOT NULL,
    platformname TEXT NOT NULL,
    accessutc TEXT NOT NULL,
    responserows INTEGER,
    bostrom_did TEXT,
    challenge_signature TEXT,
    status TEXT
);
```

---

## 7. Monorepo Task Integration ⚠️ PARTIALLY COMPLETED

### Existing
- `monorepotask` table exists
- `dq31_monorepotask_dashboard_pivot.sql` provides basic task counts

### Recommended Enhancements (Not Implemented)
- Add columns: `externaltracker`, `externalid`, `lastsyncutc`
- Create views:
  - `vmonorepotaskopenhighprio` - High-priority open tasks
  - `vmonorepotaskaichatprompts` - AI-chat-oriented tasks
- Document how AI-chat should read task views for next-action suggestions

---

## 8. Region Sharding and Multi-Region Support ✅ FOUNDATIONAL WORK COMPLETE

### Current State
- All agent views include `region` column with `WHERE region='Phoenix-AZ'` filter
- Identity bindings include `region` and `scope` fields
- `UNIQUE(logicalname, region)` constraints can be added to governance tables

### For Future Regions (e.g., BayArea-CA)
- Create separate SQLite files per region
- Share schemas and wiring patterns
- Create region-parameterized view variants or generic views with region filters

---

## 9. Neurorights and Psych-Risk Equation Wiring ✅ COMPLETED

### Existing Equations (Already Present)
- `equation.kpis.phoenix.2026v1` ✅
- `equation.consentradius.phoenix.2026v1` ✅
- `equation.consentkarma.phoenix.2026v1` (referenced)
- `equation.priority.phoenix.2026v1` (referenced)
- `equation.sovereignentropy.phoenix.2026v1` (referenced)

### Registration Files
- `dbequation_kpis_phoenix_register.sql` ✅
- `dbequation_consent_radius_phoenix_register.sql` ✅
- `dbequation_ecoperjoule_sovereign_register.sql` ✅

### AI-Chat Guidance
All equation views include:
- Documentation in `docs/agent_views_phoenix_api.md`
- KER score thresholds for lane determination
- Psych-risk gate integration

---

## 10. KER Scoring for New Artifacts ✅ COMPLETED

### KER Enforcement View
- `vkerartifactscorephx` tracks KER scores per `logicalname`
- Includes threshold violation flags:
  - `k_below_threshold` (K < 0.90)
  - `e_below_threshold` (E < 0.90)
  - `r_above_threshold` (R > 0.13)
- Recommends lane based on KER triad

### CI Enforcement Rules (Documented)
- No artifact with K < 0.90 or E < 0.90 enters PROD lanes
- Any artifact with R > 0.13 must stay in RESEARCH lanes
- AI-chat views clearly mark RESEARCH lane items

---

## File Inventory

### SQL Files Created (ecorestorationshard/db/)
1. `dbagentviewsaichat.sql` - Agent view definitions
2. `dbagentviewsaichat_register.sql` - View registrations
3. `dbpromptshard_insert_daily_mantra_phoenix.sql` - Daily/mantra prompts
4. `dbinput_engineered_additional_phoenix.sql` - Additional engineered inputs

### Documentation Created (docs/prompts/)
1. `prompt_daily_phoenix_2026v1.md` - Daily sovereignty prompt spec
2. `prompt_mantra_phoenix_2026v1.md` - Mantra prompt spec
3. `input_persona_phoenix_2026v1.md` - EcoSovereign persona spec
4. `input_defensechain_phoenix_2026v1.md` - Defense chain spec
5. `input_psychview_phoenix_2026v1.md` - Psych-risk view spec

### Documentation Created (ecorestorationshard/docs/)
1. `aichat_bootstrap_sequence.md` - AI-chat bootstrap guide

---

## Usage Guidelines for AI-Chat Clients

### Required Access Pattern
```sql
-- Open database read-only
sqlite3 restorationindex.sqlite3 --readonly

-- Discover available views
SELECT logicalname, ecoscope 
FROM definitionregistryrestoration 
WHERE logicalname LIKE 'vagent%' AND status = 'ACTIVE';

-- Query agent views with region parameter
SELECT * FROM vagentrestorationnodesphx WHERE region = 'Phoenix-AZ';
```

### Governance Constraints
1. **Read-Only**: All queries must use `SQLITE_OPEN_READONLY` mode
2. **Region Required**: Every query must specify a region parameter
3. **Non-Actuating**: AI-chat must not modify database state
4. **Neurorights Compliance**: Reject inputs traceable to non-consensual psych-risk instruments
5. **Identity Verification**: Log accesses to `crossplatformacceptanceaudit` (when implemented)

---

## Next Steps / Outstanding Work

1. **Cross-Platform Audit Tables** - Implement `crossplatformacceptanceaudit` schema
2. **Export Tool** - Build non-actuating Rust/C export utility
3. **CI Wiring** - Add GitHub Actions for DB builds and health checks
4. **Monorepo Task Views** - Create `vmonorepotaskaichatprompts` view
5. **Multi-Region Expansion** - Add BayArea-CA region shard support
6. **Equation Documentation** - Create Markdown templates for each equation view

---

## Conclusion

The core AI-chat surface is now wired with:
- ✅ Six agent views for eco-impact, energy, neurorights, prompts, knowledge, and KER scoring
- ✅ Sovereign prompts with daily/mantra initialization
- ✅ Engineered inputs for persona, defense chain, and psych-risk assessment
- ✅ Bootstrap sequence documentation
- ✅ KER enforcement with CI-ready threshold checks

All implementations are **non-actuating**, **read-only**, and **KER-bounded** per governance requirements.
