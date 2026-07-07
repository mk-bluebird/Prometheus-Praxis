// Filename: crates/air_globe_ceim/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct SpeciesConcentration {
    pub co2_mg_m3: f64,
    pub pm25_ug_m3: f64,
    pub nox_mg_m3: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct AirConditions {
    pub temperature_c: f64,
    pub relative_humidity: f64, // 0.0–1.0
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct FlowConditions {
    pub q_std_m3_s: f64, // standard volumetric flow
    pub duration_s: f64,
}

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct MassCaptureResult {
    pub m_co2_kg: f64,
    pub m_pm25_kg: f64,
    pub m_nox_kg: f64,
    pub eco_intensity_co2_kg_per_kwh: f64,
    pub eco_intensity_pm25_kg_per_kwh: f64,
    pub eco_intensity_nox_kg_per_kwh: f64,
}

fn density_correction(air: AirConditions) -> f64 {
    // Simple ideal-gas-based correction relative to 20°C, dry.
    // rho ~ P / (R T), so volumetric flow at higher T expands.
    // Reference T_ref = 293.15 K, Phoenix T ~ 308.15 K.
    let t_ref_k = 293.15;
    let t_k = air.temperature_c + 273.15;
    t_ref_k / t_k
}

pub fn ceim_mass_balance_air_globe(
    cin: SpeciesConcentration,
    cout: SpeciesConcentration,
    air: AirConditions,
    flow: FlowConditions,
    energy_kwh: f64,
) -> MassCaptureResult {
    let f_rho = density_correction(air);
    let q_m3_s = flow.q_std_m3_s * f_rho;
    let volume_m3 = q_m3_s * flow.duration_s;

    // Convert concentrations to kg/m3:
    let co2_kg_m3_in = cin.co2_mg_m3 / 1_000_000.0;
    let co2_kg_m3_out = cout.co2_mg_m3 / 1_000_000.0;

    let pm25_kg_m3_in = cin.pm25_ug_m3 / 1_000_000_000.0;
    let pm25_kg_m3_out = cout.pm25_ug_m3 / 1_000_000_000.0;

    let nox_kg_m3_in = cin.nox_mg_m3 / 1_000_000.0;
    let nox_kg_m3_out = cout.nox_mg_m3 / 1_000_000.0;

    let m_co2_kg = (co2_kg_m3_in - co2_kg_m3_out) * volume_m3;
    let m_pm25_kg = (pm25_kg_m3_in - pm25_kg_m3_out) * volume_m3;
    let m_nox_kg = (nox_kg_m3_in - nox_kg_m3_out) * volume_m3;

    let eco_intensity_co2 = if energy_kwh > 0.0 {
        m_co2_kg / energy_kwh
    } else {
        0.0
    };
    let eco_intensity_pm25 = if energy_kwh > 0.0 {
        m_pm25_kg / energy_kwh
    } else {
        0.0
    };
    let eco_intensity_nox = if energy_kwh > 0.0 {
        m_nox_kg / energy_kwh
    } else {
        0.0
    };

    MassCaptureResult {
        m_co2_kg,
        m_pm25_kg,
        m_nox_kg,
        eco_intensity_co2_kg_per_kwh: eco_intensity_co2,
        eco_intensity_pm25_kg_per_kwh: eco_intensity_pm25,
        eco_intensity_nox_kg_per_kwh: eco_intensity_nox,
    }
}
