// Filename: crates/ecosafety-nanoswarm-urban-core/src/lipschitz_aliasing.rs

use serde::{Deserialize, Serialize};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct SensorSample {
    pub node_id: String,
    pub x: f64,
    pub y: f64,
    pub z: f64,
    pub concentration: f64,
    pub timestamputc: i64,
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct LipschitzEstimates {
    pub spatial_l: f64,
    pub temporal_l: f64,
}

pub fn estimate_spatial_l(samples: &[SensorSample]) -> f64 {
    let mut l_max = 0.0;
    for (i, s1) in samples.iter().enumerate() {
        for s2 in samples.iter().skip(i + 1) {
            let dx = s1.x - s2.x;
            let dy = s1.y - s2.y;
            let dz = s1.z - s2.z;
            let dist = (dx * dx + dy * dy + dz * dz).sqrt();
            if dist > 0.0 {
                let diff = (s1.concentration - s2.concentration).abs();
                let l = diff / dist;
                if l > l_max {
                    l_max = l;
                }
            }
        }
    }
    l_max
}

pub fn estimate_temporal_l(samples: &[SensorSample]) -> f64 {
    let mut l_max = 0.0;
    // Group by node_id
    let mut by_node: std::collections::HashMap<String, Vec<&SensorSample>> =
        std::collections::HashMap::new();
    for s in samples {
        by_node.entry(s.node_id.clone()).or_default().push(s);
    }
    for (_id, mut seq) in by_node {
        seq.sort_by_key(|s| s.timestamputc);
        for w in seq.windows(2) {
            let s1 = w[0];
            let s2 = w[1];
            let dt = (s2.timestamputc - s1.timestamputc) as f64;
            if dt > 0.0 {
                let diff = (s2.concentration - s1.concentration).abs();
                let l = diff / dt;
                if l > l_max {
                    l_max = l;
                }
            }
        }
    }
    l_max
}

pub fn safe_dt(l_time: f64, delta_c_max: f64) -> f64 {
    if l_time <= 0.0 {
        return f64::INFINITY;
    }
    delta_c_max / l_time
}

pub fn safe_dx(l_spatial: f64, delta_c_max: f64) -> f64 {
    if l_spatial <= 0.0 {
        return f64::INFINITY;
    }
    delta_c_max / l_spatial
}
