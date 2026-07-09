// src/gateway/cross_constellation_fog_route.rs
// Minimal structs and handler for CrossConstellationFogRouteRequest.
//
// This module mirrors the JSON request/response shapes used by
// CrossConstellationFogRouteDuty on the Javasphere side and is
// intended for use in a non-actuating MCP/HTTP gateway daemon.

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use cyboquatic_ecosafety::{
    FogGuardBands,
    FogGuardConfig,
    FogGuardKerThresholds,
    FogGuardVerdict,
    KERWindow,
    LyapunovResidual,
    RiskCoord,
    RiskVector,
};
use fog_router_guard::{FogNodeSnapshot, FogRouteDecision, decide_route};

/// Cross-constellation FOG route evaluation request.
///
/// Matches CrossConstellationFogRouteRequest JSON from the browser.
#[derive(Clone, Debug, Deserialize)]
pub struct CrossConstellationFogRouteRequest {
    pub nodeId: String,
    pub originConstellation: String,
    pub targetConstellation: String,
    pub workflowId: String,
    pub familyId: String,
    #[serde(default)]
    pub windowId: Option<String>,
    #[serde(default)]
    pub guardConfig: Option<FogGuardConfigWire>,
}

/// Wire representation of FogGuardConfig for JSON.
///
/// This keeps the gateway decoupled from internal config defaults
/// while still allowing explicit overrides from the browser.
#[derive(Clone, Debug, Deserialize)]
pub struct FogGuardConfigWire {
    pub bands: FogGuardBandsWire,
    pub ker: FogGuardKerThresholdsWire,
}

#[derive(Clone, Debug, Deserialize)]
pub struct FogGuardBandsWire {
    pub roh_ceiling: f64,
    pub residual_max: f64,
}

#[derive(Clone, Debug, Deserialize)]
pub struct FogGuardKerThresholdsWire {
    pub k_min: f64,
    pub e_min: f64,
    pub r_max: f64,
}

impl From<FogGuardConfigWire> for FogGuardConfig {
    fn from(w: FogGuardConfigWire) -> Self {
        FogGuardConfig {
            bands: FogGuardBands {
                roh_ceiling: w.bands.roh_ceiling,
                residual_max: w.bands.residual_max,
            },
            ker: FogGuardKerThresholds {
                k_min: w.ker.k_min,
                e_min: w.ker.e_min,
                r_max: w.ker.r_max,
            },
        }
    }
}

/// Workflow risk vector r_W over key planes.
#[derive(Clone, Debug, Serialize)]
pub struct WorkflowRiskVector {
    pub r_energy: f64,
    pub r_carbon: f64,
    pub r_topology: f64,
    pub r_biodiv: f64,
}

/// Cross-constellation gate diagnostics.
#[derive(Clone, Debug, Serialize)]
pub struct CrossGateDiagnostics {
    pub rW: WorkflowRiskVector,
    pub rW_norm: f64,
    pub maxLocalRepoRisk: f64,
    pub ecoHamiltonianDelta: f64,
}

/// Local KER and Lyapunov context.
#[derive(Clone, Debug, Serialize)]
pub struct LocalContext {
    pub ker: KerSummary,
    pub roh: f64,
    pub residual: f64,
}

/// KER summary for wire format.
#[derive(Clone, Debug, Serialize)]
pub struct KerSummary {
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub kerdeployable: bool,
}

/// Origin constellation context summary.
#[derive(Clone, Debug, Serialize)]
pub struct ExternalContext {
    pub originLane: String,
    pub originKer: KerSummary,
    pub originCorridorId: String,
}

/// Cross-constellation FOG route response.
///
/// Matches CrossConstellationFogRouteResponse JSON.
#[derive(Clone, Debug, Serialize)]
pub struct CrossConstellationFogRouteResponse {
    pub r#type: String,
    pub nodeId: String,
    pub originConstellation: String,
    pub targetConstellation: String,
    pub workflowId: String,
    pub familyId: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub windowId: Option<String>,

    pub gateStatus: String,
    pub gateDiagnostics: CrossGateDiagnostics,

    pub fogRouteDecision: String,
    pub fogGuardVerdict: String,

    pub localContext: LocalContext,
    pub externalContext: ExternalContext,
}

/// Minimal handler signature for cross-constellation FOG route evaluation.
///
/// This function is intended to be called from the gateway's request router
/// when type == "CrossConstellationFogRouteRequest".
pub fn handle_cross_constellation_fog_route(
    req: CrossConstellationFogRouteRequest,
    // Dependencies injected from the gateway:
    // - a cross-constellation index client,
    // - a local ecosafety/SQLite client,
    // - an EcoHamiltonian gate implementation.
    deps: &mut GatewayDeps,
) -> anyhow::Result<CrossConstellationFogRouteResponse> {
    // 1. Fetch external lane and KER context via cross-constellation-index.
    let external = deps.fetch_external_context(
        &req.originConstellation,
        &req.workflowId,
    )?;

    // 2. Fetch local ecosafety context (risk, residual, KER) for nodeId.
    let local_env = deps.fetch_local_envelope(&req.nodeId, &req.familyId)?;
    let local_risk: RiskVector = local_env.risk;
    let local_residual: LyapunovResidual = local_env.residual;
    let local_ker: KERWindow = local_env.ker;

    // 3. Build workflow risk vector r_W and gate diagnostics.
    let (r_w, r_w_norm, max_local_repo_risk, eco_h_delta) =
        deps.compute_workflow_risk_and_gate(
            &external,
            &local_env,
        )?;

    // 4. Apply EcoHamiltonian gate.
    let gate_ok = deps.eco_hamiltonian_gate(
        r_w_norm,
        max_local_repo_risk,
        eco_h_delta,
    )?;

    let gate_status = if gate_ok {
        "Accepted".to_string()
    } else {
        "Rejected".to_string()
    };

    // 5. If gate is accepted, build FogNodeSnapshot and run local fog-router-guard.
    let (fog_route_decision_str, fog_guard_verdict_str) = if gate_ok {
        let snapshot = FogNodeSnapshot {
            lane: local_env.lane,
            risk: local_risk,
            ker_window: local_ker,
            prev_residual: local_residual,
            evidencehex: local_env.evidencehex.clone(),
            did: local_env.did.clone(),
            corridor_present: deps.corridor_present(&req.nodeId, &req.familyId)?,
        };

        let guard_cfg = match &req.guardConfig {
            Some(wire) => FogGuardConfig::from(wire.clone()),
            None => FogGuardConfig::default(),
        };

        let decision = decide_route(&snapshot, Some(guard_cfg.clone()));
        let verdict = deps.last_fog_guard_verdict(); // or compute inside decide_route

        (match decision {
            FogRouteDecision::AllowRoute => "AllowRoute".to_string(),
            FogRouteDecision::BlockRoute => "BlockRoute".to_string(),
        },
        match verdict {
            FogGuardVerdict::Allow => "Allow".to_string(),
            FogGuardVerdict::Stop => "Stop".to_string(),
        })
    } else {
        ("BlockRoute".to_string(), "Stop".to_string())
    };

    // 6. Pack local and external context summaries.
    let local_ker_summary = KerSummary {
        k: local_ker.k(),
        e: local_ker.e(),
        r: local_ker.r(),
        kerdeployable: local_ker.kerdeployable(),
    };

    let local_roh = RiskCoord::new_clamped(local_ker.r()).value();
    let local_ctx = LocalContext {
        ker: local_ker_summary,
        roh: local_roh,
        residual: local_residual.value,
    };

    let external_ctx = ExternalContext {
        originLane: external.origin_lane.clone(),
        originKer: KerSummary {
            k: external.origin_k,
            e: external.origin_e,
            r: external.origin_r,
            kerdeployable: external.origin_kerdeployable,
        },
        originCorridorId: external.origin_corridor_id.clone(),
    };

    let resp = CrossConstellationFogRouteResponse {
        r#type: "CrossConstellationFogRouteResponse".to_string(),
        nodeId: req.nodeId,
        originConstellation: req.originConstellation,
        targetConstellation: req.targetConstellation,
        workflowId: req.workflowId,
        familyId: req.familyId,
        windowId: req.windowId,
        gateStatus: gate_status,
        gateDiagnostics: CrossGateDiagnostics {
            rW: WorkflowRiskVector {
                r_energy: r_w.r_energy,
                r_carbon: r_w.r_carbon,
                r_topology: r_w.r_topology,
                r_biodiv: r_w.r_biodiv,
            },
            rW_norm: r_w_norm,
            maxLocalRepoRisk: max_local_repo_risk,
            ecoHamiltonianDelta: eco_h_delta,
        },
        fogRouteDecision: fog_route_decision_str,
        fogGuardVerdict: fog_guard_verdict_str,
        localContext: local_ctx,
        externalContext: external_ctx,
    };

    Ok(resp)
}
