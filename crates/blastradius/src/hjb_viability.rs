// Filename: crates/cpvm_viability/src/hjb_viability.rs
// Destination: mk-bluebird/eco_restoration_shard (mono-repo)

//! CPVM viability scalar V over a 3D state space (energy, hydraulic head,
//! contaminant concentration), solved via a grid-based HJB-like value
//! iteration. Uses nalgebra for state vectors and is designed to be
//! paired with kani for assertion-based verification of invariants.

use nalgebra::{Vector3};
use std::f64::INFINITY;

/// Continuous state x = (E, H, C) as a nalgebra vector.
pub type State = Vector3<f64>;

/// Continuous control u; for simplicity we treat this as a 3D adjustment
/// to (E, H, C). In the full stack this would be derived from actuator
/// commands with bounds and corridor-aware constraints.
pub type Control = Vector3<f64>;

/// Discrete grid index over the 3D state space.
#[derive(Clone, Copy, Debug)]
pub struct GridIndex {
    pub i_e: usize,
    pub i_h: usize,
    pub i_c: usize,
}

/// Viability grid storing V(x) values at discrete samples.
pub struct ViabilityGrid {
    pub n_e: usize,
    pub n_h: usize,
    pub n_c: usize,
    pub v_values: Vec<f64>, // length = n_e * n_h * n_c
    pub e_coords: Vec<f64>,
    pub h_coords: Vec<f64>,
    pub c_coords: Vec<f64>,
}

impl ViabilityGrid {
    pub fn new(n_e: usize, n_h: usize, n_c: usize,
               e_min: f64, e_max: f64,
               h_min: f64, h_max: f64,
               c_min: f64, c_max: f64) -> Self
    {
        let mut e_coords = Vec::with_capacity(n_e);
        let mut h_coords = Vec::with_capacity(n_h);
        let mut c_coords = Vec::with_capacity(n_c);

        for i in 0..n_e {
            let alpha = i as f64 / (n_e - 1) as f64;
            e_coords.push(e_min + alpha * (e_max - e_min));
        }
        for j in 0..n_h {
            let alpha = j as f64 / (n_h - 1) as f64;
            h_coords.push(h_min + alpha * (h_max - h_min));
        }
        for k in 0..n_c {
            let alpha = k as f64 / (n_c - 1) as f64;
            c_coords.push(c_min + alpha * (c_max - c_min));
        }

        let total = n_e * n_h * n_c;
        ViabilityGrid {
            n_e,
            n_h,
            n_c,
            v_values: vec![0.0; total],
            e_coords,
            h_coords,
            c_coords,
        }
    }

    fn idx(&self, gi: GridIndex) -> usize {
        gi.i_e * self.n_h * self.n_c + gi.i_h * self.n_c + gi.i_c
    }

    pub fn get_v(&self, gi: GridIndex) -> f64 {
        self.v_values[self.idx(gi)]
    }

    pub fn set_v(&mut self, gi: GridIndex, v: f64) {
        let idx = self.idx(gi);
        self.v_values[idx] = v;
    }

    pub fn state_at(&self, gi: GridIndex) -> State {
        let e = self.e_coords[gi.i_e];
        let h = self.h_coords[gi.i_h];
        let c = self.c_coords[gi.i_c];
        State::new(e, h, c)
    }
}

/// Corridor-safe set predicate over normalized risk coordinates.
/// r_E, r_H, r_C ∈ [0,1]; hard corridor is at 1.0.
fn in_safe_set(r_e: f64, r_h: f64, r_c: f64) -> bool {
    r_e <= 1.0 && r_h <= 1.0 && r_c <= 1.0
}

/// Example normalization functions from physical E, H, C to risk
/// coordinates r_E, r_H, r_C using min-max corridors. In practice these
/// must match your qpudatashard CorridorBands for each plane.
fn normalize_e(e: f64, e_safe_min: f64, e_hard_min: f64) -> f64 {
    if e <= e_hard_min {
        1.0
    } else if e >= e_safe_min {
        0.0
    } else {
        (e_safe_min - e) / (e_safe_min - e_hard_min)
    }
}

fn normalize_h(h: f64, h_safe_max: f64, h_hard_max: f64) -> f64 {
    if h >= h_hard_max {
        1.0
    } else if h <= h_safe_max {
        0.0
    } else {
        (h - h_safe_max) / (h_hard_max - h_safe_max)
    }
}

fn normalize_c(c: f64, c_safe_max: f64, c_hard_max: f64) -> f64 {
    if c >= c_hard_max {
        1.0
    } else if c <= c_safe_max {
        0.0
    } else {
        (c - c_safe_max) / (c_hard_max - c_safe_max)
    }
}

/// Dynamics f(x, u) for the CPVM node.
/// This is a placeholder affine model; in the full stack this must be
/// replaced with physics-consistent models for energy, hydraulics, and
/// contaminant transport.
fn dynamics(x: &State, u: &Control) -> State {
    // Simple linear drift plus control.
    // x_dot = A x + B u, here approximated as control-dominated.
    State::new(u[0], u[1], u[2])
}

/// Running cost L(x, u) used in the HJB update; penalizes proximity to
/// corridor edges and large controls.
fn running_cost(r_e: f64, r_h: f64, r_c: f64, u: &Control) -> f64 {
    let risk_penalty = r_e * r_e + r_h * r_h + r_c * r_c;
    let control_penalty = u.norm_squared();
    risk_penalty + control_penalty
}

/// Discrete-time HJB-style update at a grid point:
/// V_next(x) = min_u { L(x,u) + V(x + dt f(x,u)) }
/// subject to staying inside the safe set.
/// This corresponds to a viability-preserving control; in the discrete
/// CPVM logic you then enforce V_next <= V_current.
pub fn hjb_update_point(
    grid: &ViabilityGrid,
    gi: GridIndex,
    controls: &[Control],
    dt: f64,
    e_safe_min: f64,
    e_hard_min: f64,
    h_safe_max: f64,
    h_hard_max: f64,
    c_safe_max: f64,
    c_hard_max: f64,
) -> f64 {
    let x = grid.state_at(gi);

    // Normalize to risk coordinates.
    let r_e = normalize_e(x[0], e_safe_min, e_hard_min);
    let r_h = normalize_h(x[1], h_safe_max, h_hard_max);
    let r_c = normalize_c(x[2], c_safe_max, c_hard_max);

    if !in_safe_set(r_e, r_h, r_c) {
        // Outside safe set: large penalty.
        return INFINITY;
    }

    let mut best_v = INFINITY;

    for u in controls {
        let x_dot = dynamics(&x, u);
        let x_next = x + x_dot * dt;

        // Normalize next state risks and enforce corridor.
        let r_e_next = normalize_e(x_next[0], e_safe_min, e_hard_min);
        let r_h_next = normalize_h(x_next[1], h_safe_max, h_hard_max);
        let r_c_next = normalize_c(x_next[2], c_safe_max, c_hard_max);

        if !in_safe_set(r_e_next, r_h_next, r_c_next) {
            // Control not viable; skip.
            continue;
        }

        // Find nearest grid cell for x_next (simple nearest-neighbor).
        let i_e_next = nearest_index(&grid.e_coords, x_next[0]);
        let i_h_next = nearest_index(&grid.h_coords, x_next[1]);
        let i_c_next = nearest_index(&grid.c_coords, x_next[2]);

        let gi_next = GridIndex { i_e: i_e_next, i_h: i_h_next, i_c: i_c_next };
        let v_next = grid.get_v(gi_next);
        let cost = running_cost(r_e_next, r_h_next, r_c_next, u);

        let candidate = cost + v_next;
        if candidate < best_v {
            best_v = candidate;
        }
    }

    best_v
}

/// Helper to find nearest grid index to a coordinate.
fn nearest_index(coords: &[f64], x: f64) -> usize {
    let mut best_idx = 0;
    let mut best_dist = (coords[0] - x).abs();
    for (i, &c) in coords.iter().enumerate().skip(1) {
        let d = (c - x).abs();
        if d < best_dist {
            best_dist = d;
            best_idx = i;
        }
    }
    best_idx
}

/// Perform one global HJB iteration over the grid.
/// This can be called repeatedly until convergence.
/// In a CPVM-safe implementation, kani would be used to assert that
/// the resulting discrete-time policy respects V_next <= V_current
/// outside the safe interior.
pub fn hjb_value_iteration_step(
    grid: &mut ViabilityGrid,
    controls: &[Control],
    dt: f64,
    e_safe_min: f64,
    e_hard_min: f64,
    h_safe_max: f64,
    h_hard_max: f64,
    c_safe_max: f64,
    c_hard_max: f64,
) {
    let mut new_v = grid.v_values.clone();

    for i_e in 0..grid.n_e {
        for i_h in 0..grid.n_h {
            for i_c in 0..grid.n_c {
                let gi = GridIndex { i_e, i_h, i_c };
                let v_updated = hjb_update_point(
                    grid,
                    gi,
                    controls,
                    dt,
                    e_safe_min,
                    e_hard_min,
                    h_safe_max,
                    h_hard_max,
                    c_safe_max,
                    c_hard_max,
                );
                new_v[grid.idx(gi)] = v_updated;
            }
        }
    }

    grid.v_values = new_v;
}

/// Example discrete CPVM-safe step that can be subjected to kani proofs:
/// given current V(x) and a candidate control u, ensure that
/// - next state stays inside corridors, and
/// - V_next(x_next) <= V_current(x_current).
pub fn cpvm_safe_step(
    grid: &ViabilityGrid,
    gi: GridIndex,
    u: &Control,
    dt: f64,
    e_safe_min: f64,
    e_hard_min: f64,
    h_safe_max: f64,
    h_hard_max: f64,
    c_safe_max: f64,
    c_hard_max: f64,
) -> bool {
    let x = grid.state_at(gi);
    let v_current = grid.get_v(gi);

    let x_dot = dynamics(&x, u);
    let x_next = x + x_dot * dt;

    let r_e_next = normalize_e(x_next[0], e_safe_min, e_hard_min);
    let r_h_next = normalize_h(x_next[1], h_safe_max, h_hard_max);
    let r_c_next = normalize_c(x_next[2], c_safe_max, c_hard_max);

    if !in_safe_set(r_e_next, r_h_next, r_c_next) {
        return false;
    }

    let i_e_next = nearest_index(&grid.e_coords, x_next[0]);
    let i_h_next = nearest_index(&grid.h_coords, x_next[1]);
    let i_c_next = nearest_index(&grid.c_coords, x_next[2]);
    let gi_next = GridIndex { i_e: i_e_next, i_h: i_h_next, i_c: i_c_next };

    let v_next = grid.get_v(gi_next);

    // Discrete Lyapunov/viability condition: V_next <= V_current.
    v_next <= v_current
}
