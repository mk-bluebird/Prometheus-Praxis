#!/bin/bash
# .ai-chat/analyze_code.sh
# Native code analysis script for AI-chat context generation
# Uses only built-in shell tools (grep, find, wc, cat, etc.)

set -e

REPO_ROOT="${REPO_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
OUTPUT_DIR="$REPO_ROOT/.ai-chat/output"

mkdir -p "$OUTPUT_DIR"

echo "=== EcoNet Repository Code Analysis ===" | tee "$OUTPUT_DIR/analysis_summary.txt"
echo "Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "Repository: $REPO_ROOT" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

# === LUA ANALYSIS ===
echo "--- LUA MODULES ---" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
LUA_FILES=$(find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null)
LUA_COUNT=$(echo "$LUA_FILES" | grep -c . || echo 0)
echo "Total Lua files: $LUA_COUNT" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

if [ "$LUA_COUNT" -gt 0 ]; then
    echo "Lua Functions:" | tee -a "$OUTPUT_DIR/lua_functions.txt"
    echo "$LUA_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            relpath="${file#$REPO_ROOT/}"
            funcs=$(grep -n "^function\|^local.*function" "$file" 2>/dev/null || true)
            if [ -n "$funcs" ]; then
                echo "File: $relpath" >> "$OUTPUT_DIR/lua_functions.txt"
                echo "$funcs" >> "$OUTPUT_DIR/lua_functions.txt"
                echo "" >> "$OUTPUT_DIR/lua_functions.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/lua_functions.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    
    echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    echo "Lua FFI Usage:" | tee -a "$OUTPUT_DIR/lua_ffi.txt"
    echo "$LUA_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            ffi_defs=$(grep -A5 "ffi.cdef" "$file" 2>/dev/null || true)
            if [ -n "$ffi_defs" ]; then
                relpath="${file#$REPO_ROOT/}"
                echo "File: $relpath" >> "$OUTPUT_DIR/lua_ffi.txt"
                echo "$ffi_defs" >> "$OUTPUT_DIR/lua_ffi.txt"
                echo "" >> "$OUTPUT_DIR/lua_ffi.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/lua_ffi.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
fi

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

# === KOTLIN ANALYSIS ===
echo "--- KOTLIN MODULES ---" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
KT_FILES=$(find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null)
KT_COUNT=$(echo "$KT_FILES" | grep -c . || echo 0)
echo "Total Kotlin files: $KT_COUNT" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

if [ "$KT_COUNT" -gt 0 ]; then
    echo "Kotlin Functions:" | tee -a "$OUTPUT_DIR/kotlin_functions.txt"
    echo "$KT_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            relpath="${file#$REPO_ROOT/}"
            funcs=$(grep -n "fun \|external fun\|suspend fun" "$file" 2>/dev/null || true)
            if [ -n "$funcs" ]; then
                echo "File: $relpath" >> "$OUTPUT_DIR/kotlin_functions.txt"
                echo "$funcs" >> "$OUTPUT_DIR/kotlin_functions.txt"
                echo "" >> "$OUTPUT_DIR/kotlin_functions.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/kotlin_functions.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    
    echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    echo "Kotlin JNI Externals:" | tee -a "$OUTPUT_DIR/kotlin_jni.txt"
    echo "$KT_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            externals=$(grep -n "external fun" "$file" 2>/dev/null || true)
            if [ -n "$externals" ]; then
                relpath="${file#$REPO_ROOT/}"
                echo "File: $relpath" >> "$OUTPUT_DIR/kotlin_jni.txt"
                echo "$externals" >> "$OUTPUT_DIR/kotlin_jni.txt"
                echo "" >> "$OUTPUT_DIR/kotlin_jni.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/kotlin_jni.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    
    echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    echo "Kotlin Companion Objects:" | tee -a "$OUTPUT_DIR/kotlin_companions.txt"
    echo "$KT_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            companions=$(grep -A3 "companion object" "$file" 2>/dev/null || true)
            if [ -n "$companions" ]; then
                relpath="${file#$REPO_ROOT/}"
                echo "File: $relpath" >> "$OUTPUT_DIR/kotlin_companions.txt"
                echo "$companions" >> "$OUTPUT_DIR/kotlin_companions.txt"
                echo "" >> "$OUTPUT_DIR/kotlin_companions.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/kotlin_companions.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    
    echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
    echo "Kotlin Coroutines:" | tee -a "$OUTPUT_DIR/kotlin_coroutines.txt"
    echo "$KT_FILES" | while read -r file; do
        if [ -f "$file" ]; then
            coroutines=$(grep -n "suspend\|async\|awaitAll\|launch" "$file" 2>/dev/null || true)
            if [ -n "$coroutines" ]; then
                relpath="${file#$REPO_ROOT/}"
                echo "File: $relpath" >> "$OUTPUT_DIR/kotlin_coroutines.txt"
                echo "$coroutines" >> "$OUTPUT_DIR/kotlin_coroutines.txt"
                echo "" >> "$OUTPUT_DIR/kotlin_coroutines.txt"
            fi
        fi
    done
    cat "$OUTPUT_DIR/kotlin_coroutines.txt" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
fi

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

# === CROSS-LANGUAGE PATTERNS ===
echo "--- CROSS-LANGUAGE INTEGRATION ---" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

echo "Blast Radius Implementations:" | tee -a "$OUTPUT_DIR/blast_radius.txt"
grep -rn "blast.*radius\|BlastRadius" "$REPO_ROOT" --include="*.lua" --include="*.kt" 2>/dev/null | head -30 | tee -a "$OUTPUT_DIR/blast_radius.txt"

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "Cyboquatic Implementations:" | tee -a "$OUTPUT_DIR/cyboquatic.txt"
grep -rn "cyboquatic\|Cyboquatic\|cybo" "$REPO_ROOT" --include="*.lua" --include="*.kt" 2>/dev/null | head -30 | tee -a "$OUTPUT_DIR/cyboquatic.txt"

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "KER-related Code:" | tee -a "$OUTPUT_DIR/ker_code.txt"
grep -rn "kerband\|KER\|KerBand" "$REPO_ROOT" --include="*.lua" --include="*.kt" 2>/dev/null | head -20 | tee -a "$OUTPUT_DIR/ker_code.txt"

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"

# === LINE COUNTS ===
echo "--- CODE STATISTICS ---" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "Lua lines:" | tee -a "$OUTPUT_DIR/line_counts.txt"
find "$REPO_ROOT" -name "*.lua" -type f -exec wc -l {} + 2>/dev/null | tail -1 | tee -a "$OUTPUT_DIR/line_counts.txt"

echo "Kotlin lines:" | tee -a "$OUTPUT_DIR/line_counts.txt"
find "$REPO_ROOT" -name "*.kt" -type f -exec wc -l {} + 2>/dev/null | tail -1 | tee -a "$OUTPUT_DIR/line_counts.txt"

echo "" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "=== Analysis Complete ===" | tee -a "$OUTPUT_DIR/analysis_summary.txt"
echo "Output files written to: $OUTPUT_DIR"
