local foundation = require("lua.tools.first_wave_foundation")

local material_assessment = {}

local function required_score(material, key)
    local value, err = foundation.require_number(material, key, 0.0, 1.0)
    assert(value, err)
    return value
end

function material_assessment.assess(material)
    assert(type(material) == "table", "material must be a table")

    local name, name_err = foundation.require_string(material, "name", 1, 160)
    assert(name, name_err)

    local biodegradability = required_score(material, "biodegradability")
    local recyclability = required_score(material, "recyclability")
    local toxicity_risk = required_score(material, "toxicity_risk")
    local durability = required_score(material, "durability")
    local water_footprint_risk = required_score(material, "water_footprint_risk")
    local energy_footprint_risk = required_score(material, "energy_footprint_risk")
    local habitat_risk = required_score(material, "habitat_risk")
    local evidence_quality = required_score(material, "evidence_quality")

    local ecological_benefit = foundation.weighted_mean(
        {
            biodegradability,
            recyclability,
            1.0 - toxicity_risk,
            1.0 - water_footprint_risk,
            1.0 - energy_footprint_risk,
            1.0 - habitat_risk,
        },
        { 0.24, 0.20, 0.22, 0.12, 0.10, 0.12 }
    )

    local metrics = foundation.project_metrics({
        completeness = 1.0,
        evidence_quality = evidence_quality,
        ecological_benefit = ecological_benefit,
        toxicity_risk = toxicity_risk,
        habitat_risk = habitat_risk,
        water_risk = water_footprint_risk,
        uncertainty = 1.0 - evidence_quality,
    })

    local recommendation
    if metrics.harm_risk <= 0.20 and metrics.eco_impact_value >= 0.70 then
        recommendation = "preferred_for_pilot"
    elseif metrics.harm_risk <= 0.40 and metrics.knowledge_factor >= 0.55 then
        recommendation = "eligible_for_review"
    else
        recommendation = "collect_more_evidence"
    end

    return {
        name = name,
        biodegradability = biodegradability,
        recyclability = recyclability,
        durability = durability,
        ecological_benefit = foundation.round(ecological_benefit, 6),
        recommendation = recommendation,
        knowledge_factor = metrics.knowledge_factor,
        eco_impact_value = metrics.eco_impact_value,
        harm_risk = metrics.harm_risk,
    }
end

return material_assessment
