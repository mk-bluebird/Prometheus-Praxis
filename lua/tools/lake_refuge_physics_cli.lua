local physics = require("eco_restoration.lake_refuge_physics")

local result = physics.assess({
    stratification = {
        gravity_m_s2 = 9.80665,
        thermal_expansion_per_c = 0.0003,
        temperature_difference_c = 4.0,
        mean_depth_m = 3.0,
        kinematic_viscosity_m2_s = 0.0000009,
        thermal_diffusivity_m2_s = 0.000000143
    },
    initial_depth_m = 5.0,
    current_depth_m = 3.0,
    critical_refuge_volume_m3 = 2500.0,
    evidence_quality = 0.80,
    species = {
        {
            name = "razorback_sucker_reference_envelope",
            temperature_min_c = 15.0,
            temperature_max_c = 25.0,
            minimum_do_mg_l = 4.0
        },
        {
            name = "warmwater_community_reference_envelope",
            temperature_min_c = 18.0,
            temperature_max_c = 28.0,
            minimum_do_mg_l = 3.0
        }
    },
    refuge_cells = {
        {volume_m3 = 1200.0, temperature_c = 21.0, dissolved_oxygen_mg_l = 5.2},
        {volume_m3 = 900.0, temperature_c = 24.0, dissolved_oxygen_mg_l = 4.8},
        {volume_m3 = 1000.0, temperature_c = 27.0, dissolved_oxygen_mg_l = 4.2},
        {volume_m3 = 700.0, temperature_c = 22.0, dissolved_oxygen_mg_l = 2.8}
    }
})

io.write(string.format("stratification_number=%.6f\n", result.stratification_number))
io.write(string.format("drawdown_depth_ratio=%.6f\n", result.drawdown_depth_ratio))
io.write(string.format("stratification_retention_ratio=%.6f\n", result.stratification_retention_ratio))
io.write(string.format("community_refuge_volume_m3=%.2f\n", result.community_refuge_volume_m3))
io.write("limiting_species=", result.limiting_species, "\n")
io.write(string.format(
    "limiting_species_refuge_volume_m3=%.2f\n",
    result.limiting_species_refuge_volume_m3
))
io.write(string.format("refuge_ratio=%.6f\n", result.refuge_ratio))
io.write("corridor_status=", result.corridor_status, "\n")
io.write(string.format("knowledge_factor=%.4f\n", result.knowledge_factor))
io.write(string.format("eco_impact_value=%.4f\n", result.eco_impact_value))
io.write(string.format("harm_risk=%.4f\n", result.harm_risk))
