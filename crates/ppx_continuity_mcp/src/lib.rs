//! ppx_continuity_mcp
//!
//! Read-only MCP-facing boundary for the Prometheus-Praxis continuity kernel.
//! - Wraps `ppx_continuity_kernel::ContinuityKernel`.
//! - Exposes task-level JSON tools suitable for Anthropic MCP integration.
//! - No writes, no identity/rights sinks, no actuation.
//!
//! Tools (method names):
//! - "get_psych_continuity_for_subject"
//! - "get_user_continuity_preference"
//! - "list_neuroright_corridors_for_context"
//! - "list_system_wellbeing_components"
//! - "list_sovereignty_guarantees"

use ppx_continuity_kernel::ContinuityKernel;
use serde::{Deserialize, Serialize};
use thiserror::Error;

// -----------------------------------------------------------------------------
// Error type
// -----------------------------------------------------------------------------

#[derive(Debug, Error)]
pub enum McpError {
    #[error("Kernel error: {0}")]
    Kernel(#[from] ppx_continuity_kernel::KernelError),
    #[error("JSON error: {0}")]
    Serde(#[from] serde_json::Error),
    #[error("Unknown method: {0}")]
    UnknownMethod(String),
    #[error("Invalid request: {0}")]
    InvalidRequest(String),
}

// -----------------------------------------------------------------------------
// MCP-style request/response envelope
// -----------------------------------------------------------------------------

/// Generic MCP-style request, with a method name and JSON-encoded params.
/// The outer transport (Anthropic MCP runtime) handles session/auth; this
/// crate deals only with task-level semantics.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct McpRequest {
    /// Client-supplied identifier for correlating responses.
    pub id: String,
    /// Method name, e.g. "get_psych_continuity_for_subject".
    pub method: String,
    /// Arbitrary JSON parameters, decoded per-method.
    pub params: serde_json::Value,
}

/// Generic MCP-style response.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct McpResponse {
    pub id: String,
    pub result: Option<serde_json::Value>,
    pub error: Option<McpErrorPayload>,
}

/// Minimal error payload for MCP responses.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct McpErrorPayload {
    pub code: String,
    pub message: String,
}

// -----------------------------------------------------------------------------
// Method-specific parameter and result types
// -----------------------------------------------------------------------------

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GetPsychContinuityForSubjectParams {
    pub subject_did: String,
    /// Optional RFC3339 strings.
    pub from_utc: Option<String>,
    pub to_utc: Option<String>,
    /// Maximum number of evidence rows to return (0 = no limit).
    pub limit: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GetUserContinuityPreferenceParams {
    pub subject_did: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ListNeurorightCorridorsForContextParams {
    pub context_tag: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ListSystemWellBeingComponentsParams {
    pub system_id: String,
    pub context_tag: String,
}

/// No params needed: list all sovereignty guarantees.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ListSovereigntyGuaranteesParams {}

/// Helper enum for dispatch: each MCP method maps to a typed params struct.
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
pub enum McpMethodParams {
    #[serde(rename = "get_psych_continuity_for_subject")]
    GetPsychContinuityForSubject(GetPsychContinuityForSubjectParams),
    #[serde(rename = "get_user_continuity_preference")]
    GetUserContinuityPreference(GetUserContinuityPreferenceParams),
    #[serde(rename = "list_neuroright_corridors_for_context")]
    ListNeurorightCorridorsForContext(ListNeurorightCorridorsForContextParams),
    #[serde(rename = "list_system_wellbeing_components")]
    ListSystemWellBeingComponents(ListSystemWellBeingComponentsParams),
    #[serde(rename = "list_sovereignty_guarantees")]
    ListSovereigntyGuarantees(ListSovereigntyGuaranteesParams),
}

// -----------------------------------------------------------------------------
// Core dispatcher
// -----------------------------------------------------------------------------

/// Main entry point: handle a single MCP request with a live ContinuityKernel.
///
/// - `kernel` is a read-only handle to the SQLite continuity DB.
/// - Returns a typed `McpResponse` that can be serialized to JSON.
pub fn handle_request(
    kernel: &ContinuityKernel,
    req: McpRequest,
) -> Result<McpResponse, McpError> {
    let method = req.method.clone();

    // Attempt to decode params based on method name.
    let params_enum = match method.as_str() {
        "get_psych_continuity_for_subject" => {
            let p: GetPsychContinuityForSubjectParams =
                serde_json::from_value(req.params.clone())?;
            McpMethodParams::GetPsychContinuityForSubject(p)
        }
        "get_user_continuity_preference" => {
            let p: GetUserContinuityPreferenceParams =
                serde_json::from_value(req.params.clone())?;
            McpMethodParams::GetUserContinuityPreference(p)
        }
        "list_neuroright_corridors_for_context" => {
            let p: ListNeurorightCorridorsForContextParams =
                serde_json::from_value(req.params.clone())?;
            McpMethodParams::ListNeurorightCorridorsForContext(p)
        }
        "list_system_wellbeing_components" => {
            let p: ListSystemWellBeingComponentsParams =
                serde_json::from_value(req.params.clone())?;
            McpMethodParams::ListSystemWellBeingComponents(p)
        }
        "list_sovereignty_guarantees" => {
            let p: ListSovereigntyGuaranteesParams =
                serde_json::from_value(req.params.clone())?;
            McpMethodParams::ListSovereigntyGuarantees(p)
        }
        other => return Err(McpError::UnknownMethod(other.to_string())),
    };

    let result_value = match params_enum {
        McpMethodParams::GetPsychContinuityForSubject(p) => {
            let json_str = kernel.json_psych_continuity_for_subject(
                &p.subject_did,
                p.from_utc.as_deref(),
                p.to_utc.as_deref(),
                p.limit,
            )?;
            serde_json::from_str(&json_str)?
        }
        McpMethodParams::GetUserContinuityPreference(p) => {
            let json_str = kernel.json_user_continuity_preference(&p.subject_did)?;
            serde_json::from_str(&json_str)?
        }
        McpMethodParams::ListNeurorightCorridorsForContext(p) => {
            let json_str = kernel.json_neuroright_corridors_for_context(&p.context_tag)?;
            serde_json::from_str(&json_str)?
        }
        McpMethodParams::ListSystemWellBeingComponents(p) => {
            let json_str =
                kernel.json_system_wellbeing_components(&p.system_id, &p.context_tag)?;
            serde_json::from_str(&json_str)?
        }
        McpMethodParams::ListSovereigntyGuarantees(_p) => {
            let json_str = kernel.json_sovereignty_guarantees()?;
            serde_json::from_str(&json_str)?
        }
    };

    Ok(McpResponse {
        id: req.id,
        result: Some(result_value),
        error: None,
    })
}

// -----------------------------------------------------------------------------
// Helpers to render errors into responses
// -----------------------------------------------------------------------------

impl McpError {
    fn to_payload(&self) -> McpErrorPayload {
        match self {
            McpError::Kernel(e) => McpErrorPayload {
                code: "KERNEL_ERROR".to_string(),
                message: e.to_string(),
            },
            McpError::Serde(e) => McpErrorPayload {
                code: "JSON_ERROR".to_string(),
                message: e.to_string(),
            },
            McpError::UnknownMethod(m) => McpErrorPayload {
                code: "UNKNOWN_METHOD".to_string(),
                message: format!("Unknown method: {m}"),
            },
            McpError::InvalidRequest(msg) => McpErrorPayload {
                code: "INVALID_REQUEST".to_string(),
                message: msg.clone(),
            },
        }
    }
}

/// Convenience: wrap a failed request into a response envelope.
pub fn error_response(id: String, err: McpError) -> McpResponse {
    McpResponse {
        id,
        result: None,
        error: Some(err.to_payload()),
    }
}
