#![forbid(unsafe_code)]

use kani::any;
use crate::eco_impact_for_source;

#[kani::proof]
fn eco_score_in_unit_interval() {
    let kind: String = any();
    let score = eco_impact_for_source(&kind);
    assert!(score >= 0.0);
    assert!(score <= 1.0);
}
