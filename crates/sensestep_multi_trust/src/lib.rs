// Filename: crates/sensestep_multi_trust/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct WindowStats {
    pub mean: f64,
    pub variance: f64,
    pub drift_rate: f64,
    pub missing_rate: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ChannelStats {
    pub flow: WindowStats,
    pub pfas: WindowStats,
    pub ecoli: WindowStats,
    pub temp: WindowStats,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct ChannelWeights {
    pub w_flow: f64,
    pub w_pfas: f64,
    pub w_ecoli: f64,
    pub w_temp: f64,
}

impl ChannelWeights {
    pub fn normalized(self) -> Self {
        let sum = self.w_flow + self.w_pfas + self.w_ecoli + self.w_temp;
        if sum == 0.0 {
            Self {
                w_flow: 0.25,
                w_pfas: 0.25,
                w_ecoli: 0.25,
                w_temp: 0.25,
            }
        } else {
            Self {
                w_flow: self.w_flow / sum,
                w_pfas: self.w_pfas / sum,
                w_ecoli: self.w_ecoli / sum,
                w_temp: self.w_temp / sum,
            }
        }
    }
}

fn displacement_scalar(
    stats: WindowStats,
    var_threshold: f64,
    drift_threshold: f64,
    miss_threshold: f64,
) -> f64 {
    let v = (stats.variance / var_threshold).min(1.0).max(0.0);
    let d = (stats.drift_rate.abs() / drift_threshold).min(1.0).max(0.0);
    let m = (stats.missing_rate / miss_threshold).min(1.0).max(0.0);

    let alpha_v = 0.4;
    let alpha_d = 0.4;
    let alpha_m = 0.2;

    let d_t = alpha_v * v + alpha_d * d + alpha_m * m;
    d_t.min(1.0).max(0.0)
}

pub fn channel_displacements(stats: &ChannelStats) -> (f64, f64, f64, f64) {
    let d_flow = displacement_scalar(stats.flow, 0.01, 0.001, 0.05);
    let d_pfas = displacement_scalar(stats.pfas, 0.0001, 0.00001, 0.02);
    let d_ecoli = displacement_scalar(stats.ecoli, 0.1, 0.01, 0.05);
    let d_temp = displacement_scalar(stats.temp, 0.5, 0.05, 0.1);

    (d_flow, d_pfas, d_ecoli, d_temp)
}

pub fn node_trust_displacement(
    stats: &ChannelStats,
    weights: ChannelWeights,
) -> f64 {
    let (d_flow, d_pfas, d_ecoli, d_temp) = channel_displacements(stats);
    let w = weights.normalized();

    let sum_sq =
        w.w_flow * d_flow * d_flow +
        w.w_pfas * d_pfas * d_pfas +
        w.w_ecoli * d_ecoli * d_ecoli +
        w.w_temp * d_temp * d_temp;

    sum_sq.sqrt().min(1.0).max(0.0)
}

pub fn badj_from_displacement(d_node: f64, beta: f64) -> f64 {
    1.0 + beta * d_node
}

pub fn compute_window_stats(samples: &[f64]) -> WindowStats {
    let n = samples.len() as f64;
    if n == 0.0 {
        return WindowStats {
            mean: 0.0,
            variance: 0.0,
            drift_rate: 0.0,
            missing_rate: 1.0,
        };
    }

    let mean = samples.iter().sum::<f64>() / n;
    let variance = samples
        .iter()
        .map(|x| {
            let dx = x - mean;
            dx * dx
        })
        .sum::<f64>()
        / n;

    let drift_rate = samples.last().unwrap() - samples.first().unwrap();

    let missing_count = samples.iter().filter(|x| !x.is_finite()).count() as f64;
    let missing_rate = missing_count / n;

    WindowStats {
        mean,
        variance,
        drift_rate,
        missing_rate,
    }
}
