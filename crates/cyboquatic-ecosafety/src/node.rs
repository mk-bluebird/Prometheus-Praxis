// Filename: crates/cyboquatic-ecosafety/src/node.rs
//! Node ecosafety envelope and risk sample schema.

use serde::{Deserialize, Serialize};

use crate::{KERWindow, LyapunovResidual, LyapunovWeights, RiskCoord, RiskVector};

/// Lane tag for Cyboquatic nodes.
///
/// This is used to distinguish research pilots from production
/// deployments and to apply different KER thresholds if needed.[file:23]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum CyboLane {
    /// Research-only lane, exploratory or high-uncertainty.
    Research,
    /// Pilot / limited deployment lane.
    Pilot,
    /// Production lane (Phoenix city-scale).
    Production,
}

/// Immutable risk sample taken from a Cyboquatic node.
///
/// This is a non-actuating, schema-bound snapshot designed to map
/// 1:1 onto ALN and SQL columns for ecosafety diagnostics:
///
/// - PFAS and CEC concentrations and their normalized risk.
/// - SAT corridor state and breakthrough risk.
/// - Surcharge and hydraulic lane.
/// - Biodiversity / habitat metric.
/// - Vt residual and KER-derived deployability.
/// - Evidence hex and Bostrom DID for provenance.[file:21][file:23][file:25]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct NodeRiskSample {
    pfas_ng_l: f64,
    cec_index: f64,
    trap_index: f64,
    sat_index: f64,
    surcharge_index: f64,
    biodiv_index: f64,
    vt: f64,
    lane: CyboLane,
    ker_deployable: bool,
    evidence_hex: String,
    did: String,
}

impl NodeRiskSample {
    /// Construct a new risk sample from raw indices and ecosafety summaries.
    ///
    /// `vt` is the current Lyapunov residual, `ker_window` summarizes
    /// recent behavior, and `lane` tags the governance lane.[file:23][file:24]
    pub fn new(
        pfas_ng_l: f64,
        cec_index: f64,
        trap_index: f64,
        sat_index: f64,
        surcharge_index: f64,
        biodiv_index: f64,
        vt: f64,
        lane: CyboLane,
        ker_window: &KERWindow,
        evidence_hex: String,
        did: String,
    ) -> Self {
        let ker_deployable = ker_window.ker_deployable();
        Self {
            pfas_ng_l,
            cec_index,
            trap_index,
            sat_index,
            surcharge_index,
            biodiv_index,
            vt,
            lane,
            ker_deployable,
            evidence_hex,
            did,
        }
    }

    /// Raw PFAS concentration in ng/L as used in ALN/SQL corridors.
    pub fn pfas_ng_l(&self) -> f64 {
        self.pfas_ng_l
    }

    /// Composite CEC index (pharmaceuticals, PFAS, and other CECs).
    pub fn cec_index(&self) -> f64 {
        self.cec_index
    }

    /// TRAP index (turbine risk and pressure derivative abstraction).
    pub fn trap_index(&self) -> f64 {
        self.trap_index
    }

    /// SAT corridor index (wet/dry duty, clogging, breakthrough).
    pub fn sat_index(&self) -> f64 {
        self.sat_index
    }

    /// Surcharge / hydraulic risk index.
    pub fn surcharge_index(&self) -> f64 {
        self.surcharge_index
    }

    /// Biodiversity / habitat index for the node.
    pub fn biodiv_index(&self) -> f64 {
        self.biodiv_index
    }

    /// Current Lyapunov residual Vt.
    pub fn vt(&self) -> f64 {
        self.vt
    }

    /// Governance lane tag.
    pub fn lane(&self) -> CyboLane {
        self.lane
    }

    /// Whether this node is ker-deployable under Phoenix production gates.
    pub fn ker_deployable(&self) -> bool {
        self.ker_deployable
    }

    /// Hex-encoded evidence digest (evidencehex) of the node shard.
    pub fn evidence_hex(&self) -> &str {
        &self.evidence_hex
    }

    /// Bostrom DID associated with this node's governance.
    pub fn did(&self) -> &str {
        &self.did
    }
}

/// Ecosafety envelope for a Cyboquatic node at one timestep.
///
/// This is the Rust-side mirror of the ALN particle
/// `CyboNodeEcosafetyEnvelope`, capturing the live risk vector,
/// residual, KER window, lane, and provenance fields.[file:23][file:25]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct CyboNodeEcosafetyEnvelope {
    lane: CyboLane,
    risk: RiskVector,
    weights: LyapunovWeights,
    residual: LyapunovResidual,
    ker: KERWindow,
    evidence_hex: String,
    did: String,
}

impl CyboNodeEcosafetyEnvelope {
    /// Create a new envelope from normalized risk coordinates and governance tags.
    pub fn new(
        lane: CyboLane,
        risk: RiskVector,
        weights: LyapunovWeights,
        prev_residual: LyapunovResidual,
        ker_prev: KERWindow,
        evidence_hex: String,
        did: String,
    ) -> Self {
        let residual = LyapunovResidual::from_vector(&risk, &weights);
        let mut ker = ker_prev;
        ker.update(prev_residual, residual, &risk);

        Self {
            lane,
            risk,
            weights,
            residual,
            ker,
            evidence_hex,
            did,
        }
    }

    /// Governance lane of this node.
    pub fn lane(&self) -> CyboLane {
        self.lane
    }

    /// Current normalized risk vector.
    pub fn risk(&self) -> &RiskVector {
        &self.risk
    }

    /// Current Lyapunov weights.
    pub fn weights(&self) -> &LyapunovWeights {
        &self.weights
    }

    /// Current Lyapunov residual.
    pub fn residual(&self) -> LyapunovResidual {
        self.residual
    }

    /// Current KER window.
    pub fn ker(&self) -> &KERWindow {
        &self.ker
    }

    /// Evidence hex (evidencehex) binding this envelope to qpudatashards.
    pub fn evidence_hex(&self) -> &str {
        &self.evidence_hex
    }

    /// Bostrom DID for this node instance.
    pub fn did(&self) -> &str {
        &self.did
    }

    /// Whether this envelope is ker-deployable under production gates.
    pub fn ker_deployable(&self) -> bool {
        self.ker.ker_deployable()
    }

    /// Export a SQL/ALN-compatible `NodeRiskSample` for diagnostics.
    pub fn to_risk_sample(&self) -> NodeRiskSample {
        NodeRiskSample::new(
            0.0,
            0.0,
            0.0,
            self.risk.r_sat.value(),
            self.risk.r_surcharge.value(),
            self.risk.r_biodiv.value(),
            self.residual.value,
            self.lane,
            &self.ker,
            self.evidence_hex.clone(),
            self.did.clone(),
        )
    }
}
