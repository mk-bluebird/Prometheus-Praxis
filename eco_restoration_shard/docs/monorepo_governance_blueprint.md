# Monorepo Governance Blueprint

## Overview

This document explains how `eco_restoration_shard` consolidates restoration, energy, and MT6883 governance functions for the Phoenix-AZ constellation. The monorepo serves as the single source of truth for governance-grade artifacts including database schemas, tooling, CI workflows, agent APIs, and documentation.

**Repository:** [github.com/mk-bluebird/eco_restoration_shard](https://github.com/mk-bluebird/eco_restoration_shard)

**Primary Author:** `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`

---

## Repository Layout

```
eco_restoration_shard/
├── db/
│   ├── tasks_monorepo_consolidation.sql    # Task tracking table schema
│   ├── insert_tasks_monorepo_consolidation.sql  # 13 seeded consolidation tasks
│   ├── monorepo_task_index.csv             # Human-readable task index
│   ├── db_repo_index.sql                   # Repo/repofile registration (restoration.repoindex.phoenix)
│   ├── db_definition_registry_restoration.sql  # DefinitionRegistry (restoration.definitionregistry.phoenix)
│   ├── db_restoration_blastradius_phoenix.sql  # Blastradius grammar (restoration.blastradius.phoenix.2026v1)
│   ├── db_ecoperjoule_policy_energy.sql    # Eco-per-joule policy (energy.ecoperjoule.policy.phoenix.2026v1)
│   ├── db_mt6883_lane_continuity.sql       # MT6883 continuity (mt6883.lane.continuity.phoenix.2026v1)
│   ├── db_restoration_identity_binding.sql # Identity bindings (restoration.identitybinding.phoenix.2026v1)
│   └── db_agent_views_phoenix.sql          # Agent-facing views (agent.views.phoenix.2026v1)
├── src/
│   ├── tools/
│   │   └── restoration_energy_mt6883_tool.cpp  # C++ governance tool
│   └── lib/
│       └── governance_spine_client.rs      # Rust governance client
├── .github/
│   └── workflows/
│       ├── ci_definitionregistry.yml       # DefinitionRegistry enforcement
│       └── ci_identity_binding.yml         # Identity binding enforcement
└── docs/
    └── monorepo_governance_blueprint.md    # This document
```

---

## Governance Database Schema

### Core Tables

| Table | Purpose | Logicalname |
|-------|---------|-------------|
| `monorepo_task` | Tracks consolidation tasks by category, scope, priority | `restoration.taskindex.phoenix` |
| `repo` / `repofile` | Registers repository and file metadata | `restoration.repoindex.phoenix` |
| `definitionregistry_restoration` | Central registry for all governance artifacts | `restoration.definitionregistry.phoenix` |
| `blastradiusindex` | Blast radius metrics with restoration extensions | `restoration.blastradius.phoenix.2026v1` |
| `ecoperjoule_policy` | Energy efficiency policies per lane/domain | `energy.ecoperjoule.policy.phoenix.2026v1` |
| `lanestatusshard` | Lane status with MT6883/neuroethic fields | `mt6883.lane.continuity.phoenix.2026v1` |
| `restoration_identity_binding` | Binds Bostrom addresses to artifacts | `restoration.identitybinding.phoenix.2026v1` |

### Key Views

| View | Description |
|------|-------------|
| `v_blastradius_restoration_phx` | Phoenix-scoped restoration blast radius data |
| `v_restoration_nodes_phx` | Aggregated restoration metrics by node |
| `v_cyboquatic_ecoperjoule_prod_phx` | Production-eligible eco-per-joule policies |
| `v_mt6883_lane_continuity` | MT6883 continuity status for all lanes |
| `v_agent_*` | Agent-friendly views with simplified schemas |

---

## Restoration, Ecoperjoule, and MT6883 Integration

### Restoration Blastradius (`db_restoration_blastradius_phoenix.sql`)

Extends the base `blastradiusindex` table with:
- `restorationradius_m`: Restoration impact radius in meters
- `restoration_ok`: Boolean indicating restoration approval
- Phoenix-scoped views filtering to `region = 'PHOENIX-AZ'`

**Sample Query:**
```sql
SELECT node_id, restoration_status, gw_risk_max
FROM v_blastradius_restoration_phx
WHERE restoration_ok = 1;
```

### Eco-Per-Joule Policy (`db_ecoperjoule_policy_energy.sql`)

Defines energy efficiency thresholds for each lane/domain/assetclass combination:
- `min_ecoperjoule`, `max_ecoperjoule`: Efficiency ratio bounds
- `threshold_warning`, `threshold_critical`: Compliance thresholds
- `prod_eligible`: Production eligibility flag

**Energy Case Logic:**
```sql
CASE 
    WHEN min_ecoperjoule >= max_ecoperjoule THEN 'OPTIMAL_ENERGY_CASE'
    WHEN min_ecoperjoule >= threshold_warning THEN 'ACCEPTABLE_ENERGY_CASE'
    WHEN min_ecoperjoule >= threshold_critical THEN 'MARGINAL_ENERGY_CASE'
    ELSE 'CRITICAL_ENERGY_CASE'
END AS energy_case
```

### MT6883 Lane Continuity (`db_mt6883_lane_continuity.sql`)

Integrates MT6883 and neuroethic review into lane status:
- `mt6883_ok`: MT6883 continuity verification
- `neuroethic_radius_hours`: Neuroethic review window
- `neuroethic_ok`: Neuroethic compliance status

**Continuity Score Calculation:**
```sql
-- 0-100 score based on restoration_ok, mt6883_ok, neuroethic_ok, and lane status
ROUND(
    (CASE WHEN restoration_ok = 1 THEN 25.0 ELSE 0.0 END +
     CASE WHEN mt6883_ok = 1 THEN 25.0 ELSE 0.0 END +
     CASE WHEN neuroethic_ok = 1 THEN 25.0 ELSE 0.0 END +
     CASE WHEN status = 'ACTIVE' THEN 25.0 ELSE 0.0 END), 2
) AS continuity_score
```

---

## DefinitionRegistry Usage

The `definitionregistry_restoration` table is the central catalog for all governance artifacts. Every SQL schema, tool, CI workflow, and document must be registered with:

- **logicalname**: Unique identifier (e.g., `restoration.blastradius.phoenix.2026v1`)
- **versiontag**: Version string (e.g., `2026v1`)
- **artifact_type**: `SQL_SCHEMA`, `VIEW`, `TOOL_CPP`, `TOOL_RUST`, `CI_WORKFLOW`, `DOC`
- **file_path**: Relative path within the repository
- **status**: `REGISTERED`, `ACTIVE`, `DEPRECATED`, `REVOKED`

**Registration Example:**
```sql
INSERT INTO definitionregistry_restoration (
    logicalname, versiontag, artifact_type, file_path, status,
    region, scope, description, author_bostrom
) VALUES (
    'restoration.blastradius.phoenix.2026v1',
    '2026v1',
    'SQL_SCHEMA',
    'db/db_restoration_blastradius_phoenix.sql',
    'ACTIVE',
    'PHOENIX-AZ',
    'PHOENIX-AZ',
    'Restoration-aware blastradius grammar with Phoenix-scoped views.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
);
```

---

## Identity Bindings

All governance artifacts must have a corresponding entry in `restoration_identity_binding` that binds:
- **bostrom_address**: Author identifier (`bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`)
- **logicalname**: Reference to DefinitionRegistry entry
- **file_path**: Path to the bound file
- **dbrole**: `GOVERNANCE`, `TOOLING`, `CI`, `AGENT_API`, or `DOCS`

**Binding Example:**
```sql
INSERT INTO restoration_identity_binding (
    bostrom_address, logicalname, file_path, region, scope, dbrole, role_description
) VALUES (
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'restoration.blastradius.phoenix.2026v1',
    'db/db_restoration_blastradius_phoenix.sql',
    'PHOENIX-AZ',
    'REGION',
    'GOVERNANCE',
    'Primary author of restoration-aware blastradius grammar.'
);
```

---

## Tooling Interfaces

### C++ Governance Tool (`src/tools/restoration_energy_mt6883_tool.cpp`)

Non-actuating query tool supporting:
- `--contracts`: Query active restoration contracts
- `--restoration-planes`: Query production-eligible restoration planes
- `--ecoperjoule`: Query eco-per-joule production policies
- `--mt6883`: Query MT6883 lane continuity
- `--summary`: Governance summary metrics
- `--format=json`: JSON output mode

**Usage:**
```bash
./restoration_energy_mt6883_tool --db db/restorationindex.sqlite3 --summary
./restoration_energy_mt6883_tool --format=json --ecoperjoule
```

### Rust Governance Spine Client (`src/lib/governance_spine_client.rs`)

Typed Rust API wrapping `restorationindex.sqlite3`:

```rust
use governance_spine_client::GovernanceSpineClient;

let client = GovernanceSpineClient::open("db/restorationindex.sqlite3")?;

// Get restoration nodes
let nodes = client.get_restoration_nodes_phoenix()?;

// Get MT6883 lanes requiring attention
let attention = client.get_mt6883_attention_required()?;

// Verify identity binding
let bound = client.verify_binding("db/example.sql", "bostrom18sd...")?;
```

---

## CI Enforcement

### DefinitionRegistry Enforcement (`.github/workflows/ci_definitionregistry.yml`)

Scans for all `.sql`, `.rs`, `.cpp`, `.yml`, and `.md` files in governance directories and verifies each has a corresponding `definitionregistry_restoration` entry. Fails build if unregistered artifacts are found.

### Identity Binding Enforcement (`.github/workflows/ci_identity_binding.yml`)

Checks that all changed governance files have identity bindings with the approved Bostrom address. Rejects anonymous or unbound changes.

---

## Task Tracking

The `monorepo_task` table tracks 13 seeded consolidation tasks across categories:

| Category | Count | Scope |
|----------|-------|-------|
| `GOVERNANCE_DB` | 6 | PHOENIX-AZ |
| `TOOLING` | 3 | PHOENIX-AZ, GLOBAL_PATTERN |
| `CI` | 2 | CONSTELLATION |
| `AGENT_API` | 1 | CONSTELLATION |
| `DOCS` | 1 | CONSTELLATION |

View open tasks:
```sql
SELECT task_id, category, title, logicalname, priority
FROM monorepo_task
WHERE status = 'OPEN'
ORDER BY 
    CASE priority WHEN 'HIGH' THEN 1 WHEN 'MEDIUM' THEN 2 ELSE 3 END,
    task_id;
```

---

## Extending to Other Regions

To instantiate this governance pattern for another region (e.g., `ATLANTIS-CA`):

1. Copy Phoenix SQL files with region-specific naming
2. Update `region` defaults from `'PHOENIX-AZ'` to `'ATLANTIS-CA'`
3. Register new artifacts in `definitionregistry_restoration` with logicalnames like:
   - `restoration.blastradius.atlantis.2026v1`
   - `energy.ecoperjoule.policy.atlantis.2026v1`
   - `mt6883.lane.continuity.atlantis.2026v1`
4. Add identity bindings for the new region's artifacts
5. Update CI workflows if region-specific checks are needed

---

## References

- **Bostrom Address:** `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- **Repo Target:** `github.com/mk-bluebird/eco_restoration_shard`
- **Database:** `db/restorationindex.sqlite3`
- **Task Index:** `db/monorepo_task_index.csv`
