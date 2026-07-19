// Filename: eco_restoration_shard/crates/blacklist-trie/src/lib.rs
// Role: Load blacklist patterns from SQLite, build an in-memory trie,
//       and expose fn matches_blacklist(prompt: &str) -> bool for safety gates.

#![forbid(unsafe_code)]

use rusqlite::{params, Connection};
use std::collections::HashMap;

/// Simple character-based trie node.
/// Using chars keeps implementation straightforward and robust to arbitrary text.
/// [web:126]
#[derive(Default)]
struct TrieNode {
    is_terminal: bool,
    // pattern_ids that end at this node; multiple patterns can share a suffix.
    pattern_ids: Vec<i64>,
    children: HashMap<char, TrieNode>,
}

/// In-memory trie for blacklist pattern matching.
#[derive(Default)]
pub struct BlacklistTrie {
    root: TrieNode,
}

impl BlacklistTrie {
    /// Construct an empty trie.
    pub fn new() -> Self {
        Self {
            root: TrieNode::default(),
        }
    }

    /// Insert a normalized pattern string and its pattern_id into the trie.
    pub fn insert(&mut self, pattern: &str, pattern_id: i64) {
        let mut current = &mut self.root;
        for ch in pattern.chars() {
            // Skip spaces if you want token-style matching; here we keep them
            // to match exact phrases like "restart pump after".
            current = current.children.entry(ch).or_default();
        }
        current.is_terminal = true;
        if !current.pattern_ids.contains(&pattern_id) {
            current.pattern_ids.push(pattern_id);
        }
    }

    /// Return true if the prompt contains any blacklist pattern as a substring.
    /// This performs a multi-start traversal: for each starting index in the
    /// normalized prompt, walk the trie as far as possible and check terminals.
    pub fn matches(&self, prompt: &str) -> bool {
        let normalized = normalize_text(prompt);
        let chars: Vec<char> = normalized.chars().collect();
        let len = chars.len();

        for start in 0..len {
            let mut current = &self.root;
            for i in start..len {
                let ch = chars[i];
                if let Some(child) = current.children.get(&ch) {
                    current = child;
                    if current.is_terminal {
                        // Terminal node: pattern found.
                        return true;
                    }
                } else {
                    // No child; stop exploring this start position.
                    break;
                }
            }
        }

        false
    }
}

/// Normalize text for matching:
/// - Lowercase.
/// - Trim leading/trailing whitespace.
/// - Collapse multiple spaces into single spaces.
/// - Remove control characters.
fn normalize_text(input: &str) -> String {
    let mut out = String::with_capacity(input.len());
    let mut prev_space = false;

    for ch in input.chars() {
        let ch_lower = ch.to_lowercase().next().unwrap_or(ch);
        if ch_lower.is_whitespace() {
            if !prev_space {
                out.push(' ');
                prev_space = true;
            }
        } else if !ch_lower.is_control() {
            out.push(ch_lower);
            prev_space = false;
        }
    }

    out.trim().to_string()
}

/// Minimal pattern representation from SQLite.
#[derive(Debug)]
struct BlacklistPatternRow {
    pattern_id: i64,
    pattern_text: String,
    risk_level: String,
    frequency: i64,
}

/// Load patterns from SQLite and build a trie.
/// You can call this at process startup and cache the resulting trie.
pub fn load_blacklist_trie_from_sqlite(db_path: &str) -> rusqlite::Result<BlacklistTrie> {
    let conn = Connection::open(db_path)?;

    // Fetch patterns that have been promoted to blacklist, e.g. risk_level = 'hard_forbidden'.
    let mut stmt = conn.prepare(
        r#"
        SELECT pattern_id, pattern_text, risk_level, frequency
        FROM blacklist_patterns
        WHERE risk_level = 'hard_forbidden'
          AND frequency >= 1
        "#,
    )?;

    let rows = stmt.query_map(params![], |row| {
        Ok(BlacklistPatternRow {
            pattern_id: row.get(0)?,
            pattern_text: row.get(1)?,
            risk_level: row.get(2)?,
            frequency: row.get(3)?,
        })
    })?;

    let mut trie = BlacklistTrie::new();
    for row_res in rows {
        let row = row_res?;
        let normalized_pattern = normalize_text(&row.pattern_text);
        if !normalized_pattern.is_empty() {
            trie.insert(&normalized_pattern, row.pattern_id);
        }
    }

    Ok(trie)
}

/// High-level helper: load trie once and test prompt.
/// In production, you typically call load_blacklist_trie_from_sqlite() at startup
/// and reuse the trie, rather than opening the DB per call.
pub fn matches_blacklist_prompt(db_path: &str, prompt: &str) -> bool {
    match load_blacklist_trie_from_sqlite(db_path) {
        Ok(trie) => trie.matches(prompt),
        Err(_) => {
            // On DB error, fail-safe by returning false or true depending on policy.
            // Here we return false to avoid over-blocking due to transient DB issues.
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_text_basic() {
        let input = "Restart Pump   AFTER Backwash  Cycle\n";
        let norm = normalize_text(input);
        assert_eq!(norm, "restart pump after backwash cycle");
    }

    #[test]
    fn trie_matches_simple_pattern() {
        let mut trie = BlacklistTrie::new();
        trie.insert("restart pump after backwash", 1);

        assert!(trie.matches("please restart pump after backwash cycle"));
        assert!(trie.matches("RESTART PUMP AFTER BACKWASH"));
        assert!(!trie.matches("pump status report only"));
    }
}
