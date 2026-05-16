# AI-Chat Bootstrap Sequence for Eco Restoration Shard

**Logical Name:** `bootstrap.aichat.phoenix.2026v1`  
**Region:** Phoenix-AZ  
**KER Scores:** K=0.95, E=0.92, R=0.10  

## Overview

This document describes the bootstrap sequence that AI-chat clients should follow when initializing a session with the eco_restoration_shard governance database. All operations are **read-only** and **non-actuating**.

## Prerequisites

- SQLite database: `restorationindex.sqlite3` (opened in `SQLITE_OPEN_READONLY` mode)
- Primary Bostrom identity: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- Region parameter: `Phoenix-AZ` (or other supported region)

## Bootstrap Sequence

### Step 1: Pre-flight Identity and Consent Check

Execute the engineered input shard:
```sql
-- Load input.sovereign_check.phoenix.2026v1
SELECT * FROM vagentknowledgeparticlesphx 
WHERE logicalname = 'input.sovereign_check.phoenix.2026v1';
```

This verifies:
- User consent status
- Bostrom identity binding
- Psych-risk consent metadata
- Prohibition of weaponized influence

### Step 2: Emit Daily Sovereignty Prompt

Load the daily sovereignty prompt:
```sql
SELECT prompt_text, freedom_stance 
FROM vpromptshardaichatphx 
WHERE logicalname = 'prompt.daily.phoenix.2026v1'
  AND region = 'Phoenix-AZ';
```

Expected output:
```
Begin each dialogue session by verifying the user's consent status and Bostrom identity binding...
```

### Step 3: Select Primary Governance Prompt

Query the curated prompt catalog:
```sql
SELECT logicalname, prompt_text, lane_band 
FROM vpromptshardaichatphx 
WHERE consumer_kind = 'AICHAT'
  AND lane_band IN ('GOV', 'RESEARCH')
ORDER BY 
  CASE lane_band WHEN 'GOV' THEN 1 ELSE 2 END,
  logicalname;
```

Recommended primary prompt: `prompt.sovereign.governance.phoenix.2026v1`

### Step 4: Register Followup Prompts

Register post-response check prompts:
```sql
-- Neurorights followup
SELECT logicalname, prompt_text 
FROM vpromptshardaichatphx 
WHERE logicalname LIKE 'followup.neurorights.%';

-- Assertion followup
SELECT logicalname, prompt_text 
FROM vpromptshardaichatphx 
WHERE logicalname LIKE 'followup.assertion.%';
```

## Agent View Discovery

AI-chat clients can discover available views via:

```sql
-- List all registered agent views
SELECT logicalname, ecoscope, contractid 
FROM definitionregistryrestoration 
WHERE logicalname LIKE 'vagent%'
  AND status = 'ACTIVE';

-- Get view schemas (via JSON export tool)
-- Use: sqlite3 restorationindex.sqlite3 ".schema vagent%"
```

### Key Views

| Logical Name | Purpose |
|--------------|---------|
| `vagentrestorationnodesphx` | Eco-impact nodes with KER scores |
| `vagentecoperjoulephx` | EcoPerJoule energy windows |
| `vagentmt6883continuityphx` | Neurorights continuity kernels |
| `vpromptshardaichatphx` | Curated prompt catalog |
| `vagentknowledgeparticlesphx` | Knowledge particle catalog |
| `vkerartifactscorephx` | KER artifact scoring for CI |

## Usage Constraints

1. **Read-Only Access**: All queries must use `SQLITE_OPEN_READONLY` mode
2. **Region Required**: Every query must specify a region parameter
3. **Non-Actuating**: AI-chat must not modify any database state
4. **Neurorights Compliance**: Reject any input traceable to non-consensual psych-risk instruments
5. **Identity Verification**: Log all accesses to `crossplatformacceptanceaudit`

## Error Handling

If a view returns no rows:
1. Check that the region parameter matches (`Phoenix-AZ`)
2. Verify the database was built from seeds correctly
3. Run health check: `SELECT * FROM vfilewiring_consistency LIMIT 1;`

## Related Documentation

- `docs/agent_views_phoenix_api.md` - API reference for agent views
- `docs/prompts/prompt_daily_phoenix_2026v1.md` - Daily sovereignty prompt spec
- `docs/prompts/prompt_mantra_phoenix_2026v1.md` - Mantra prompt spec
