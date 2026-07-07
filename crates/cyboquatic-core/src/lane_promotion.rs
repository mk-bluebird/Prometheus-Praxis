// filename: crates/cyboquatic-core/src/lane_promotion.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::Scalar;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Lane {
    Exp,
    ExpProd,
    Prod,
}

#[derive(Debug, Clone)]
pub struct KerSnapshot {
    pub k: Scalar,
    pub e: Scalar,
    pub r: Scalar,
    pub vt: Scalar,
    pub lane: Lane,
}

#[derive(Debug, Clone)]
pub struct LanePromotionSuggestion {
    pub from: Lane,
    pub to: Lane,
    pub reason: String,
}

#[derive(Debug, Clone)]
pub struct LanePromotionRecommender {
    pub k_min_exp_to_expprod: Scalar,
    pub e_min_exp_to_expprod: Scalar,
    pub r_max_exp_to_expprod: Scalar,
    pub vt_nonexpansion_margin: Scalar,
    pub k_min_expprod_to_prod: Scalar,
    pub e_min_expprod_to_prod: Scalar,
    pub r_max_expprod_to_prod: Scalar,
}

impl Default for LanePromotionRecommender {
    fn default() -> Self {
        LanePromotionRecommender {
            k_min_exp_to_expprod: 0.90,
            e_min_exp_to_expprod: 0.90,
            r_max_exp_to_expprod: 0.20,
            vt_nonexpansion_margin: 0.01,
            k_min_expprod_to_prod: 0.93,
            e_min_expprod_to_prod: 0.90,
            r_max_expprod_to_prod: 0.13,
        }
    }
}

impl LanePromotionRecommender {
    pub fn recommend(&self, history: &[KerSnapshot]) -> Option<LanePromotionSuggestion> {
        if history.len() < 2 {
            return None;
        }

        let latest = history.last().unwrap();
        let prev = &history[history.len() - 2];

        let vt_nonexpanding = latest.vt <= prev.vt + self.vt_nonexpansion_margin;

        match latest.lane {
            Lane::Exp => {
                if latest.k >= self.k_min_exp_to_expprod
                    && latest.e >= self.e_min_exp_to_expprod
                    && latest.r <= self.r_max_exp_to_expprod
                    && vt_nonexpanding
                {
                    Some(LanePromotionSuggestion {
                        from: Lane::Exp,
                        to: Lane::ExpProd,
                        reason: format!(
                            "K/E/R and Vt trend meet EXP→EXPPROD advisory thresholds (K={:.3}, E={:.3}, R={:.3}, Vt={:.4}≤{:.4})",
                            latest.k, latest.e, latest.r, latest.vt, prev.vt + self.vt_nonexpansion_margin
                        ),
                    })
                } else {
                    None
                }
            }
            Lane::ExpProd => {
                if latest.k >= self.k_min_expprod_to_prod
                    && latest.e >= self.e_min_expprod_to_prod
                    && latest.r <= self.r_max_expprod_to_prod
                    && vt_nonexpanding
                {
                    Some(LanePromotionSuggestion {
                        from: Lane::ExpProd,
                        to: Lane::Prod,
                        reason: format!(
                            "K/E/R and Vt trend meet EXPPROD→PROD advisory thresholds (K={:.3}, E={:.3}, R={:.3}, Vt={:.4}≤{:.4})",
                            latest.k, latest.e, latest.r, latest.vt, prev.vt + self.vt_nonexpansion_margin
                        ),
                    })
                } else {
                    None
                }
            }
            Lane::Prod => None,
        }
    }
}
