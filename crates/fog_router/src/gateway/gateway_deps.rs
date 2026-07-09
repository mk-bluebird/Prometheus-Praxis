// src/gateway/gateway_deps.rs
// GatewayDeps trait definition for cross-constellation FOG routing.
//
// This trait is non-actuating: implementations must not emit ROS2/fieldbus
// messages or touch hardware. They may only read ALN/SQLite state and
// compute diagnostics/invariants.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use cyboquatic_ecosafety::{
    CyboLane,
    CyboNodeEcosafetyEnvelope,
    KERWindow,
    LyapunovResidual,
    RiskVector,
};
use fog_router_guard::FogRouteDecision;

#[derive(Clone, Debug, Serialize)]
pub struct ExternalKerContext {
    pub origin_lane: String,
    pub origin_k: f64,
    pub origin_e: f64,
    pub origin_r: f64,
    pub origin_kerdeployable: bool,
    pub origin_corridor_id: String,
}

#[derive(Clone, Debug, Serialize)]
pub struct WorkflowRiskVector {
    pub r_energy: f64,
    pub r_carbon: f64,
    pub r_topology: f64,
    pub r_biodiv: f64,
}

#[derive(Clone, Debug, Serialize)]
pub struct WorkflowRiskDiagnostics {
    pub r_w: WorkflowRiskVector,
    pub r_w_norm: f64,
    pub max_local_repo_risk: f64,
    pub eco_h_delta: f64,
}

/// Local envelope projection used by GatewayDeps.
/// Mirrors CyboNodeEcosafetyEnvelope but may add index metadata.
#[derive(Clone, Debug, Serialize)]
pub struct LocalEnvelopeContext {
    pub lane: CyboLane,
    pub risk: RiskVector,
    pub residual: LyapunovResidual,
    pub ker: KERWindow,
    pub evidencehex: String,
    pub did: String,
}

/// GatewayDeps encapsulates non-actuating dependencies required
/// for cross-constellation FOG route evaluation.
pub trait GatewayDeps {
    /// Fetch external lane and KER context from cross-constellation-index
    /// for the given origin constellation and workflow.
    fn fetch_external_context(
        &mut self,
        origin_constellation: &str,
        workflow_id: &str,
    ) -> anyhow::Result<ExternalKerContext>;

    /// Fetch local ecosafety envelope for nodeId/familyId from SQLite/ALN.
    fn fetch_local_envelope(
        &mut self,
        node_id: &str,
        family_id: &str,
    ) -> anyhow::Result<LocalEnvelopeContext>;

    /// Compute cross-constellation workflow risk and EcoHamiltonian diagnostics.
    ///
    /// This must:
    /// - Construct r_W from external and local planes.
    /// - Compute r_W_norm (e.g., max coordinate or weighted norm).
    /// - Compute max_local_repo_risk for the target constellation.
    /// - Compute eco_h_delta, the change in EcoHamiltonian potential V(x).
    fn compute_workflow_risk_and_gate(
        &mut self,
        external: &ExternalKerContext,
        local: &LocalEnvelopeContext,
    ) -> anyhow::Result<WorkflowRiskDiagnostics>;

    /// Apply EcoHamiltonian gate predicate:
    /// - r_W_norm must be bounded by max_local_repo_risk.
    /// - eco_h_delta must be <= 0 (or within a small epsilon slack).
    fn eco_hamiltonian_gate(
        &mut self,
        diag: &WorkflowRiskDiagnostics,
    ) -> anyhow::Result<bool>;

    /// Determine whether a valid corridor exists for this node and family.
    ///
    /// Implementation typically queries vlaneadmissibility/vshardker views.
    fn corridor_present(
        &mut self,
        node_id: &str,
        family_id: &str,
    ) -> anyhow::Result<bool>;

    /// Retrieve the FogGuardVerdict corresponding to the last fog-router-guard
    /// evaluation, or compute it directly alongside FogRouteDecision.
    fn last_fog_guard_verdict(&mut self) -> FogGuardVerdictAdapter;
}

/// Adapter for FogGuardVerdict in a gateway-safe form.
#[derive(Clone, Debug, Serialize)]
pub struct FogGuardVerdictAdapter {
    pub verdict: String, // "Allow" or "Stop"
}
