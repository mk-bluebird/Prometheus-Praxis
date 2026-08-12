-- File: lua/eco_restoration/wbgt_lane_router.lua

local function route(current_wbgt_c, predicted_ai_increment_c, derate_wbgt_c, maximum_wbgt_c)
    if type(current_wbgt_c) ~= "number" or type(predicted_ai_increment_c) ~= "number" or
       type(derate_wbgt_c) ~= "number" or type(maximum_wbgt_c) ~= "number" or
       predicted_ai_increment_c < 0 or derate_wbgt_c > maximum_wbgt_c then
        return "operator_review_queue"
    end

    local projected = current_wbgt_c + predicted_ai_increment_c
    if projected >= maximum_wbgt_c then
        return "operator_review_queue"
    end
    if projected >= derate_wbgt_c then
        return "reduced_resource_queue"
    end
    return "low_impact_queue"
end

return { route = route }
