// File: crates/volatility-guard/src/checksafestep.rs
use crate::{EcoPlane, NonOffsettablePlane};

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum SafeStepVerdict {
    Allow,
    Restricted,
    Stop,
}

pub struct SafeStepContext<'a> {
    pub non_offsettable_planes: Vec<&'a dyn NonOffsettablePlane>,
    pub all_planes: Vec<&'a dyn EcoPlane>,
}

impl<'a> SafeStepContext<'a> {
    pub fn checksafestep(&self, current_verdict: SafeStepVerdict) -> SafeStepVerdict {
        if self
            .non_offsettable_planes
            .iter()
            .any(|plane| plane.hard_band_breach())
        {
            return SafeStepVerdict::Stop;
        }

        let uncompensated_worsening = self.non_offsettable_planes.iter().any(|plane| {
            self.all_planes.iter().any(|other| {
                plane.strictly_degrades(*other) && plane.never_compensated_by(*other)
            })
        });

        if uncompensated_worsening && current_verdict == SafeStepVerdict::Allow {
            SafeStepVerdict::Restricted
        } else {
            current_verdict
        }
    }
}
