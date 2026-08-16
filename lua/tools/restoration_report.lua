local foundation = require("lua.tools.first_wave_foundation")

local report = {}

local function csv_escape(value)
    local text = tostring(value or "")
    if text:find('[,"\n\r]') then
        return '"' .. text:gsub('"', '""') .. '"'
    end
    return text
end

function report.validate_observation(observation)
    assert(type(observation) == "table", "observation must be a table")

    local project_id, id_err = foundation.require_string(observation, "project_id", 1, 120)
    if not project_id then
        return nil, id_err
    end

    local input_mass, input_err = foundation.require_number(
        observation,
        "restoration_input_kg",
        0.0
    )
    if not input_mass then
        return nil, input_err
    end

    local diverted_mass, diverted_err = foundation.require_number(
        observation,
        "waste_diverted_kg",
        0.0,
        input_mass
    )
    if not diverted_mass then
        return nil, diverted_err
    end

    local habitat_gain, habitat_err = foundation.require_number(
        observation,
        "habitat_gain_index",
        0.0,
        1.0
    )
    if not habitat_gain then
        return nil, habitat_err
    end

    local evidence_quality, evidence_err = foundation.require_number(
        observation,
        "evidence_quality",
        0.0,
        1.0
    )
    if not evidence_quality then
        return nil, evidence_err
    end

    local uncertainty, uncertainty_err = foundation.require_number(
        observation,
        "uncertainty",
        0.0,
        1.0
    )
    if not uncertainty then
        return nil, uncertainty_err
    end

    return {
        project_id = project_id,
        restoration_input_kg = input_mass,
        waste_diverted_kg = diverted_mass,
        habitat_gain_index = habitat_gain,
        evidence_quality = evidence_quality,
        uncertainty = uncertainty,
    }
end

function report.assess_observation(observation)
    local valid, err = report.validate_observation(observation)
    assert(valid, err)

    local diversion_rate = foundation.waste_diversion_rate(
        valid.waste_diverted_kg,
        valid.restoration_input_kg
    )

    local metrics = foundation.project_metrics({
        completeness = 1.0,
        evidence_quality = valid.evidence_quality,
        ecological_benefit = foundation.weighted_mean(
            { diversion_rate, valid.habitat_gain_index },
            { 0.55, 0.45 }
        ),
        toxicity_risk = 0.0,
        habitat_risk = 1.0 - valid.habitat_gain_index,
        water_risk = 0.0,
        uncertainty = valid.uncertainty,
    })

    return {
        project_id = valid.project_id,
        restoration_input_kg = valid.restoration_input_kg,
        waste_diverted_kg = valid.waste_diverted_kg,
        waste_diversion_rate = foundation.round(diversion_rate, 6),
        habitat_gain_index = valid.habitat_gain_index,
        knowledge_factor = metrics.knowledge_factor,
        eco_impact_value = metrics.eco_impact_value,
        harm_risk = metrics.harm_risk,
    }
end

function report.write_csv(path, assessed_observations)
    assert(type(path) == "string" and path ~= "", "path must be a non-empty string")
    assert(type(assessed_observations) == "table", "assessed_observations must be a table")

    local file = assert(io.open(path, "w"))
    file:write(
        "project_id,restoration_input_kg,waste_diverted_kg,waste_diversion_rate,"
        .. "habitat_gain_index,knowledge_factor,eco_impact_value,harm_risk\n"
    )

    for index = 1, #assessed_observations do
        local row = assessed_observations[index]
        assert(type(row) == "table", "each assessed observation must be a table")

        file:write(table.concat({
            csv_escape(row.project_id),
            csv_escape(row.restoration_input_kg),
            csv_escape(row.waste_diverted_kg),
            csv_escape(row.waste_diversion_rate),
            csv_escape(row.habitat_gain_index),
            csv_escape(row.knowledge_factor),
            csv_escape(row.eco_impact_value),
            csv_escape(row.harm_risk),
        }, ","))
        file:write("\n")
    end

    file:close()
end

return report
