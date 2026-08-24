local EvidenceCard = require("eco_restoration.civic_facility_maintenance_evidence_card")

local function render_list(label, values)
  if #values == 0 then
    io.write(label .. ": none\n")
    return
  end

  io.write(label .. ":\n")
  for _, value in ipairs(values) do
    io.write("  - " .. value .. "\n")
  end
end

local card = EvidenceCard.research_only_card()
local validation = EvidenceCard.validate(card)
local admission = EvidenceCard.composite_admissibility(card)
local scores = EvidenceCard.score(card)

io.write("artifact_name: " .. card.artifact_name .. "\n")
io.write("current_state: " .. card.current_state .. "\n")
io.write("authorization_status: " .. card.authorization_status .. "\n")
io.write("schema_valid: " .. tostring(validation.valid) .. "\n")
io.write("admissible: " .. tostring(admission.admissible) .. "\n")
io.write("composite_score: " .. string.format("%.2f", admission.composite_score) .. "\n")
io.write("knowledge_factor: " .. string.format("%.2f", scores.knowledge_factor) .. "\n")
io.write("eco_impact_value: " .. string.format("%.2f", scores.eco_impact_value) .. "\n")
io.write("harm_risk: " .. string.format("%.2f", scores.harm_risk) .. "\n")

render_list("validation_errors", validation.errors)
render_list("validation_warnings", validation.warnings)
render_list("failed_invariants", admission.failed_invariants)
render_list("unresolved_invariants", admission.unresolved_invariants)
render_list("active_stop_conditions", admission.active_stop_conditions)
