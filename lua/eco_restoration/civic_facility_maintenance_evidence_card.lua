local EvidenceCard = {}

local VALID_STATES = {
  research_only = true,
  co_design = true,
  bounded_pilot = true,
  evidence_review = true,
  limited_operation = true,
  incremental_expansion = true,
  not_admissible = true,
}

local VALID_VERDICTS = {
  pass = true,
  fail = true,
  insufficient_evidence = true,
  policy_requirement = true,
}

local REQUIRED_INVARIANTS = {
  "local_human_safety",
  "least_intrusive_sensing",
  "minimal_operational_data",
  "functional_equality",
  "graceful_offline_behavior",
  "repair_first_lifecycle",
  "ecological_admissibility",
  "non_actuating_governance",
  "human_service_parity",
  "contestability",
  "worker_sovereignty",
  "public_accountability",
}

local PERMITTED_FIELDS = {
  asset_id = true,
  observed_at_window = true,
  measurement_type = true,
  measurement_value = true,
  measurement_unit = true,
  calibration_state = true,
  condition = true,
  raw_payload_retained = true,
}

local PROHIBITED_FIELDS = {
  resident_data_schema = true,
  worker_identity = true,
  device_serial = true,
  persistent_location_history = true,
  worker_badge_data = true,
  operator_route_trace = true,
  shift_timing = true,
  productivity_score = true,
  video = true,
  audio = true,
  biometric_data = true,
  device_identifier = true,
  ble_scan = true,
  wifi_scan = true,
  device_discovery = true,
  persistent_operator_machine_linkage = true,
}

local PERMITTED_SCRUBBER_MEASUREMENTS = {
  energy_consumption = true,
  battery_state = true,
  water_dispensed = true,
  tank_condition = true,
  fault_state = true,
  component_service_interval = true,
}

local PERMITTED_SENSOR_MEASUREMENTS = {
  ambient_temperature = true,
  relative_humidity = true,
  surface_temperature = true,
  door_state = true,
  leak_detected = true,
  fixed_asset_status = true,
}

local STOP_CONDITIONS = {
  tracking_cannot_be_disabled = true,
  identity_required_for_work_order = true,
  environmental_or_public_health_nonconformance = true,
  worker_representative_refusal = true,
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

local function is_string(value)
  return type(value) == "string" and value ~= ""
end

local function is_boolean(value)
  return type(value) == "boolean"
end

local function append(list, value)
  list[#list + 1] = value
end

local function sorted_copy(values)
  local copy = {}
  for index, value in ipairs(values) do
    copy[index] = value
  end
  table.sort(copy)
  return copy
end

local function normalize_verdict(verdict)
  if type(verdict) ~= "string" then
    return nil
  end
  return verdict:lower():gsub("%s+", "_")
end

local function validate_measurement(record, permitted_measurements, errors)
  if type(record) ~= "table" then
    append(errors, "Each measurement record must be a table.")
    return
  end

  for field_name in pairs(record) do
    if PROHIBITED_FIELDS[field_name] then
      append(errors, "Prohibited field found in measurement record: " .. field_name)
    elseif not PERMITTED_FIELDS[field_name] then
      append(errors, "Unapproved field found in measurement record: " .. field_name)
    end
  end

  for field_name in pairs(PERMITTED_FIELDS) do
    if record[field_name] == nil then
      append(errors, "Missing required permitted field: " .. field_name)
    end
  end

  if record.asset_id ~= nil and not is_string(record.asset_id) then
    append(errors, "asset_id must be a non-empty authority-assigned string.")
  end

  if record.observed_at_window ~= nil and not is_string(record.observed_at_window) then
    append(errors, "observed_at_window must be a non-empty aggregation interval.")
  end

  if record.measurement_type ~= nil then
    if not is_string(record.measurement_type) then
      append(errors, "measurement_type must be a non-empty string.")
    elseif not permitted_measurements[record.measurement_type] then
      append(errors, "Measurement type is outside this asset boundary: " .. record.measurement_type)
    end
  end

  if record.measurement_value ~= nil and type(record.measurement_value) ~= "number" then
    append(errors, "measurement_value must be numeric.")
  end

  if record.measurement_unit ~= nil and not is_string(record.measurement_unit) then
    append(errors, "measurement_unit must be a non-empty string.")
  end

  if record.calibration_state ~= nil then
    local valid_calibration = {
      verified = true,
      expired = true,
      uncertain = true,
      failed = true,
    }
    if not valid_calibration[record.calibration_state] then
      append(errors, "calibration_state must be verified, expired, uncertain, or failed.")
    end
  end

  if record.condition ~= nil and not is_string(record.condition) then
    append(errors, "condition must be a non-empty approved condition enum.")
  end

  if record.raw_payload_retained ~= nil and not is_boolean(record.raw_payload_retained) then
    append(errors, "raw_payload_retained must be boolean.")
  end
end

function EvidenceCard.new(card)
  assert(type(card) == "table", "Evidence card must be a table.")

  local copy = {}
  for key, value in pairs(card) do
    copy[key] = value
  end

  copy.artifact_name = copy.artifact_name or "CivicFacilityMaintenanceCellResearchEvidenceCard"
  copy.current_state = copy.current_state or "research_only"
  copy.authorization_status = copy.authorization_status or "none"
  copy.invariants = copy.invariants or {}
  copy.measurements = copy.measurements or {}
  copy.stop_conditions = copy.stop_conditions or {}
  copy.unresolved_evidence = copy.unresolved_evidence or {}
  copy.advancement_evidence = copy.advancement_evidence or {}

  return copy
end

function EvidenceCard.validate(card)
  local errors = {}
  local warnings = {}

  if type(card) ~= "table" then
    return {
      valid = false,
      errors = { "Evidence card must be a table." },
      warnings = warnings,
    }
  end

  if card.artifact_name ~= "CivicFacilityMaintenanceCellResearchEvidenceCard" then
    append(errors, "artifact_name must be CivicFacilityMaintenanceCellResearchEvidenceCard.")
  end

  if not VALID_STATES[card.current_state] then
    append(errors, "current_state is invalid.")
  end

  if card.current_state == "research_only" and card.authorization_status ~= "none" then
    append(errors, "research_only cards must not claim an authorization status other than none.")
  end

  if card.authorization_status == "city_approved" then
    append(errors, "This research-boundary module cannot represent City authorization.")
  end

  if type(card.invariants) ~= "table" then
    append(errors, "invariants must be a table.")
  else
    for _, invariant_name in ipairs(REQUIRED_INVARIANTS) do
      local verdict = normalize_verdict(card.invariants[invariant_name])
      if verdict == nil then
        append(errors, "Missing invariant verdict: " .. invariant_name)
      elseif not VALID_VERDICTS[verdict] then
        append(errors, "Invalid verdict for invariant: " .. invariant_name)
      end
    end
  end

  if type(card.measurements) ~= "table" then
    append(errors, "measurements must be a table.")
  else
    for _, entry in ipairs(card.measurements) do
      if type(entry) ~= "table" then
        append(errors, "Measurement entry must be a table.")
      elseif entry.asset_class == "human_operated_floor_scrubber" then
        validate_measurement(entry.record, PERMITTED_SCRUBBER_MEASUREMENTS, errors)
      elseif entry.asset_class == "fixed_asset_sensor_kit" then
        validate_measurement(entry.record, PERMITTED_SENSOR_MEASUREMENTS, errors)
      else
        append(errors, "Unsupported or excluded asset_class in measurement entry.")
      end
    end
  end

  if type(card.stop_conditions) ~= "table" then
    append(errors, "stop_conditions must be a table.")
  else
    for condition_name, active in pairs(card.stop_conditions) do
      if not STOP_CONDITIONS[condition_name] then
        append(errors, "Unknown stop condition: " .. tostring(condition_name))
      elseif active ~= true and active ~= false then
        append(errors, "Stop condition values must be boolean: " .. condition_name)
      elseif active == true then
        append(warnings, "Active research stop condition: " .. condition_name)
      end
    end
  end

  return {
    valid = #errors == 0,
    errors = errors,
    warnings = warnings,
  }
end

function EvidenceCard.composite_admissibility(card)
  local validation = EvidenceCard.validate(card)
  local failed = {}
  local unresolved = {}

  if not validation.valid then
    return {
      admissible = false,
      composite_score = 0,
      status = "invalid_evidence_card",
      failed_invariants = failed,
      unresolved_invariants = unresolved,
      errors = validation.errors,
      warnings = validation.warnings,
    }
  end

  for _, invariant_name in ipairs(REQUIRED_INVARIANTS) do
    local verdict = normalize_verdict(card.invariants[invariant_name])
    if verdict == "fail" then
      append(failed, invariant_name)
    elseif verdict ~= "pass" then
      append(unresolved, invariant_name)
    end
  end

  local active_stops = {}
  for condition_name, active in pairs(card.stop_conditions) do
    if active then
      append(active_stops, condition_name)
    end
  end

  local admissible =
    card.current_state == "limited_operation"
    and #failed == 0
    and #unresolved == 0
    and #active_stops == 0

  return {
    admissible = admissible,
    composite_score = admissible and 1 or 0,
    status = admissible and "admissible" or "not_admissible",
    failed_invariants = sorted_copy(failed),
    unresolved_invariants = sorted_copy(unresolved),
    active_stop_conditions = sorted_copy(active_stops),
    errors = validation.errors,
    warnings = validation.warnings,
  }
end

function EvidenceCard.score(card)
  local validation = EvidenceCard.validate(card)
  if not validation.valid then
    return {
      knowledge_factor = 0,
      eco_impact_value = 0,
      harm_risk = 1,
      validation = validation,
    }
  end

  local completed_invariants = 0
  local unresolved_invariants = 0
  local failed_invariants = 0

  for _, invariant_name in ipairs(REQUIRED_INVARIANTS) do
    local verdict = normalize_verdict(card.invariants[invariant_name])
    if verdict == "pass" then
      completed_invariants = completed_invariants + 1
    elseif verdict == "fail" then
      failed_invariants = failed_invariants + 1
    else
      unresolved_invariants = unresolved_invariants + 1
    end
  end

  local evidence_count = #card.advancement_evidence
  local unresolved_count = #card.unresolved_evidence
  local measurement_count = #card.measurements

  local evidence_completeness = clamp(evidence_count / 6, 0, 1)
  local invariant_completeness = completed_invariants / #REQUIRED_INVARIANTS
  local uncertainty_penalty = clamp(unresolved_count / 12, 0, 1)

  local knowledge_factor = clamp(
    0.55 * invariant_completeness
      + 0.30 * evidence_completeness
      + 0.15 * clamp(measurement_count / 20, 0, 1)
      - 0.25 * uncertainty_penalty,
    0,
    1
  )

  local ecological_evidence = type(card.ecological_accounting) == "table"
    and card.ecological_accounting.complete == true
    and card.ecological_accounting.approved_material_safety == true
    and card.ecological_accounting.approved_wastewater_path == true
    and card.ecological_accounting.repair_first_plan == true

  local eco_impact_value = ecological_evidence
    and clamp(0.35 + 0.65 * knowledge_factor, 0, 1)
    or 0

  local active_stop_count = 0
  for _, active in pairs(card.stop_conditions) do
    if active then
      active_stop_count = active_stop_count + 1
    end
  end

  local harm_risk = clamp(
    0.20
      + 0.45 * (unresolved_invariants / #REQUIRED_INVARIANTS)
      + 0.25 * (failed_invariants / #REQUIRED_INVARIANTS)
      + 0.10 * clamp(active_stop_count / 4, 0, 1),
    0,
    1
  )

  return {
    knowledge_factor = knowledge_factor,
    eco_impact_value = eco_impact_value,
    harm_risk = harm_risk,
    validation = validation,
  }
end

function EvidenceCard.research_only_card()
  local invariants = {}
  for _, invariant_name in ipairs(REQUIRED_INVARIANTS) do
    invariants[invariant_name] = "insufficient_evidence"
  end

  invariants.least_intrusive_sensing = "policy_requirement"
  invariants.minimal_operational_data = "policy_requirement"
  invariants.functional_equality = "policy_requirement"
  invariants.non_actuating_governance = "policy_requirement"
  invariants.worker_sovereignty = "policy_requirement"

  return EvidenceCard.new({
    artifact_name = "CivicFacilityMaintenanceCellResearchEvidenceCard",
    current_state = "research_only",
    authorization_status = "none",
    invariants = invariants,
    measurements = {},
    stop_conditions = {
      tracking_cannot_be_disabled = false,
      identity_required_for_work_order = false,
      environmental_or_public_health_nonconformance = false,
      worker_representative_refusal = false,
    },
    unresolved_evidence = {
      "Exact scrubber make and model.",
      "Exact fixed-asset sensor make and model.",
      "Specific civic facility and management authority.",
      "Maintenance authority and worker representative review.",
      "Cleaning-material SDS and wastewater discharge pathway.",
      "Vendor firmware data-retention audit.",
    },
    advancement_evidence = {},
    ecological_accounting = {
      complete = false,
      approved_material_safety = false,
      approved_wastewater_path = false,
      repair_first_plan = false,
    },
  })
end

return EvidenceCard
