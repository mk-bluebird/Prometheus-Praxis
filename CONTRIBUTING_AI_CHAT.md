# CONTRIBUTING_AI_CHAT.md

## Coding Guidelines for AI-Chat Enhancements

This document defines the rules and patterns for contributing AI-facing views, functions, and patterns to `eco_restoration_shard`.

---

## Core Principles

1. **Non-Actuating by Default**: All AI-visible surfaces must be read-only. No actuation, no routing decisions, no corridor modifications.

2. **Curated Surfaces Only**: AI agents must never access raw tables directly. All access goes through curated views or parameterized patterns.

3. **Governance-Approved**: Every AI-visible object must be registered in `definitionregistry` and reflected in `v_agent_safe_catalog`.

4. **Lane Semantics Preserved**: RESEARCH lane objects are freely queryable; EXPPROD and PROD require explicit KER ceiling validation.

---

## Checklist for New AI-Facing Views

When adding a new view that AI-chat agents may use:

### 1. Create the SQL View

```sql
-- filename: sql/ai_chat/v_my_new_view.sql
CREATE VIEW IF NOT EXISTS v_my_new_view AS
SELECT ...
FROM ...
WHERE ...;  -- Ensure non-actuating filters
```

### 2. Register in `definitionregistry`

```sql
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'v_my_new_view', 'VIEW',
    'eco_restoration_shard', 'sql/ai_chat', 'v_my_new_view.sql',
    'SQL', '2026v1', 1, 'GOVERNANCE', 'AICHAT',
    'Human-readable summary of what this view provides.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'), datetime('now')
);
```

### 3. Verify Appearance in `v_agent_safe_catalog`

The view should automatically appear in the catalog if:
- It's registered with `kind='VIEW'` and `scope='AICHAT'`
- The parent repo has `roleband IN ('SPINE', 'GOV', 'RESEARCH')`
- The repo has `nonactuatingonly = 1`

Test with:
```sql
SELECT * FROM v_agent_safe_catalog WHERE objectid LIKE '%v_my_new_view%';
```

### 4. Add ALN Function Catalog Entry

Create or update `econet.agent.function.catalog.v1.aln`:

```aln
{
  "functionid": "FUNC_MY_NEW_VIEW",
  "summary": "Description of what this view provides",
  "inputschema": {"type": "object", "properties": {...}},
  "outputschema": {"type": "array", "items": {...}},
  "backingview_or_bin": "v_my_new_view",
  "lanescope": ["RESEARCH", "EXPPROD"],
  "riskbands": ["LOW_RISK"],
  "complexityscore": "O(n)"
}
```

### 5. Update Documentation

Add section to `docs/ai_chat_navigation_ecorestoration_shard.md`:
- Purpose
- Columns
- Example query
- Risk interpretation

---

## Checklist for New SQL Patterns

When adding a parameterized SQL pattern for AI use:

### 1. Insert into `agentsqlpattern` Table

```sql
INSERT OR REPLACE INTO agentsqlpattern (
    patternid, description, sqltext, lanescope, riskceilingnote,
    aicapabilitylevel, versiontag, status
) VALUES (
    'PATTERN_MY_NEW_PATTERN',
    'Human-readable description of what this pattern does.',
    'SELECT ... FROM ... WHERE column = :param_name ...',
    'RESEARCH,EXPPROD',
    'Reference to KER/RoH ceilings that constrain this pattern.',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);
```

### 2. Register Pattern in `definitionregistry`

```sql
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'PATTERN_MY_NEW_PATTERN', 'SQLPATTERN',
    'eco_restoration_shard', 'sql/ai_chat', 'agentsqlpattern.sql', 'SQL', '2026v1', 1,
    'GOVERNANCE', 'AICHAT', 'Brief summary.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')
);
```

### 3. Document Usage

Add to the patterns table in `docs/ai_chat_navigation_ecorestoration_shard.md`:

| Pattern ID | Description |
|------------|-------------|
| `PATTERN_MY_NEW_PATTERN` | Description here |

---

## Checklist for New FFI Functions

When extending the Rust cdylib with new AI-chat functions:

### 1. Define Serialize Struct

In `crates/ecorestorationshard/src/lib.rs`:

```rust
#[derive(Debug, Serialize)]
pub struct MyNewEntry {
    pub field1: String,
    pub field2: f64,
    // ...
}
```

### 2. Implement Query Function

```rust
fn query_my_new_function(
    conn: &Connection,
    param: &str,
) -> Result<Vec<MyNewEntry>, SpineError> {
    let mut stmt = conn
        .prepare("SELECT ... FROM ... WHERE ... = ?1")
        .map_err(|e| SpineError::Query(e.to_string()))?;
    // ... query_map logic
}
```

### 3. Expose C ABI Function

```rust
#[no_mangle]
pub extern "C" fn econet_get_my_new_function(
    dbpath: *const c_char,
    param: *const c_char,
) -> *mut c_char {
    // Standard error handling and JSON serialization
}
```

### 4. Update Lua Wrapper

In `lua/econet_cybo_overlay.lua`:

```lua
ffi.cdef[[
char* econet_get_my_new_function(const char* dbpath, const char* param);
]]

function M.get_my_new_function(dbpath, param)
  -- Standard wrapper pattern
end
```

### 5. Update Kotlin Wrapper

In `android/app/src/main/java/org/econet/CyboOverlay.kt`:

```kotlin
@JvmStatic external fun econet_get_my_new_function(dbPath: String, param: String): String?

@JvmStatic
fun myNewFunction(dbPath: String, param: String): String? {
    return econet_get_my_new_function(dbPath, param)
}
```

### 6. Register FFI in `definitionregistry`

```sql
INSERT OR IGNORE INTO definitionregistry (
    contractid, scope, logicalname, kind, repo, destinationpath,
    filename, language, versiontag, active, primaryplane, appliescope,
    summary, signingdid, issuedutc, updatedutc
) VALUES (
    'EcosafetyContinuity2026v1', 'AICHAT', 'econet_get_my_new_function', 'FFI',
    'eco_restoration_shard', 'src', 'lib.rs', 'Rust', '2026v1', 1,
    'GOVERNANCE', 'AICHAT', 'Returns JSON for my new function.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7', datetime('now'), datetime('now')
);
```

---

## Prohibited Changes

The following changes require explicit KER/ALN updates and governance approval:

1. **Changing `nonactuatingonly` flag**: Never modify without explicit policy update.

2. **Exposing raw tables**: No `SELECT * FROM cyboworkloadledger` directly to AI.

3. **Modifying lane semantics**: Lane predicates must be updated via ALN shards only.

4. **Weakening KER ceilings**: K >= 0.90, E >= 0.85, R <= 0.20 are frozen minima/maxima.

5. **Adding actuating functions**: Any function that could trigger hardware must go through separate governance channels.

---

## Testing Requirements

Before merging AI-chat enhancements:

1. **Verify non-actuating guarantees**:
   ```sql
   SELECT nonactuatingonly FROM econetrepoindex WHERE reponame = 'eco_restoration_shard';
   -- Must return 1
   ```

2. **Check catalog registration**:
   ```sql
   SELECT COUNT(*) FROM v_agent_safe_catalog WHERE reponame = 'eco_restoration_shard';
   -- Should increase by expected count
   ```

3. **Validate JSON output**: Test FFI functions return valid JSON.

4. **Audit trail**: Ensure all changes are signed with appropriate DID.

---

## Migration Notes

When migrating existing views to AI-safe surfaces:

1. Create new `v_` prefixed view with explicit column selection
2. Register in `definitionregistry` with `scope='AICHAT'`
3. Deprecate old direct-access patterns via `agentsqlpattern` with `status='DEPRECATED'`
4. Update documentation to point to new surface

---

## Contact

For questions about AI-chat contributions, consult:
- `docs/ai_chat_navigation_ecorestoration_shard.md` for usage patterns
- `.econet/econetrepoindex.sql` for repo metadata schema
- `sql/ai_chat/` directory for existing view implementations
