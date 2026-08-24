local CorridorConnectivity = {}

local SPECIES = {
  pollinator = true,
  bird = true,
  small_mammal = true,
}

local PROHIBITED_FIELDS = {
  resident_identity = true,
  resident_demographics = true,
  resident_profile = true,
  household_income = true,
  disability_status = true,
  augmentation_status = true,
  device_identifier = true,
  persistent_location_history = true,
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

local function validate_unit_interval(value, field_name, errors)
  if type(value) ~= "number" or value < 0 or value > 1 then
    append(errors, field_name .. " must be in [0, 1].")
  end
end

function CorridorConnectivity.validate_cell(cell)
  local errors = {}

  if type(cell) ~= "table" then
    return {
      valid = false,
      errors = { "Grid cell must be a table." },
    }
  end

  for field_name in pairs(cell) do
    if PROHIBITED_FIELDS[field_name] then
      append(errors, "Prohibited field in ecological corridor input: " .. field_name)
    end
  end

  if type(cell.cell_id) ~= "string" or cell.cell_id == "" then
    append(errors, "cell_id must be a non-empty string.")
  end

  if type(cell.resistance) ~= "table" then
    append(errors, "resistance must be a table indexed by species.")
  else
    for species_name in pairs(SPECIES) do
      validate_unit_interval(cell.resistance[species_name], "resistance." .. species_name, errors)
    end
  end

  validate_unit_interval(cell.habitat_quality, "habitat_quality", errors)
  validate_unit_interval(cell.public_access_preserved, "public_access_preserved", errors)
  validate_unit_interval(cell.displacement_risk, "displacement_risk", errors)
  validate_unit_interval(cell.maintenance_feasibility, "maintenance_feasibility", errors)

  return {
    valid = #errors == 0,
    errors = errors,
  }
end

function CorridorConnectivity.path_cost(cells, species_name)
  assert(SPECIES[species_name], "Unsupported species class.")
  assert(type(cells) == "table" and #cells > 0, "cells must be a non-empty table.")

  local total_cost = 0
  local errors = {}

  for _, cell in ipairs(cells) do
    local validation = CorridorConnectivity.validate_cell(cell)
    if not validation.valid then
      for _, error_message in ipairs(validation.errors) do
        append(errors, error_message)
      end
    else
      total_cost = total_cost + cell.resistance[species_name]
    end
  end

  if #errors > 0 then
    return nil, errors
  end

  return total_cost, {}
end

function CorridorConnectivity.evaluate_corridor(cells)
  assert(type(cells) == "table" and #cells > 0, "cells must be a non-empty table.")

  local validation_errors = {}
  local total_habitat = 0
  local total_access = 0
  local total_displacement = 0
  local total_maintenance = 0

  for _, cell in ipairs(cells) do
    local validation = CorridorConnectivity.validate_cell(cell)
    if not validation.valid then
      for _, error_message in ipairs(validation.errors) do
        append(validation_errors, error_message)
      end
    else
      total_habitat = total_habitat + cell.habitat_quality
      total_access = total_access + cell.public_access_preserved
      total_displacement = total_displacement + cell.displacement_risk
      total_maintenance = total_maintenance + cell.maintenance_feasibility
    end
  end

  if #validation_errors > 0 then
    return {
      admissible = false,
      knowledge_factor = 0,
      eco_impact_value = 0,
      harm_risk = 1,
      errors = validation_errors,
    }
  end

  local count = #cells
  local mean_habitat = total_habitat / count
  local mean_access = total_access / count
  local mean_displacement = total_displacement / count
  local mean_maintenance = total_maintenance / count

  local connectivity = {}
  local aggregate_connectivity = 0

  for species_name in pairs(SPECIES) do
    local cost = CorridorConnectivity.path_cost(cells, species_name)
    local normalized_connectivity = clamp(1 - (cost / count), 0, 1)

    connectivity[species_name] = {
      path_cost = cost,
      connectivity_score = normalized_connectivity,
    }

    aggregate_connectivity = aggregate_connectivity + normalized_connectivity
  end

  aggregate_connectivity = aggregate_connectivity / 3

  local knowledge_factor = clamp(
    0.45 * mean_maintenance + 0.35 * mean_habitat + 0.20 * mean_access,
    0,
    1
  )

  local eco_impact_value = clamp(
    0.60 * aggregate_connectivity + 0.40 * mean_habitat,
    0,
    1
  )

  local harm_risk = clamp(
    0.60 * mean_displacement
      + 0.25 * (1 - mean_access)
      + 0.15 * (1 - mean_maintenance),
    0,
    1
  )

  local admissible =
    mean_access >= 0.90
    and mean_displacement <= 0.20
    and knowledge_factor >= 0.50
    and eco_impact_value >= 0.45
    and harm_risk <= 0.35

  return {
    admissible = admissible,
    connectivity = connectivity,
    knowledge_factor = knowledge_factor,
    eco_impact_value = eco_impact_value,
    harm_risk = harm_risk,
    errors = {},
  }
end

return CorridorConnectivity
