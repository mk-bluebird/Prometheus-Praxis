use serde::{Deserialize, Serialize};
use uuid::Uuid;

use ecospine::KER;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Question {
    pub text: String,
    pub options: Vec<String>,
    pub correct_index: usize,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Assessment {
    pub questions: Vec<Question>,
    pub passing_threshold: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EducationPrompt {
    pub prompt_id: Uuid,
    pub topic: String,
    pub difficulty: u8,          // 1-5
    pub content: String,         // markdown or docspec id
    pub prerequisites: Vec<String>,
    pub ker: KER,
    pub assessment: Option<Assessment>,
}
