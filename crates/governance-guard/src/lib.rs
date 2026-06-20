// filename: crates/governance-guard/src/lib.rs
// repo: mk-bluebird/eco_restoration_shard
// destination: Eco-Fort/crates/governance-guard/src/lib.rs

#![forbid(unsafe_code)]
#![warn(missing_docs)]

//! Governance guard functions for KER upgrades, lane admissibility,
//! and MT6883 RoH ceilings. This crate is non-actuating and is intended
//! to be used by CI, schedulers, and AI tools in read-only contexts.

use serde::{Deserialize, Serialize};

/// KER state before and after an upgrade or promotion.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerUpgradeState {
    /// Old K value.
    pub k_old: f64,
    /// New K value.
    pub k_new: f64,
    /// Old E value.
    pub e_old: f64,
    /// New E value.
    pub e_new: f64,
    /// Old R value.
    pub r_old: f64,
    /// New R value.
    pub r_new: f64,
    /// Old Lyapunov residual V_t.
    pub vt_old: f64,
    /// New Lyapunov residual V_{t+1}.
    pub vt_new: f64,
    /// Allowed Lyapunov slack.
    pub vt_epsilon: f64,
}

/// Result of a KER upgrade check.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KerUpgradeVerdict {
    /// True if monotonicity holds for K, E, and R.
    pub monotone_ok: bool,
    /// True if Lyapunov constraint holds.
    pub lyapunov_ok: bool,
    /// Human-readable explanation.
    pub reason: String,
}

/// Check KER monotonicity and Lyapunov residual constraints.
pub fn ker_upgrade_ok(state: &KerUpgradeState) -> KerUpgradeVerdict {
    let mut monotone_ok = true;
    let mut lyapunov_ok = true;
    let mut reason_parts: Vec<String> = Vec::new();

    if state.k_new < state.k_old {
        monotone_ok = false;
        reason_parts.push("K_new < K_old".to_string());
    }
    if state.e_new < state.e_old {
        monotone_ok = false;
        reason_parts.push("E_new < E_old".to_string());
    }
    if state.r_new > state.r_old {
        monotone_ok = false;
        reason_parts.push("R_new > R_old".to_string());
    }

    if state.vt_new > state.vt_old + state.vt_epsilon {
        lyapunov_ok = false;
        reason_parts.push("V_new > V_old + epsilon".to_string());
    }

    if reason_parts.is_empty() {
        reason_parts.push("KER upgrade monotone and Lyapunov-safe".to_string());
    }

    KerUpgradeVerdict {
        monotone_ok,
        lyapunov_ok,
        reason: reason_parts.join("; "),
    }
}

/// Lane admissibility inputs combining KER, RoH, and Cyboquatic flags.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneAdmissibilityInput {
    /// Lane label, e.g. "PROD" or "EXPPROD".
    pub lane: String,
    /// K value in 0..1.
    pub k: f64,
    /// E value in 0..1.
    pub e: f64,
    /// R value in 0..1.
    pub r: f64,
    /// RoH value in 0..1.
    pub roh: f64,
    /// RoH ceiling for this corridor.
    pub roh_ceiling: f64,
    /// True if Lyapunov check passed for this shard.
    pub lyapunov_ok: bool,
    /// For Cyboquatic shards: 1 if carbon-negative corridor holds.
    pub carbonnegative_ok: bool,
    /// For Cyboquatic shards: 1 if restoration corridor holds.
    pub restoration_ok: bool,
}

/// Lane admissibility verdict.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneAdmissibilityVerdict {
    /// True if all predicates for this lane are satisfied.
    pub admissible: bool,
    /// True if K exceeds lane minimum.
    pub k_ok: bool,
    /// True if E exceeds lane minimum.
    pub e_ok: bool,
    /// True if R is below lane maximum.
    pub r_ok: bool,
    /// True if RoH respects the ceiling.
    pub roh_ok: bool,
    /// True if Cyboquatic preconditions hold when required.
    pub cyboquatic_ok: bool,
    /// Human-readable explanation.
    pub reason: String,
}

/// Lane-specific thresholds for K, E, R, and RoH.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LaneThresholds {
    /// Lane name.
    pub lane: String,
    /// Minimum K required.
    pub min_k: f64,
    /// Minimum E required.
    pub min_e: f64,
    /// Maximum R allowed.
    pub max_r: f64,
    /// Maximum RoH allowed (usually <= 0.30 for healthcare).
    pub max_roh: f64,
}

/// Check whether a lane is admissible under the current thresholds.
pub fn lane_admissible(input: &LaneAdmissibilityInput, thresholds: &LaneThresholds) -> LaneAdmissibilityVerdict {
    let mut admissible = true;
    let mut reason_parts: Vec<String> = Vec::new();

    let k_ok = input.k >= thresholds.min_k;
    if !k_ok {
        admissible = false;
        reason_parts.push("K below minimum".to_string());
    }

    let e_ok = input.e >= thresholds.min_e;
    if !e_ok {
        admissible = false;
        reason_parts.push("E below minimum".to_string());
    }

    let r_ok = input.r <= thresholds.max_r;
    if !r_ok {
        admissible = false;
        reason_parts.push("R above maximum".to_string());
    }

    let roh_ok = input.roh <= thresholds.max_roh && input.roh <= input.roh_ceiling;
    if !roh_ok {
        admissible = false;
        reason_parts.push("RoH exceeds ceiling".to_string());
    }

    if !input.lyapunov_ok {
        admissible = false;
        reason_parts.push("Lyapunov violation".to_string());
    }

    let mut cyboquatic_ok = true;
    if input.lane == "EXPPROD" || input.lane == "PROD" {
        if !input.carbonnegative_ok {
            cyboquatic_ok = false;
            admissible = false;
            reason_parts.push("carbonnegativeok = 0 for PROD/EXPPROD".to_string());
        }
        if !input.restoration_ok {
            cyboquatic_ok = false;
            admissible = false;
            reason_parts.push("restorationok = 0 for PROD/EXPPROD".to_string());
        }
    }

    if reason_parts.is_empty() {
        reason_parts.push("Lane admissible".to_string());
    }

    LaneAdmissibilityVerdict {
        admissible,
        k_ok,
        e_ok,
        r_ok,
        roh_ok,
        cyboquatic_ok,
        reason: reason_parts.join("; "),
    }
}

/// MT6883-specific RoH check using a hard ceiling (typically 0.30).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoHCheckInput {
    /// Observed RoH value.
    pub roh: f64,
    /// Corridor-wide RoH ceiling (e.g. 0.30).
    pub roh_ceiling: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoHCheckVerdict {
    /// True if roh <= roh_ceiling.
    pub roh_ok: bool,
    /// Human-readable explanation.
    pub reason: String,
}

/// Check RoH against a ceiling.
pub fn roh_ceiling_ok(input: &RoHCheckInput) -> RoHCheckVerdict {
    if input.roh <= input.roh_ceiling {
        RoHCheckVerdict {
            roh_ok: true,
            reason: "RoH within ceiling".to_string(),
        }
    } else {
        RoHCheckVerdict {
            roh_ok: false,
            reason: "RoH exceeds ceiling".to_string(),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ker_upgrade_accepts_monotone_safe() {
        let state = KerUpgradeState {
            k_old: 0.9,
            k_new: 0.95,
            e_old: 0.9,
            e_new: 0.93,
            r_old: 0.12,
            r_new: 0.10,
            vt_old: 0.5,
            vt_new: 0.5,
            vt_epsilon: 1e-6,
        };
        let verdict = ker_upgrade_ok(&state);
        assert!(verdict.monotone_ok);
        assert!(verdict.lyapunov_ok);
    }

    #[test]
    fn roh_ceiling_rejects_violation() {
        let input = RoHCheckInput {
            roh: 0.31,
            roh_ceiling: 0.30,
        };
        let verdict = roh_ceiling_ok(&input);
        assert!(!verdict.roh_ok);
    }

    #[test]
    fn lane_admissible_rejects_bad_cyboquatic() {
        let thresholds = LaneThresholds {
            lane: "PROD".to_string(),
            min_k: 0.9,
            min_e: 0.9,
            max_r: 0.13,
            max_roh: 0.30,
        };
        let input = LaneAdmissibilityInput {
            lane: "PROD".to_string(),
            k: 0.92,
            e: 0.91,
            r: 0.12,
            roh: 0.29,
            roh_ceiling: 0.30,
            lyapunov_ok: true,
            carbonnegative_ok: false,
            restoration_ok: true,
        };
        let verdict = lane_admissible(&input, &thresholds);
        assert!(!verdict.admissible);
        assert!(!verdict.cyboquatic_ok);
    }
}
