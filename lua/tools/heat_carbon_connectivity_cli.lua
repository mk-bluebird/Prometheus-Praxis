local metrics = require("eco_restoration.heat_carbon_connectivity_metrics")

local heat_stress = metrics.ecological_heat_stress({
    air_temperature_c = 43.0,
    radiant_temperature_c = 56.0,
    relative_humidity_percent = 28.0
})

local capacity = metrics.carbon_negative_workload_capacity({
    verified_removal_kg_co2e = 1800.0,
    embodied_carbon_kg_co2e = 350.0,
    maintenance_carbon_kg_co2e = 80.0,
    noncompute_carbon_kg_co2e = 120.0,
    uncertainty_deduction_kg_co2e = 300.0,
    lifecycle_energy_per_work_unit_kwh = 0.80,
    clean_energy_available_kwh = 950.0,
    operational_energy_per_work_unit_kwh = 0.50,
    safe_power_kw = 30.0,
    operational_power_per_work_unit_kw = 0.05,
    annual_operational_carbon_kg_co2e = 210.0
})

local connectivity = metrics.assess_connectivity({
    {
        edge_id = "canal_a_to_b",
        flow_capacity = 1.4,
        water_quality_value = 0.15,
        water_quality_critical = 0.00
    },
    {
        edge_id = "canal_b_to_c",
        flow_capacity = 0.9,
        water_quality_value = 0.42,
        water_quality_critical = 0.00
    },
    {
        edge_id = "canal_c_to_lake",
        flow_capacity = 1.1,
        water_quality_value = 0.25,
        water_quality_critical = 0.00
    }
}, 4.0, 0.70)

io.write(string.format("ecological_heat_stress=%.4f\n", heat_stress))
io.write(string.format(
    "allowed_workload_capacity=%.4f\n",
    capacity.allowed_workload_capacity
))
io.write(string.format(
    "embodied_to_annual_operational_carbon_ratio=%.4f\n",
    capacity.embodied_to_annual_operational_carbon_ratio
))
io.write("carbon_negative=", tostring(capacity.carbon_negative), "\n")
io.write(string.format("capacity_normalized_hci=%.4f\n", connectivity.capacity_normalized_hci))
io.write("limiting_edge_id=", connectivity.limiting_edge_id, "\n")
io.write("corridor_status=", connectivity.corridor_status, "\n")
