// File: crates/volatility-guard/src/checksafestep_kani.rs
#![cfg(kani)]

use crate::{EcoPlane, NonOffsettablePlane, SafeStepContext, SafeStepVerdict};
use kani::any;

struct StubPlane {
    coordinate: f64,
    hard_breach: bool,
    uncompensated_worsening: bool,
}

impl EcoPlane for StubPlane {
    fn coordinate(&self) -> f64 {
        self.coordinate
    }

    fn strictly_degrades(&self, _: &dyn EcoPlane) -> bool {
        self.uncompensated_worsening
    }
}

impl NonOffsettablePlane for StubPlane {
    fn hard_band_breach(&self) -> bool {
        self.hard_breach
    }

    fn never_compensated_by(&self, _: &dyn EcoPlane) -> bool {
        self.uncompensated_worsening
    }
}

#[kani::proof]
fn hard_breach_never_allows_step() {
    let hard_breach: bool = any();
    let worsening: bool = any();
    let current = match any::<u8>() % 3 {
        0 => SafeStepVerdict::Allow,
        1 => SafeStepVerdict::Restricted,
        _ => SafeStepVerdict::Stop,
    };
    let plane = StubPlane {
        coordinate: 0.5,
        hard_breach,
        uncompensated_worsening: worsening,
    };
    let context = SafeStepContext {
        non_offsettable_planes: vec![&plane],
        all_planes: vec![&plane],
    };
    let result = context.checksafestep(current);

    if hard_breach {
        assert_eq!(result, SafeStepVerdict::Stop);
    }
    if !hard_breach && worsening && current == SafeStepVerdict::Allow {
        assert_eq!(result, SafeStepVerdict::Restricted);
    }
}
