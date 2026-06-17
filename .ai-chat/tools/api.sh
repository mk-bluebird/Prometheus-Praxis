#!/bin/bash
# =============================================================================
# AI-Chat Platform Unified API Gateway
# =============================================================================
# Purpose: Single entry point for AI-chat platforms to access all repository tools
# Usage:   ./api.sh <action> [parameters...]
# Actions:
#   crawl       Run repository crawler
#   search      Search codebase
#   extract     Extract code blocks
#   index       Generate function/class indices
#   summary     Quick repository summary
#   help        Show this help message
# =============================================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
TOOLS_DIR="$SCRIPT_DIR"
OUTPUT_DIR="$SCRIPT_DIR/output"
DATA_DIR="$SCRIPT_DIR/data"

# Ensure REPO_ROOT is correct (should be /workspace, not /workspace/.ai-chat)
if [[ "$(basename "$REPO_ROOT")" == ".ai-chat" ]]; then
    REPO_ROOT="$(dirname "$REPO_ROOT")"
fi

mkdir -p "$OUTPUT_DIR" "$DATA_DIR"

# Color codes for terminal output (disabled for JSON mode)
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1" >&2
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

# Validate that required tools exist
validate_tools() {
    local missing=()
    for tool in crawler.sh search.sh extract.sh; do
        if [[ ! -x "$TOOLS_DIR/$tool" ]]; then
            missing+=("$tool")
        fi
    done
    
    if [[ ${#missing[@]} -gt 0 ]]; then
        log_warn "Missing executable tools: ${missing[*]}"
        log_info "Run: chmod +x $TOOLS_DIR/*.sh"
        return 1
    fi
    return 0
}

# Action: Crawl
do_crawl() {
    log_info "Running repository crawler..."
    "$TOOLS_DIR/crawler.sh" "$@"
}

# Action: Search
do_search() {
    log_info "Searching repository..."
    "$TOOLS_DIR/search.sh" "$@"
}

# Action: Extract
do_extract() {
    log_info "Extracting code..."
    "$TOOLS_DIR/extract.sh" "$@"
}

# Action: Index
do_index() {
    log_info "Generating indices..."
    
    local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")
    local index_file="$OUTPUT_DIR/function_index.md"
    
    cat > "$index_file" << EOF
# Repository Function Index

**Generated:** $timestamp

---

## Lua Functions

EOF
    
    find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null | while read -r file; do
        local rel_path="${file#$REPO_ROOT/}"
        echo "### $rel_path" >> "$index_file"
        grep -nE '^\s*(local\s+)?function\s+' "$file" 2>/dev/null | head -20 >> "$index_file" || echo "_No functions found_" >> "$index_file"
        echo "" >> "$index_file"
    done
    
    cat >> "$index_file" << EOF

## Kotlin Functions

EOF
    
    find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null | while read -r file; do
        local rel_path="${file#$REPO_ROOT/}"
        echo "### $rel_path" >> "$index_file"
        grep -nE '^\s*(fun|override fun)\s+' "$file" 2>/dev/null | head -20 >> "$index_file" || echo "_No functions found_" >> "$index_file"
        echo "" >> "$index_file"
    done
    
    cat >> "$index_file" << EOF

## Rust Functions

EOF
    
    find "$REPO_ROOT" -name "*.rs" -type f 2>/dev/null | while read -r file; do
        local rel_path="${file#$REPO_ROOT/}"
        echo "### $rel_path" >> "$index_file"
        grep -nE '^\s*pub\s+fn\s+' "$file" 2>/dev/null | head -20 >> "$index_file" || echo "_No functions found_" >> "$index_file"
        echo "" >> "$index_file"
    done
    
    echo "Index generated: $index_file"
}

# Action: Summary
do_summary() {
    local format="${1:-text}"
    
    local total_lua=$(find "$REPO_ROOT" -name "*.lua" -type f 2>/dev/null | wc -l)
    local total_kt=$(find "$REPO_ROOT" -name "*.kt" -type f 2>/dev/null | wc -l)
    local total_rs=$(find "$REPO_ROOT" -name "*.rs" -type f 2>/dev/null | wc -l)
    local total_lines_lua=$(find "$REPO_ROOT" -name "*.lua" -type f -exec cat {} \; 2>/dev/null | wc -l || echo 0)
    local total_lines_kt=$(find "$REPO_ROOT" -name "*.kt" -type f -exec cat {} \; 2>/dev/null | wc -l || echo 0)
    local total_lines_rs=$(find "$REPO_ROOT" -name "*.rs" -type f -exec cat {} \; 2>/dev/null | wc -l || echo 0)
    
    if [[ "$format" == "json" ]]; then
        cat << EOF
{
  "repository": "$(basename "$REPO_ROOT")",
  "generated_at": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")",
  "statistics": {
    "lua": {
      "files": $total_lua,
      "lines": $total_lines_lua
    },
    "kotlin": {
      "files": $total_kt,
      "lines": $total_lines_kt
    },
    "rust": {
      "files": $total_rs,
      "lines": $total_lines_rs
    },
    "total": {
      "files": $((total_lua + total_kt + total_rs)),
      "lines": $((total_lines_lua + total_lines_kt + total_lines_rs))
    }
  },
  "tools_available": [
    "crawler.sh",
    "search.sh",
    "extract.sh",
    "api.sh"
  ],
  "ai_chat_ready": true
}
EOF
    else
        cat << EOF
# Repository Summary

**Repository:** $(basename "$REPO_ROOT")  
**Generated:** $(date -u +"%Y-%m-%dT%H:%M:%SZ")

## Statistics

| Language | Files | Lines of Code |
|----------|-------|---------------|
| Lua      | $total_lua | $total_lines_lua |
| Kotlin   | $total_kt | $total_lines_kt |
| Rust     | $total_rs | $total_lines_rs |
| **Total**| $((total_lua + total_kt + total_rs)) | $((total_lines_lua + total_lines_kt + total_lines_rs)) |

## Available Tools

- **crawler.sh** - Full repository indexing
- **search.sh** - Code search with filters
- **extract.sh** - Code extraction with dependencies
- **api.sh** - Unified API gateway (this tool)

## AI-Chat Integration

This repository is equipped with native tooling for AI-chat platforms:
- No external dependencies required
- POSIX-compatible shell scripts
- JSON and Markdown output formats
- Cross-language pattern recognition

EOF
    fi
}

# Main entry point
main() {
    if [[ $# -eq 0 ]]; then
        echo "AI-Chat Platform API Gateway"
        echo ""
        echo "Usage: $0 <action> [parameters...]"
        echo ""
        echo "Actions:"
        echo "  crawl       Run repository crawler"
        echo "  search      Search codebase"
        echo "  extract     Extract code blocks"
        echo "  index       Generate function/class indices"
        echo "  summary     Quick repository summary"
        echo "  help        Show detailed help"
        echo ""
        echo "Examples:"
        echo "  $0 summary --json"
        echo "  $0 search \"blast_radius\" --lang lua"
        echo "  $0 extract src/example.lua --function myFunction --markdown"
        echo "  $0 crawl --full --json"
        exit 0
    fi
    
    local action="$1"
    shift
    
    case "$action" in
        crawl)
            do_crawl "$@"
            ;;
        search)
            do_search "$@"
            ;;
        extract)
            do_extract "$@"
            ;;
        index)
            do_index
            ;;
        summary)
            do_summary "$@"
            ;;
        help|--help|-h)
            cat << EOF
AI-Chat Platform API Gateway - Detailed Help
=============================================

The API gateway provides unified access to all repository tools for AI-chat platforms.

ACTIONS:

1. crawl - Repository Crawling & Indexing
   Usage: api.sh crawl [options]
   Options:
     --full       Full repository crawl (default)
     --lua        Lua files only
     --kotlin     Kotlin files only
     --rust       Rust files only
     --json       Output as JSON
     --markdown   Output as Markdown
     --summary    Quick summary only
   
   Example: api.sh crawl --full --json

2. search - Code Search Engine
   Usage: api.sh search <query> [options]
   Options:
     --lang lua|kotlin|rust    Filter by language
     --type function|class|module  Filter by code element
     --context N               Include N lines context
     --json                    JSON output
     --count                   Match counts only
     --files                   File list only
   
   Example: api.sh search "verify" --lang kotlin --json

3. extract - Code Extraction
   Usage: api.sh extract <file> [options]
   Options:
     --full          Extract entire file
     --function NAME Extract specific function
     --class NAME    Extract specific class
     --with-deps     Include dependencies
     --json          JSON output
     --markdown      Markdown code block
   
   Example: api.sh extract src/main.kt --function processData --markdown

4. index - Generate Indices
   Usage: api.sh index
   Generates comprehensive function index in Markdown format.

5. summary - Repository Summary
   Usage: api.sh summary [--json]
   Provides quick statistics about the repository.

INTEGRATION NOTES FOR AI-CHAT PLATFORMS:

- All tools use only POSIX-native utilities (bash, grep, find, awk, sed, wc)
- No external dependencies or installations required
- Output formats: JSON (machine-readable) or Markdown (human-readable)
- Safe error handling with fallbacks
- Timestamped outputs for cache invalidation
- Relative paths from repository root

For web-based AI-chat platforms:
- Execute via shell command integration
- Parse JSON outputs for structured data
- Use Markdown outputs for direct display
- Cache results based on timestamps

EOF
            ;;
        *)
            log_error "Unknown action: $action"
            echo "Use '$0 help' for usage information."
            exit 1
            ;;
    esac
}

main "$@"
