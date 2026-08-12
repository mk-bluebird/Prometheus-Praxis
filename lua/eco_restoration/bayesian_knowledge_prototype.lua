-- File: lua/eco_restoration/bayesian_knowledge_prototype.lua

local function update(posterior, confidence, reliability)
    local positive = reliability * confidence + (1 - reliability) * (1 - confidence)
    local negative = reliability * (1 - confidence) + (1 - reliability) * confidence
    return posterior * positive / (posterior * positive + (1 - posterior) * negative)
end

local function posterior(prior, ai_confidence, evidence)
    local value = update(prior, ai_confidence, 0.90)
    for _, item in ipairs(evidence) do
        value = update(value, item.confidence, item.reliability)
    end
    return value
end

return { posterior = posterior }
