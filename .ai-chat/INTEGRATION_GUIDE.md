# AI-Chat Platform Integration Guide

## Overview

This repository includes native tooling designed for AI-chat platform integration. All tools use only POSIX-compatible shell utilities (bash, grep, find, awk, sed, wc) requiring **no external installations**.

## Quick Start

### For AI-Chat Platforms

```bash
# Get repository summary in JSON format
./.ai-chat/tools/api.sh summary --json

# Crawl entire repository
./.ai-chat/tools/api.sh crawl --full --json

# Search for specific patterns
./.ai-chat/tools/api.sh search "verify" --lang kotlin --json

# Extract specific code blocks
./.ai-chat/tools/api.sh extract src/main.kt --function processData --markdown
```

### For Web-Based AI-Chat Interfaces

The tools support two output formats optimized for different use cases:

1. **JSON** - Machine-readable, ideal for parsing and structured data extraction
2. **Markdown** - Human-readable, ideal for direct display in chat interfaces

## Tool Catalog

### 1. API Gateway (`api.sh`)

**Purpose:** Unified entry point for all repository operations

**Actions:**
| Action | Description | Example |
|--------|-------------|---------|
| `crawl` | Full repository indexing | `api.sh crawl --full --json` |
| `search` | Code search with filters | `api.sh search "blast" --lang lua` |
| `extract` | Code block extraction | `api.sh extract file.lua --function foo` |
| `index` | Generate function indices | `api.sh index` |
| `summary` | Repository statistics | `api.sh summary --json` |

### 2. Crawler (`crawler.sh`)

**Purpose:** Generate comprehensive repository indices

**Options:**
```bash
./crawler.sh --full --json          # Full crawl, JSON output
./crawler.sh --lua --markdown       # Lua files only, Markdown
./crawler.sh --kotlin --json        # Kotlin files only, JSON
./crawler.sh --rust --markdown      # Rust files only, Markdown
./crawler.sh --summary              # Quick summary
```

**Output Structure (JSON):**
```json
{
  "metadata": {
    "generated_at": "2024-01-15T10:30:00Z",
    "repository": "repo-name",
    "mode": "full",
    "scope": "detailed"
  },
  "indices": [
    {"type": "lua_files", "files": [...]},
    {"type": "kotlin_files", "files": [...]},
    {"type": "rust_files", "files": [...]},
    {"type": "cross_references", "patterns": [...]}
  ],
  "summary": {
    "total_lua_files": 10,
    "total_kotlin_files": 8,
    "total_rust_files": 5
  }
}
```

### 3. Search Engine (`search.sh`)

**Purpose:** Fast, accurate code search with context

**Options:**
```bash
./search.sh "pattern" --lang lua           # Search Lua files
./search.sh "verify" --type function       # Search functions only
./search.sh "crypto" --context 5 --json    # 5 lines context, JSON output
./search.sh "KER" --count                  # Match count only
./search.sh "bostrom" --files              # File list only
```

**Filters:**
- `--lang`: `lua`, `kotlin`, `rust`
- `--type`: `function`, `class`, `module`, `pattern`
- `--context`: Number of surrounding lines (default: 3)

### 4. Code Extractor (`extract.sh`)

**Purpose:** Extract complete code blocks with dependencies

**Options:**
```bash
./extract.sh src/module.lua --full                    # Entire file
./extract.sh src/module.lua --function myFunc         # Specific function
./extract.sh src/Class.kt --class MyClass             # Specific class
./extract.sh src/lib.rs --with-deps --markdown        # With dependencies
```

**Output Formats:**
- Plain text (default)
- JSON (structured with metadata)
- Markdown (code blocks with language hints)

## Integration Patterns

### Pattern 1: Pre-Chat Repository Analysis

Before starting a chat session, AI platforms can:

```bash
# Generate baseline index
./.ai-chat/tools/api.sh crawl --full --json > /tmp/repo_index.json

# Cache function indices
./.ai-chat/tools/api.sh index

# Store summary for quick reference
./.ai-chat/tools/api.sh summary --json > /tmp/repo_summary.json
```

### Pattern 2: Context-Aware Search During Chat

When users ask questions about specific code:

```bash
# User: "Show me the verify function in Kotlin"
./.ai-chat/tools/api.sh search "verify" --lang kotlin --type function --json

# User: "What files contain blast_radius?"
./.ai-chat/tools/api.sh search "blast_radius" --files
```

### Pattern 3: Code Extraction for Analysis

When deep analysis is needed:

```bash
# User: "Analyze the KER implementation in Lua"
./.ai-chat/tools/api.sh extract src/ker.lua --full --with-deps --markdown
```

### Pattern 4: Cross-Language Research

For understanding patterns across languages:

```bash
# Find all crypto-related code
./.ai-chat/tools/api.sh search "crypto" --json

# Compare implementations
./.ai-chat/tools/api.sh search "hash" --lang lua --json
./.ai-chat/tools/api.sh search "hash" --lang kotlin --json
./.ai-chat/tools/api.sh search "hash" --lang rust --json
```

## Output Specifications

### JSON Schema (Crawler)

```typescript
interface CrawlResult {
  metadata: {
    generated_at: string;      // ISO 8601 timestamp
    repository: string;         // Repository name
    mode: string;               // "full" | "lua" | "kotlin" | "rust"
    scope: string;              // "summary" | "detailed"
  };
  indices: Array<{
    type: string;
    files?: Array<{
      path: string;
      lines: number;
      functions?: number;
      classes?: number;
      has_ffi?: boolean;
      has_coroutines?: boolean;
      has_jni?: boolean;
      is_cdylib?: boolean;
    }>;
    patterns?: Array<{
      pattern: string;
      lua_files: number;
      kotlin_files: number;
      rust_files: number;
    }>;
  }>;
  summary: {
    total_lua_files: number;
    total_kotlin_files: number;
    total_rust_files: number;
  };
}
```

### Markdown Format (Search Results)

```markdown
# Search Results for: <query>

Language: <lang> | Type: <type> | Context: <N> lines

---

=== File: <relative-path> (<N> matches) ===
<line-num>: <content>
<context-before>
<match>
<context-after>
```

## Error Handling

All tools implement safe error handling:

- Missing files: Graceful fallback with warning messages
- No matches: Empty result sets (not errors)
- Invalid options: Clear usage messages
- Permission issues: Informative error output

## Performance Considerations

### Optimization Tips

1. **Use `--count` or `--files`** for quick existence checks
2. **Filter by language** when possible to reduce search space
3. **Cache JSON outputs** using timestamps for invalidation
4. **Use `--summary` mode** for initial repository scans

### Expected Performance

| Operation | Small Repo (<50 files) | Medium Repo (<200 files) | Large Repo (>500 files) |
|-----------|----------------------|-------------------------|------------------------|
| Summary   | <1s                  | <2s                     | <5s                    |
| Crawl     | 2-5s                 | 5-15s                   | 15-60s                 |
| Search    | <1s                  | 1-3s                    | 3-10s                  |
| Extract   | <1s                  | <1s                     | 1-2s                   |

## Security Notes

- Tools only read files; no write operations on source code
- No network access required
- No external dependencies or package downloads
- Safe for CI/CD integration
- Sandboxed execution recommended for untrusted repositories

## Troubleshooting

### Common Issues

**Issue:** Tools not executable  
**Solution:** `chmod +x .ai-chat/tools/*.sh`

**Issue:** No results from search  
**Solution:** Try broader patterns or remove language filters

**Issue:** JSON parsing errors  
**Solution:** Ensure complete command execution before parsing

**Issue:** Slow performance on large repos  
**Solution:** Use `--lang` filter to narrow search scope

## Best Practices for AI-Chat Platforms

1. **Pre-compute indices** during repository clone/import
2. **Cache results** with timestamp-based invalidation
3. **Use JSON for internal processing**, Markdown for user display
4. **Implement rate limiting** for repeated searches
5. **Provide fallback messages** when tools are unavailable
6. **Log tool usage** for debugging and optimization

## Extension Points

The tooling architecture supports extension:

- Add new language support in `crawler.sh` and `search.sh`
- Extend pattern matching in `generate_cross_references()`
- Add custom output formatters
- Integrate with external indexing systems

## Support & Contribution

For issues or enhancements related to AI-chat tooling:

1. Check existing documentation in `.ai-chat/`
2. Review tool source code for extension points
3. Test changes with `--help` flags for each tool
4. Validate JSON outputs with standard parsers

---

**Generated:** $(date -u +"%Y-%m-%dT%H:%M:%SZ")  
**Tools Version:** 1.0.0  
**Compatibility:** POSIX shell (bash 4.0+, grep, find, awk, sed, wc)
