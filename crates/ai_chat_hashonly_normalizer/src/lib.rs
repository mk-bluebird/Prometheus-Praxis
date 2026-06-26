// Path: crates/ai_chat_hashonly_normalizer/src/lib.rs
// Role: Convert raw AI chat transcripts into HASHONLY AiChatSummaryV1 KOs.

#![forbid(unsafe_code)]

use std::fmt;
use std::time::Duration;

/// Mirror of ALE.IDENTITY.DATALABOR.AICHAT.001
/// TopicVectorHashAtom and AiChatSummaryV1 as Rust types.

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ConsentFlag {
    ExplicitOptIn,
    ExplicitOptOut,
    EmergencyOverride,
    ResearchOnly,
    ClinicalOnly,
    AnonymizedAnalytics,
    MonetizationForbidden,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ConsentFlags {
    pub flags: Vec<ConsentFlag>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SourceKind {
    AiChatSummaryV1,
    OtherLaborEvent,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct TopicVectorHashAtom {
    pub algo: String,   // e.g. "ALLOWED-HASH-V1"
    pub value: String,  // hex-encoded hash
    pub dim: u32,       // embedding dimensionality before hashing
    pub salt_id: String,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AiChatSummaryV1 {
    pub summary_id: String,
    pub topic_vector_hash: TopicVectorHashAtom,
    pub labor_span_seconds: u64,
    pub source_kind: SourceKind,
    pub hashonly: bool,
    pub consent: ConsentFlags,
    pub domain_label: String,
    pub symptom_cluster_label: String,
    pub locality_label: String,
    pub created_at_utc: String,
    pub tool_chain_id: String,
    pub normalizer_version: String,
}

/// Input to the normalizer: raw transcript plus coarse metadata.
/// No raw transcript is ever stored in the output KO.
#[derive(Debug, Clone)]
pub struct RawChatLaborInput {
    pub raw_transcript: String,
    pub labor_span: Duration,
    pub consent: ConsentFlags,
    pub domain_label: String,
    pub symptom_cluster_label: String,
    pub locality_label: String,
    pub tool_chain_id: String,
    pub created_at_utc: String, // ISO-8601
}

/// Errors that can occur during normalization.
#[derive(Debug)]
pub enum NormalizerError {
    EmptyTranscript,
    ZeroLaborSpan,
    EmbeddingFailure(String),
    HashingFailure(String),
    InvalidMetadata(String),
}

impl fmt::Display for NormalizerError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            NormalizerError::EmptyTranscript => write!(f, "empty transcript"),
            NormalizerError::ZeroLaborSpan => write!(f, "labor span is zero"),
            NormalizerError::EmbeddingFailure(msg) => write!(f, "embedding failure: {msg}"),
            NormalizerError::HashingFailure(msg) => write!(f, "hashing failure: {msg}"),
            NormalizerError::InvalidMetadata(msg) => write!(f, "invalid metadata: {msg}"),
        }
    }
}

impl std::error::Error for NormalizerError {}

/// Abstract hashing interface so you can swap primitives cleanly under C1.
pub trait Hasher {
    /// Hashes the provided bytes and returns a hex-encoded digest string.
    fn hash(&self, input: &[u8]) -> Result<String, NormalizerError>;

    /// Returns the algorithm label to store in TopicVectorHashAtom.algo.
    fn algo_label(&self) -> &str;
}

/// Placeholder hash implementation using an allowed cryptographic primitive.
/// Replace with your non-blacklisted choice as needed.
pub struct AllowedHashV1;

impl Hasher for AllowedHashV1 {
    fn hash(&self, input: &[u8]) -> Result<String, NormalizerError> {
        use sha2::{Digest, Sha256};

        let mut hasher = Sha256::new();
        hasher.update(input);
        let digest = hasher.finalize();
        Ok(hex::encode(digest))
    }

    fn algo_label(&self) -> &str {
        "ALLOWED-HASH-V1"
    }
}

/// Abstract embedding interface (you can back this with any vectorizer).
pub trait Embedder {
    /// Convert a raw transcript into a numeric vector.
    fn embed(&self, transcript: &str) -> Result<Vec<f32>, NormalizerError>;
}

/// Example trivial embedder: hashes lines into a fixed-dim vector.
/// Replace with a real model-backed embedder in deployment.
pub struct TrivialEmbedder {
    dim: usize,
}

impl TrivialEmbedder {
    pub fn new(dim: usize) -> Self {
        Self { dim }
    }
}

impl Embedder for TrivialEmbedder {
    fn embed(&self, transcript: &str) -> Result<Vec<f32>, NormalizerError> {
        if transcript.trim().is_empty() {
            return Err(NormalizerError::EmbeddingFailure(
                "transcript empty after trimming".to_string(),
            ));
        }

        let mut vec = vec![0f32; self.dim];
        for (i, b) in transcript.bytes().enumerate() {
            let idx = i % self.dim;
            vec[idx] += (b as f32) / 255.0;
        }
        Ok(vec)
    }
}

/// Normalizer configuration.
pub struct AiChatNormalizerConfig<H: Hasher, E: Embedder> {
    pub hasher: H,
    pub embedder: E,
    pub salt_id: String,
    pub normalizer_version: String,
}

impl<H: Hasher, E: Embedder> AiChatNormalizerConfig<H, E> {
    /// Main entrypoint: produce a HASHONLY AiChatSummaryV1 from raw input.
    pub fn normalize(
        &self,
        input: RawChatLaborInput,
    ) -> Result<AiChatSummaryV1, NormalizerError> {
        if input.raw_transcript.trim().is_empty() {
            return Err(NormalizerError::EmptyTranscript);
        }
        if input.labor_span.as_secs() == 0 {
            return Err(NormalizerError::ZeroLaborSpan);
        }
        if input.created_at_utc.trim().is_empty() {
            return Err(NormalizerError::InvalidMetadata(
                "created_at_utc is empty".to_string(),
            ));
        }

        // 1. Embed transcript into a numeric vector (no text stored).
        let embedding = self.embedder.embed(&input.raw_transcript)?;
        if embedding.is_empty() {
            return Err(NormalizerError::EmbeddingFailure(
                "embedder returned empty vector".to_string(),
            ));
        }

        // 2. Prepare bytes for hashing: serialize floats to bytes deterministically.
        let mut bytes = Vec::with_capacity(embedding.len() * 4);
        for value in &embedding {
            bytes.extend_from_slice(&value.to_le_bytes());
        }

        // 3. Hash embedding bytes.
        let digest_hex = self.hasher.hash(&bytes)?;
        let dim_u32 = u32::try_from(embedding.len()).map_err(|_| {
            NormalizerError::EmbeddingFailure("embedding dimension exceeds u32".to_string())
        })?;

        let topic_hash = TopicVectorHashAtom {
            algo: self.hasher.algo_label().to_string(),
            value: digest_hex,
            dim: dim_u32,
            salt_id: self.salt_id.clone(),
        };

        // 4. Build summary ID from hash and timestamp; no raw text included.
        let summary_id = format!("aichat:{}:{}", topic_hash.value, input.created_at_utc);

        let ko = AiChatSummaryV1 {
            summary_id,
            topic_vector_hash: topic_hash,
            labor_span_seconds: input.labor_span.as_secs(),
            source_kind: SourceKind::AiChatSummaryV1,
            hashonly: true,
            consent: input.consent,
            domain_label: input.domain_label,
            symptom_cluster_label: input.symptom_cluster_label,
            locality_label: input.locality_label,
            created_at_utc: input.created_at_utc,
            tool_chain_id: input.tool_chain_id,
            normalizer_version: self.normalizer_version.clone(),
        };

        Ok(ko)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn default_flags() -> ConsentFlags {
        ConsentFlags {
            flags: vec![ConsentFlag::ExplicitOptIn, ConsentFlag::MonetizationForbidden],
        }
    }

    #[test]
    fn normalizer_produces_hashonly_summary() {
        let hasher = AllowedHashV1;
        let embedder = TrivialEmbedder::new(64);
        let cfg = AiChatNormalizerConfig {
            hasher,
            embedder,
            salt_id: "phx-2026".to_string(),
            normalizer_version: "aichat-normalizer-v1".to_string(),
        };

        let input = RawChatLaborInput {
            raw_transcript: "User: I feel exhausted.\nAssistant: Let's talk about your sleep.".to_string(),
            labor_span: Duration::from_secs(120),
            consent: default_flags(),
            domain_label: "health".to_string(),
            symptom_cluster_label: "sleep".to_string(),
            locality_label: "phoenix-metro".to_string(),
            tool_chain_id: "mcp://aichat-normalizer/1".to_string(),
            created_at_utc: "2026-06-26T07:00:00Z".to_string(),
        };

        let summary = cfg.normalize(input).expect("normalization failed");

        assert!(summary.hashonly);
        assert_eq!(summary.source_kind, SourceKind::AiChatSummaryV1);
        assert!(!summary.summary_id.is_empty());
        assert!(!summary.topic_vector_hash.value.is_empty());
        assert_eq!(summary.topic_vector_hash.algo, "ALLOWED-HASH-V1");
        assert_eq!(summary.topic_vector_hash.dim, 64);
    }

    #[test]
    fn empty_transcript_is_rejected() {
        let hasher = AllowedHashV1;
        let embedder = TrivialEmbedder::new(32);
        let cfg = AiChatNormalizerConfig {
            hasher,
            embedder,
            salt_id: "phx-2026".to_string(),
            normalizer_version: "aichat-normalizer-v1".to_string(),
        };

        let input = RawChatLaborInput {
            raw_transcript: "   ".to_string(),
            labor_span: Duration::from_secs(60),
            consent: default_flags(),
            domain_label: "health".to_string(),
            symptom_cluster_label: "none".to_string(),
            locality_label: "phoenix-metro".to_string(),
            tool_chain_id: "mcp://aichat-normalizer/1".to_string(),
            created_at_utc: "2026-06-26T07:00:00Z".to_string(),
        };

        let err = cfg.normalize(input).unwrap_err();
        matches!(err, NormalizerError::EmptyTranscript);
    }
}
