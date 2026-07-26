// filename: crates/plane-weights/src/lib.rs
// destination: Prometheus-Praxis/crates/plane-weights/src/lib.rs

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

/// Plane weights library entry point, useful for quick crate sanity checks.
pub fn hello() -> &'static str {
    "plane_weights"
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hello() {
        assert_eq!(hello(), "plane_weights");
    }

    #[test]
    fn snapshot_is_stable() {
        let snap1 = crate::compute_canonical_snapshot_hash();
        let snap2 = crate::compute_canonical_snapshot_hash();
        assert_eq!(snap1.snapshothash, snap2.snapshothash);
    }

    #[test]
    fn nonoffsettable_violation_detects_out_of_band() {
        let weights = crate::canonical_plane_weights();
        let v = crate::nonoffsettable_violation(&weights, "carbon", 0.5);
        assert!(v);
    }

    #[test]
    fn nonoffsettable_violation_ignores_offsettable_plane() {
        let weights = crate::canonical_plane_weights();
        let v = crate::nonoffsettable_violation(&weights, "energy", 0.5);
        assert!(!v);
    }
}

/// Frozen plane weights and non-offsettable bands used by the ecosafety spine.
/// This crate is the single source of truth for plane-level weights in Prometheus-Praxis.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeight {
    pub planename: String,
    pub weight: f64,
    pub nonoffsettable: bool,
    pub corridormin: Option<f64>,
    pub corridormax: Option<f64>,
}

/// Snapshot hash used for governance integrity checks.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlaneWeightsSnapshot {
    pub snapshothash: String,
}

/// Canonical, in-code view of the current weights.
/// Must match the DB rows in plane_weights and the history table.
pub fn canonical_plane_weights() -> Vec<PlaneWeight> {
    vec![
        PlaneWeight {
            planename: "carbon".to_string(),
            weight: 0.30,
            nonoffsettable: true,
            corridormin: Some(-0.10),
            corridormax: Some(0.10),
        },
        PlaneWeight {
            planename: "biodiversity".to_string(),
            weight: 0.25,
            nonoffsettable: true,
            corridormin: Some(-0.05),
            corridormax: Some(0.05),
        },
        PlaneWeight {
            planename: "restoration".to_string(),
            weight: 0.20,
            nonoffsettable: false,
            corridormin: Some(-0.20),
            corridormax: Some(0.20),
        },
        PlaneWeight {
            planename: "energy".to_string(),
            weight: 0.10,
            nonoffsettable: false,
            corridormin: Some(-0.30),
            corridormax: Some(0.30),
        },
        PlaneWeight {
            planename: "hydrology".to_string(),
            weight: 0.10,
            nonoffsettable: false,
            corridormin: Some(-0.15),
            corridormax: Some(0.15),
        },
        PlaneWeight {
            planename: "topology".to_string(),
            weight: 0.05,
            nonoffsettable: false,
            corridormin: Some(-0.05),
            corridormax: Some(0.05),
        },
    ]
}

/// Compute a stable hash string over the canonical weights for governance comparison.
pub fn compute_canonical_snapshot_hash() -> PlaneWeightsSnapshot {
    use std::collections::hash_map::DefaultHasher;
    use std::hash::{Hash, Hasher};

    let weights = canonical_plane_weights();
    let mut s = DefaultHasher::new();
    for w in &weights {
        w.planename.hash(&mut s);
        let scaled = (w.weight * 1_000_000.0).round() as i64;
        scaled.hash(&mut s);
        w.nonoffsettable.hash(&mut s);

        if let Some(min) = w.corridormin {
            let scaled_min = (min * 1_000_000.0).round() as i64;
            scaled_min.hash(&mut s);
        } else {
            0_i64.hash(&mut s);
        }

        if let Some(max) = w.corridormax {
            let scaled_max = (max * 1_000_000.0).round() as i64;
            scaled_max.hash(&mut s);
        } else {
            0_i64.hash(&mut s);
        }
    }

    let hash_value = s.finish();
    let hex = format!("{:016x}", hash_value);

    PlaneWeightsSnapshot { snapshothash: hex }
}

/// Check if a plane with nonoffsettable = true is outside its corridor.
pub fn nonoffsettable_violation(weights: &[PlaneWeight], plane_name: &str, residual: f64) -> bool {
    if let Some(pw) = weights.iter().find(|w| w.planename == plane_name) {
        if pw.nonoffsettable {
            if let Some(min) = pw.corridormin {
                if residual < min {
                    return true;
                }
            }
            if let Some(max) = pw.corridormax {
                if residual > max {
                    return true;
                }
            }
        }
    }
    false
}
