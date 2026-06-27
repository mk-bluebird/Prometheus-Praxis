// filename: spine/ker_residual/src/lib.rs
// destination: github.com/mk-bluebird/eco_restoration_shard

#![forbid(unsafe_code)]

/// Scalar type for Lyapunov math; align with SQLite REAL and existing spine.
pub type Scalar = f64;

/// Risk coordinate for a single plane, r_j ∈ [0,1].
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct RiskCoord {
    pub value: Scalar,
}

/// Plane-level weight and flags from planeweightsplane.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PlaneWeight {
    pub weight: Scalar,
    pub non_offsettable: bool,
    /// Gold-band ceiling for this plane (corridor gold band).
    pub gold_ceiling: Scalar,
}

/// Bundle of the three planes you asked about plus their weights.
#[derive(Clone, Debug, PartialEq)]
pub struct MultiPlaneResidualInput {
    pub r_topology: RiskCoord,
    pub r_bio: RiskCoord,
    pub r_thermal: RiskCoord,
    pub w_topology: PlaneWeight,
    pub w_bio: PlaneWeight,
    pub w_thermal: PlaneWeight,
}

/// Single Lyapunov scalar plus per-plane contributions.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct MultiPlaneResidual {
    pub v_total: Scalar,
    pub v_topology: Scalar,
    pub v_bio: Scalar,
    pub v_thermal: Scalar,
}

/// Verdict for a safestep check between two time steps.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum SafestepVerdict {
    Accepted,
    /// Non-offsettable plane worsened beyond gold band.
    NonOffsettableViolation {
        plane: NonOffsettablePlane,
        r_prev: Scalar,
        r_next: Scalar,
        gold_ceiling: Scalar,
    },
    /// Generic Lyapunov violation V_{t+1} > V_t + ε.
    LyapunovViolation {
        v_prev: Scalar,
        v_next: Scalar,
        epsilon_v: Scalar,
    },
}

/// Identifier for which non-offsettable plane failed.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum NonOffsettablePlane {
    Topology,
    Bio,
    Thermal,
}

/// Global Lyapunov guard configuration, mirroring SafestepConfig.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct SafestepConfig {
    /// Allowed numerical slack for V_{t+1} ≤ V_t + ε.
    pub epsilon_v: Scalar,
}

/// Compute V_t = Σ w_j r_j^2 for the three planes.
pub fn compute_multi_plane_residual(input: &MultiPlaneResidualInput) -> MultiPlaneResidual {
    let v_topology = input.w_topology.weight * input.r_topology.value * input.r_topology.value;
    let v_bio = input.w_bio.weight * input.r_bio.value * input.r_bio.value;
    let v_thermal = input.w_thermal.weight * input.r_thermal.value * input.r_thermal.value;

    MultiPlaneResidual {
        v_total: v_topology + v_bio + v_thermal,
        v_topology,
        v_bio,
        v_thermal,
    }
}

/// Safestep check for two consecutive states, enforcing:
/// 1. V_{t+1} ≤ V_t + ε (Lyapunov descent with slack)
/// 2. Any non-offsettable plane may not worsen beyond its gold band.
pub fn check_safestep_multi_plane(
    prev: &MultiPlaneResidualInput,
    next: &MultiPlaneResidualInput,
    cfg: SafestepConfig,
) -> SafestepVerdict {
    let v_prev = compute_multi_plane_residual(prev).v_total;
    let v_next = compute_multi_plane_residual(next).v_total;

    if v_next > v_prev + cfg.epsilon_v {
        return SafestepVerdict::LyapunovViolation {
            v_prev,
            v_next,
            epsilon_v: cfg.epsilon_v,
        };
    }

    // Topology plane
    if prev.w_topology.non_offsettable {
        let r_prev = prev.r_topology.value;
        let r_next = next.r_topology.value;
        if r_next > r_prev && r_next > prev.w_topology.gold_ceiling {
            return SafestepVerdict::NonOffsettableViolation {
                plane: NonOffsettablePlane::Topology,
                r_prev,
                r_next,
                gold_ceiling: prev.w_topology.gold_ceiling,
            };
        }
    }

    // Biodiversity plane
    if prev.w_bio.non_offsettable {
        let r_prev = prev.r_bio.value;
        let r_next = next.r_bio.value;
        if r_next > r_prev && r_next > prev.w_bio.gold_ceiling {
            return SafestepVerdict::NonOffsettableViolation {
                plane: NonOffsettablePlane::Bio,
                r_prev,
                r_next,
                gold_ceiling: prev.w_bio.gold_ceiling,
            };
        }
    }

    // Thermal plane (can be configured non-offsettable if desired).
    if prev.w_thermal.non_offsettable {
        let r_prev = prev.r_thermal.value;
        let r_next = next.r_thermal.value;
        if r_next > r_prev && r_next > prev.w_thermal.gold_ceiling {
            return SafestepVerdict::NonOffsettableViolation {
                plane: NonOffsettablePlane::Thermal,
                r_prev,
                r_next,
                gold_ceiling: prev.w_thermal.gold_ceiling,
            };
        }
    }

    SafestepVerdict::Accepted
}

#[cfg(test)]
mod tests {
    use super::*;

    fn w(weight: Scalar, non_offsettable: bool, gold_ceiling: Scalar) -> PlaneWeight {
        PlaneWeight {
            weight,
            non_offsettable,
            gold_ceiling,
        }
    }

    fn r(v: Scalar) -> RiskCoord {
        RiskCoord { value: v }
    }

    #[test]
    fn residual_sums_three_planes() {
        let input = MultiPlaneResidualInput {
            r_topology: r(0.3),
            r_bio: r(0.4),
            r_thermal: r(0.5),
            w_topology: w(1.0, true, 0.2),
            w_bio: w(2.0, true, 0.2),
            w_thermal: w(0.5, false, 0.8),
        };
        let res = compute_multi_plane_residual(&input);
        let expected_v = 1.0 * 0.3 * 0.3 + 2.0 * 0.4 * 0.4 + 0.5 * 0.5 * 0.5;
        assert!((res.v_total - expected_v).abs() < 1e-12);
    }

    #[test]
    fn safestep_rejects_lyapunov_increase() {
        let prev = MultiPlaneResidualInput {
            r_topology: r(0.1),
            r_bio: r(0.1),
            r_thermal: r(0.1),
            w_topology: w(1.0, true, 0.2),
            w_bio: w(1.0, true, 0.2),
            w_thermal: w(1.0, false, 0.8),
        };
        let next = MultiPlaneResidualInput {
            r_topology: r(0.5),
            r_bio: r(0.5),
            r_thermal: r(0.5),
            ..prev.clone()
        };
        let cfg = SafestepConfig { epsilon_v: 1e-6 };
        let verdict = check_safestep_multi_plane(&prev, &next, cfg);
        match verdict {
            SafestepVerdict::LyapunovViolation { .. } => {}
            _ => panic!("expected LyapunovViolation"),
        }
    }

    #[test]
    fn safestep_rejects_non_offsettable_bio_breach() {
        let prev = MultiPlaneResidualInput {
            r_topology: r(0.1),
            r_bio: r(0.15),
            r_thermal: r(0.2),
            w_topology: w(1.0, true, 0.2),
            w_bio: w(1.0, true, 0.2),
            w_thermal: w(1.0, false, 0.8),
        };
        let next = MultiPlaneResidualInput {
            r_bio: r(0.25),
            ..prev.clone()
        };
        let cfg = SafestepConfig { epsilon_v: 1e-6 };
        let verdict = check_safestep_multi_plane(&prev, &next, cfg);
        match verdict {
            SafestepVerdict::NonOffsettableViolation { plane, .. } => {
                assert_eq!(plane, NonOffsettablePlane::Bio);
            }
            _ => panic!("expected NonOffsettableViolation for Bio"),
        }
    }

    #[test]
    fn safestep_accepts_improvement_under_gold() {
        let prev = MultiPlaneResidualInput {
            r_topology: r(0.3),
            r_bio: r(0.3),
            r_thermal: r(0.3),
            w_topology: w(1.0, true, 0.4),
            w_bio: w(1.0, true, 0.4),
            w_thermal: w(1.0, false, 0.8),
        };
        let next = MultiPlaneResidualInput {
            r_topology: r(0.2),
            r_bio: r(0.2),
            r_thermal: r(0.2),
            ..prev.clone()
        };
        let cfg = SafestepConfig { epsilon_v: 1e-6 };
        let verdict = check_safestep_multi_plane(&prev, &next, cfg);
        assert_eq!(verdict, SafestepVerdict::Accepted);
    }
}
