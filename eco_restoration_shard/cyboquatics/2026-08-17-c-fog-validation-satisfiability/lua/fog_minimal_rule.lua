local function finite_nonnegative(value, name)
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, name .. " must be finite numeric")
    assert(value >= 0.0, name .. " must be non-negative")
    return value
end

local function parse_criteria(arguments)
    assert(#arguments >= 2 and #arguments % 2 == 0,
        "provide paired <value> <threshold> arguments for one or more criteria")

    local criteria = {}
    for index = 1, #arguments, 2 do
        local value = finite_nonnegative(tonumber(arguments[index]), "criterion value")
        local threshold = finite_nonnegative(tonumber(arguments[index + 1]), "criterion threshold")
        assert(threshold > 0.0, "criterion threshold must be positive")

        table.insert(criteria, {
            position = (index + 1) / 2,
            value = value,
            threshold = threshold,
            passed = value >= threshold
        })
    end
    return criteria
end

local function evaluate_conjunction(criteria)
    local failed = {}
    local minimum_ratio = math.huge

    for _, criterion in ipairs(criteria) do
        local ratio = criterion.value / criterion.threshold
        if ratio < minimum_ratio then
            minimum_ratio = ratio
        end
        if not criterion.passed then
            table.insert(failed, criterion)
        end
    end

    return {
        p_fog = #failed == 0 and 1 or 0,
        failed = failed,
        safety_margin = minimum_ratio - 1.0
    }
end

local ok, result_or_error = pcall(function()
    local criteria = parse_criteria(arg)
    return evaluate_conjunction(criteria)
end)

if not ok then
    io.stderr:write("error: " .. result_or_error .. "\n")
    os.exit(65)
end

local result = result_or_error
print("P_fog=" .. result.p_fog)
print(string.format("minimum_normalized_margin=%.8f", result.safety_margin))

if result.p_fog == 1 then
    print("media_state=UNMODELED_MEDIA_THRESHOLD_CONJUNCTION_MET")
    print("routing_action=MANUAL_FIELD_REVIEW")
else
    print("media_state=NOT_ALL_REQUIRED_UNMODELED_MEDIA_THRESHOLDS_MET")
    print("routing_action=RETAIN_FOR_SAMPLING_AND_CONTEXT_REVIEW")
    for _, failed in ipairs(result.failed) do
        print(string.format(
            "failed_criterion_%d=value=%.8f threshold=%.8f",
            failed.position,
            failed.value,
            failed.threshold
        ))
    end
end
