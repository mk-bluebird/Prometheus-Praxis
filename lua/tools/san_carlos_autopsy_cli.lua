local oxygen = require("eco_restoration.lake_oxygen_temperature_budget")

local state = {
    dissolved_oxygen_mg_l = 5.2,
    water_temperature_c = 29.0
}

local parameters = {
    reaeration_per_second = 0.000015,
    benthic_oxygen_demand_mg_l_s = 0.000010,
    biota_respiration_mg_l_s = 0.000008,
    algal_respiration_mg_l_s = 0.000006,
    mixing_gain_per_second = 0.000004,
    evidence_quality = 0.70
}

local night_forcing = {
    photosynthesis_mg_l_s = 0.0,
    light_fraction = 0.0,
    mixed_layer_oxygen_mg_l = 5.8
}

local assessment = oxygen.assess(
    state,
    parameters,
    night_forcing,
    3600.0,
    4.0
)

io.write(string.format(
    "projected_dissolved_oxygen_mg_l=%.4f\n",
    assessment.projected_state.dissolved_oxygen_mg_l
))
io.write(string.format(
    "dissolved_oxygen_margin_mg_l=%.4f\n",
    assessment.dissolved_oxygen_margin_mg_l
))
io.write("corridor_status=", assessment.corridor_status, "\n")
io.write(string.format("knowledge_factor=%.4f\n", assessment.knowledge_factor))
io.write(string.format("eco_impact_value=%.4f\n", assessment.eco_impact_value))
io.write(string.format("harm_risk=%.4f\n", assessment.harm_risk))
