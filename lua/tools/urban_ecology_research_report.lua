local UrbanHeatRecoveryPrioritizer = require("eco_restoration.urban_heat_recovery_prioritizer")
local CorridorConnectivity = require("eco_restoration.multi_species_corridor_connectivity")

local function write_score(label, value)
  io.write(label .. ": " .. string.format("%.3f", value) .. "\n")
end

local sites = {
  {
    site_id = "facility_roof_a",
    intervention = "cool_roof",
    area_m2 = 1500,
    heat_exposure_index = 0.78,
    expected_temperature_reduction_c = 4.0,
    water_feasibility = 0.95,
    maintenance_capacity = 0.80,
    accessibility_preserved = true,
    displacement_risk = 0.02,
    public_access_preserved = true,
    habitat_compatibility = 0.30,
    embodied_burden = 0.25,
    operational_water_burden = 0.05,
    repairability = 0.75,
  },
  {
    site_id = "public_pavement_b",
    intervention = "cool_pavement",
    area_m2 = 2800,
    heat_exposure_index = 0.84,
    expected_temperature_reduction_c = 3.0,
    water_feasibility = 0.98,
    maintenance_capacity = 0.75,
    accessibility_preserved = true,
    displacement_risk = 0.03,
    public_access_preserved = true,
    habitat_compatibility = 0.15,
    embodied_burden = 0.35,
    operational_water_burden = 0.05,
    repairability = 0.60,
  },
}

local corridor_cells = {
  {
    cell_id = "corridor_01",
    resistance = { pollinator = 0.20, bird = 0.35, small_mammal = 0.45 },
    habitat_quality = 0.78,
    public_access_preserved = 1.00,
    displacement_risk = 0.05,
    maintenance_feasibility = 0.75,
  },
  {
    cell_id = "corridor_02",
    resistance = { pollinator = 0.30, bird = 0.30, small_mammal = 0.40 },
    habitat_quality = 0.72,
    public_access_preserved = 0.95,
    displacement_risk = 0.08,
    maintenance_feasibility = 0.80,
  },
  {
    cell_id = "corridor_03",
    resistance = { pollinator = 0.25, bird = 0.40, small_mammal = 0.50 },
    habitat_quality = 0.70,
    public_access_preserved = 0.95,
    displacement_risk = 0.10,
    maintenance_feasibility = 0.70,
  },
}

local ranked_sites = UrbanHeatRecoveryPrioritizer.rank_sites(sites)
local corridor = CorridorConnectivity.evaluate_corridor(corridor_cells)

io.write("urban_heat_recovery_rankings\n")
for index, site in ipairs(ranked_sites) do
  io.write(index .. ". " .. site.site_id .. " (" .. site.intervention .. ")\n")
  io.write("admissible: " .. tostring(site.admissible) .. "\n")
  write_score("priority_score", site.priority_score)
  write_score("knowledge_factor", site.knowledge_factor)
  write_score("eco_impact_value", site.eco_impact_value)
  write_score("harm_risk", site.harm_risk)
end

io.write("\nmulti_species_corridor_assessment\n")
io.write("admissible: " .. tostring(corridor.admissible) .. "\n")
write_score("knowledge_factor", corridor.knowledge_factor)
write_score("eco_impact_value", corridor.eco_impact_value)
write_score("harm_risk", corridor.harm_risk)

for species_name, result in pairs(corridor.connectivity or {}) do
  io.write(species_name .. "_path_cost: " .. string.format("%.3f", result.path_cost) .. "\n")
  io.write(species_name .. "_connectivity_score: " .. string.format("%.3f", result.connectivity_score) .. "\n")
end
