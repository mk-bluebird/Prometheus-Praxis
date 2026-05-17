// filename: eco_restoration_shard/src/block_26_30.rs

// Non-actuating models and governance logic for questions 26–30.
// All math reuses the frozen Lyapunov/KER grammar and existing EcoNet spine surfaces.[file:7][file:11]

use std::f32;

// ---------- 26. Synergy bonus / penalty over K/E/R ----------

/// SynergyModel encodes how to combine two technologies' K/E/R into a joint
/// eco-wealth multiplier with explicit penalties for negative synergy.[file:7]
#[derive(Clone, Copy, Debug)]
pub struct KerTriple {
    pub k: f32,
    pub e: f32,
    pub r: f32,
}

#[derive(Clone, Copy, Debug)]
pub struct SynergyParams {
    /// Weight on joint K and E for positive synergy.
    pub alpha_k: f32,
    pub alpha_e: f32,
    /// Penalty scaling for negative K/E interactions.
    pub beta_k: f32,
    pub beta_e: f32,
    /// Penalty scaling for increased risk R when combined.
    pub gamma_r: f32,
    /// Minimum joint R floor (cannot be better than best constituent).
    pub r_floor: f32,
}

/// Compute pairwise K/E/R deltas and a synergy score in [-1,1].
///
/// Intuition:
/// - Positive synergy if the combination improves K and/or E beyond the
///   max of the two inputs without increasing R.[file:7][file:11]
/// - Negative synergy if K/E worsen or R increases versus the min/max baselines.
/// - Negative synergy produces a penalty that can dominate bonuses so
///   net eco-wealth cannot be minted for harmful combinations.[file:11]
pub fn synergy_score_pair(a: KerTriple, b: KerTriple, joint: KerTriple, p: SynergyParams) -> f32 {
    let k_base = a.k.max(b.k);
    let e_base = a.e.max(b.e);
    let r_best = a.r.min(b.r);

    let dk = joint.k - k_base;
    let de = joint.e - e_base;
    let dr = joint.r - r_best;

    let pos = (p.alpha_k * dk.max(0.0)) + (p.alpha_e * de.max(0.0));
    let neg = (p.beta_k * (-dk.min(0.0)))
            + (p.beta_e * (-de.min(0.0)))
            + (p.gamma_r * dr.max(0.0));

    let raw = pos - neg;

    // Normalize into [-1,1] with a smooth saturation.
    raw.tanh()
}

/// Derive an eco-wealth multiplier from synergy score.
/// - s > 0 ⇒ bonus up to +20%.
/// - s < 0 ⇒ penalty up to -100% (wealth factor can be pushed to 0),
///   so no positive eco-wealth is minted when synergy is strongly negative.[file:11]
pub fn synergy_wealth_multiplier(s: f32) -> f32 {
    let s_clamped = s.max(-1.0).min(1.0);
    if s_clamped >= 0.0 {
        // Positive synergy: linear ramp up to +0.2.
        1.0 + 0.2 * s_clamped
    } else {
        // Negative synergy: up to full cancellation.
        (1.0 + s_clamped).max(0.0)
    }
}

// ---------- 27. Lane promotion lifecycle for RestorationTechnology ----------

/// RestorationTechnology enum – lanes governed in ecosafety spine.[file:11]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RestorationTechnology {
    LaserPhytoEnhanced,
    // Other variants could be added here.
}

/// Lane states for technology deployments.[file:7]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TechLane {
    Research,
    ExpProd,
    Prod,
}

#[derive(Clone, Debug)]
pub struct LanePromotionConfig {
    pub min_k_research: f32,
    pub min_e_research: f32,
    pub max_r_research: f32,

    pub min_k_expprod: f32,
    pub min_e_expprod: f32,
    pub max_r_expprod: f32,

    pub min_k_prod: f32,
    pub min_e_prod: f32,
    pub max_r_prod: f32,

    /// Minimum replay windows required for each lane transition.
    pub min_windows_research_to_expprod: u32,
    pub min_windows_expprod_to_prod: u32,

    /// Multisig thresholds (e.g., 2 of 3 domain experts + 1 governance DID).
    pub min_domain_sigs: u32,
    pub min_governance_sigs: u32,
}

#[derive(Clone, Debug)]
pub struct ReplayWindowSummary {
    pub window_count: u32,
    pub k_avg: f32,
    pub e_avg: f32,
    pub r_max: f32,
    pub v_non_increasing: bool, // Lyapunov safestep satisfied over windows.[file:7]
}

/// Check if replay-based conditions for Research → ExpProd are satisfied.
pub fn can_promote_research_to_expprod(
    tech: RestorationTechnology,
    replay: &ReplayWindowSummary,
    cfg: &LanePromotionConfig,
    domain_sigs: u32,
    governance_sigs: u32,
) -> bool {
    if tech != RestorationTechnology::LaserPhytoEnhanced {
        return false;
    }
    if replay.window_count < cfg.min_windows_research_to_expprod {
        return false;
    }
    if replay.k_avg < cfg.min_k_expprod
        || replay.e_avg < cfg.min_e_expprod
        || replay.r_max > cfg.max_r_expprod
        || !replay.v_non_increasing
    {
        return false;
    }
    if domain_sigs < cfg.min_domain_sigs {
        return false;
    }
    if governance_sigs < cfg.min_governance_sigs {
        return false;
    }
    true
}

/// Check if ExpProd → Prod promotion is allowed.
pub fn can_promote_expprod_to_prod(
    tech: RestorationTechnology,
    replay: &ReplayWindowSummary,
    cfg: &LanePromotionConfig,
    domain_sigs: u32,
    governance_sigs: u32,
) -> bool {
    if tech != RestorationTechnology::LaserPhytoEnhanced {
        return false;
    }
    if replay.window_count < cfg.min_windows_expprod_to_prod {
        return false;
    }
    if replay.k_avg < cfg.min_k_prod
        || replay.e_avg < cfg.min_e_prod
        || replay.r_max > cfg.max_r_prod
        || !replay.v_non_increasing
    {
        return false;
    }
    if domain_sigs < cfg.min_domain_sigs {
        return false;
    }
    if governance_sigs < cfg.min_governance_sigs {
        return false;
    }
    true
}

/// Example configuration for laser-phyto-enhanced technology.
/// - Requires strong K/E and low R over multiple windows.[file:7][file:11]
pub fn example_laser_phyto_lane_config() -> LanePromotionConfig {
    LanePromotionConfig {
        min_k_research: 0.70,
        min_e_research: 0.70,
        max_r_research: 0.40,

        min_k_expprod: 0.90,
        min_e_expprod: 0.90,
        max_r_expprod: 0.20,

        min_k_prod: 0.94,
        min_e_prod: 0.92,
        max_r_prod: 0.13,

        min_windows_research_to_expprod: 12, // e.g., 12 monthly windows.
        min_windows_expprod_to_prod: 24,

        min_domain_sigs: 2,
        min_governance_sigs: 1,
    }
}

// ---------- 28. Trusted telemetry vs compromised data ----------

/// TelemetryTrustLevel classifies how telemetry is validated.[file:7][file:11]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TelemetryTrustLevel {
    RawUnverified,
    SensorChainVerified,
    CrossPlaneConsistent,
    ExternalAnchorVerified,
}

/// Simple aggregator showing fallback anchors:
/// - Prefer sensor + cross-plane consistency.
/// - Fallback to external anchors (lab assays, independent agencies) if sensors are suspect.[file:11]
pub fn classify_telemetry_trust(
    has_signed_sensor_chain: bool,
    passes_crossplane_checks: bool,
    has_external_lab_anchor: bool,
) -> TelemetryTrustLevel {
    if has_signed_sensor_chain && passes_crossplane_checks {
        TelemetryTrustLevel::CrossPlaneConsistent
    } else if has_signed_sensor_chain {
        TelemetryTrustLevel::SensorChainVerified
    } else if has_external_lab_anchor {
        TelemetryTrustLevel::ExternalAnchorVerified
    } else {
        TelemetryTrustLevel::RawUnverified
    }
}

/// Determine whether telemetry is eligible for EcoWealthClawback recomputation.
/// - Requires at least sensor-chain verification OR external anchor.[file:11]
pub fn telemetry_eligible_for_clawback(trust: TelemetryTrustLevel) -> bool {
    match trust {
        TelemetryTrustLevel::RawUnverified => false,
        TelemetryTrustLevel::SensorChainVerified => true,
        TelemetryTrustLevel::CrossPlaneConsistent => true,
        TelemetryTrustLevel::ExternalAnchorVerified => true,
    }
}

// ---------- 29. Atomic ActionTypeWealthWeight + planeweights updates ----------

/// Simple in-process lock token for coordinating ActionTypeWealthWeight and planeweights
/// updates across Paycomp instances.[file:11]
#[derive(Clone, Debug)]
pub struct WeightUpdateLock {
    pub lock_id: String,
    pub acquired_by: String, // DID
    pub version: i64,
}

/// Consensus result for a proposed weight update.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum WeightUpdateDecision {
    Commit,
    Abort,
}

/// Proposed new weights bundle.
#[derive(Clone, Debug)]
pub struct WeightUpdateProposal {
    pub planeweights_version: i64,
    pub actiontype_version: i64,
    pub proposer_did: String,
    pub payload_hex: String, // deterministic ALN+SQL change set hex.
}

/// Verify that a proposal is consistent and ready for commit.
/// In practice, this would perform:
/// - Schema checks.
/// - KER bounds checks.
/// - Multi-sig verification across Paycomp instances.[file:11]
pub fn verify_weight_update_proposal(
    proposal: &WeightUpdateProposal,
    required_signatures: u32,
    collected_signatures: u32,
) -> WeightUpdateDecision {
    if collected_signatures < required_signatures {
        return WeightUpdateDecision::Abort;
    }
    // Additional deterministic checks would be applied here.
    WeightUpdateDecision::Commit
}

// ---------- 30. Demurrage λ in a simple heterogeneous steward model ----------

/// Steward type: hoarder vs active restorer.[file:11]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StewardType {
    Hoarder,
    Restorer,
}

/// Utility parameters for stewards.
/// u(W) = W^(1 - rho)/(1 - rho) - theta * (demurrage_cost),
/// used only qualitatively here.[file:11]
#[derive(Clone, Copy, Debug)]
pub struct StewardParams {
    pub rho: f32,     // risk aversion.
    pub theta: f32,   // sensitivity to demurrage.
    pub discount: f32,
}

/// Compute one-period wealth under demurrage λ:
/// W_{t+1} = (W_t - spend) * (1 - λ).[file:11]
pub fn next_wealth_demurrage(w_t: f32, spend: f32, lambda: f32) -> f32 {
    let remaining = (w_t - spend).max(0.0);
    remaining * (1.0 - lambda).max(0.0)
}

/// Qualitative scoring of λ (0–0.2) based on two stylized types:
/// - Too low λ ⇒ hoarding.
/// - Too high λ ⇒ panic selling by hoarders and distortion for restorers.
/// Returns a heuristic "stress score", lower is better.[file:11]
pub fn demurrage_stress_score(lambda: f32) -> f32 {
    let lam = lambda.max(0.0).min(0.2);
    let hoarder_stress = (0.02 - lam).max(0.0); // hoarder likes λ ≈ 0.
    let restorer_stress = (lam - 0.10).max(0.0); // restorer dislikes λ too high.
    hoarder_stress + restorer_stress
}
