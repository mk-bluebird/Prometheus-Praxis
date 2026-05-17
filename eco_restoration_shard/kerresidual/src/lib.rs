// filename: eco_restoration_shard/kerresidual/src/lib.rs

//! kerresidual: shared KER and Lyapunov residual math spine for EcoFort.
//!
//! Knowledge factor: 0.95
//! Eco-impact value: 0.92
//! Residual risk-of-harm: 0.12
//!
//! This crate is non-actuating, read-only with respect to any controller
//! surfaces. It provides a single source of truth for:
//!   - Risk coordinates and planes
//!   - Lyapunov residual V_t
//!   - K/E/R window metrics
//!   - ResponsibilityAxis and topology penalties
//!   - Time-decayed responsibility and r_portfolio_diversity

use serde::{Deserialize, Serialize};

/// Identifier for a normalized risk coordinate r_j in [0,1].
#[derive(Debug, Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct RiskCoordId {
    pub name: String,
    /// Lyapunov plane, e.g. "CARBON", "BIODIVERSITY", "TOPOLOGY", "RESPONSIBILITY".
    pub plane: String,
}

/// Single risk coordinate value r_j in [0,1].
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct RiskCoord {
    pub id: RiskCoordId,
    pub value: f64,
}

/// Plane-level weight and flags, mirroring PlaneWeightsShard2026v1.aln.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeight {
    pub plane: String,
    pub weight: f64,
    /// Non-offsettable planes (e.g. carbon, biodiversity, responsibility) cannot
    /// be compensated by improvements in other planes.
    pub non_offsettable: bool,
}

/// Collection of plane weights, keyed by plane name.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeights {
    pub planes: Vec<PlaneWeight>,
}

impl PlaneWeights {
    pub fn weight_for_plane(&self, plane: &str) -> f64 {
        self.planes
            .iter()
            .find(|p| p.plane == plane)
            .map(|p| p.weight)
            .unwrap_or(0.0)
    }

    pub fn is_non_offsettable(&self, plane: &str) -> bool {
        self.planes
            .iter()
            .find(|p| p.plane == plane)
            .map(|p| p.non_offsettable)
            .unwrap_or(false)
    }
}

/// Risk vector for a shard window: normalized coordinates plus optional
/// topology and responsibility overlays.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskVector {
    pub coords: Vec<RiskCoord>,
    /// Topology risk r_topology in [0,1], combining manifest drift and
    /// representation imbalance for the region/portfolio.
    pub r_topology: Option<f64>,
    /// Responsibility axis aggregate in [0,1], derived from healthcare /
    /// vampiric coordinates (e.g. r_pharma, r_toxicity, r_override).
    pub r_responsibility: Option<f64>,
    /// Portfolio diversity coordinate r_portfolio_diversity in [0,1],
    /// where higher values mean lower diversity (higher risk).
    pub r_portfolio_diversity: Option<f64>,
}

/// Per-window K/E/R snapshot (already lane-normalized and corridor-checked).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerSnapshot {
    pub k: f64,
    pub e: f64,
    pub r: f64,
}

/// Lyapunov residual result, including core and overlay terms.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct ResidualResult {
    pub vt_core: f64,
    pub vt_with_overlays: f64,
    pub r_topology: f64,
    pub r_responsibility: f64,
    pub r_portfolio_diversity: f64,
}

/// Compute Lyapunov residual V_t = Σ_j w_j * r_j^2 using plane weights and
/// normalized risk coordinates.
///
/// This function is the single source of truth for residual computation
/// used by EcoFort SQL views (vshardresidual, vshardtopologyker) and
/// higher-level EcoUnit calculations.
pub fn compute_residual(
    rv: &RiskVector,
    weights: &PlaneWeights,
) -> ResidualResult {
    let mut vt_core = 0.0;

    for coord in &rv.coords {
        let w = weights.weight_for_plane(&coord.id.plane);
        if w > 0.0 {
            vt_core += w * coord.value * coord.value;
        }
    }

    // Overlay: topology, responsibility, and portfolio diversity penalties.
    let r_topology = rv.r_topology.unwrap_or(0.0).clamp(0.0, 1.0);
    let w_topology = weights.weight_for_plane("TOPOLOGY");

    let r_resp = rv
        .r_responsibility
        .unwrap_or(0.0)
        .clamp(0.0, 1.0);
    let w_resp = weights.weight_for_plane("RESPONSIBILITY");

    let r_div = rv
        .r_portfolio_diversity
        .unwrap_or(0.0)
        .clamp(0.0, 1.0);
    let w_div = weights.weight_for_plane("PORTFOLIO_DIVERSITY");

    let vt_with_overlays =
        vt_core
        + w_topology * r_topology * r_topology
        + w_resp * r_resp * r_resp
        + w_div * r_div * r_div;

    ResidualResult {
        vt_core,
        vt_with_overlays,
        r_topology,
        r_responsibility: r_resp,
        r_portfolio_diversity: r_div,
    }
}

/// Responsibility time-decay policy: compute a decayed responsibility score
/// r_resp_decayed given:
///   - r_resp_current in [0,1]
///   - half_life_days > 0
///   - elapsed_days >= 0
///
/// The decay is non-retroactive at the ledger level: this function is used
/// only for forward-looking eligibility and gating, not to rewrite history.
pub fn decay_responsibility(
    r_resp_current: f64,
    half_life_days: f64,
    elapsed_days: f64,
) -> f64 {
    if half_life_days <= 0.0 || elapsed_days <= 0.0 {
        return r_resp_current.clamp(0.0, 1.0);
    }

    let lambda = (0.5_f64).ln() / half_life_days;
    let factor = (lambda * elapsed_days).exp();
    (r_resp_current * factor).clamp(0.0, 1.0)
}

/// Compute r_portfolio_diversity from a vector of weighted archetype shares.
/// shares[i] is the fractional weight of archetype i, 0 <= shares[i] <= 1,
/// Σ shares[i] = 1.0.
///
/// Here we use a normalized Herfindahl-Hirschman style index:
///   H = Σ shares[i]^2
///   r_div = (H - 1/N_max) / (1 - 1/N_max)  in [0,1]
/// where N_max is a governance-configured maximum archetype count used for
/// normalization (e.g. 16). This makes r_div = 0 for perfectly uniform
/// portfolios and r_div -> 1 as concentration increases.
pub fn compute_r_portfolio_diversity(
    shares: &[f64],
    n_max: usize,
) -> f64 {
    if shares.is_empty() || n_max == 0 {
        return 0.0;
    }

    let mut sum = 0.0;
    for s in shares {
        let v = s.max(0.0);
        sum += v * v;
    }

    let n_max_f = n_max as f64;
    let h_min = 1.0 / n_max_f;
    let h_max = 1.0;
    let mut r = if h_max > h_min {
        (sum - h_min) / (h_max - h_min)
    } else {
        0.0
    };

    if r < 0.0 {
        r = 0.0;
    }
    if r > 1.0 {
        r = 1.0;
    }

    r
}

/// EcoUnitKernel parameters from EcoWealthKernel2026v1.aln.
/// EcoUnit_raw = (K^alpha) * (E^beta) * ( (1.0 - R)^gamma )
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct EcoUnitKernel {
    pub alpha: f64,
    pub beta: f64,
    pub gamma: f64,
}

/// Compute raw EcoUnit from a KER snapshot and kernel exponents.
/// Caller is responsible for clamping K/E/R into [0,1] and ensuring
/// non-compensability for non-offsettable planes at governance level.
pub fn compute_ecounit_raw(
    ker: KerSnapshot,
    kernel: EcoUnitKernel,
) -> f64 {
    let k = ker.k.clamp(0.0, 1.0);
    let e = ker.e.clamp(0.0, 1.0);
    let r = ker.r.clamp(0.0, 1.0);

    let k_term = if kernel.alpha == 0.0 {
        1.0
    } else {
        k.powf(kernel.alpha)
    };

    let e_term = if kernel.beta == 0.0 {
        1.0
    } else {
        e.powf(kernel.beta)
    };

    let risk_base = (1.0 - r).max(0.0);
    let r_term = if kernel.gamma == 0.0 {
        1.0
    } else {
        risk_base.powf(kernel.gamma)
    };

    k_term * e_term * r_term
}

/// Aggregate trust discounts and clamp to [0, max_total].
pub fn aggregate_trust_discount(
    data_discount: f64,
    topology_discount: f64,
    responsibility_discount: f64,
    max_total: f64,
) -> f64 {
    let mut total =
        data_discount.max(0.0)
        + topology_discount.max(0.0)
        + responsibility_discount.max(0.0);

    if total > max_total {
        total = max_total;
    }

    total
}

/// Compute final EcoUnit:
///   EcoUnit_final = (S_region * EcoUnit_raw + B_s)
///                   * (1.0 - trust_discount_total)
pub fn compute_ecounit_final(
    ecounit_raw: f64,
    s_region: f64,
    b_s: f64,
    trust_discount_total: f64,
) -> f64 {
    let td = trust_discount_total.clamp(0.0, 0.8);
    let base = s_region * ecounit_raw + b_s;
    base * (1.0 - td)
}
