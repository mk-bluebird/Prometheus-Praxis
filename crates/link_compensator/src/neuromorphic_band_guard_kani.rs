// neuromorphic_band_guard_kani.rs
// Kani proof harnesses for neuromorphic_band_guard.

use super::*;

#[cfg(kani)]
#[kani::proof]
fn compensated_profiles_cannot_widen_corridor() {
    // Arbitrary but bounded values for vt_before and vt_after.
    let vt_before: f64 = kani::any();
    let vt_after: f64 = kani::any();

    // Assume vt_after > vt_before to model widening.
    kani::assume(vt_after > vt_before);

    let profile = NeuromorphicMaterialProfile {
        profile_id: String::from("test"),
        paper_id: String::from("paper"),
        device_label: String::from("device"),
        material_system: String::from("material"),
        energy_band: EnergyBand::Low,
        endurance_band: EnduranceBand::Mid,
        variability_band: VariabilityBand::Stable,
        eco_impact_band: EcoImpactBand::E0_0_25,
        is_compensated: true,
        compensation_source_url: String::from("url"),
        compensation_method: String::from("INTERPOLATED"),
        evidence_hex: String::from("0xDEADBEEF"),
    };

    let plan = CorridorPlan { vt_before, vt_after };
    let caps = GlobalCaps { veco_cap: 1.0 };

    let res = validate_neuromorphic_profile(&profile, &caps, &plan);
    // For any widening attempt, compensated profiles must be rejected.
    assert!(matches!(res, Err(GuardError::CompensatedWideningAttempt)));
}

#[cfg(kani)]
#[kani::proof]
fn eco_impact_band_respects_caps() {
    let caps_value: f64 = kani::any();
    kani::assume(caps_value >= 0.0 && caps_value <= 1.0);

    let caps = GlobalCaps { veco_cap: caps_value };

    // Pick a band, let Kani explore combinations.
    let profile = NeuromorphicMaterialProfile {
        profile_id: String::from("test2"),
        paper_id: String::from("paper2"),
        device_label: String::from("device2"),
        material_system: String::from("material2"),
        energy_band: EnergyBand::Low,
        endurance_band: EnduranceBand::Mid,
        variability_band: VariabilityBand::Stable,
        eco_impact_band: EcoImpactBand::E0_75_1_0,
        is_compensated: false,
        compensation_source_url: String::from("url2"),
        compensation_method: String::from("DIRECT"),
        evidence_hex: String::from("0xBEEFDEAD"),
    };

    let plan = CorridorPlan { vt_before: 0.0, vt_after: 0.0 };

    let res = validate_neuromorphic_profile(&profile, &caps, &plan);
    // If veco_cap < 0.75, highest ecoImpactBand must be rejected.
    if caps.veco_cap < 0.75 {
        assert!(matches!(res, Err(GuardError::EcoImpactExceedsCaps)));
    }
}
