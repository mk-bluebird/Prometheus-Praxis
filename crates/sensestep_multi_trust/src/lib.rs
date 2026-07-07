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
