#!/bin/bash
# =============================================================================
# AI-Chat Platform Crawler & Indexer
# =============================================================================
# Purpose: Generate structured, machine-readable indices for AI-chat platforms
# Usage:   ./crawler.sh [options]
# Options:
#   --full       Full repository crawl (default)
#   --lua        Lua files only
#   --kotlin     Kotlin files only
#   --rust       Rust files only
#   --json       Output as JSON
#   --markdown   Output as Markdown (default)
#   --summary    Quick summary only
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
OUTPUT_DIR="$SCRIPT_DIR/output"
DATA_DIR="$SCRIPT_DIR/data"

# Ensure directories exist
mkdir -p "$OUTPUT_DIR" "$DATA_DIR"

# Default options
MODE="full"
FORMAT="markdown"
SCOPE="summary"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --full) MODE="full"; shift ;;
        --lua) MODE="lua"; shift ;;
        --kotlin) MODE="kotlin"; shift ;;
        --rust) MODE="rust"; shift ;;
        --json) FORMAT="json"; shift ;;
        --markdown) FORMAT="markdown"; shift ;;
        --summary) SCOPE="summary"; shift ;;
        --detailed) SCOPE="detailed"; shift ;;
        --help)
            echo "Usage: $0 [--full|--lua|--kotlin|--rust] [--json|--markdown] [--summary|--detailed]"
            exit 0
            ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# Timestamp for reports
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# =============================================================================
# Core Crawler Functions
# =============================================================================

crawl_lua_files() {
    local output_file="$1"
    echo "Crawling Lua files..." >&2
    
    if [[ "$FORMAT" == "json" ]]; then
        echo '  {"type": "lua_files", "files": [' >> "$output_file"
        local first=true
        find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local func_count=$(grep -cE '^\s*(local\s+)?function\s+' "$file" 2>/dev/null || echo "0")
            
            if [[ "$first" != "true" ]]; then
                echo ',' >> "$output_file"
            fi
            first=false
            
            cat >> "$output_file" << EOF
    {
      "path": "$rel_path",
      "lines": $line_count,
      "functions": $func_count,
      "has_ffi": $(grep -q 'ffi\.' "$file" 2>/dev/null && echo "true" || echo "false"),
      "has_module": $(grep -q 'module\|require' "$file" 2>/dev/null && echo "true" || echo "false")
    }
EOF
        done
        echo '  ]}' >> "$output_file"
    else
        echo "## Lua Files Index" >> "$output_file"
        echo "" >> "$output_file"
        find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local func_count=$(grep -cE '^\s*(local\s+)?function\s+' "$file" 2>/dev/null || echo "0")
            local has_ffi=$(grep -q 'ffi\.' "$file" 2>/dev/null && echo "Yes" || echo "No")
            
            echo "- **$rel_path**" >> "$output_file"
            echo "  - Lines: $line_count | Functions: $func_count | FFI: $has_ffi" >> "$output_file"
        done
        echo "" >> "$output_file"
    fi
}

crawl_kotlin_files() {
    local output_file="$1"
    echo "Crawling Kotlin files..." >&2
    
    if [[ "$FORMAT" == "json" ]]; then
        echo '  {"type": "kotlin_files", "files": [' >> "$output_file"
        local first=true
        find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local class_count=$(grep -cE '(class|object|interface)\s+\w+' "$file" 2>/dev/null || echo "0")
            local fun_count=$(grep -cE '^\s*(fun|override fun)' "$file" 2>/dev/null || echo "0")
            
            if [[ "$first" != "true" ]]; then
                echo ',' >> "$output_file"
            fi
            first=false
            
            cat >> "$output_file" << EOF
    {
      "path": "$rel_path",
      "lines": $line_count,
      "classes": $class_count,
      "functions": $fun_count,
      "has_coroutines": $(grep -q 'suspend\|CoroutineScope' "$file" 2>/dev/null && echo "true" || echo "false"),
      "has_jni": $(grep -q 'external\|System.loadLibrary' "$file" 2>/dev/null && echo "true" || echo "false")
    }
EOF
        done
        echo '  ]}' >> "$output_file"
    else
        echo "## Kotlin Files Index" >> "$output_file"
        echo "" >> "$output_file"
        find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local class_count=$(grep -cE '(class|object|interface)\s+\w+' "$file" 2>/dev/null || echo "0")
            local has_coroutines=$(grep -q 'suspend\|CoroutineScope' "$file" 2>/dev/null && echo "Yes" || echo "No")
            
            echo "- **$rel_path**" >> "$output_file"
            echo "  - Lines: $line_count | Classes: $class_count | Coroutines: $has_coroutines" >> "$output_file"
        done
        echo "" >> "$output_file"
    fi
}

crawl_rust_files() {
    local output_file="$1"
    echo "Crawling Rust files..." >&2
    
    if [[ "$FORMAT" == "json" ]]; then
        echo '  {"type": "rust_files", "files": [' >> "$output_file"
        local first=true
        find "$REPO_ROOT" -name "*.rs" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local pub_count=$(grep -cE '^\s*pub\s+(fn|struct|enum|mod)' "$file" 2>/dev/null || echo "0")
            
            if [[ "$first" != "true" ]]; then
                echo ',' >> "$output_file"
            fi
            first=false
            
            cat >> "$output_file" << EOF
    {
      "path": "$rel_path",
      "lines": $line_count,
      "public_items": $pub_count,
      "is_cdylib": $(grep -q '#\[no_mangle\]\|extern.*"C"' "$file" 2>/dev/null && echo "true" || echo "false"),
      "has_serde": $(grep -q 'serde\|Serialize\|Deserialize' "$file" 2>/dev/null && echo "true" || echo "false")
    }
EOF
        done
        echo '  ]}' >> "$output_file"
    else
        echo "## Rust Files Index" >> "$output_file"
        echo "" >> "$output_file"
        find "$REPO_ROOT" -name "*.rs" -type f 2>/dev/null | while read -r file; do
            local rel_path="${file#$REPO_ROOT/}"
            local line_count=$(wc -l < "$file" 2>/dev/null || echo "0")
            local is_cdylib=$(grep -q '#\[no_mangle\]\|extern.*"C"' "$file" 2>/dev/null && echo "Yes" || echo "No")
            
            echo "- **$rel_path**" >> "$output_file"
            echo "  - Lines: $line_count | CDyLib Export: $is_cdylib" >> "$output_file"
        done
        echo "" >> "$output_file"
    fi
}

generate_function_index() {
    local lang="$1"
    local output_file="$2"
    local pattern=""
    local ext=""
    
    case "$lang" in
        lua)
            ext="lua"
            pattern='^\s*(local\s+)?function\s+(\w+)'
            ;;
        kotlin)
            ext="kt"
            pattern='^\s*(fun|override fun)\s+(\w+)'
            ;;
        rust)
            ext="rs"
            pattern='^\s*pub\s+fn\s+(\w+)'
            ;;
    esac
    
    echo "Generating $lang function index..." >&2
    
    find "$REPO_ROOT" -name "*.$ext" -type f 2>/dev/null | while read -r file; do
        local rel_path="${file#$REPO_ROOT/}"
        grep -nE "$pattern" "$file" 2>/dev/null | while IFS=: read -r line_num content; do
            local func_name=$(echo "$content" | sed -E 's/.*function\s+(\w+).*/\1/' | sed -E 's/.*fun\s+(\w+).*/\1/' | sed -E 's/.*fn\s+(\w+).*/\1/')
            echo "$rel_path:$line_num:$func_name" >> "$output_file"
        done
    done
}

generate_cross_references() {
    local output_file="$1"
    echo "Generating cross-references..." >&2
    
    # Find common patterns across languages
    local patterns=("blast" "radius" "ker" "bostrom" "did" "crypto" "hash" "verify")
    
    if [[ "$FORMAT" == "json" ]]; then
        echo '  {"type": "cross_references", "patterns": [' >> "$output_file"
        local first=true
        for pattern in "${patterns[@]}"; do
            local lua_matches=$(find "$REPO_ROOT" -name "*.lua" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            local kt_matches=$(find "$REPO_ROOT" -name "*.kt" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            local rs_matches=$(find "$REPO_ROOT" -name "*.rs" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            
            if [[ "$first" != "true" ]]; then
                echo ',' >> "$output_file"
            fi
            first=false
            
            cat >> "$output_file" << EOF
    {
      "pattern": "$pattern",
      "lua_files": $lua_matches,
      "kotlin_files": $kt_matches,
      "rust_files": $rs_matches
    }
EOF
        done
        echo '  ]}' >> "$output_file"
    else
        echo "## Cross-Language Pattern References" >> "$output_file"
        echo "" >> "$output_file"
        echo "| Pattern | Lua Files | Kotlin Files | Rust Files |" >> "$output_file"
        echo "|---------|-----------|--------------|------------|" >> "$output_file"
        for pattern in "${patterns[@]}"; do
            local lua_matches=$(find "$REPO_ROOT" -name "*.lua" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            local kt_matches=$(find "$REPO_ROOT" -name "*.kt" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            local rs_matches=$(find "$REPO_ROOT" -name "*.rs" -type f -exec grep -li "$pattern" {} \; 2>/dev/null | wc -l)
            echo "| $pattern | $lua_matches | $kt_matches | $rs_matches |" >> "$output_file"
        done
        echo "" >> "$output_file"
    fi
}

# =============================================================================
# Main Execution
# =============================================================================

main() {
    local report_file="$OUTPUT_DIR/crawl_report"
    if [[ "$FORMAT" == "json" ]]; then
        report_file="${report_file}.json"
    else
        report_file="${report_file}.md"
    fi
    
    echo "AI-Chat Platform Crawler - Starting at $TIMESTAMP" >&2
    echo "Mode: $MODE | Format: $FORMAT | Scope: $SCOPE" >&2
    
    # Initialize output file
    if [[ "$FORMAT" == "json" ]]; then
        cat > "$report_file" << EOF
{
  "metadata": {
    "generated_at": "$TIMESTAMP",
    "repository": "$(basename "$REPO_ROOT")",
    "mode": "$MODE",
    "scope": "$SCOPE"
  },
  "indices": [
EOF
    else
        cat > "$report_file" << EOF
# AI-Chat Platform Repository Index

**Generated:** $TIMESTAMP  
**Repository:** $(basename "$REPO_ROOT")  
**Mode:** $MODE | **Format:** $FORMAT | **Scope:** $SCOPE

---

EOF
    fi
    
    # Execute crawls based on mode
    case "$MODE" in
        full)
            crawl_lua_files "$report_file"
            crawl_kotlin_files "$report_file"
            crawl_rust_files "$report_file"
            generate_cross_references "$report_file"
            ;;
        lua)
            crawl_lua_files "$report_file"
            generate_function_index "lua" "$DATA_DIR/lua_functions.idx"
            ;;
        kotlin)
            crawl_kotlin_files "$report_file"
            generate_function_index "kotlin" "$DATA_DIR/kotlin_functions.idx"
            ;;
        rust)
            crawl_rust_files "$report_file"
            generate_function_index "rust" "$DATA_DIR/rust_functions.idx"
            ;;
    esac
    
    # Close JSON structure if needed
    if [[ "$FORMAT" == "json" ]]; then
        echo '  ]' >> "$report_file"
        echo '}' >> "$report_file"
    fi
    
    # Generate quick summary
    if [[ "$SCOPE" != "summary" ]] || [[ "$MODE" == "full" ]]; then
        local total_lua=$(find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null | wc -l)
        local total_kt=$(find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null | wc -l)
        local total_rs=$(find "$REPO_ROOT" -name "*.rs" -type f 2>/dev/null | wc -l)
        
        if [[ "$FORMAT" == "json" ]]; then
            # Append summary to JSON
            sed -i.bak 's/  ]$/  ],\n  "summary": {\n    "total_lua_files": '"$total_lua"',\n    "total_kotlin_files": '"$total_kt"',\n    "total_rust_files": '"$total_rs"'\n  }\n}/' "$report_file"
            rm -f "${report_file}.bak"
        else
            echo "---" >> "$report_file"
            echo "## Summary" >> "$report_file"
            echo "- Lua files: $total_lua" >> "$report_file"
            echo "- Kotlin files: $total_kt" >> "$report_file"
            echo "- Rust files: $total_rs" >> "$report_file"
        fi
    fi
    
    echo "Crawl complete. Report saved to: $report_file" >&2
    echo "$report_file"
}

main "$@"
