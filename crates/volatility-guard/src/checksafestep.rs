// crates/volatility-guard/src/checksafestep.rs

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum SafeStepVerdict {
    Allow,
    Degrade,
    Stop,
}

pub struct SafeStepContext<'a> {
    // Non‑offsettable planes (carbon, biodiversity, neurorights, nanoswarm hazard).
    pub non_offsettable_planes: Vec<&'a dyn NonOffsettablePlane>,
    // All eco planes (including the ones above), if you need cross‑checks.
    pub all_planes: Vec<&'a dyn EcoPlane>,
}

impl<'a> SafeStepContext<'a> {
    pub fn checksafestep(&self, current_verdict: SafeStepVerdict) -> SafeStepVerdict {
        // 1. If any non‑offsettable plane is in hard breach, force Stop.
        let hard_breach = self
            .non_offsettable_planes
            .iter()
            .any(|p| p.hard_band_breach());

        if hard_breach {
            return SafeStepVerdict::Stop;
        }

        // 2. If any non‑offsettable plane is degrading relative to others,
        //    and cannot be compensated, force at least Degrade.
        let mut must_degrade = false;

        for p in &self.non_offsettable_planes {
            for q in &self.all_planes {
                if p.strictly_degrades(*q) && p.never_compensated_by(*q) {
                    must_degrade = true;
                    break;
                }
            }
            if must_degrade {
                break;
            }
        }

        if must_degrade && matches!(current_verdict, SafeStepVerdict::Allow) {
            return SafeStepVerdict::Degrade;
        }

        // 3. Otherwise, keep the original verdict.
        current_verdict
    }
}
