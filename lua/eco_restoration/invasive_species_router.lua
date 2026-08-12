-- File: lua/eco_restoration/invasive_species_router.lua

local function route(invasive_probability, review_threshold)
    if type(invasive_probability) ~= "number" or type(review_threshold) ~= "number" then
        return "operator_review_queue"
    end
    if invasive_probability >= review_threshold then
        return "operator_review_queue"
    end
    return "biodiversity_monitoring_queue"
end

return { route = route }
