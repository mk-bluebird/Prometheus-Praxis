/*  
filename: src/lanes/lane_trend_analyzer.rs
destination: eco_restoration_shard/src/lanes/lane_trend_analyzer.rs
belongs-to: lane-governance
task-code: T06_LANE_TREND_ANALYZER
lane: PROD
ker-targets: {K:0.95, E:0.91, R:0.13}
*/

use crate::kerresidual::ResidualEngine;

#[derive(Debug, Clone)]
pub enum LaneKind {
    Research,
    ExpProd,
    Prod,
}

#[derive(Debug, Clone)]
pub struct LaneSample {
    pub lane: LaneKind,
    pub timestamp: i64,
    pub k: f64,
    pub e: f64,
    pub r: f64,
    pub residual: f64,
}

#[derive(Debug, Clone)]
pub struct LaneStatus {
    pub lane: LaneKind,
    pub b_slope: f64,
    pub k_band_ok: bool,
    pub e_band_ok: bool,
    pub r_band_ok: bool,
}

pub struct LaneTrendAnalyzer<'a, R: ResidualEngine> {
    residual_engine: &'a R,
}

impl<'a, R: ResidualEngine> LaneTrendAnalyzer<'a, R> {
    pub fn new(residual_engine: &'a R) -> Self {
        Self { residual_engine }
    }

    pub fn analyze(&self, samples: &[LaneSample]) -> LaneStatus {
        // simple linear regression on residual over time → slope b
        let (sum_t, sum_r, sum_tr, sum_t2, n) = samples.iter().fold(
            (0.0, 0.0, 0.0, 0.0, 0.0),
            |(st, sr, str_, st2, n), s| {
                let t = s.timestamp as f64;
                (st + t, sr + s.residual, str_ + t * s.residual, st2 + t * t, n + 1.0)
            },
        );

        let denom = n * sum_t2 - sum_t * sum_t;
        let b = if denom.abs() < 1e-9 {
            0.0
        } else {
            (n * sum_tr - sum_t * sum_r) / denom
        };

        let lane = samples.last().map(|s| s.lane.clone()).unwrap_or(LaneKind::Research);

        let (k_band_ok, e_band_ok, r_band_ok) = match lane {
            LaneKind::Research => (true, true, true), // projections only
            LaneKind::ExpProd => (self.band_ok(samples, 0.90, 0.88, 0.10)),
            LaneKind::Prod => (self.band_ok(samples, 0.94, 0.90, 0.13)),
        };

        LaneStatus {
            lane,
            b_slope: b,
            k_band_ok,
            e_band_ok,
            r_band_ok,
        }
    }

    fn band_ok(&self, samples: &[LaneSample], k_min: f64, e_min: f64, r_min: f64) -> (bool, bool, bool) {
        let last = samples.last().unwrap();
        (last.k >= k_min, last.e >= e_min, last.r >= r_min)
    }
}
