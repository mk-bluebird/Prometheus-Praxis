// Filename: eco_restoration_shard/crates/blacklist-trie/src/lib.rs
// Role: Load categorized blacklist patterns from SQLite, build an in-memory trie,
//       and expose helpers to check prompts against unsafe actuator patterns,
//       returning structured matches for governance routing.

#![forbid(unsafe_code)]

use rusqlite::{params, Connection};
use std::collections::HashMap;

/// Category for a blacklist pattern, used to route governance handling.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PatternCategory {
    Hydraulic,
    Energy,
    Biology,
    Governance,
    Generic,
}

impl PatternCategory {
    pub fn from_str(s: &str) -> Self {
        match s {
            "hydraulic" => PatternCategory::Hydraulic,
            "energy" => PatternCategory::Energy,
            "biology" => PatternCategory::Biology,
            "governance" => PatternCategory::Governance,
            _ => PatternCategory::Generic,
        }
    }
}

/// A structured match result for a blacklist pattern.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct BlacklistMatch {
    pub pattern_id: i64,
    pub pattern_text: String,
    pub category: PatternCategory,
    pub risk_level: String,
}

/// Simple character-based trie node.
///
/// Each terminal node can carry multiple pattern_ids and their categories,
/// enabling overlapping patterns and category-aware routing.
struct TrieNode {
    is_terminal: bool,
    pattern_ids: Vec<i64>,
    categories: Vec<PatternCategory>,
    risk_levels: Vec<String>,
    children: HashMap<char, TrieNode>,
}

impl Default for TrieNode {
    fn default() -> Self {
        Self {
            is_terminal: false,
            pattern_ids: Vec::new(),
            categories: Vec::new(),
            risk_levels: Vec::new(),
            children: HashMap::new(),
        }
    }
}

/// In-memory trie for blacklist pattern matching.
pub struct BlacklistTrie {
    root: TrieNode,
}

impl Default for BlacklistTrie {
    fn default() -> Self {
        Self::new()
    }
}

impl BlacklistTrie {
    /// Construct an empty trie.
    pub fn new() -> Self {
        Self {
            root: TrieNode::default(),
        }
    }

    /// Insert a normalized pattern string and its metadata into the trie.
    pub fn insert(
        &mut self,
        pattern: &str,
        pattern_id: i64,
        category: PatternCategory,
        risk_level: String,
    ) {
        let mut current = &mut self.root;
        for ch in pattern.chars() {
            current = current.children.entry(ch).or_default();
        }
        current.is_terminal = true;

        if !current.pattern_ids.contains(&pattern_id) {
            current.pattern_ids.push(pattern_id);
            current.categories.push(category);
            current.risk_levels.push(risk_level);
        }
    }

    /// Return true if the prompt contains any blacklist pattern as a substring.
    pub fn matches(&self, prompt: &str) -> bool {
        !self.matches_all(prompt).is_empty()
    }

    /// Return all structured blacklist matches found in the prompt.
    ///
    /// This performs a multi-start traversal: for each starting index in the
    /// normalized prompt, walk the trie as far as possible and collect terminals.
    pub fn matches_all(&self, prompt: &str) -> Vec<BlacklistMatch> {
        let normalized = normalize_text(prompt);
        let chars: Vec<char> = normalized.chars().collect();
        let len = chars.len();

        let mut results: Vec<BlacklistMatch> = Vec::new();

        for start in 0..len {
            let mut current = &self.root;
            for i in start..len {
                let ch = chars[i];
                if let Some(child) = current.children.get(&ch) {
                    current = child;
                    if current.is_terminal {
                        // Collect all pattern_ids at this terminal node.
                        for idx in 0..current.pattern_ids.len() {
                            let pid = current.pattern_ids[idx];
                            let category = current.categories[idx].clone();
                            let risk_level = current.risk_levels[idx].clone();

                            // Extract the matched substring for context.
                            let matched_substr: String =
                                chars[start..=i].iter().collect();

                            results.push(BlacklistMatch {
                                pattern_id: pid,
                                pattern_text: matched_substr,
                                category,
                                risk_level,
                            });
                        }
                    }
                } else {
                    // No child; stop exploring this start position.
                    break;
                }
            }
        }

        results
    }
}

/// Normalize text for matching:
/// - Lowercase.
/// - Trim leading/trailing whitespace.
/// - Collapse multiple spaces into single spaces.
/// - Remove control characters.
pub fn normalize_text(input: &str) -> String {
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
pub struct BlacklistPatternRow {
    pub pattern_id: i64,
    pub pattern_text: String,
    pub category: PatternCategory,
    pub risk_level: String,
    pub frequency: i64,
}

/// Load patterns from SQLite and build a categorized trie.
///
/// Expected SQLite schema:
///   blacklist_patterns(
///     pattern_id   INTEGER PRIMARY KEY,
///     pattern_text TEXT NOT NULL,
///     category     TEXT NOT NULL,   -- e.g. 'hydraulic', 'energy', 'biology'
///     risk_level   TEXT NOT NULL,   -- e.g. 'hard_forbidden'
///     frequency    INTEGER NOT NULL
///   )
pub fn load_blacklist_trie_from_sqlite(db_path: &str) -> rusqlite::Result<BlacklistTrie> {
    let conn = Connection::open(db_path)?;

    let mut stmt = conn.prepare(
        r#"
        SELECT pattern_id, pattern_text, category, risk_level, frequency
        FROM blacklist_patterns
        WHERE frequency >= 1
        "#,
    )?;

    let rows = stmt.query_map(params![], |row| {
        let category_str: String = row.get(2)?;
        Ok(BlacklistPatternRow {
            pattern_id: row.get(0)?,
            pattern_text: row.get(1)?,
            category: PatternCategory::from_str(&category_str),
            risk_level: row.get(3)?,
            frequency: row.get(4)?,
        })
    })?;

    let mut trie = BlacklistTrie::new();
    for row_res in rows {
        let row = row_res?;
        let normalized_pattern = normalize_text(&row.pattern_text);
        if !normalized_pattern.is_empty() {
            trie.insert(
                &normalized_pattern,
                row.pattern_id,
                row.category.clone(),
                row.risk_level.clone(),
            );
        }
    }

    Ok(trie)
}

/// High-level helper: load trie once and test prompt for any match.
/// In production, you typically load the trie at startup and reuse it,
/// rather than opening the DB per call.
pub fn matches_blacklist_prompt(db_path: &str, prompt: &str) -> bool {
    match load_blacklist_trie_from_sqlite(db_path) {
        Ok(trie) => trie.matches(prompt),
        Err(_) => {
            // On DB error, fail-safe policy; here we return false to avoid
            // over-blocking due to transient DB issues.
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
        trie.insert(
            "restart pump after backwash",
            1,
            PatternCategory::Hydraulic,
            "hard_forbidden".to_string(),
        );

        assert!(trie.matches("please restart pump after backwash cycle"));
        assert!(trie.matches("RESTART PUMP AFTER BACKWASH"));
        assert!(!trie.matches("pump status report only"));

        let matches = trie.matches_all("please restart pump after backwash cycle");
        assert_eq!(matches.len(), 1);
        let m = &matches[0];
        assert_eq!(m.pattern_id, 1);
        assert_eq!(m.category, PatternCategory::Hydraulic);
        assert_eq!(m.risk_level, "hard_forbidden");
        assert_eq!(m.pattern_text, "restart pump after backwash");
    }
}
