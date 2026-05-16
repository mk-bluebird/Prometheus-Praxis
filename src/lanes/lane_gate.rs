use super::{LaneKind, LaneStatus};

#[derive(Debug)]
pub enum LaneViolation {
    ResidualSlopePositive { b_slope: f64 },
    KBandViolation,
    EBandViolation,
    RBandViolation,
}

pub struct LaneGate {
    pub allow_research_to_expprod: bool,
    pub allow_expprod_to_prod: bool,
}

impl LaneGate {
    pub fn new() -> Self {
        Self {
            allow_research_to_expprod: true,
            allow_expprod_to_prod: true,
        }
    }

    pub fn check(&self, status: &LaneStatus) -> Result<(), Vec<LaneViolation>> {
        let mut violations = Vec::new();

        if status.b_slope > 0.0 {
            violations.push(LaneViolation::ResidualSlopePositive {
                b_slope: status.b_slope,
            });
        }

        if !status.k_band_ok {
            violations.push(LaneViolation::KBandViolation);
        }
        if !status.e_band_ok {
            violations.push(LaneViolation::EBandViolation);
        }
        if !status.r_band_ok {
            violations.push(LaneViolation::RBandViolation);
        }

        if violations.is_empty() {
            Ok(())
        } else {
            Err(violations)
        }
    }

    pub fn can_promote(&self, from: &LaneKind, to: &LaneKind, status: &LaneStatus) -> Result<(), Vec<LaneViolation>> {
        match (from, to) {
            (LaneKind::Research, LaneKind::ExpProd) if self.allow_research_to_expprod => self.check(status),
            (LaneKind::ExpProd, LaneKind::Prod) if self.allow_expprod_to_prod => self.check(status),
            _ => Err(vec![]), // disallowed transition
        }
    }
}
