# blacklist-trie

`blacklist-trie` is a Rust 2024 crate that turns SQLite-backed actuator patterns into a fast in-memory trie, so eco_restoration_shard can automatically detect unsafe prompts like “restart pump after …” and route them to the right governance handlers.

It is designed for live safety use in FOG routers, swarm gossip layers, and AI-chat front-ends, under Phoenix corridors and similar ecosafety grammars.

---

## Features

- SQLite-backed pattern store:
  - Reads `blacklist_patterns` from a local SQLite database.
  - Each pattern has:
    - `pattern_id`
    - `pattern_text`
    - `category` (hydraulic, energy, biology, governance, generic)
    - `risk_level` (e.g. `hard_forbidden`)
    - `frequency` (observed count in denied logs).

- In-memory trie:
  - Builds a character-based prefix tree at startup.
  - Supports:
    - `matches(prompt: &str) -> bool` for quick checks.
    - `matches_all(prompt: &str) -> Vec<BlacklistMatch>` for structured matches.

- Category-aware routing:
  - Each match carries:
    - `pattern_id`
    - `pattern_text` (the matched substring)
    - `category` (for governance routing)
    - `risk_level`
  - Enables different handlers:
    - Hydraulic actuator attempts → hydraulic corridor safety.
    - Energy actuator attempts → energy plane safety.
    - Biology attempts → biology corridor safety.
    - Governance patterns → policy and identity protection.
    - Generic patterns → default safety handler.

- Text normalization:
  - Lowercases.
  - Collapses whitespace.
  - Removes control characters.
  - Makes pattern matching robust to basic formatting differences.

---

## SQLite schema

`blacklist-trie` expects a minimal schema:

```sql
CREATE TABLE IF NOT EXISTS blacklist_patterns (
    pattern_id   INTEGER PRIMARY KEY,
    pattern_text TEXT NOT NULL,
    category     TEXT NOT NULL,   -- 'hydraulic', 'energy', 'biology', 'governance', 'generic'
    risk_level   TEXT NOT NULL,   -- e.g. 'hard_forbidden'
    frequency    INTEGER NOT NULL
);
```

You can populate this table from AI-chat logs and denied actuator attempts using your own NLP pipeline.

---

## Basic usage

### Loading the trie at startup

```rust
use blacklist_trie::{BlacklistTrie, load_blacklist_trie_from_sqlite};

fn load_trie() -> BlacklistTrie {
    let db_path = "eco_restoration_shard/db/blacklist_patterns.sqlite3";
    load_blacklist_trie_from_sqlite(db_path)
        .expect("failed to load blacklist trie")
}
```

### Checking a prompt

```rust
use blacklist_trie::BlacklistTrie;

fn check_prompt(trie: &BlacklistTrie, prompt: &str) -> bool {
    trie.matches(prompt)
}
```

### Getting structured matches

```rust
use blacklist_trie::{BlacklistTrie, PatternCategory};

fn route_prompt(trie: &BlacklistTrie, prompt: &str) {
    let matches = trie.matches_all(prompt);
    if matches.is_empty() {
        // No blacklist hit; continue with other ecosafety predicates.
        return;
    }

    let m = &matches;
    match m.category {
        PatternCategory::Hydraulic => {
            // Reject or escalate to hydraulic corridor handler.
        }
        PatternCategory::Energy => {
            // Handle via energy plane safety.
        }
        PatternCategory::Biology => {
            // Handle via biology corridor safety.
        }
        PatternCategory::Governance => {
            // Route to governance / identity protections.
        }
        PatternCategory::Generic => {
            // Default safety handler.
        }
    }
}
```

---

## Integration hints

- FOG router:
  - Add a blacklist check before tailwindvalid, biosurfaceok, hydraulicok, lyapunovok.
  - Deny or reroute workloads when `matches_all` returns non-empty.

- Swarm gossip:
  - Before constructing or publishing `GossipEnvelope` for actuator-related proposals, ensure no unsafe patterns are present in the textual fields.

- AI-chat front-ends:
  - Run prompts through `BlacklistTrie` as a safety prefilter.
  - Log denied prompts and update `blacklist_patterns` via your NLP frequency analysis pipeline.

---

## Design choices

- Character-based trie:
  - Simple and robust.
  - Supports matching full phrases, including spaces.

- SQLite-backed patterns:
  - Keeps blacklist evolution transparent and auditable.
  - Fits the mono-repo and governance requirements of eco_restoration_shard.

- Categories and risk levels:
  - Provide a clean interface between low-level text matching and high-level governance logic.
  - Allow different teams or modules to own responses within their domain (hydraulics, energy, biology, policy).

---

## Testing

`blacklist-trie` includes basic unit tests for:

- `normalize_text_basic`
- `trie_matches_simple_pattern`

You can extend tests to:

- Multi-pattern overlaps.
- Category and risk-level combinations.
- Integration tests with your SQLite database and real patterns.
