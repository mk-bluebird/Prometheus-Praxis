#![allow(dead_code)]

use kani::proof;

use crate::{clamp01, fold_pollutant_to_r, risk_from_row, CorridorBands, PhoenixWaterRow};

#[proof]
fn clamp01_never_negative() {
    let x: f64 = kani::any();
    let y = clamp01(x);
    kani::assert!(y >= 0.0);
    kani::assert!(y <= 1.0);
}

#[proof]
fn fold_pollutant_r_in_unit_interval() {
    let x: f64 = kani::any();
    let bands = CorridorBands {
        safe_max: 5.0,
        hard_max: 50.0,
    };
    let r = fold_pollutant_to_r(x, bands);
    kani::assert!(r >= 0.0);
    kani::assert!(r <= 1.0);
}

#[proof]
fn risk_vector_non_negative_components() {
    // Arbitrary, but finite bands; Kani will explore x across ranges.
    let bod_bands = CorridorBands { safe_max: 5.0, hard_max: 50.0 };
    let tss_bands = CorridorBands { safe_max: 10.0, hard_max: 100.0 };
    let n_bands   = CorridorBands { safe_max: 1.0, hard_max: 10.0 };
    let p_bands   = CorridorBands { safe_max: 0.5, hard_max: 5.0 };
    let cec_bands = CorridorBands { safe_max: 50.0, hard_max: 500.0 };
    let pfas_bands= CorridorBands { safe_max: 10.0, hard_max: 100.0 };

    let bod: f64 = kani::any();
    let tss: f64 = kani::any();
    let n: f64   = kani::any();
    let p: f64   = kani::any();
    let cec: f64 = kani::any();
    let pfas: f64= kani::any();

    let row = PhoenixWaterRow {
        nodeid: "vault-001".to_string(),
        windowstartts: "2026-07-06T09:00:00Z".to_string(),
        windowendts: "2026-07-06T09:15:00Z".to_string(),
        bod_mg_per_l: bod,
        tss_mg_per_l: tss,
        n_mg_per_l: n,
        p_mg_per_l: p,
        cec_ug_per_l: cec,
        pfas_ng_per_l: pfas,
    };

    let rv = risk_from_row(
        &row, bod_bands, tss_bands, n_bands, p_bands, cec_bands, pfas_bands,
    );

    kani::assert!(rv.r_bod >= 0.0 && rv.r_bod <= 1.0);
    kani::assert!(rv.r_tss >= 0.0 && rv.r_tss <= 1.0);
    kani::assert!(rv.r_n   >= 0.0 && rv.r_n   <= 1.0);
    kani::assert!(rv.r_p   >= 0.0 && rv.r_p   <= 1.0);
    kani::assert!(rv.r_cec >= 0.0 && rv.r_cec <= 1.0);
    kani::assert!(rv.r_pfas>= 0.0 && rv.r_pfas<= 1.0);
    kani::assert!(rv.r_bio >= 0.0 && rv.r_bio <= 1.0);
}
