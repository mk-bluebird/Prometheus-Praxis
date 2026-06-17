# AI-Chat Cross-Language Integration Guide

This document provides unified context for AI-chat interactions across Lua, Kotlin, and Rust code in this repository.

## Repository Overview

**Primary Purpose**: Ecological restoration through carbon-negative cyboquatic machinery, energy-efficient FOG routing, and ALN superintelligence policy language.

**Key Concepts**:
- **KER Metrics**: Knowledge (K≥0.90), Eco-impact (E≥0.90), Risk-of-harm (R≤0.13)
- **Lyapunov Residual**: V_t = Σ w_j r_j² with safestep monotonicity V_{t+1} ≤ V_t
- **Bostrom DIDs**: Decentralized identifiers for cryptographic signing
- **Non-Actuating**: Default mode; actuation requires explicit RustALN gates

## Language Interoperability Map

### Shared Functionality Across Languages

| Feature | Lua Module | Kotlin Module | Rust Library |
|---------|-----------|---------------|--------------|
| Node Scoring | `lua/cyboquatic_node_scores.lua` | `CyboNodeScores.kt` | `eco_restoration_shard_cybo` |
| Blast Radius Client | `lua/econet_blastradius_client.lua` | `BlastRadiusClient.kt` | `econet_blastradius_spine` |
| Cyboquatic Spine | `lua/lua_cyboquatic_spine.lua` | `CyboquaticSpine.kt` | `cyboquatic_spine` |
| Overlay Management | `lua/econet_overlay.lua` | `CyboOverlay.kt` | - |
| Blast Radius Inspection | `lua/econet_blastradius_inspect.lua` | `BlastRadiusInspector.kt` | - |

### Integration Patterns

#### Pattern 1: Native Library Bridge
```
Lua (FFI) ←→ Rust (cdylib) ←→ Kotlin (JNI)
```

All three languages can access the same Rust native libraries:
- **Lua**: Uses `ffi.load()` to load cdylibs
- **Kotlin**: Uses `System.loadLibrary()` for JNI
- **Rust**: Exports C-compatible functions via `#[no_mangle]` and `extern "C"`

#### Pattern 2: JSON Data Exchange
```
Native Function → JSON String → Parsed in Lua/Kotlin
```

Example flow:
1. Rust function returns `*const c_char` (JSON string)
2. Lua: `ffi.string(ptr)` converts to Lua string
3. Kotlin: Returns `String?` directly
4. Both parse JSON for structured data

#### Pattern 3: Database Access
```
SQLite Database ←→ Rust Core ←→ Lua/Kotlin Wrappers
```

Database paths flow through:
- Kotlin: `dbPath: String` parameter
- Lua: `db_path` string argument
- Rust: Opens SQLite connection internally

## AI-Chat Query Patterns

### Cross-Language Queries

1. **"Show me all implementations of [feature]"**
   ```
   User: Show me all implementations of blast radius functionality
   AI: 
   - Lua: lua/econet_blastradius_client.lua, lua/econet_blastradius_inspect.lua
   - Kotlin: BlastRadiusClient.kt, BlastRadiusInspector.kt, BlastRadiusKerInspectorActivity.kt
   - Rust: crates/econet-blastradius-spine/src/lib.rs
   ```

2. **"How does [language] integrate with native code?"**
   ```
   User: How does Kotlin integrate with native Rust code?
   AI: Kotlin uses JNI via System.loadLibrary() and @JvmStatic external fun 
       declarations. The init block loads the native library, and wrapper 
       functions provide Kotlin-friendly APIs.
   ```

3. **"What are the KER requirements for [module]?"**
   ```
   User: What are the KER requirements for node scoring modules?
   AI: All node scoring modules must maintain K≥0.90 (knowledge preservation), 
       E≥0.90 (eco-impact), and R≤0.13 (risk ceiling). See CyboNodeScores.kt 
       and cyboquatic_node_scores.lua for implementations.
   ```

### Native Analysis Commands

Use these shell commands to analyze code without installing tools:

```bash
# === LUA ANALYSIS ===
# List all Lua functions
grep -rn "^function" lua/ automation/

# Find FFI usage
grep -rn "ffi\." lua/

# Show module returns
grep -n "^return" lua/*.lua

# === KOTLIN ANALYSIS ===
# List all Kotlin functions
find . -name "*.kt" -exec grep -l "fun " {} \;

# Find JNI externals
grep -rn "external fun" **/*.kt

# Show companion objects
grep -A5 "companion object" **/*.kt

# Find coroutine usage
grep -rn "suspend\|async\|await" **/*.kt

# === CROSS-LANGUAGE ===
# Find matching functionality
grep -rn "blast.*radius" lua/ **/*.kt --include="*.lua" --include="*.kt"

# Count lines per language
find . -name "*.lua" -exec wc -l {} + | tail -1
find . -name "*.kt" -exec wc -l {} + | tail -1

# Show file structure
tree -L 3 lua/ android/ app/ 2>/dev/null || find lua/ android/ app/ -type f | head -30
```

## Bostrom DID Reference

### Primary DIDs
- **Primary**: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- **Secure**: `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

### Verification Flow (Kotlin Example)
```kotlin
// 1. Extract address from DID
val address = signerDid.removePrefix("bostrom")

// 2. Hash message
val messageHash = MessageDigest.getInstance("SHA-256")
    .digest(message.toByteArray())

// 3. Decode signature
val signatureBytes = Base58.decode(signature)

// 4. Verify with Ed25519
val isValid = Ed25519.verify(signature, messageHash, publicKey)
```

## ALN Schema Reference

### Frozen Schemas (Must Not Change)

| Schema | Version | Purpose |
|--------|---------|---------|
| `ecosafety.riskvector.v2` | v2 | Core Lyapunov & KER math |
| `ecosafety.corridors.v2` | v2 | Safety band definitions |
| `PlaneWeightsShard2026v1` | v1 | Non-offsettable planes |
| `NonActuatingWorkload` | v1 | Workload monotonicity |
| `ProvenanceKernel2026v1` | v1 | Evidence hash rules |

### Chat Context for ALN
When discussing ALN schemas, always reference:
1. Spec hash integrity
2. Signing DID
3. Evidence chain preservation
4. Migration status (see MIGRATION.md)

## Quick Reference: File Locations

### Lua Files
```
/workspace/lua/
├── econet_overlay.lua
├── cyboquatic_node_scores.lua
├── econet_blastradius_client.lua
├── econet_blastradius_inspect.lua
├── econet_cybo_overlay.lua
├── lua_cyboquatic_overlay.lua
├── lua_cyboquatic_spine.lua
└── cyboquatic_eco_overlay.lua

/workspace/automation/
└── ci_orchestrator.lua

/workspace/crates/econet-hardware/lua/
└── blast_radius_guard.lua
```

### Kotlin Files
```
/workspace/androidapp/src/main/java/org/econet/
└── CyboquaticSpine.kt

/workspace/android/app/src/main/java/org/econet/
├── CyboOverlay.kt
├── BlastRadiusClient.kt
├── ker/BlastRadiusDao.kt
└── CyboNodeScores.kt

/workspace/android/app/src/main/java/com/econet/cyboquatic/
└── BlastRadiusInspector.kt

/workspace/app/src/main/java/org/econet/blast/
├── BlastRadiusKerInspectorActivity.kt
└── KerInspectorDbHelper.kt

/workspace/app/src/main/java/org/mkbluebird/cyberquatic/
├── sync/AdaptiveSyncStrategy.kt
└── crypto/BostromVerifier.kt
```

## Usage Examples

### Example Session 1: Understanding Architecture
```
User: How do Lua and Kotlin share functionality in this repo?
AI: Both languages serve as wrappers around shared Rust cdylibs. For example,
    node scoring is implemented in Rust (eco_restoration_shard_cybo), with
    Lua using FFI (cyboquatic_node_scores.lua) and Kotlin using JNI 
    (CyboNodeScores.kt). This allows cross-platform consistency while 
    supporting both scripting (Lua) and Android (Kotlin) use cases.
```

### Example Session 2: Debugging Integration
```
User: My Lua FFI call is failing. How should I debug it?
AI: Check these steps:
    1. Verify the cdylib exists: ls target/release/libeco_restoration_shard_cybo.so
    2. Check FFI definitions match Rust signatures: grep -A10 "ffi.cdef" lua/*.lua
    3. Ensure proper memory management: lib.econet_cybo_free_json(ptr) after use
    4. Test with simple call first: print(M.list_ecorestorative_nodes("/tmp/test.db", 0.2))
```

### Example Session 3: KER Compliance Check
```
User: What KER bands apply to the adaptive sync strategy?
AI: AdaptiveSyncStrategy.kt must maintain:
    - K ≥ 0.90: Preserve knowledge of node states and sync history
    - E ≥ 0.90: Ensure eco-restorative impact through selective syncing
    - R ≤ 0.13: Limit risk by checking battery (20% threshold) and proximity
    The implementation skips sync when conditions aren't met, preserving corridors.
```
