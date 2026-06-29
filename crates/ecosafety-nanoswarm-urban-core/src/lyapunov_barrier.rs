// Filename: crates/ecosafety-nanoswarm-urban-core/src/lyapunov_barrier.rs

use crate::types::{Residual, RiskCoord, CorridorBands};

#[derive(Clone, Copy, Debug)]
pub struct LyapunovBarrierWeights {
    pub alpha_pollutant: f64,
    pub beta_ceiling: f64,
}

#[derive(Clone, Copy, Debug)]
pub struct LyapunovBarrierCorridors {
    pub pollutant_bands: CorridorBands,
    pub ceiling_bands: CorridorBands,
    /// Minimum allowed headroom h_c = c_max - c.
    pub min_headroom: f64,
}

#[derive(Clone, Debug)]
pub struct LyapunovBarrierState {
    pub pollutant_error: f64,
    pub agent_concentration: f64,
    pub agent_ceiling: f64,
}

fn barrier_phi(h: f64, min_headroom: f64) -> f64 {
    // Smoothed barrier: 1 / (h - min_headroom)^2 for h > min_headroom.
    if h <= min_headroom {
        f64::INFINITY
    } else {
        let d = h - min_headroom;
        1.0 / (d * d)
    }
}

fn normalize_coord(x: f64, bands: CorridorBands) -> f64 {
    if x <= bands.safe {
        0.0
    } else if x >= bands.hard {
        1.0
    } else {
        (x - bands.safe) / (bands.hard - bands.safe)
    }
}

pub fn compute_residual(
    state: &LyapunovBarrierState,
    weights: &LyapunovBarrierWeights,
    corridors: &LyapunovBarrierCorridors,
) -> Residual {
    let h_c = state.agent_ceiling - state.agent_concentration;
    let v_local = weights.alpha_pollutant * state.pollutant_error.powi(2)
        + weights.beta_ceiling * barrier_phi(h_c, corridors.min_headroom);

    let r_poll = normalize_coord(state.pollutant_error.abs(), corridors.pollutant_bands);
    let r_ceiling = normalize_coord(h_c.max(0.0), corridors.ceiling_bands);

    let rc_poll = RiskCoord {
        r: r_poll,
        sigma: 0.0,
        bands: corridors.pollutant_bands,
    };

    let rc_ceiling = RiskCoord {
        r: r_ceiling,
        sigma: 0.0,
        bands: corridors.ceiling_bands,
    };

    Residual {
        vt: v_local,
        coords: vec![rc_poll, rc_ceiling],
    }
}

/// Simple safestep contract for the barrier: non-increasing V and intact headroom.
pub fn safestep_barrier(prev: &Residual, next: &Residual) -> bool {
    let any_hard = next.coords.iter().any(|c| c.r >= 1.0);
    if any_hard {
        return false;
    }
    next.vt <= prev.vt
}
