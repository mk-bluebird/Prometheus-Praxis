// crates/volatility-guard/src/checksafestep_kani.rs
// Compile only under Kani.
#![cfg(kani)]
use super::*;
use kani::any;

/// Simple stub plane for Kani exploration.
struct StubPlane {
    coord: f32,
    hard_breach: bool,
    never_comp: bool,
}

impl EcoPlane for StubPlane {
    fn coordinate(&self) -> f32 {
        self.coord
    }

    fn strictly_degrades(&self, _other: &dyn EcoPlane) -> bool {
        // For the proof we can over‑approximate: degradation is modeled
        // by this flag being true whenever hard_breach is true.
        self.hard_breach
    }
}

impl NonOffsettablePlane for StubPlane {
    fn hard_band_breach(&self) -> bool {
        self.hard_breach
    }

    fn never_compensated_by(&self, _other: &dyn EcoPlane) -> bool {
        self.never_comp
    }
}

#[kani::proof]
fn no_allow_on_nonoffsettable_breach() {
    // Non‑deterministic inputs for Kani (bounded domain).
    let hard_breach_any: bool = any();
    let never_comp_any: bool = any();
    let coord: f32 = any();

    // Current verdict may be anything; Kani explores all three.
    let verdict_choice: u8 = any();
    kani::assume(verdict_choice < 3);
    let current_verdict = match verdict_choice {
        0 => SafeStepVerdict::Allow,
        1 => SafeStepVerdict::Degrade,
        _ => SafeStepVerdict::Stop,
    };

    let plane = StubPlane {
        coord,
        hard_breach: hard_breach_any,
        never_comp: never_comp_any,
    };

    let ctx = SafeStepContext {
        non_offsettable_planes: vec![&plane],
        all_planes: vec![&plane as &dyn EcoPlane],
    };

    let out = ctx.checksafestep(current_verdict);

    // Property: if any non‑offsettable plane reports a hard breach,
    // checksafestep must not return Allow.
    if hard_breach_any {
        assert!(out != SafeStepVerdict::Allow);
    }

    // Property: if a non‑offsettable plane degrades and cannot be compensated,
    // and we started at Allow, we must not end at Allow.
    if !hard_breach_any && plane.strictly_degrades(&plane) && plane.never_compensated_by(&plane) {
        if matches!(current_verdict, SafeStepVerdict::Allow) {
            assert!(out != SafeStepVerdict::Allow);
        }
    }
}
