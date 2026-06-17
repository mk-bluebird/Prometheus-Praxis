# Lua Code AI-Chat Context

This document provides structured context for AI-chat interactions with Lua code in this repository.

## Repository Lua Files

### Core Lua Modules

| File | Purpose | Key Functions |
|------|---------|---------------|
| `lua/econet_overlay.lua` | Non-actuating Lua overlay for smart-simplicity summaries | `new_repo_summary()`, `rank_artifacts()`, `to_chat_summary()` |
| `lua/cyboquatic_node_scores.lua` | Lua harness for Cyboquatic diagnostics via FFI | `list_ecorestorative_nodes()` |
| `lua/econet_blastradius_client.lua` | Blast radius client for impact analysis | Client interaction functions |
| `lua/econet_blastradius_inspect.lua` | Blast radius inspection utilities | Inspection & analysis functions |
| `lua/econet_cybo_overlay.lua` | Cyboquatic overlay layer | Overlay management |
| `lua/lua_cyboquatic_overlay.lua` | Alternative cyboquatic overlay | Overlay operations |
| `lua/lua_cyboquatic_spine.lua` | Cyboquatic spine interface | Spine connectivity |
| `lua/cyboquatic_eco_overlay.lua` | Eco-focused cyboquatic overlay | Eco-overlay functions |
| `automation/ci_orchestrator.lua` | CI orchestration logic | Orchestration functions |
| `crates/econet-hardware/lua/blast_radius_guard.lua` | Hardware blast radius guard | Guard functions |

### Lua Code Patterns

#### Module Structure
```lua
-- Standard module pattern used in this repository
local ModuleName = {}

local InternalMethods = {}

function InternalMethods:method_name(args)
    -- Implementation
    return result
end

local MetaTable = { __index = InternalMethods }

function ModuleName.new_constructor(row)
    local obj = {
        field1 = row.field1,
        field2 = row.field2,
    }
    return setmetatable(obj, MetaTable)
end

return ModuleName
```

#### FFI Integration Pattern
```lua
local ffi = require("ffi")

ffi.cdef[[
    const char* function_name(const char* arg1, double arg2);
    void free_function(char* ptr);
]]

local lib = ffi.load("library_name")

local M = {}

function M.wrapper_function(arg1, arg2)
    local result = lib.function_name(arg1, arg2)
    -- Process result
    return result
end

return M
```

### KER Band Integration

All Lua modules follow KER (Knowledge, Eco-impact, Risk-of-harm) principles:
- **K ≥ 0.90**: Knowledge preservation
- **E ≥ 0.90**: Eco-impact threshold
- **R ≤ 0.13**: Risk-of-harm ceiling

### Chat Query Templates

Use these templates when querying Lua code:

1. **Function Discovery**: "What functions does [module] provide?"
2. **Pattern Matching**: "Show me examples of [pattern] in Lua code"
3. **Integration**: "How does [module] integrate with Rust/FFI?"
4. **KER Compliance**: "What KER bands apply to [module]?"

## AI-Chat Usage Examples

### Example 1: Understanding Module Purpose
```
User: What is the purpose of econet_overlay.lua?
AI: The econet_overlay.lua module provides non-actuating Lua overlay 
    functionality using tables and metatables for smart-simplicity summaries.
    It includes RepoMethods with to_chat_summary() for generating human-readable
    artifact descriptions including KER band targets.
```

### Example 2: FFI Integration
```
User: How does cyboquatic_node_scores.lua interact with Rust?
AI: The module uses LuaJIT FFI to call into a Rust cdylib named 
    "eco_restoration_shard_cybo". It defines C function signatures 
    for econet_cybo_list_nodes() and econet_cybo_free_json(), then 
    wraps them in a Lua-friendly API.
```

## Native Tooling for Lua Analysis

Without installing tools, you can analyze Lua code using:

```bash
# List all Lua functions
grep -n "^function" lua/*.lua

# Find module exports
grep -n "^local.*=" lua/*.lua | grep -v "function"

# Show FFI definitions
grep -A5 "ffi.cdef" lua/*.lua

# Count lines per file
wc -l lua/*.lua
```
