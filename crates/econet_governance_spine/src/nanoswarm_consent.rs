// Filename: crates/econet_governance_spine/src/nanoswarm_consent.rs
// Destination: crates/econet_governance_spine/src/nanoswarm_consent.rs
// License: MIT OR Apache-2.0
// Edition: 2024
// rust-version = "1.85"
// !forbid_unsafecode

use std::fmt;

use serde::{Deserialize, Serialize};

use crate::{
    GovernanceSpine,
    SpineError,
};

/// Lane classification for AI workloads.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum WorkloadLane {
    Research,
    Exp,
    Prod,
}

/// Kind of AI agent/workload.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub enum AiAgentKind {
    AiChat,
    Qpu,
    Planner,
    NanoswarmGovernor,
    Other(String),
}

/// Inputs describing a logical AI workload window.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WorkloadNodeWindowInputs {
    pub workload_id: String,
    pub node_id: String,
    pub region: String,
    pub lane_target: WorkloadLane,
    pub ai_agent_kind: AiAgentKind,
    pub prompt_logical_name: String,
    pub workload_logical_name: String,
    pub consent_kernel_defid: String,
    pub nanoswarm_consent_kernel_defid: String,
    pub prompt_defid: String,
    pub workload_defid: String,
    pub expected_k: f64,
    pub expected_e: f64,
    pub expected_r: f64,
}

/// Decision returned by consent validation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum ConsentDecision {
    Allowed {
        workload_id: String,
        node_id: String,
        lane: WorkloadLane,
        window_id: String,
    },
    Blocked {
        reason_code: ConsentBlockReason,
        details: String,
    },
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ConsentBlockReason {
    MissingDefinitionRegistry,
    SovereignConsentMissing,
    SovereignConsentRevoked,
    LaneNotAllowed,
    MT6883NeurorightsViolation,
    BlastRadiusViolation,
    KerEnvelopeViolation,
    NonOffsettablePlaneViolation,
    ProdAccessNotNonActuating,
}

/// Configuration for consent evaluation.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConsentKernelConfig {
    pub allowed_research_lanes: Vec<WorkloadLane>,
    pub allowed_exp_lanes: Vec<WorkloadLane>,
    pub allowed_prod_lanes: Vec<WorkloadLane>,
    pub sovereign_dids: Vec<String>,
    pub bostrom_addresses: Vec<String>,
    pub require_non_actuating_for_prod: bool,
    pub non_offsettable_plane_ids: Vec<String>,
}

impl Default for ConsentKernelConfig {
    fn default() -> Self {
        ConsentKernelConfig {
            allowed_research_lanes: vec![WorkloadLane::Research],
            allowed_exp_lanes: vec![WorkloadLane::Research, WorkloadLane::Exp],
            allowed_prod_lanes: vec![WorkloadLane::Research],
            sovereign_dids: Vec::new(),
            bostrom_addresses: Vec::new(),
            require_non_actuating_for_prod: true,
            non_offsettable_plane_ids: vec![
                "NEURORIGHTS.CHANNEL".to_string(),
                "SPECIES.SOUL".to_string(),
                "CARBON.NETINTENSITY".to_string(),
                "BIODIV.CONNECTIVITY".to_string(),
            ],
        }
    }
}

impl fmt::Display for ConsentBlockReason {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let s = match self {
            ConsentBlockReason::MissingDefinitionRegistry => "MissingDefinitionRegistry",
            ConsentBlockReason::SovereignConsentMissing => "SovereignConsentMissing",
            ConsentBlockReason::SovereignConsentRevoked => "SovereignConsentRevoked",
            ConsentBlockReason::LaneNotAllowed => "LaneNotAllowed",
            ConsentBlockReason::MT6883NeurorightsViolation => "MT6883NeurorightsViolation",
            ConsentBlockReason::BlastRadiusViolation => "BlastRadiusViolation",
            ConsentBlockReason::KerEnvelopeViolation => "KerEnvelopeViolation",
            ConsentBlockReason::NonOffsettablePlaneViolation => "NonOffsettablePlaneViolation",
            ConsentBlockReason::ProdAccessNotNonActuating => "ProdAccessNotNonActuating",
        };
        write!(f, "{}", s)
    }
}

/// High-level entry point: validate combined consent for an AI workload.
pub fn validate_ai_workload_consent(
    spine: &GovernanceSpine,
    inputs: &WorkloadNodeWindowInputs,
    config: &ConsentKernelConfig,
) -> Result<ConsentDecision, SpineError> {
    // Step 1: resolve definitions in DefinitionRegistry.
    if let Err(e) = resolve_definitions(spine, inputs) {
        return Ok(ConsentDecision::Blocked {
            reason_code: ConsentBlockReason::MissingDefinitionRegistry,
            details: format!("DefinitionRegistry resolution failed: {}", e),
        });
    }

    // Step 2: human/sovereign consent kernel.
    if let Err(reason) = validate_human_consent(spine, inputs, config) {
        return Ok(ConsentDecision::Blocked {
            reason_code: reason,
            details: "Human/sovereign consent kernel denied workload".to_string(),
        });
    }

    // Step 3: nanoswarm ecosafety consent kernel.
    if let Err(reason) = validate_nanoswarm_consent(spine, inputs, config) {
        return Ok(ConsentDecision::Blocked {
            reason_code: reason,
            details: "Nanoswarm ecosafety consent kernel denied workload".to_string(),
        });
    }

    // Compose a non-actuating window ID (e.g., hash of workload + node + lane).
    let window_id = format!(
        "WNW:{}:{}:{:?}",
        inputs.workload_id, inputs.node_id, inputs.lane_target
    );

    Ok(ConsentDecision::Allowed {
        workload_id: inputs.workload_id.clone(),
        node_id: inputs.node_id.clone(),
        lane: inputs.lane_target,
        window_id,
    })
}

/// Resolve prompt, workload, and kernel definitions via DefinitionRegistry.
/// This is a read-only check; it fails if any artifact is unregistered.
fn resolve_definitions(
    spine: &GovernanceSpine,
    inputs: &WorkloadNodeWindowInputs,
) -> Result<(), SpineError> {
    let _prompt_entry = spine.get_definition_registry_entry(&inputs.prompt_defid)?;
    let _workload_entry = spine.get_definition_registry_entry(&inputs.workload_defid)?;
    let _consent_kernel_entry =
        spine.get_definition_registry_entry(&inputs.consent_kernel_defid)?;
    let _nanoswarm_consent_kernel_entry =
        spine.get_definition_registry_entry(&inputs.nanoswarm_consent_kernel_defid)?;
    Ok(())
}

/// Human/sovereign consent kernel: DR, consent state machine, lane/scope.
fn validate_human_consent(
    spine: &GovernanceSpine,
    inputs: &WorkloadNodeWindowInputs,
    config: &ConsentKernelConfig,
) -> Result<(), ConsentBlockReason> {
    // Lane and scope checks.
    match inputs.lane_target {
        WorkloadLane::Research => {
            if !config.allowed_research_lanes.contains(&inputs.lane_target) {
                return Err(ConsentBlockReason::LaneNotAllowed);
            }
        }
        WorkloadLane::Exp => {
            if !config.allowed_exp_lanes.contains(&inputs.lane_target) {
                return Err(ConsentBlockReason::LaneNotAllowed);
            }
        }
        WorkloadLane::Prod => {
            if !config.allowed_prod_lanes.contains(&inputs.lane_target) {
                return Err(ConsentBlockReason::LaneNotAllowed);
            }
            if config.require_non_actuating_for_prod {
                // For PROD, AI workloads must be non-actuating by design.
                if !is_non_actuating_workload(inputs) {
                    return Err(ConsentBlockReason::ProdAccessNotNonActuating);
                }
            }
        }
    }

    // Sovereign consent envelope via Cybercore consent kernel.
    // Here we assume a spine accessor that returns the current ConsentState for the subject/node.
    let consent_state = spine
        .get_consent_state_for_node(&inputs.node_id)
        .map_err(|_| ConsentBlockReason::SovereignConsentMissing)?;

    if !is_consent_state_opted_in_eco_only(&consent_state) {
        return Err(ConsentBlockReason::SovereignConsentRevoked);
    }

    Ok(())
}

/// Nanoswarm ecosafety consent kernel: MT6883, blast radius, lanes, KER, non-offsettable planes.
fn validate_nanoswarm_consent(
    spine: &GovernanceSpine,
    inputs: &WorkloadNodeWindowInputs,
    config: &ConsentKernelConfig,
) -> Result<(), ConsentBlockReason> {
    // MT6883 neurorights / health profiles.
    let mt_profile = spine
        .get_mt6883_profile_for_node(&inputs.node_id)
        .map_err(|_| ConsentBlockReason::MT6883NeurorightsViolation)?;
    if !mt_profile.neurorights_ok {
        return Err(ConsentBlockReason::MT6883NeurorightsViolation);
    }

    // Blast radius and adjacency via vshardblast.
    let blast = spine
        .get_blast_radius_for_node(&inputs.node_id)
        .map_err(|_| ConsentBlockReason::BlastRadiusViolation)?;
    if !blast_is_within_envelope(&blast) {
        return Err(ConsentBlockReason::BlastRadiusViolation);
    }

    // Lane status and KER residual via spine views.
    let lane_status = spine
        .get_lane_status_for_node(&inputs.node_id)
        .map_err(|_| ConsentBlockReason::KerEnvelopeViolation)?;
    let ker_residual = spine
        .get_ker_residual_for_node(&inputs.node_id)
        .map_err(|_| ConsentBlockReason::KerEnvelopeViolation)?;

    // Expected K/E/R must be within nanoswarm ecosafety corridors.
    if inputs.expected_k < ker_residual.k_min || inputs.expected_e < ker_residual.e_min {
        return Err(ConsentBlockReason::KerEnvelopeViolation);
    }
    if inputs.expected_r > ker_residual.r_max {
        return Err(ConsentBlockReason::KerEnvelopeViolation);
    }

    // Non-offsettable planes: no worsening allowed.
    let planes = spine
        .get_plane_coordinates_for_node(&inputs.node_id, &config.non_offsettable_plane_ids)
        .map_err(|_| ConsentBlockReason::NonOffsettablePlaneViolation)?;

    if planes.iter().any(|p| p.expected_delta > 0.0) {
        // Any positive delta on non-offsettable planes is forbidden.
        return Err(ConsentBlockReason::NonOffsettablePlaneViolation);
    }

    // Lane gating: AI workloads only in research/non-actuating lanes.
    if matches!(inputs.lane_target, WorkloadLane::Prod | WorkloadLane::Exp) {
        if !lane_status.is_non_actuating_ai_ok {
            return Err(ConsentBlockReason::KerEnvelopeViolation);
        }
    }

    Ok(())
}

/// Simple heuristic: treat chat/planner workloads as non-actuating by default.
fn is_non_actuating_workload(inputs: &WorkloadNodeWindowInputs) -> bool {
    match inputs.ai_agent_kind {
        AiAgentKind::AiChat | AiAgentKind::Planner => true,
        AiAgentKind::NanoswarmGovernor | AiAgentKind::Qpu => false,
        AiAgentKind::Other(_) => false,
    }
}

/// Placeholder consent state check; in practice, this should match Cybercore's ConsentState enum.
fn is_consent_state_opted_in_eco_only(consent_state: String) -> bool {
    consent_state == "OptedInEcoOnly"
}

/// Placeholder blast radius envelope check; calibrate using PlaneWeightsShard and RoH ceilings.
fn blast_is_within_envelope(blast: crate::BlastRadius) -> bool {
    blast.radiusmeters <= 500.0 && blast.adjacencycount <= 16
}
