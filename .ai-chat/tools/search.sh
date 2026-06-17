#!/bin/bash
# =============================================================================
# AI-Chat Platform Search Engine
# =============================================================================
# Purpose: Provide fast, accurate search capabilities for AI-chat platforms
# Usage:   ./search.sh <query> [options]
# Options:
#   --lang lua|kotlin|rust    Filter by language
#   --type function|class|module|pattern  Filter by code element type
#   --context N               Include N lines of context (default: 3)
#   --json                    Output as JSON (default: plain text)
#   --count                   Show match counts only
#   --files                   List matching files only
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
DATA_DIR="$SCRIPT_DIR/data"

mkdir -p "$DATA_DIR"

# Default options
LANG_FILTER=""
TYPE_FILTER=""
CONTEXT_LINES=3
FORMAT="text"
MODE="results"

# Parse arguments
QUERY=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --lang) LANG_FILTER="$2"; shift 2 ;;
        --type) TYPE_FILTER="$2"; shift 2 ;;
        --context) CONTEXT_LINES="$2"; shift 2 ;;
        --json) FORMAT="json"; shift ;;
        --count) MODE="count"; shift ;;
        --files) MODE="files"; shift ;;
        --help)
            echo "Usage: $0 <query> [--lang lua|kotlin|rust] [--type function|class|module] [--context N] [--json|--count|--files]"
            exit 0
            ;;
        -*) echo "Unknown option: $1"; exit 1 ;;
        *) QUERY="$1"; shift ;;
    esac
done

if [[ -z "$QUERY" ]]; then
    echo "Error: Query required. Use: $0 <search-term> [options]" >&2
    exit 1
fi

# Build file extension filter
get_extensions() {
    case "$LANG_FILTER" in
        lua) echo "lua" ;;
        kotlin) echo "kt" ;;
        rust) echo "rs" ;;
        *) echo "lua kt rs" ;;
    esac
}

# Build grep pattern based on type filter
get_pattern() {
    local base_pattern="$QUERY"
    
    case "$TYPE_FILTER" in
        function)
            # Pattern for functions across languages
            echo "(function\s+$base_pattern|fun\s+$base_pattern|fn\s+$base_pattern)"
            ;;
        class)
            # Pattern for classes/objects
            echo "(class\s+$base_pattern|object\s+$base_pattern|struct\s+$base_pattern)"
            ;;
        module)
            # Pattern for modules
            echo "(module\($base_pattern|mod\s+$base_pattern|package.*$base_pattern)"
            ;;
        *)
            echo "$base_pattern"
            ;;
    esac
}

# Search with context
search_with_context() {
    local pattern="$1"
    local ext="$2"
    local output_format="$3"
    
    local results=""
    local match_count=0
    
    while IFS= read -r file; do
        if [[ -f "$file" ]]; then
            local rel_path="${file#$REPO_ROOT/}"
            
            # Perform search with context
            local matches=$(grep -n -B "$CONTEXT_LINES" -A "$CONTEXT_LINES" -i "$pattern" "$file" 2>/dev/null || true)
            
            if [[ -n "$matches" ]]; then
                local file_matches=$(echo "$matches" | grep -c "^" || echo "0")
                match_count=$((match_count + file_matches))
                
                if [[ "$MODE" == "files" ]]; then
                    echo "$rel_path"
                elif [[ "$output_format" == "json" ]]; then
                    # Escape special characters for JSON
                    local escaped_matches=$(echo "$matches" | sed 's/\\/\\\\/g' | sed 's/"/\\"/g' | sed ':a;N;$!ba;s/\n/\\n/g')
                    echo "{\"file\": \"$rel_path\", \"matches\": $file_matches, \"content\": \"$escaped_matches\"}"
                else
                    echo "=== File: $rel_path ($file_matches matches) ==="
                    echo "$matches"
                    echo ""
                fi
            fi
        fi
    done < <(find "$REPO_ROOT" -name "*.$ext" -type f 2>/dev/null)
    
    if [[ "$MODE" == "count" ]]; then
        echo "$match_count"
    fi
}

# Main search execution
main() {
    local pattern=$(get_pattern)
    local extensions=$(get_extensions)
    
    if [[ "$FORMAT" == "json" ]]; then
        echo "{"
        echo "  \"query\": \"$QUERY\","
        echo "  \"filters\": {"
        echo "    \"language\": \"${LANG_FILTER:-all}\","
        echo "    \"type\": \"${TYPE_FILTER:-any}\""
        echo "  },"
        echo "  \"results\": ["
    else
        echo "# Search Results for: $QUERY"
        echo ""
        echo "Language: ${LANG_FILTER:-all} | Type: ${TYPE_FILTER:-any} | Context: $CONTEXT_LINES lines"
        echo ""
        echo "---"
        echo ""
    fi
    
    local first=true
    for ext in $extensions; do
        local result=$(search_with_context "$pattern" "$ext" "$FORMAT")
        
        if [[ -n "$result" ]]; then
            if [[ "$FORMAT" == "json" ]] && [[ "$first" != "true" ]]; then
                echo ","
            fi
            first=false
            echo "$result"
        fi
    done
    
    if [[ "$FORMAT" == "json" ]]; then
        echo "  ]"
        echo "}"
    fi
}

main
