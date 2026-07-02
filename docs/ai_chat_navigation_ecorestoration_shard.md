# AI Chat Navigation Guide for eco_restoration_shard

## Overview

This document explains the **non-actuating spine** of `eco_restoration_shard` and how AI-chat agents can safely interact with it. The shard is designed to provide **read-only, diagnostic-only** access to EcoNet KER (Knowledge-Ecological-Responsibility) data, blast-radius surfaces, and Cyboquatic workload ledgers.

## Key Principles

1. **Non-Actuating Guarantee**: All views, FFI functions, and SQL patterns in this shard are strictly read-only. No hardware actuation, no routing decisions, no corridor modifications.

2. **AI-Safe Surfaces Only**: AI agents must only use curated views and patterns listed in `v_agent_safe_catalog`. Raw table access is forbidden.

3. **Lane Semantics Preserved**: RESEARCH lane objects may be queried freely; EXPPROD and PROD require explicit KER ceiling checks.

## Main AI-Visible Views

### 1. `veconet_repo_manifest_agent`

**Purpose**: One-shot answer to "What is this repo, what lanes and contracts apply, and is actuation forbidden here?"

**Columns**:
- `reponame`: Repository name
- `githubslug`: GitHub repository slug
- `roleband`: SPINE, RESEARCH, GOV, etc.
- `lanedefault`: Default lane (RESEARCH, EXPPROD, PROD)
- `nonactuatingonly`: 1 if actuation is forbidden
- `region`: Region code
- `didowner`: Bostrom DID of repo owner
- `primarylanguages`: Aggregated languages from layers
- `contractssummary`: Policy contracts attached
- `kertargetk`, `kertargete`, `kertargetr`: KER targets
- `riskband`: Computed risk classification
- `aicapabilityflag`: What AI may do (e.g., MAY_ANSWER_KER_ONLY)

**Example Query**:
```sql
SELECT * FROM veconet_repo_manifest_agent WHERE reponame = 'eco_restoration_shard';
```

### 2. `vmachine_blastradius`

**Purpose**: Summarize blast-radius per node/machine across HYDRAULIC, CARBON, BIODIVERSITY planes with precomputed vt sensitivity bands.

**Columns**:
- `nodeid`: Source node identifier
- `targettype`, `targetid`: Impact target
- `impacttype`: HYDRAULICS, CARBON, BIODIVERSITY, etc.
- `total_impact_score`: Sum of impact scores
- `mean_vt_sensitivity`: Average Lyapunov sensitivity
- `vt_sensitivity_band`: VT_LOW_SENSITIVITY, VT_MODERATE_SENSITIVITY, VT_HIGH_SENSITIVITY
- `safestepok`: 1 if Lyapunov descent is safe

**Example Query**:
```sql
SELECT * FROM vmachine_blastradius WHERE nodeid = 'PHX-CYBOQ-MAR-001';
```

### 3. `vcyboworkloadnodewindow`

**Purpose**: Windowed aggregates of `cyboworkloadledger` per node: energy requests, surplus, vt delta, accept fractions.

**Columns**:
- `nodeid`, `channel`: Node and channel identifiers
- `window_start_utc`, `window_end_utc`: Time bounds
- `total_ereqj`, `total_esurplusj`: Energy aggregates
- `surplus_fraction`: Efficiency metric
- `accept_fraction`: Fraction of workloads accepted
- `mean_vtbefore`, `mean_vtafter`, `mean_delta_vt`: Lyapunov residuals
- `vt_improved`: 1 if vt decreased (ecological improvement)
- `carbon_safe`, `biodiv_safe`: Risk safety flags

**Example Query**:
```sql
SELECT * FROM vcyboworkloadnodewindow WHERE nodeid = 'PHX-CYBOQ-MAR-001';
```

## Authoritative Tool Catalogs

### `v_agent_safe_catalog`

This view is the **sole source of truth** for AI-visible tools. It lists all non-actuating, governance-approved objects:

- **Object kinds**: VIEW, FFI, SQLPATTERN, PROMPT, ALN
- **Capability levels**: NONE, DOC, READONLY_SPINE, PROTO_AGENT
- **Risk bands**: LOW_RISK, MODERATE_RISK, HIGH_RISK

**Usage**:
```sql
SELECT objectid, objectkind, aicapabilitylevel, contractssummary
FROM v_agent_safe_catalog
WHERE reponame = 'eco_restoration_shard'
  AND status = 'ACTIVE';
```

### `agentsqlpattern` Table

Parameterized SQL templates that AI agents should use instead of synthesizing joins:

| Pattern ID | Description |
|------------|-------------|
| `PATTERN_SAFE_PROD_SHARDS` | Find PROD-eligible shards with high K, E, low R |
| `PATTERN_CYBO_NODE_IMPROVEMENT` | List nodes that improved vt and reduced carbon |
| `PATTERN_NODE_BLASTRADIUS_SUMMARY` | Get blast-radius summary for a node |
| `PATTERN_REPO_MANIFEST_QUERY` | Query repo manifest with AI capability flags |
| `PATTERN_NODE_WORKLOAD_WINDOW` | Get workload window aggregates |
| `PATTERN_AI_SAFE_CATALOG_LOOKUP` | List AI-visible objects for a repo |

**Example Usage**:
```sql
SELECT sqltext FROM agentsqlpattern WHERE patternid = 'PATTERN_SAFE_PROD_SHARDS';
-- Then execute with parameters :region, :min_k, :min_e, :max_r
```

## FFI Functions (Rust cdylib)

The following JSON-returning functions are available via the cdylib:

| Function | Purpose |
|----------|---------|
| `econet_get_repo_manifest_agent(dbpath, reponame)` | Returns JSON manifest for a repo |
| `econet_get_agent_safe_catalog(dbpath, reponame)` | Returns JSON array of AI-safe catalog objects |
| `econet_get_node_window(dbpath, nodeid)` | Returns JSON workload window aggregates |
| `econet_get_blastradius_summary(dbpath, nodeid)` | Returns JSON blast-radius summary |

### LuaJIT Wrapper (`econet_cybo_overlay.lua`)

```lua
local cybo = require("econet_cybo_overlay")

local manifest_json = cybo.get_repo_manifest_agent("/path/to/db.sqlite", "eco_restoration_shard")
local catalog_json = cybo.get_agent_safe_catalog("/path/to/db.sqlite", "eco_restoration_shard")
local window_json = cybo.get_node_window("/path/to/db.sqlite", "PHX-CYBOQ-MAR-001")
local blast_json = cybo.get_blastradius_summary("/path/to/db.sqlite", "PHX-CYBOQ-MAR-001")
```

### Kotlin Wrapper (`CyboOverlay.kt`)

```kotlin
val manifestJson = CyboOverlay.repoManifestAgent(dbPath, "eco_restoration_shard")
val catalogJson = CyboOverlay.agentSafeCatalog(dbPath, "eco_restoration_shard")
val windowJson = CyboOverlay.nodeWindow(dbPath, "PHX-CYBOQ-MAR-001")
val blastJson = CyboOverlay.blastRadiusSummary(dbPath, "PHX-CYBOQ-MAR-001")
```

## ALN Function Catalog

The ALN shard `econet.agent.function.catalog.v1.aln` (to be created) will describe each AI-visible function with:

- `functionid`: Unique identifier
- `summary`: Human-readable description
- `inputschema`, `outputschema`: JSON schema for I/O
- `backingview_or_bin`: SQL view or binary backing
- `lanescope`: Allowed lanes
- `riskbands`: Risk classification
- `complexityscore`: Computational complexity hint

## For Contributors

### Rules for New AI-Facing Views

1. **Register in `definitionregistry`**:
   ```sql
   INSERT INTO definitionregistry (..., kind='VIEW', scope='AICHAT', ...) VALUES (...);
   ```

2. **Mirror in `v_agent_safe_catalog`**: The view will automatically appear if registered correctly.

3. **Document in ALN function catalog**: Add entry to `econet.agent.function.catalog.v1.aln`.

4. **No raw table exposure**: Only curated views/patterns are AI-visible.

5. **Preserve non-actuating guarantees**: No changes to lane semantics without explicit KER/ALN updates.

## Interpretation Guidelines

### Risk Bands

- **LOW_RISK**: `kertargetr <= 0.20` - Safe for AI reasoning
- **MODERATE_RISK**: `0.20 < kertargetr <= 0.40` - Requires human review for actuation proposals
- **HIGH_RISK**: `kertargetr > 0.40` - AI should not propose changes

### AI Capability Flags

- **MAY_ANSWER_KER_ONLY**: AI may answer questions about KER metrics but not propose SQL
- **MAY_PROPOSE_NONACTUATING_SQL**: AI may propose read-only SQL queries
- **HUMAN_REVIEW_REQUIRED**: All AI output requires human validation

### Vt Sensitivity Bands

- **VT_LOW_SENSITIVITY**: Mean sensitivity <= 0.05 - Stable ecological surface
- **VT_MODERATE_SENSITIVITY**: 0.05 < mean <= 0.10 - Monitor closely
- **VT_HIGH_SENSITIVITY**: Mean > 0.10 - High ecological volatility

## Summary

AI-chat agents interacting with `eco_restoration_shard` should:

1. Start by querying `veconet_repo_manifest_agent` to understand repo safety/lanes
2. Consult `v_agent_safe_catalog` for available tools
3. Use `agentsqlpattern` for safe, parameterized queries
4. Interpret results through the lens of KER ceilings and vt sensitivity bands
5. Never assume actuation is permitted - always check `nonactuatingonly` flag
