// File: crates/materials-eco-knowledge-labeller/src/heuristics.rs
//! Heuristics for prioritising which material descriptions are worth labelling.

use regex::Regex;

use crate::model::MaterialText;

/// A pre‑computed set of regex patterns used for heuristic scoring.
pub struct HeuristicPatterns {
    iso_pattern: Regex,
    astm_pattern: Regex,
    lca_pattern: Regex,
    greenwash_pattern: Regex,
}

impl HeuristicPatterns {
    /// Construct default patterns for ISO, ASTM, LCA/EPD, and marketing phrases.
    pub fn default() -> Self {
        let iso_pattern = Regex::new(r"(?i)\bISO\s*\d{4,5}\b").unwrap();
        let astm_pattern = Regex::new(r"(?i)\bASTM\s+[A-Z]?\d+\b").unwrap();
        let lca_pattern = Regex::new(r"(?i)\bEPD\b|\bLCA\b").unwrap();
        let greenwash_pattern =
            Regex::new(r"(?i)\beco[-\s]?friendly\b|\bsustainable\b|\bgreen\b").unwrap();

        Self {
            iso_pattern,
            astm_pattern,
            lca_pattern,
            greenwash_pattern,
        }
    }

    /// Compute a heuristic priority score [0,1] indicating how valuable it is
    /// to invest expert time labelling this description.
    pub fn score_pre_label_priority(&self, text: &str) -> f64 {
        let mut score = 0.0;

        if self.iso_pattern.is_match(text) {
            score += 0.2;
        }
        if self.astm_pattern.is_match(text) {
            score += 0.2;
        }
        if self.lca_pattern.is_match(text) {
            score += 0.2;
        }
        if self.greenwash_pattern.is_match(text) {
            score += 0.1;
        }

        let length = text.split_whitespace().count();
        if length < 10 {
            score *= 0.3;
        }

        if score > 1.0 {
            1.0
        } else if score < 0.0 {
            0.0
        } else {
            score
        }
    }
}

/// Filter a corpus down to candidates worth labelling, based on a minimum priority.
pub fn filter_corpus_for_labelling(
    texts: impl IntoIterator<Item = MaterialText>,
    patterns: &HeuristicPatterns,
    min_priority: f64,
) -> Vec<MaterialText> {
    let mut out = Vec::new();
    for mt in texts {
        let p = patterns.score_pre_label_priority(&mt.text);
        if p >= min_priority {
            out.push(mt);
        }
    }
    out
}
