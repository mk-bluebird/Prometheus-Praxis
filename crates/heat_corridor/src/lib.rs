// Filename: crates/heat_corridor/src/lib.rs
#[macro_export]
macro_rules! corridor_check {
    ($h:expr, $safe:expr, $gold:expr, $hard:expr) => {{
        let h_val: f64 = $h;
        let h_safe: f64 = $safe;
        let h_gold: f64 = $gold;
        let h_hard: f64 = $hard;

        // Assert admissibility vs legal (hard) and gold thresholds.
        if h_val > h_hard {
            panic!(
                "heat corridor breach: H={} exceeds hard limit {}",
                h_val, h_hard
            );
        }
        if h_val > h_gold {
            // allowed but flagged as above gold band
            eprintln!(
                "heat corridor warning: H={} exceeds gold band {}",
                h_val, h_gold
            );
        }
    }};
}

pub fn heat_risk_coordinate(
    wbgt_c: f64,
    pet_c: f64,
    beta_wbgt: f64,
    beta_pet: f64,
    h0_c: f64,
    h_safe_c: f64,
    h_hard_c: f64,
) -> f64 {
    let sum = beta_wbgt + beta_pet;
    let bw = if sum == 0.0 { 0.5 } else { beta_wbgt / sum };
    let bp = if sum == 0.0 { 0.5 } else { beta_pet / sum };

    let h = bw * wbgt_c + bp * pet_c;

    let d = (h - h0_c).abs();
    let d_safe = (h_safe_c - h0_c).abs();
    let d_hard = (h_hard_c - h0_c).abs();

    if d <= d_safe {
        0.0
    } else if d <= d_hard {
        (d - d_safe) / (d_hard - d_safe)
    } else {
        1.0
    }
}
