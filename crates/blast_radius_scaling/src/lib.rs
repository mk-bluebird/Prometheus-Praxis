// Filename: crates/blast_radius_scaling/src/lib.rs
// Destination: Prometheus-Praxis/crates/blast_radius_scaling/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024
// rust-version: 1.85

#![forbid(unsafe_code)]

use std::f64;

/// Hydraulics and geometry inputs for a blast-radius event.
/// Designed to align with cyboquatic drainage-decay and surcharge kernels.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BlastInput {
    pub rho: f64,
    pub g: f64,
    pub surcharge_head_m: f64,
    pub channel_width_m: f64,
    pub channel_depth_m: f64,
    pub channel_length_m: f64,
    pub velocity_mps: f64,
    pub fog_confinement_factor: f64,
}

/// Dimensionless similarity variables for energy and radius.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct SimilarityVariables {
    pub e_star: f64,
    pub r_star_overtop: f64,
    pub r_star_scour: f64,
}

/// Blast radius outputs, aligned with surcharge diagnostics.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct BlastOutputs {
    pub radius_overtop_m: f64,
    pub radius_scour_m: f64,
}

/// Reference scales for corridor-normalized similarity variables.
/// These should be configured per corridor and hex registry.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct CorridorScales {
    pub head_ref_m: f64,
    pub width_ref_m: f64,
    pub depth_ref_m: f64,
    pub velocity_ref_mps: f64,
}

/// Compute dimensionless energy and radius variables for a single event.
/// The similarity variables are:
/// - E* = alpha_H E_H* + alpha_v E_v* + alpha_FOG C*
/// - R* = R / L_c, with L_c = sqrt(W * D)
pub fn compute_similarity_variables(
    input: BlastInput,
    outputs: BlastOutputs,
    scales: CorridorScales,
    alpha_h: f64,
    alpha_v: f64,
    alpha_fog: f64,
) -> SimilarityVariables {
    let rho = if input.rho <= 0.0 { 1000.0 } else { input.rho };
    let g = if input.g <= 0.0 { 9.80665 } else { input.g };

    let h = if input.surcharge_head_m < 0.0 {
        0.0
    } else {
        input.surcharge_head_m
    };
    let w = if input.channel_width_m <= 0.0 {
        1.0
    } else {
        input.channel_width_m
    };
    let d = if input.channel_depth_m <= 0.0 {
        1.0
    } else {
        input.channel_depth_m
    };
    let v = if input.velocity_mps < 0.0 {
        0.0
    } else {
        input.velocity_mps
    };

    let head_ref = if scales.head_ref_m <= 0.0 {
        1.0
    } else {
        scales.head_ref_m
    };
    let width_ref = if scales.width_ref_m <= 0.0 {
        1.0
    } else {
        scales.width_ref_m
    };
    let depth_ref = if scales.depth_ref_m <= 0.0 {
        1.0
    } else {
        scales.depth_ref_m
    };
    let vel_ref = if scales.velocity_ref_mps <= 0.0 {
        1.0
    } else {
        scales.velocity_ref_mps
    };

    let e_h = rho * g * h * h * w;
    let e_h_ref = rho * g * head_ref * head_ref * width_ref;
    let e_h_star = if e_h_ref > 0.0 { e_h / e_h_ref } else { 0.0 };

    let area = w * d;
    let e_v = 0.5 * rho * v * v * area;
    let e_v_ref = 0.5 * rho * vel_ref * vel_ref * width_ref * depth_ref;
    let e_v_star = if e_v_ref > 0.0 { e_v / e_v_ref } else { 0.0 };

    let c_star = input.fog_confinement_factor;

    let e_star = alpha_h * e_h_star + alpha_v * e_v_star + alpha_fog * c_star;

    let l_c = (w * d).sqrt();
    let l_c_safe = if l_c <= 0.0 { 1.0 } else { l_c };

    let r_star_overtop = if outputs.radius_overtop_m >= 0.0 {
        outputs.radius_overtop_m / l_c_safe
    } else {
        0.0
    };

    let r_star_scour = if outputs.radius_scour_m >= 0.0 {
        outputs.radius_scour_m / l_c_safe
    } else {
        0.0
    };

    SimilarityVariables {
        e_star,
        r_star_overtop,
        r_star_scour,
    }
}

/// Fit a simple power-law master curve R* = a * (E*)^beta
/// using least squares on log-log data.
/// Returns (a, beta).
pub fn fit_power_law_master_curve(samples: &[SimilarityVariables]) -> Option<(f64, f64)> {
    if samples.is_empty() {
        return None;
    }

    let mut xs = Vec::new();
    let mut ys = Vec::new();

    for s in samples.iter() {
        if s.e_star <= 0.0 {
            continue;
        }
        let r_comb = s.r_star_overtop.max(s.r_star_scour);
        if r_comb <= 0.0 {
            continue;
        }
        xs.push(s.e_star.ln());
        ys.push(r_comb.ln());
    }

    let n = xs.len();
    if n < 2 {
        return None;
    }

    let mut sum_x = 0.0;
    let mut sum_y = 0.0;
    let mut sum_xx = 0.0;
    let mut sum_xy = 0.0;

    for i in 0..n {
        let x = xs[i];
        let y = ys[i];
        sum_x += x;
        sum_y += y;
        sum_xx += x * x;
        sum_xy += x * y;
    }

    let n_f = n as f64;
    let denom = n_f * sum_xx - sum_x * sum_x;
    if denom.abs() < f64::EPSILON {
        return None;
    }

    let beta = (n_f * sum_xy - sum_x * sum_y) / denom;
    let intercept = (sum_y - beta * sum_x) / n_f;
    let a = intercept.exp();

    Some((a, beta))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_similarity_variables_basic() {
        let input = BlastInput {
            rho: 1000.0,
            g: 9.80665,
            surcharge_head_m: 1.0,
            channel_width_m: 2.0,
            channel_depth_m: 1.0,
            channel_length_m: 50.0,
            velocity_mps: 1.5,
            fog_confinement_factor: 0.3,
        };

        let outputs = BlastOutputs {
            radius_overtop_m: 5.0,
            radius_scour_m: 8.0,
        };

        let scales = CorridorScales {
            head_ref_m: 1.0,
            width_ref_m: 2.0,
            depth_ref_m: 1.0,
            velocity_ref_mps: 1.5,
        };

        let vars = compute_similarity_variables(input, outputs, scales, 0.5, 0.4, 0.1);
        assert!(vars.e_star > 0.0);
        assert!(vars.r_star_overtop > 0.0);
        assert!(vars.r_star_scour > 0.0);
    }

    #[test]
    fn test_power_law_fit() {
        let samples = vec![
            SimilarityVariables {
                e_star: 0.5,
                r_star_overtop: 0.4,
                r_star_scour: 0.5,
            },
            SimilarityVariables {
                e_star: 1.0,
                r_star_overtop: 0.7,
                r_star_scour: 0.8,
            },
            SimilarityVariables {
                e_star: 2.0,
                r_star_overtop: 1.1,
                r_star_scour: 1.3,
            },
        ];

        let fit = fit_power_law_master_curve(&samples).unwrap();
        assert!(fit.0 > 0.0);
        assert!(fit.1 > 0.0);
    }
}
