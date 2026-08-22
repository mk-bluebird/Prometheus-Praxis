local PfasSorptionExperiment = {}

local function finite_number(value, name)
    assert(type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge,
        name .. " must be finite")
    return value
end

local function bounded(value, name, minimum, maximum)
    finite_number(value, name)
    assert(value >= minimum and value <= maximum,
        name .. " is outside its allowed range")
    return value
end

local function positive(value, name)
    finite_number(value, name)
    assert(value > 0.0, name .. " must be positive")
    return value
end

local function nonempty(value, name)
    assert(type(value) == "string" and value:match("%S"),
        name .. " must be a non-empty string")
    return value
end

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end
    if value > 1.0 then
        return 1.0
    end
    return value
end

local function validate_design(design)
    assert(type(design) == "table", "design must be a table")
    nonempty(design.site_id, "site_id")
    nonempty(design.pfas_analyte, "pfas_analyte")
    positive(design.bulk_density_kg_m3, "bulk_density_kg_m3")
    bounded(design.volumetric_water_content, "volumetric_water_content", 0.0001, 1.0)
    positive(design.kd_min_l_kg, "kd_min_l_kg")
    positive(design.kd_max_l_kg, "kd_max_l_kg")
    assert(design.kd_max_l_kg > design.kd_min_l_kg,
        "kd_max_l_kg must exceed kd_min_l_kg")

    positive(design.sediment_replicates, "sediment_replicates")
    assert(design.sediment_replicates == math.floor(design.sediment_replicates),
        "sediment_replicates must be an integer")

    positive(design.concentration_levels, "concentration_levels")
    assert(design.concentration_levels == math.floor(design.concentration_levels),
        "concentration_levels must be an integer")

    positive(design.equilibration_hours, "equilibration_hours")
    bounded(design.method_detection_limit_ng_l, "method_detection_limit_ng_l", 0.0, math.huge)
    bounded(design.target_lowest_concentration_ng_l, "target_lowest_concentration_ng_l", 0.0, math.huge)
    bounded(design.matrix_spike_recovery_fraction, "matrix_spike_recovery_fraction", 0.0, 1.0)
    bounded(design.field_blank_contamination_ng_l, "field_blank_contamination_ng_l", 0.0, math.huge)
    bounded(design.allowed_blank_fraction_of_lowest_level,
        "allowed_blank_fraction_of_lowest_level", 0.0, 1.0)
    bounded(design.wildlife_exposure_risk, "wildlife_exposure_risk", 0.0, 1.0)
    bounded(design.containment_verified, "containment_verified", 0.0, 1.0)
    bounded(design.chain_of_custody_verified, "chain_of_custody_verified", 0.0, 1.0)
end

function PfasSorptionExperiment.retardation_factor(bulk_density_kg_m3, kd_l_kg, volumetric_water_content)
    positive(bulk_density_kg_m3, "bulk_density_kg_m3")
    positive(kd_l_kg, "kd_l_kg")
    bounded(volumetric_water_content, "volumetric_water_content", 0.0001, 1.0)

    local density_kg_l = bulk_density_kg_m3 / 1000.0
    return 1.0 + (density_kg_l * kd_l_kg) / volumetric_water_content
end

function PfasSorptionExperiment.evaluate(design)
    validate_design(design)

    local rf_min = PfasSorptionExperiment.retardation_factor(
        design.bulk_density_kg_m3,
        design.kd_min_l_kg,
        design.volumetric_water_content
    )
    local rf_max = PfasSorptionExperiment.retardation_factor(
        design.bulk_density_kg_m3,
        design.kd_max_l_kg,
        design.volumetric_water_content
    )

    local detection_limit_ok =
        design.method_detection_limit_ng_l <= design.target_lowest_concentration_ng_l / 3.0

    local blank_limit_ng_l =
        design.target_lowest_concentration_ng_l
        * design.allowed_blank_fraction_of_lowest_level

    local blanks_ok = design.field_blank_contamination_ng_l <= blank_limit_ng_l
    local recovery_ok =
        design.matrix_spike_recovery_fraction >= 0.70
        and design.matrix_spike_recovery_fraction <= 1.30

    local design_span = math.log(design.kd_max_l_kg / design.kd_min_l_kg)
    local replication_score = clamp01(design.sediment_replicates / 5.0)
    local concentration_score = clamp01(design.concentration_levels / 5.0)
    local span_score = clamp01(design_span / math.log(100.0))
    local evidence_score = clamp01(
        0.30 * replication_score
        + 0.25 * concentration_score
        + 0.25 * span_score
        + 0.20 * design.matrix_spike_recovery_fraction
    )

    local containment_ok =
        design.containment_verified == 1.0
        and design.chain_of_custody_verified == 1.0
        and design.wildlife_exposure_risk == 0.0

    local eligible =
        detection_limit_ok
        and blanks_ok
        and recovery_ok
        and containment_ok
        and design.equilibration_hours > 0.0

    local reasons = {}
    if not detection_limit_ok then
        reasons[#reasons + 1] = "detection_limit_not_low_enough"
    end
    if not blanks_ok then
        reasons[#reasons + 1] = "field_blank_contamination_exceeds_limit"
    end
    if not recovery_ok then
        reasons[#reasons + 1] = "matrix_spike_recovery_outside_acceptance_range"
    end
    if design.containment_verified ~= 1.0 then
        reasons[#reasons + 1] = "containment_not_verified"
    end
    if design.chain_of_custody_verified ~= 1.0 then
        reasons[#reasons + 1] = "chain_of_custody_not_verified"
    end
    if design.wildlife_exposure_risk ~= 0.0 then
        reasons[#reasons + 1] = "wildlife_exposure_risk_not_zero"
    end

    return {
        site_id = design.site_id,
        pfas_analyte = design.pfas_analyte,
        retardation_factor_min = rf_min,
        retardation_factor_max = rf_max,
        blank_limit_ng_l = blank_limit_ng_l,
        knowledge_factor = evidence_score,
        eco_impact_value = eligible and 0.80 or 0.0,
        harm_risk = eligible and 0.0 or 1.0,
        verdict = eligible and "eligible_for_controlled_lab_review" or "blocked",
        blocked_reasons = reasons
    }
end

return PfasSorptionExperiment
