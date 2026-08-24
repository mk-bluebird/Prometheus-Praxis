local UrbanHeatRecoveryPrioritizer = {}

local VALID_INTERVENTIONS = {
  cool_roof = true,
  green_roof = true,
  tree_canopy = true,
  permeable_surface = true,
  cool_pavement = true,
}

local REQUIRED_SITE_FIELDS = {
  site_id = "string",
  intervention = "string",
  area_m2 = "number",
  heat_exposure_index = "number",
  expected_temperature_reduction_c = "number",
  water_feasibility = "number",
  maintenance_capacity = "number",
  accessibility_preserved = "boolean",
  displacement_risk = "number",
  public_access_preserved = "boolean",
  habitat_compatibility = "number",
  embodied_burden = "number",
  operational_water_burden = "number",
  repairability = "number",
}

local PROHIBITED_FIELDS = {
  resident_identity = true,
  resident_demographics = true,
  resident_profile = true,
  worker_identity = true,
  device_identifier = true,
  augmentation_status = true,
  disability_status = true,
  persistent_location_history = true,
  behavioral_profile = true,
}

local function clamp(value, minimum, maximum)
  if value < minimum then
    return minimum
  end
  if value > maximum then
    return maximum
  end
  return value
end

local function append(values, value)
  values[#values + 1] = value
end

local function require_unit_interval(value, field_name, errors)
  if type(value) ~= "number" or value < 0 or value > 1 then
    append(errors, field_name .. " must be a number in the interval [0, 1].")
  end
end

local function normalize_weights(weights)
  local defaults = {
    heat = 0.30,
    cooling = 0.20,
    water = 0.10,
    maintenance = 0.10,
    habitat = 0.10,
    repairability = 0.10,
    burden = 0.10,
  }

  if type(weights) ~= "table" then
    return defaults
  end

  local total = 0
  local normalized = {}

  for key, default_value in pairs(defaults) do
    local value = weights[key]
    if type(value) ~= "number" or value < 0 then
      value = default_value
    end
    normalized[key] = value
    total = total + value
  end

  if total <= 0 then
    return defaults
  end

  for key, value in pairs(normalized) do
    normalized[key] = value / total
  end

  return normalized
end

function UrbanHeatRecoveryPrioritizer.validate_site(site)
  local errors = {}

  if type(site) ~= "table" then
    return {
      valid = false,
      errors = { "Site record must be a table." },
    }
  end

  for field_name in pairs(site) do
    if PROHIBITED_FIELDS[field_name] then
      append(errors, "Prohibited identity or profiling field: " .. field_name)
    end
  end

  for field_name, expected_type in pairs(REQUIRED_SITE_FIELDS) do
    if type(site[field_name]) ~= expected_type then
      append(errors, field_name .. " must be a " .. expected_type .. ".")
    end
  end

  if type(site.site_id) == "string" and site.site_id == "" then
    append(errors, "site_id must not be empty.")
  end

  if type(site.intervention) == "string" and not VALID_INTERVENTIONS[site.intervention] then
    append(errors, "intervention is not an approved heat-recovery intervention.")
  end

  if type(site.area_m2) == "number" and site.area_m2 <= 0 then
    append(errors, "area_m2 must be greater than zero.")
  end

  if type(site.expected_temperature_reduction_c) == "number" and site.expected_temperature_reduction_c < 0 then
    append(errors, "expected_temperature_reduction_c must be zero or greater.")
  end

  for _, field_name in ipairs({
    "heat_exposure_index",
    "water_feasibility",
    "maintenance_capacity",
    "displacement_risk",
    "habitat_compatibility",
    "embodied_burden",
    "operational_water_burden",
    "repairability",
  }) do
    require_unit_interval(site[field_name], field_name, errors)
  end

  if site.accessibility_preserved ~= true then
    append(errors, "Accessibility preservation is a hard gate and must be true.")
  end

  if site.public_access_preserved ~= true then
    append(errors, "Public-access preservation is a hard gate and must be true.")
  end

  return {
    valid = #errors == 0,
    errors = errors,
  }
end

function UrbanHeatRecoveryPrioritizer.score_site(site, weights)
  local validation = UrbanHeatRecoveryPrioritizer.validate_site(site)
  if not validation.valid then
    return {
      admissible = false,
      priority_score = 0,
      knowledge_factor = 0,
      eco_impact_value = 0,
      harm_risk = 1,
      errors = validation.errors,
    }
  end

  local w = normalize_weights(weights)
  local cooling_scale_c = 15
  local area_scale_m2 = 10000
  local normalized_cooling = clamp(site.expected_temperature_reduction_c / cooling_scale_c, 0, 1)
  local normalized_area = clamp(site.area_m2 / area_scale_m2, 0, 1)

  local heat_benefit = clamp(
    0.70 * site.heat_exposure_index + 0.30 * normalized_area,
    0,
    1
  )

  local ecological_benefit = clamp(
    0.45 * heat_benefit
      + 0.25 * normalized_cooling
      + 0.15 * site.habitat_compatibility
      + 0.15 * site.repairability,
    0,
    1
  )

  local lifecycle_burden = clamp(
    0.55 * site.embodied_burden + 0.45 * site.operational_water_burden,
    0,
    1
  )

  local knowledge_factor = clamp(
    0.20
      + 0.20 * site.maintenance_capacity
      + 0.20 * site.water_feasibility
      + 0.20 * site.repairability
      + 0.20 * site.habitat_compatibility,
    0,
    1
  )

  local harm_risk = clamp(
    0.50 * site.displacement_risk
      + 0.30 * lifecycle_burden
      + 0.20 * (1 - site.maintenance_capacity),
    0,
    1
  )

  local priority_score = clamp(
    w.heat * heat_benefit
      + w.cooling * normalized_cooling
      + w.water * site.water_feasibility
      + w.maintenance * site.maintenance_capacity
      + w.habitat * site.habitat_compatibility
      + w.repairability * site.repairability
      + w.burden * (1 - lifecycle_burden),
    0,
    1
  )

  local admissible =
    site.accessibility_preserved
    and site.public_access_preserved
    and site.displacement_risk <= 0.25
    and harm_risk <= 0.45
    and knowledge_factor >= 0.50
    and ecological_benefit >= 0.40

  return {
    admissible = admissible,
    priority_score = admissible and priority_score or 0,
    knowledge_factor = knowledge_factor,
    eco_impact_value = ecological_benefit,
    harm_risk = harm_risk,
    lifecycle_burden = lifecycle_burden,
    errors = {},
  }
end

function UrbanHeatRecoveryPrioritizer.rank_sites(sites, weights)
  assert(type(sites) == "table", "sites must be a table.")

  local ranked = {}

  for _, site in ipairs(sites) do
    local result = UrbanHeatRecoveryPrioritizer.score_site(site, weights)

    ranked[#ranked + 1] = {
      site_id = type(site) == "table" and site.site_id or "invalid_site",
      intervention = type(site) == "table" and site.intervention or "unknown",
      admissible = result.admissible,
      priority_score = result.priority_score,
      knowledge_factor = result.knowledge_factor,
      eco_impact_value = result.eco_impact_value,
      harm_risk = result.harm_risk,
      errors = result.errors,
    }
  end

  table.sort(ranked, function(left, right)
    if left.admissible ~= right.admissible then
      return left.admissible
    end
    return left.priority_score > right.priority_score
  end)

  return ranked
end

return UrbanHeatRecoveryPrioritizer
