#!/bin/bash
# =============================================================================
# AI-Chat Platform Code Extractor
# =============================================================================
# Purpose: Extract complete code blocks with dependencies for AI-chat analysis
# Usage:   ./extract.sh <file-path> [options]
# Options:
#   --full          Extract entire file (default)
#   --function NAME Extract specific function and its dependencies
#   --class NAME    Extract specific class/object and dependencies
#   --with-deps     Include imported/required dependencies
#   --json          Output as JSON
#   --markdown      Output as Markdown code block
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
OUTPUT_DIR="$SCRIPT_DIR/output"

mkdir -p "$OUTPUT_DIR"

# Default options
MODE="full"
TARGET=""
INCLUDE_DEPS=false
FORMAT="text"

# Parse arguments
FILE_PATH=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --full) MODE="full"; shift ;;
        --function) MODE="function"; TARGET="$2"; shift 2 ;;
        --class) MODE="class"; TARGET="$2"; shift 2 ;;
        --with-deps) INCLUDE_DEPS=true; shift ;;
        --json) FORMAT="json"; shift ;;
        --markdown) FORMAT="markdown"; shift ;;
        --help)
            echo "Usage: $0 <file-path> [--full|--function NAME|--class NAME] [--with-deps] [--json|--markdown]"
            exit 0
            ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *) FILE_PATH="$1"; shift ;;
    esac
done

if [[ -z "$FILE_PATH" ]]; then
    echo "Error: File path required. Use: $0 <path/to/file.ext> [options]" >&2
    exit 1
fi

# Resolve file path
if [[ ! "$FILE_PATH" = /* ]]; then
    FULL_PATH="$REPO_ROOT/$FILE_PATH"
else
    FULL_PATH="$FILE_PATH"
fi

if [[ ! -f "$FULL_PATH" ]]; then
    echo "Error: File not found: $FULL_PATH" >&2
    exit 1
fi

# Detect language from extension
get_language() {
    local ext="${FILE_PATH##*.}"
    case "$ext" in
        lua) echo "lua" ;;
        kt) echo "kotlin" ;;
        rs) echo "rust" ;;
        *) echo "unknown" ;;
    esac
}

# Extract dependencies (imports/requires)
extract_dependencies() {
    local file="$1"
    local lang="$2"
    
    case "$lang" in
        lua)
            grep -E '^\s*(local\s+)?(require|module|dofile|loadfile)' "$file" 2>/dev/null || true
            ;;
        kotlin)
            grep -E '^\s*(import|package)' "$file" 2>/dev/null || true
            ;;
        rust)
            grep -E '^\s*(use|mod|extern crate)' "$file" 2>/dev/null || true
            ;;
    esac
}

# Extract specific function
extract_function() {
    local file="$1"
    local func_name="$2"
    local lang="$3"
    
    case "$lang" in
        lua)
            # Find function definition and extract until end
            awk -v fname="$func_name" '
                BEGIN { in_func = 0; brace_count = 0 }
                /function\s+/ {
                    if ($0 ~ "function\\s+" fname) {
                        in_func = 1
                        brace_count = 1
                    }
                }
                in_func {
                    print
                    if (/{/) brace_count++
                    if (/}/) brace_count--
                    if (/end/ && brace_count <= 1) {
                        print
                        exit
                    }
                }
            ' "$file" 2>/dev/null || true
            ;;
        kotlin)
            # Extract function with proper brace matching
            awk -v fname="$func_name" '
                BEGIN { in_func = 0; brace_count = 0 }
                /fun\s+/ {
                    if ($0 ~ "fun\\s+" fname) {
                        in_func = 1
                    }
                }
                in_func {
                    print
                    gsub(/[^{]/, "", temp=$0); brace_count += length(temp)
                    gsub(/[^}]/, "", temp=$0); brace_count -= length(temp)
                    if (brace_count <= 0 && /}/) exit
                }
            ' "$file" 2>/dev/null || true
            ;;
        rust)
            # Extract function with proper brace matching
            awk -v fname="$func_name" '
                BEGIN { in_func = 0; brace_count = 0 }
                /fn\s+/ {
                    if ($0 ~ "fn\\s+" fname) {
                        in_func = 1
                    }
                }
                in_func {
                    print
                    gsub(/[^{]/, "", temp=$0); brace_count += length(temp)
                    gsub(/[^}]/, "", temp=$0); brace_count -= length(temp)
                    if (brace_count <= 0 && /}/) exit
                }
            ' "$file" 2>/dev/null || true
            ;;
    esac
}

# Extract class/object
extract_class() {
    local file="$1"
    local class_name="$2"
    local lang="$3"
    
    case "$lang" in
        lua)
            # Extract module or class-like structure
            grep -A 100 "module\|class.*$class_name" "$file" 2>/dev/null | head -150 || true
            ;;
        kotlin)
            # Extract class/object/interface
            awk -v cname="$class_name" '
                BEGIN { in_class = 0; brace_count = 0 }
                /(class|object|interface)\s+/ {
                    if ($0 ~ cname) {
                        in_class = 1
                    }
                }
                in_class {
                    print
                    gsub(/[^{]/, "", temp=$0); brace_count += length(temp)
                    gsub(/[^}]/, "", temp=$0); brace_count -= length(temp)
                    if (brace_count <= 0 && /}/) exit
                }
            ' "$file" 2>/dev/null || true
            ;;
        rust)
            # Extract struct/enum/impl
            awk -v sname="$class_name" '
                BEGIN { in_struct = 0; brace_count = 0 }
                /(struct|enum|impl)\s+/ {
                    if ($0 ~ sname) {
                        in_struct = 1
                    }
                }
                in_struct {
                    print
                    gsub(/[^{]/, "", temp=$0); brace_count += length(temp)
                    gsub(/[^}]/, "", temp=$0); brace_count -= length(temp)
                    if (brace_count <= 0 && /}/) exit
                }
            ' "$file" 2>/dev/null || true
            ;;
    esac
}

# Main extraction
main() {
    local lang=$(get_language)
    local rel_path="${FILE_PATH#$REPO_ROOT/}"
    
    if [[ "$FORMAT" == "json" ]]; then
        echo "{"
        echo "  \"file\": \"$rel_path\","
        echo "  \"language\": \"$lang\","
        echo "  \"mode\": \"$MODE\","
        if [[ -n "$TARGET" ]]; then
            echo "  \"target\": \"$TARGET\","
        fi
        echo "  \"content\": \""
    elif [[ "$FORMAT" == "markdown" ]]; then
        echo "\`\`\`$lang"
        echo "-- File: $rel_path"
        echo "-- Mode: $MODE${TARGET:+ | Target: $TARGET}"
        echo ""
    fi
    
    case "$MODE" in
        full)
            if [[ "$INCLUDE_DEPS" == "true" ]]; then
                echo "-- Dependencies:" >&2
                extract_dependencies "$FULL_PATH" "$lang" >&2
                echo "" >&2
            fi
            cat "$FULL_PATH"
            ;;
        function)
            if [[ -z "$TARGET" ]]; then
                echo "Error: Function name required for --function mode" >&2
                exit 1
            fi
            extract_function "$FULL_PATH" "$TARGET" "$lang"
            ;;
        class)
            if [[ -z "$TARGET" ]]; then
                echo "Error: Class name required for --class mode" >&2
                exit 1
            fi
            extract_class "$FULL_PATH" "$TARGET" "$lang"
            ;;
    esac
    
    if [[ "$FORMAT" == "json" ]]; then
        echo "\","
        echo "  \"lines\": $(wc -l < "$FULL_PATH")"
        echo "}"
    elif [[ "$FORMAT" == "markdown" ]]; then
        echo ""
        echo "\`\`\`"
    fi
}

main
