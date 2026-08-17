local function numeric_argument(index, name)
    local value = tonumber(arg[index])
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, name .. " must be finite numeric")
    assert(value >= 0.0, name .. " must be non-negative")
    return value
end

local function classify(
    oil_mg_l,
    tss_mg_l,
    turbidity_ntu,
    oil_threshold_mg_l,
    tss_threshold_mg_l,
    turbidity_threshold_ntu
)
    assert(oil_threshold_mg_l > 0.0, "oil threshold must be positive")
    assert(tss_threshold_mg_l > 0.0, "TSS threshold must be positive")
    assert(turbidity_threshold_ntu > 0.0, "turbidity threshold must be positive")

    local fog = oil_mg_l > oil_threshold_mg_l
        and tss_mg_l > tss_threshold_mg_l
        and turbidity_ntu > turbidity_threshold_ntu

    if fog then
        return {
            p_fog = 1,
            media_state = "UNMODELED_MIXED_OIL_WATER_SEDIMENT",
            route = "MANUAL_FIELD_REVIEW",
            knowledge_factor = 0.45,
            eco_impact_value = 0.35,
            harm_risk = 0.75
        }
    end

    return {
        p_fog = 0,
        media_state = "FIELD_SCREENED_NOT_ALL_FOG_CONDITIONS_MET",
        route = "RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW",
        knowledge_factor = 0.60,
        eco_impact_value = 0.50,
        harm_risk = 0.35
    }
end

if #arg ~= 6 then
    io.stderr:write(
        "usage: lua fog_router.lua <oil_mg_L> <TSS_mg_L> <turbidity_NTU> " ..
        "<oil_threshold_mg_L> <TSS_threshold_mg_L> <turbidity_threshold_NTU>\n"
    )
    os.exit(64)
end

local ok, result = pcall(
    classify,
    numeric_argument(1, "oil_mg_L"),
    numeric_argument(2, "TSS_mg_L"),
    numeric_argument(3, "turbidity_NTU"),
    numeric_argument(4, "oil_threshold_mg_L"),
    numeric_argument(5, "TSS_threshold_mg_L"),
    numeric_argument(6, "turbidity_threshold_NTU")
)

if not ok then
    io.stderr:write("error: " .. result .. "\n")
    os.exit(65)
end

print("P_fog=" .. result.p_fog)
print("media_state=" .. result.media_state)
print("route=" .. result.route)
print(string.format("knowledge_factor=%.2f", result.knowledge_factor))
print(string.format("eco_impact_value=%.2f", result.eco_impact_value))
print(string.format("harm_risk=%.2f", result.harm_risk))
