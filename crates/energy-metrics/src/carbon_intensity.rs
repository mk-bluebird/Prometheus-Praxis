// crates/energy-metrics/src/carbon_intensity.rs
fn phoenix_carbon_intensity(hour_utc: u8) -> f32 {
    // Phoenix MST = UTC - 7 (no DST currently)
    let hour_local = (hour_utc + 24 - 7) % 24;
    
    match hour_local {
        0..=5 => 0.38,   // Night: high natural gas fraction
        6..=9 => 0.42,   // Morning ramp-up
        10..=16 => 0.35, // Peak solar (lowest carbon)
        17..=20 => 0.52, // Evening peak (gas peakers)
        21..=23 => 0.45, // Night baseline
        _ => 0.45
    }
}

fn compute_carbon_offset(joules_per_cycle: f32, timestamp: DateTime<Utc>) -> f32 {
    let kwh = joules_per_cycle / 3.6e6;
    let ci = phoenix_carbon_intensity(timestamp.hour() as u8);
    let carbon_kg = kwh * ci;
    
    // Budget: 0.01 kg CO₂e per cycle
    carbon_kg - 0.01
}
