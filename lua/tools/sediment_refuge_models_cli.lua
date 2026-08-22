local models = require("eco_restoration.sediment_refuge_models")

local result = models.assess({
    sod = {
        sod_reference_mg_m2_h = 1.20,
        q10 = 2.0,
        temperature_c = 30.0,
        reference_temperature_c = 20.0,
        organic_fraction = 0.08,
        organic_multiplier = 4.0,
        contaminant_inhibition_fraction = 0.0
    },
    bathymetry_cells = {
        {bed_elevation_m = 100.0, area_m2 = 800.0},
        {bed_elevation_m = 101.0, area_m2 = 1100.0},
        {bed_elevation_m = 102.0, area_m2 = 1200.0},
        {bed_elevation_m = 103.5, area_m2 = 900.0}
    },
    water_level_m = 105.0,
    minimum_refuge_depth_m = 2.0,
    delta_h_m = 0.10,
    minimum_refuge_volume_m3 = 4500.0,
    maximum_sod_mg_m2_h = 3.0,
    evidence_quality = 0.80
})

io.write(string.format(
    "sediment_oxygen_demand_mg_m2_h=%.6f\n",
    result.sediment_oxygen_demand_mg_m2_h
))
io.write(string.format("refuge_volume_m3=%.2f\n", result.refuge_volume_m3))
io.write(string.format("refuge_area_m2=%.2f\n", result.refuge_area_m2))
io.write(string.format(
    "d_refuge_volume_d_h_m2=%.2f\n",
    result.d_refuge_volume_d_h_m2
))
io.write("connectivity_warning=", tostring(result.connectivity_warning), "\n")
io.write("corridor_status=", result.corridor_status, "\n")
io.write(string.format("knowledge_factor=%.4f\n", result.knowledge_factor))
io.write(string.format("eco_impact_value=%.4f\n", result.eco_impact_value))
io.write(string.format("harm_risk=%.4f\n", result.harm_risk))
