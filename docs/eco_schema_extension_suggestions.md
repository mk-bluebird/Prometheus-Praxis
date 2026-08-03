# EcoNet / Prometheus Shared Schema Study and Extension Ideas

This note analyzes existing SQLite schemas (`mcp_repo`, `mcp_file`, `mcp_tool`, canal and PFAS tables) and proposes minor, non‑breaking extensions to better expose C++ and non‑Rust tools.[78][59]

## 1. Existing Schema Highlights

- `mcp_repo`:
  - `roleband` in ('SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP').
  - `primaryplane` (e.g., CARBON, BIODIVERSITY).
  - `nonactuatingonly` flag indicating read‑only, non‑actuating repositories.[78]

- `mcp_file`:
  - `filekind` includes 'SQL','RUST','ALN','CSV','DOC','LUA','KOTLIN','ANDROID','JSON','OTHER'.
  - `filerole` includes 'SCHEMA','INDEX','MCP_TOOL','CONFIG','DOC','VIEW','DATASET'.[78]

- `mcp_tool`:
  - `toolkind` in ('COMMAND','FILE','HTTP','SQL_QUERY','RUST_FN').
  - `resourcemode` in ('READONLY','MUTATING').
  - Governance metadata: `lanedefault`, `planebands`, `ker_hint`, `citizen_ready`, `neuroflag`.[78]

- Canal/PFAS tables:
  - `canal_node`, `cyboquatic_workload_telemetry`, `canal_ker_canal_invariant`, `pfas_corridor_state`.
  - Phoenix hex registry: `phoenix_hex_registry` with risk coordinates and weights.[59]

## 2. Non‑Breaking Extension Ideas

### 2.1 Add C++ Filekind and Toolkind

Extend `mcp_file.filekind` CHECK list to include 'CPP':

```sql
ALTER TABLE mcp_file
  RENAME TO mcp_file_old;

-- (Recreate with CPP in filekind, then copy data.)
-- This can also be done by defining CPP as 'OTHER' in the interim, but explicit CPP improves clarity.
```

Alternatively, treat C++ as 'OTHER' and add a new column:

```sql
ALTER TABLE mcp_file
  ADD COLUMN language_hint TEXT NULL; -- e.g., 'CPP','RUST','LUA','KOTLIN','JAVA'
```

Extend `mcp_tool.toolkind` CHECK to include 'CPP_FN':

```sql
ALTER TABLE mcp_tool
  ADD COLUMN cpp_hint TEXT NULL; -- symbol name or header path
```

These changes can be introduced via new columns and values without breaking existing rows.

### 2.2 Non‑Rust Endpoint Types

Extend `mcp_endpoint.endpoint_type` CHECK to recognise non‑Rust bindings:

- Already includes ('CLI','SQL','RUST_FN','LUA','KOTLIN','ANDROID_VIEW').[78]
- Optionally add 'CPP_FN' to indicate direct C++ function bindings:

```sql
-- Add CPP_FN via a compatible schema migration:
-- endpoint_type TEXT NOT NULL CHECK (endpoint_type IN ('CLI','SQL','RUST_FN','LUA','KOTLIN','ANDROID_VIEW','CPP_FN'))
```

This makes C++ eco tools first‑class MCP endpoints, discoverable via `v_cpp_eco_tools`.

### 2.3 C++ Tool Metadata

Add a small metadata table for C++ tools:

```sql
CREATE TABLE IF NOT EXISTS cpp_tool_meta (
    cppmeta_id INTEGER PRIMARY KEY AUTOINCREMENT,
    toolid INTEGER NOT NULL REFERENCES mcp_tool(toolid) ON DELETE CASCADE,
    header_path TEXT NOT NULL,
    namespace TEXT NOT NULL,
    symbol_name TEXT NOT NULL,
    input_signature TEXT NOT NULL,
    output_signature TEXT NOT NULL
);
```

This table does not change core MCP semantics but gives AI agents a structured way to discover C++ eco tool signatures.

### 2.4 Eco‑Plane Hints in Canal Telemetry

Add optional plane hint column to `cyboquatic_workload_telemetry`:

```sql
ALTER TABLE cyboquatic_workload_telemetry
  ADD COLUMN plane_hint TEXT NULL; -- e.g., 'HYDRAULICS','ENERGY','TOPOLOGY','BIODIVERSITY'
```

This helps AI agents and CLIs map telemetry rows to specific planes and KER vectors.

### 2.5 AI‑Chat Playground Annotations

Attach annotations to MCP tools for AI‑chat playground use:

```sql
ALTER TABLE mcp_tool
  ADD COLUMN ai_playground_hint TEXT NULL; -- e.g., 'eco_material', 'pfas_corridor', 'blast_radius'
```

This allows views like `v_ai_chat_eco_playground` to filter or tag tools explicitly for agent usage.

## 3. Summary

These extensions:

- Do not alter existing constraints or semantics for Rust or SQL tools.
- Introduce language hints and C++‑specific metadata in companion tables and columns.
- Promote C++ eco modules to first‑class MCP tools via `CPP_FN` endpoint types and `cpp_tool_meta`.
- Help AI‑chat agents discover and traverse eco artifacts across C++, Rust, SQL, Lua, Kotlin, and Java while preserving non‑actuating, KER‑aligned governance.[78][59]
