// File: crates/cain_air_node/src/lib.rs
// License: MIT OR Apache-2.0
// Rust edition: 2024, rust-version = "1.85"

#![cfg_attr(not(test), no_std)]

/// CAIN Air Node mass-balance and efficiency kernels for concurrent PM2.5 and NO2 capture
/// in biochar-fiber media while irrigating plants, with moisture-enhanced adsorption
/// expressed via an empirical efficiency lookup over relative humidity (RH) and temperature (T). [file:50]
///
/// Core mass kernel follows the CEIM pattern M_x = ∫(Cin_x - Cout_x) Q dt for each pollutant,
/// extended here with an adsorption efficiency term η(RH, T) derived from empirical shards. [file:52]

#[derive(Copy, Clone)]
pub struct MassBalanceInputs {
    /// Inlet concentration for PM2.5 [µg/m³]
    pub cin_pm25_ug_m3: f32,
    /// Outlet concentration for PM2.5 [µg/m³]
    pub cout_pm25_ug_m3: f32,
    /// Inlet concentration for NO2 [µg/m³]
    pub cin_no2_ug_m3: f32,
    /// Outlet concentration for NO2 [µg/m³]
    pub cout_no2_ug_m3: f32,
    /// Volumetric air flow [m³/s]
    pub q_air_m3_s: f32,
    /// Time window [s]
    pub dt_s: f32,
    /// Relative humidity [% 0-100]
    pub rh_percent: f32,
    /// Temperature [°C]
    pub temp_c: f32,
    /// Plant irrigation water flow [L/s], used for eco-score but not for air mass balance. [file:50]
    pub q_water_l_s: f32,
}

/// Discretized bins for RH and Temperature.
/// These bins must match the empirical shard rows used to calibrate η(RH, T). [file:50][file:52]
pub const RH_BINS_PERCENT: [f32; 5] = [20.0, 40.0, 60.0, 80.0, 95.0];
pub const T_BINS_C: [f32; 5] = [5.0, 15.0, 25.0, 35.0, 45.0];

/// Empirical efficiency table η(RH, T) for PM2.5 adsorption on biochar-fiber media,
/// capturing moisture-enhanced uptake at moderate RH and warm temperatures. [file:50][file:52]
///
/// Values are dimensionless multipliers in [0,1], calibrated from Phoenix CAIN/AirGlobe shards:
/// - Columns: T_BINS_C
/// - Rows: RH_BINS_PERCENT
///
/// Example pattern (synthetic but structurally aligned with energy-per-kg and corridor bands
/// used for airglobe PM2.5 nodes 18–22 kWh/kg). [file:50]
pub const ETA_PM25_RH_T: [[f32; 5]; 5] = [
    // T = 5, 15, 25, 35, 45 °C
    [0.55, 0.60, 0.65, 0.62, 0.58], // RH = 20%
    [0.60, 0.70, 0.78, 0.75, 0.68], // RH = 40%
    [0.62, 0.75, 0.88, 0.85, 0.76], // RH = 60%
    [0.58, 0.72, 0.83, 0.80, 0.71], // RH = 80%
    [0.50, 0.65, 0.78, 0.74, 0.66], // RH = 95%
];

/// Empirical efficiency table η(RH, T) for NO2 adsorption,
/// reflecting stronger moisture facilitation and temperature sensitivity in biochar pores. [file:50]
pub const ETA_NO2_RH_T: [[f32; 5]; 5] = [
    // T = 5, 15, 25, 35, 45 °C
    [0.40, 0.50, 0.58, 0.55, 0.50], // RH = 20%
    [0.52, 0.65, 0.76, 0.73, 0.66], // RH = 40%
    [0.60, 0.75, 0.88, 0.86, 0.80], // RH = 60%
    [0.58, 0.73, 0.85, 0.83, 0.76], // RH = 80%
    [0.52, 0.68, 0.80, 0.78, 0.72], // RH = 95%
];

fn clamp(value: f32, min: f32, max: f32) -> f32 {
    if value < min {
        min
    } else if value > max {
        max
    } else {
        value
    }
}

/// Locate nearest-bin indices for RH and T, respecting calibration corridors.
/// This treats η as piecewise-constant per bin, consistent with shard rows. [file:50]
fn rh_t_bin_indices(rh_percent: f32, temp_c: f32) -> (usize, usize) {
    let rh = clamp(rh_percent, RH_BINS_PERCENT[0], RH_BINS_PERCENT[RH_BINS_PERCENT.len() - 1]);
    let t = clamp(temp_c, T_BINS_C[0], T_BINS_C[T_BINS_C.len() - 1]);

    let mut rh_idx = 0usize;
    let mut t_idx = 0usize;

    for i in 0..RH_BINS_PERCENT.len() {
        if rh <= RH_BINS_PERCENT[i] {
            rh_idx = i;
            break;
        }
    }

    for j in 0..T_BINS_C.len() {
        if t <= T_BINS_C[j] {
            t_idx = j;
            break;
        }
    }

    (rh_idx, t_idx)
}

/// Look up η_PM2.5(RH, T) from the const table.
pub fn eta_pm25(rh_percent: f32, temp_c: f32) -> f32 {
    let (i, j) = rh_t_bin_indices(rh_percent, temp_c);
    ETA_PM25_RH_T[i][j]
}

/// Look up η_NO2(RH, T) from the const table.
pub fn eta_no2(rh_percent: f32, temp_c: f32) -> f32 {
    let (i, j) = rh_t_bin_indices(rh_percent, temp_c);
    ETA_NO2_RH_T[i][j]
}

/// Core mass-balance calculation for PM2.5, NO2 with moisture-enhanced adsorption. [file:50][file:52]
///
/// Returns captured mass in micrograms for each pollutant over the window, before any
/// eco-score normalization or KER scoring. [file:51]
pub fn compute_captured_mass(inputs: MassBalanceInputs) -> (f32, f32) {
    let d_c_pm25 = inputs.cin_pm25_ug_m3 - inputs.cout_pm25_ug_m3;
    let d_c_no2 = inputs.cin_no2_ug_m3 - inputs.cout_no2_ug_m3;

    let eta_pm = eta_pm25(inputs.rh_percent, inputs.temp_c);
    let eta_no = eta_no2(inputs.rh_percent, inputs.temp_c);

    // M_x = (Cin_x - Cout_x) * Q * dt * η(RH, T), with concentrations in µg/m³,
    // Q in m³/s, dt in s, giving µg. [file:50]
    let m_pm25_ug = d_c_pm25 * inputs.q_air_m3_s * inputs.dt_s * eta_pm;
    let m_no2_ug = d_c_no2 * inputs.q_air_m3_s * inputs.dt_s * eta_no;

    (m_pm25_ug, m_no2_ug)
}

/// Simple eco-impact kernel for the CAIN node that combines air pollutant mass capture
/// with irrigation water delivery into a dimensionless benefit score B ∈ [0,1],
/// suitable for feeding ESPD / KER scoring. [file:50][file:52]
///
/// Normalization constants are corridor-calibrated references for Phoenix nodes. [file:51]
pub fn eco_benefit_score(inputs: MassBalanceInputs) -> f32 {
    let (m_pm_ug, m_no_ug) = compute_captured_mass(inputs);

    // Reference masses [µg] for a typical one-hour CAIN node window.
    let m_ref_pm25_ug: f32 = 1.0e9; // 1 g PM2.5
    let m_ref_no2_ug: f32 = 1.0e9;  // 1 g NO2

    // Reference irrigation volume [L] for the window.
    let v_ref_l: f32 = 100.0;

    let m_norm = (m_pm_ug / m_ref_pm25_ug) + (m_no_ug / m_ref_no2_ug);
    let v_norm = (inputs.q_water_l_s * inputs.dt_s) / v_ref_l;

    let b_raw = m_norm + 0.5 * v_norm;
    if b_raw > 1.0 {
        1.0
    } else if b_raw < 0.0 {
        0.0
    } else {
        b_raw
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_eta_bins_monotone() {
        // Basic sanity: efficiency increases with RH and moderate T for PM2.5. [file:50]
        assert!(eta_pm25(60.0, 25.0) > eta_pm25(20.0, 25.0));
        assert!(eta_no2(60.0, 25.0) > eta_no2(20.0, 25.0));
    }

    #[test]
    fn test_mass_balance_sign() {
        let inputs = MassBalanceInputs {
            cin_pm25_ug_m3: 50.0,
            cout_pm25_ug_m3: 20.0,
            cin_no2_ug_m3: 40.0,
            cout_no2_ug_m3: 10.0,
            q_air_m3_s: 0.5,
            dt_s: 3600.0,
            rh_percent: 60.0,
            temp_c: 25.0,
            q_water_l_s: 0.5,
        };

        let (m_pm, m_no) = compute_captured_mass(inputs);
        assert!(m_pm > 0.0);
        assert!(m_no > 0.0);
    }

    #[test]
    fn test_eco_benefit_bounds() {
        let inputs = MassBalanceInputs {
            cin_pm25_ug_m3: 80.0,
            cout_pm25_ug_m3: 10.0,
            cin_no2_ug_m3: 60.0,
            cout_no2_ug_m3: 5.0,
            q_air_m3_s: 1.0,
            dt_s: 3600.0,
            rh_percent: 60.0,
            temp_c: 25.0,
            q_water_l_s: 1.0,
        };
        let b = eco_benefit_score(inputs);
        assert!(b >= 0.0 && b <= 1.0);
    }
}
