# AI-Chat Integration for EcoNet Repository

This directory provides AI-chat functionality for Lua and Kotlin code within the repository, using only native tooling (no installations required).

## Contents

| File | Purpose |
|------|---------|
| `LUA_CHAT_CONTEXT.md` | Structured context for AI-chat interactions with Lua code |
| `KOTLIN_CHAT_CONTEXT.md` | Structured context for AI-chat interactions with Kotlin code |
| `CROSS_LANGUAGE_GUIDE.md` | Unified guide for cross-language AI-chat queries |
| `analyze_code.sh` | Native shell script for automated code analysis |
| `output/` | Generated analysis reports (created on script execution) |

## Quick Start

### For AI Assistants

When users ask about Lua or Kotlin code in this repository:

1. **Reference the context files**:
   - Lua questions → See `LUA_CHAT_CONTEXT.md`
   - Kotlin questions → See `KOTLIN_CHAT_CONTEXT.md`
   - Cross-language questions → See `CROSS_LANGUAGE_GUIDE.md`

2. **Use native analysis commands**:
   ```bash
   # Run the analysis script
   cd /workspace/.ai-chat && ./analyze_code.sh
   
   # Or use direct grep/find commands from the context files
   ```

3. **Follow KER principles** in responses:
   - Knowledge preservation (K ≥ 0.90)
   - Eco-impact awareness (E ≥ 0.90)
   - Risk mitigation (R ≤ 0.13)

### For Developers

```bash
# Generate fresh analysis
cd /workspace/.ai-chat
./analyze_code.sh

# View generated reports
cat output/analysis_summary.txt
cat output/lua_functions.txt
cat output/kotlin_jni.txt
```

## Key Concepts

### KER Metrics
All code follows KER (Knowledge, Eco-impact, Risk-of-harm) principles:
- **K ≥ 0.90**: Knowledge preservation threshold
- **E ≥ 0.90**: Eco-impact minimum
- **R ≤ 0.13**: Risk-of-harm ceiling

### Bostrom DIDs
Cryptographic identifiers used throughout:
- Primary: `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`
- Secure: `bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc`

### Language Interop Pattern
```
Lua (FFI) ←→ Rust (cdylib) ←→ Kotlin (JNI)
```

Both Lua and Kotlin serve as wrappers around shared Rust native libraries.

## Native Tooling Commands

No tools need to be installed. Use these built-in commands:

```bash
# === LUA ===
grep -n "^function" lua/*.lua          # List functions
grep -A5 "ffi.cdef" lua/*.lua          # Show FFI definitions
wc -l lua/*.lua                         # Count lines

# === KOTLIN ===
grep -n "fun " **/*.kt                  # List functions
grep -n "external fun" **/*.kt         # Find JNI declarations
grep -A3 "companion object" **/*.kt    # Show constants

# === CROSS-LANGUAGE ===
grep -rn "blast.*radius" --include="*.lua" --include="*.kt" .
find . -name "*.lua" -o -name "*.kt" | xargs wc -l
```

## AI-Chat Query Templates

### Lua Queries
- "What functions does [module.lua] provide?"
- "How does [module.lua] use FFI?"
- "Show me the module pattern in [file]"

### Kotlin Queries
- "What JNI methods does [Module.kt] expose?"
- "How are coroutines used in [file]?"
- "What Bostrom DID operations are in [module]?"

### Cross-Language Queries
- "Compare Lua and Kotlin implementations of [feature]"
- "Show all blast radius code across languages"
- "How do both languages integrate with Rust?"

## Output Files

After running `analyze_code.sh`, these files are generated in `output/`:

| File | Content |
|------|---------|
| `analysis_summary.txt` | Complete analysis report |
| `lua_functions.txt` | All Lua function definitions |
| `lua_ffi.txt` | Lua FFI C definitions |
| `kotlin_functions.txt` | All Kotlin function definitions |
| `kotlin_jni.txt` | Kotlin JNI external declarations |
| `kotlin_companions.txt` | Kotlin companion object constants |
| `kotlin_coroutines.txt` | Kotlin coroutine usage |
| `blast_radius.txt` | Cross-language blast radius code |
| `cyboquatic.txt` | Cyboquatic-related code references |
| `ker_code.txt` | KER metric implementations |
| `line_counts.txt` | Code statistics by language |

## Compatibility Notes

- **No installations required**: Uses only bash, grep, find, wc, cat
- **Portable**: Works on any POSIX-compliant system
- **Extensible**: Add new patterns to `analyze_code.sh` as needed
- **AI-friendly**: Structured markdown for easy LLM consumption

## Related Documentation

- Main README: `/workspace/README.md`
- Migration guide: `/workspace/MIGRATION.md`
- ALN specs: `/workspace/aln-specs/`
