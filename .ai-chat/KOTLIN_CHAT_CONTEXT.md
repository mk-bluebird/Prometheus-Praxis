# Kotlin Code AI-Chat Context

This document provides structured context for AI-chat interactions with Kotlin code in this repository.

## Repository Kotlin Files

### Core Kotlin Modules

| File | Package | Purpose | Key Functions/Methods |
|------|---------|---------|----------------------|
| `androidapp/src/main/java/org/econet/CyboquaticSpine.kt` | `org.econet` | Cyboquatic spine JNI wrapper | `nodeBlastRadius()`, `workloadWindow()` |
| `android/app/src/main/java/org/econet/CyboOverlay.kt` | `org.econet` | Cyboquatic overlay interface | Overlay management |
| `android/app/src/main/java/org/econet/BlastRadiusClient.kt` | `org.econet` | Blast radius client JNI wrapper | `listShardsForRegion()`, `listImprovementOk()` |
| `android/app/src/main/java/org/econet/ker/BlastRadiusDao.kt` | `org.econet.ker` | KER database access object | Database operations |
| `android/app/src/main/java/org/econet/CyboNodeScores.kt` | `org.econet` | Node scoring facade | `listEcorestorativeNodes()` |
| `android/app/src/main/java/com/econet/cyboquatic/BlastRadiusInspector.kt` | `com.econet.cyboquatic` | Blast radius inspection | Inspection logic |
| `app/src/main/java/org/econet/blast/BlastRadiusKerInspectorActivity.kt` | `org.econet.blast` | Android Activity for KER inspection | Activity lifecycle |
| `app/src/main/java/org/econet/blast/KerInspectorDbHelper.kt` | `org.econet.blast` | SQLite helper for KER data | Database helper methods |
| `app/src/main/java/org/mkbluebird/cyberquatic/sync/AdaptiveSyncStrategy.kt` | `org.mkbluebird.cyberquatic.sync` | Adaptive sync strategy | `performAdaptiveSync()` |
| `app/src/main/java/org/mkbluebird/cyberquatic/crypto/BostromVerifier.kt` | `org.mkbluebird.cyberquatic.crypto` | Bostrom DID signature verification | `verifySignature()` |

### Kotlin Code Patterns

#### JNI/FFI Pattern (Rust Integration)
```kotlin
object ModuleName {
    init {
        System.loadLibrary("native_library_name")
    }

    @JvmStatic external fun native_function(arg1: String, arg2: Double): String?

    @JvmStatic
    fun wrapper_function(arg1: String, arg2: Double): String? {
        return native_function(arg1, arg2)
    }
}
```

#### Coroutine-based Async Pattern
```kotlin
class AdaptiveSyncStrategy(
    private val api: CyboquaticAPI,
    private val db: KERDatabase,
    private val locationProvider: LocationProvider
) {
    companion object {
        const val SYNC_RADIUS_METERS = 1000.0
        const val BATTERY_THRESHOLD = 20
    }

    suspend fun performAdaptiveSync(): SyncResult {
        // Async operations with coroutine support
        val updates = nearbyNodes.map { node ->
            async { api.getKERScore(node.nodeId) }
        }.awaitAll()
        // Process results
    }
}
```

#### Cryptographic Verification Pattern
```kotlin
class BostromVerifier {
    companion object {
        private const val EXPECTED_PREFIX = "bostrom"
    }

    fun verifySignature(
        message: String,
        signature: String,
        signerDid: String
    ): VerificationResult {
        // Extract address from DID
        // Verify ed25519 signature
        // Return verification result
    }
}
```

### KER Band Integration

All Kotlin modules follow KER (Knowledge, Eco-impact, Risk-of-harm) principles:
- **K ≥ 0.90**: Knowledge preservation
- **E ≥ 0.90**: Eco-impact threshold  
- **R ≤ 0.13**: Risk-of-harm ceiling

### Bostrom DID Integration

Kotlin crypto modules work with Bostrom DIDs:
- Primary DID: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- Secure DID: `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

### Chat Query Templates

Use these templates when querying Kotlin code:

1. **Function Discovery**: "What functions does [module] provide?"
2. **JNI Integration**: "How does [module] interact with native Rust libraries?"
3. **Coroutine Usage**: "Show me async patterns in [module]"
4. **Crypto Operations**: "How are Bostrom DIDs verified in [module]?"
5. **Database Access**: "What DAO methods are available in [module]?"

## AI-Chat Usage Examples

### Example 1: Understanding Module Purpose
```
User: What is the purpose of CyboquaticSpine.kt?
AI: The CyboquaticSpine.kt module provides a Kotlin/Android JNI wrapper 
    around the Rust cdylib "cyboquatic_spine". It exposes two main functions:
    - nodeBlastRadius(dbPath, nodeId): Returns blast radius data for a node
    - workloadWindow(dbPath, nodeId): Returns workload window information
    Both functions load JSON data from native Rust implementations.
```

### Example 2: JNI Integration
```
User: How does BlastRadiusClient.kt interact with Rust?
AI: The module uses System.loadLibrary() to load "econet_blastradius_spine" 
    and defines external functions marked with @JvmStatic external. The 
    public wrapper functions listShardsForRegion() and listImprovementOk() 
    call the native implementations and return JSON strings.
```

### Example 3: Crypto Verification
```
User: How does BostromVerifier.kt verify signatures?
AI: The BostromVerifier class verifies ed25519 signatures against Bostrom DIDs.
    It extracts the address from the DID (removing "bostrom" prefix), hashes 
    the message with SHA-256, decodes the Base58 signature, and uses Ed25519 
    verification to confirm the signature matches the public key derived from 
    the address.
```

## Native Tooling for Kotlin Analysis

Without installing tools, you can analyze Kotlin code using:

```bash
# List all Kotlin functions/methods
grep -n "fun " app/src/main/java/**/*.kt android/**/*.kt

# Find external/JNI declarations
grep -n "external fun" **/*.kt

# Show companion object constants
grep -A3 "companion object" **/*.kt

# Find coroutine usage
grep -n "suspend\|async\|await" **/*.kt

# Count lines per file
wc -l **/*.kt
```

## Related Lua Integration

Kotlin modules often mirror Lua functionality:
- `CyboNodeScores.kt` ↔ `lua/cyboquatic_node_scores.lua`
- `BlastRadiusClient.kt` ↔ `lua/econet_blastradius_client.lua`
- `CyboquaticSpine.kt` ↔ `lua/lua_cyboquatic_spine.lua`

Both use the same underlying Rust cdylibs for native operations.
