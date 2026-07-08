// Filename: crates/cyboquatic_core/src/lyapunov.rs
// Destination: mk-bluebird/eco_restoration_shard (mono-repo)

//! Lyapunov residual computation for a cyboquatic node over
//! energy, hydraulics, and biological planes, using a summed
//! quadratic form V_t = Σ w_i r_i^2 where r_i ∈ [0,1].

#![allow(clippy::derive_partial_eq_without_eq)]

use std::fmt::Debug;

/// Normalized risk coordinates for a cyboquatic node.
/// Values are expected in [0.0, 1.0] after corridor folding.
#[derive(Clone, Copy, Debug)]
pub struct RiskVector3 {
    pub r_energy: f64,
    pub r_hydraulic: f64,
    pub r_bio: f64,
}

/// Optional expanded biological risk coordinates.
#[derive(Clone, Copy, Debug)]
pub struct RiskVector5 {
    pub r_energy: f64,
    pub r_hydraulic: f64,
    pub r_pathogen: f64,
    pub r_fouling: f64,
    pub r_cec: f64,
}

/// Lyapunov weights for the three-plane residual.
#[derive(Clone, Copy, Debug)]
pub struct LyapunovWeights3 {
    pub w_energy: f64,
    pub w_hydraulic: f64,
    pub w_bio: f64,
}

/// Lyapunov weights for the expanded five-coordinate residual.
#[derive(Clone, Copy, Debug)]
pub struct LyapunovWeights5 {
    pub w_energy: f64,
    pub w_hydraulic: f64,
    pub w_pathogen: f64,
    pub w_fouling: f64,
    pub w_cec: f64,
}

/// Minimal qpudatashard view for a cyboquatic node.
/// This mirrors the NodeShard layout used in prior FOG routing work:
/// energy, hydraulics, biology, plus a local residual slot.
///
/// In the full mono-repo this struct should be kept consistent with
/// the ALN/CSV schema used for qpudatashardsparticles*Cyboquatic*.
#[derive(Clone, Copy, Debug)]
pub struct QpuDatashard {
    // Energy plane
    pub esurplus_j: f64,       // EsurplusJ
    pub pmargin_kw: f64,       // PmarginkW
    pub d_edt_w: f64,          // dEdt (W or J/s), sign indicates trend

    // Hydraulics plane
    pub q_m3_s: f64,           // Q (flow)
    pub hlr_m_per_h: f64,      // HLR
    pub r_surcharge: f64,      // normalized surcharge risk in [0,1]

    // Biological plane (already normalized risk coordinates)
    pub r_pathogen: f64,       // [0,1]
    pub r_fouling: f64,        // [0,1]
    pub r_cec: f64,            // [0,1]

    // Local residual view
    pub vt_local: f64,         // previously computed V_t for this node
}

/// Clamp helper to keep risk coordinates inside [0,1].
fn clamp01(x: f64) -> f64 {
    if x < 0.0 {
        0.0
    } else if x > 1.0 {
        1.0
    } else {
        x
    }
}

/// Fold the energy-plane shard fields into a single normalized risk
/// coordinate r_energy ∈ [0,1]. The exact corridor normalization
/// should match the CorridorBands rows for energy in qpudatashards.
/// Here we use a simple illustrative surrogate that increases risk
/// as surplus shrinks and power margin drops.
pub fn fold_energy_risk(shard: &QpuDatashard,
                        esurplus_safe_min_j: f64,
                        esurplus_hard_min_j: f64,
                        pmargin_safe_min_kw: f64,
                        pmargin_hard_min_kw: f64) -> f64
{
    // Surplus risk: 0 at safe_min, 1 at hard_min or below.
    let surplus = shard.esurplus_j;
    let r_surplus = if surplus <= esurplus_hard_min_j {
        1.0
    } else if surplus >= esurplus_safe_min_j {
        0.0
    } else {
        let num = esurplus_safe_min_j - surplus;
        let den = esurplus_safe_min_j - esurplus_hard_min_j;
        clamp01(num / den)
    };

    // Margin risk: 0 at safe_min, 1 at hard_min or below.
    let pmargin = shard.pmargin_kw;
    let r_margin = if pmargin <= pmargin_hard_min_kw {
        1.0
    } else if pmargin >= pmargin_safe_min_kw {
        0.0
    } else {
        let num = pmargin_safe_min_kw - pmargin;
        let den = pmargin_safe_min_kw - pmargin_hard_min_kw;
        clamp01(num / den)
    };

    // Combine energy risks; weighting can be tuned via corridors.
    // Here we take a simple max to remain conservative.
    let r_energy = r_surplus.max(r_margin);

    clamp01(r_energy)
}

/// Fold hydraulics-plane fields into r_hydraulic ∈ [0,1].
/// Since r_surcharge is already a normalized corridor coordinate,
/// we can use it directly after clamping.
pub fn fold_hydraulic_risk(shard: &QpuDatashard) -> f64 {
    clamp01(shard.r_surcharge)
}

/// Fold biological-plane fields into a single r_bio ∈ [0,1]
/// as the max of r_pathogen, r_fouling, r_cec.
pub fn fold_bio_risk(shard: &QpuDatashard) -> f64 {
    let r_p = clamp01(shard.r_pathogen);
    let r_f = clamp01(shard.r_fouling);
    let r_c = clamp01(shard.r_cec);
    clamp01(r_p.max(r_f).max(r_c))
}

/// Build the three-coordinate risk vector from a qpudatashard and
/// corridor parameters for the energy plane.
pub fn risk_vector3_from_shard(
    shard: &QpuDatashard,
    esurplus_safe_min_j: f64,
    esurplus_hard_min_j: f64,
    pmargin_safe_min_kw: f64,
    pmargin_hard_min_kw: f64,
) -> RiskVector3 {
    RiskVector3 {
        r_energy: fold_energy_risk(
            shard,
            esurplus_safe_min_j,
            esurplus_hard_min_j,
            pmargin_safe_min_kw,
            pmargin_hard_min_kw,
        ),
        r_hydraulic: fold_hydraulic_risk(shard),
        r_bio: fold_bio_risk(shard),
    }
}

/// Build the five-coordinate risk vector using the same energy and
/// hydraulic folding but keeping the biological coordinates separate.
pub fn risk_vector5_from_shard(
    shard: &QpuDatashard,
    esurplus_safe_min_j: f64,
    esurplus_hard_min_j: f64,
    pmargin_safe_min_kw: f64,
    pmargin_hard_min_kw: f64,
) -> RiskVector5 {
    RiskVector5 {
        r_energy: fold_energy_risk(
            shard,
            esurplus_safe_min_j,
            esurplus_hard_min_j,
            pmargin_safe_min_kw,
            pmargin_hard_min_kw,
        ),
        r_hydraulic: fold_hydraulic_risk(shard),
        r_pathogen: clamp01(shard.r_pathogen),
        r_fouling: clamp01(shard.r_fouling),
        r_cec: clamp01(shard.r_cec),
    }
}

/// Compute V_t for the three-plane residual.
/// V_t = w_E r_E^2 + w_H r_H^2 + w_B r_B^2
pub fn compute_vt3(rv: &RiskVector3, w: &LyapunovWeights3) -> f64 {
    w.w_energy * rv.r_energy * rv.r_energy
        + w.w_hydraulic * rv.r_hydraulic * rv.r_hydraulic
        + w.w_bio * rv.r_bio * rv.r_bio
}

/// Compute V_t for the expanded five-coordinate residual.
/// V_t = Σ w_i r_i^2 over {energy, hydraulic, pathogen, fouling, cec}
pub fn compute_vt5(rv: &RiskVector5, w: &LyapunovWeights5) -> f64 {
    w.w_energy * rv.r_energy * rv.r_energy
        + w.w_hydraulic * rv.r_hydraulic * rv.r_hydraulic
        + w.w_pathogen * rv.r_pathogen * rv.r_pathogen
        + w.w_fouling * rv.r_fouling * rv.r_fouling
        + w.w_cec * rv.r_cec * rv.r_cec
}

/// Simple Lyapunov non-increase check for local node updates.
/// In the full CPVM stack this would be paired with kani assertions
/// and shard-backed corridor predicates to ensure V_t_next <= V_t_prev.
pub fn lyapunov_non_increasing(vt_prev: f64, vt_next: f64) -> bool {
    vt_next <= vt_prev
}
