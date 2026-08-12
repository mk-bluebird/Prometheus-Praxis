-- File: lua/eco_restoration/lane_threshold_alert_router.lua
local M = {}

function M.route(row)
    assert(type(row) == "table" and type(row.status) == "string",
           "alert row with status is required")
    local k = tonumber(row.mean_knowledge_factor)
    local e = tonumber(row.mean_eco_impact_value)
    local r = tonumber(row.mean_risk)
    assert(k and e and r, "numeric K/E/R alert values are required")

    local severity = (k < 0.40 or e < 0.40 or r > 0.60) and "CRITICAL" or "REVIEW"
    return {
        alert_id = row.alert_id,
        threshold_set_id = row.threshold_set_id,
        queue = severity == "CRITICAL" and "operator_review_queue" or "calibration_queue",
        severity = severity,
        message = string.format(
            "Threshold set %s requires human validation: K=%.3f E=%.3f R=%.3f",
            row.threshold_set_id, k, e, r
        )
    }
end

return M
