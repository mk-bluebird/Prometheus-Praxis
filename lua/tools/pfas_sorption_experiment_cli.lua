local experiment = require("eco_restoration.pfas_sorption_experiment_design")

local design = {
    site_id = "phoenix_canal_sediment_characterization",
    pfas_analyte = "PFOS",
    bulk_density_kg_m3 = 1450.0,
    volumetric_water_content = 0.25,
    kd_min_l_kg = 0.2,
    kd_max_l_kg = 20.0,
    sediment_replicates = 5,
    concentration_levels = 5,
    equilibration_hours = 48.0,
    method_detection_limit_ng_l = 0.5,
    target_lowest_concentration_ng_l = 3.0,
    matrix_spike_recovery_fraction = 0.95,
    field_blank_contamination_ng_l = 0.05,
    allowed_blank_fraction_of_lowest_level = 0.10,
    wildlife_exposure_risk = 0.0,
    containment_verified = 1.0,
    chain_of_custody_verified = 1.0
}

local result = experiment.evaluate(design)

io.write("site_id=", result.site_id, "\n")
io.write("pfas_analyte=", result.pfas_analyte, "\n")
io.write(string.format("retardation_factor_min=%.6f\n", result.retardation_factor_min))
io.write(string.format("retardation_factor_max=%.6f\n", result.retardation_factor_max))
io.write(string.format("blank_limit_ng_l=%.6f\n", result.blank_limit_ng_l))
io.write(string.format("knowledge_factor=%.4f\n", result.knowledge_factor))
io.write(string.format("eco_impact_value=%.4f\n", result.eco_impact_value))
io.write(string.format("harm_risk=%.4f\n", result.harm_risk))
io.write("verdict=", result.verdict, "\n")

if #result.blocked_reasons > 0 then
    io.write("blocked_reasons=", table.concat(result.blocked_reasons, ","), "\n")
end
