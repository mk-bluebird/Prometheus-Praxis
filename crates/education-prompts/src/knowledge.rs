use serde::{Deserialize, Serialize};
use aln_core::Did;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StewardKnowledgeState {
    pub steward_did: Did,
    pub completed_prompts: Vec<String>,
    pub knowledge_multiplier: f64,
}

impl StewardKnowledgeState {
    pub fn apply_completion(&mut self, prompt_topic: &str, increment: f64, max_multiplier: f64) {
        if !self.completed_prompts.iter().any(|t| t == prompt_topic) {
            self.completed_prompts.push(prompt_topic.to_string());
            self.knowledge_multiplier =
                (self.knowledge_multiplier + increment).min(max_multiplier);
        }
    }

    pub fn decay(&mut self, factor: f64) {
        self.knowledge_multiplier *= factor;
    }

    pub fn effective_ker(&self, base_ker: &ecospine::KER) -> ecospine::KER {
        ecospine::KER {
            k: base_ker.k * self.knowledge_multiplier,
            e: base_ker.e,
            r: base_ker.r,
        }
    }
}
