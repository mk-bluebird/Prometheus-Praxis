-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: lua/tools/first_wave_property_tests.lua
-- Destination: lua/tools

local foundation = require("lua.tools.first_wave_foundation")

local property_tests = {}

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function approximately_equal(left, right, absolute_tolerance, relative_tolerance)
    assert(finite_number(left), "left must be finite")
    assert(finite_number(right), "right must be finite")

    absolute_tolerance = absolute_tolerance or 1e-12
    relative_tolerance = relative_tolerance or 1e-10

    local difference = math.abs(left - right)
    local scale = math.max(math.abs(left), math.abs(right), 1.0)

    return difference <= absolute_tolerance
        + relative_tolerance * scale
end

local function deterministic_uniform(seed)
    local state = seed or 1

    return function()
        state = (1103515245 * state + 12345) % 2147483648
        return state / 2147483648
    end
end

local function random_between(random_uniform, minimum, maximum)
    return minimum + (maximum - minimum) * random_uniform()
end

local function assert_property(condition, name, details)
    if not condition then
        error(string.format(
            "property failed: %s; details=%s",
            name,
            details or "unspecified"
        ))
    end
end

function property_tests.run_foundation_properties(options)
    options = options or {}

    local iterations = options.iterations or 1000
    local seed = options.seed or 1776

    assert(type(iterations) == "number" and iterations % 1 == 0
        and iterations >= 1,
        "iterations must be a positive integer")
    assert(type(seed) == "number" and seed % 1 == 0,
        "seed must be an integer")

    local random_uniform = deterministic_uniform(seed)
    local checks = 0

    for _ = 1, iterations do
        local a = random_between(random_uniform, -1000.0, 999.0)
        local b = random_between(random_uniform, a + 1e-9, 1000.0)
        local x = random_between(random_uniform, -5000.0, 5000.0)

        local once = foundation.clamp(x, a, b)
        local twice = foundation.clamp(once, a, b)

        assert_property(
            approximately_equal(once, twice),
            "clamp_idempotence",
            string.format("x=%g,a=%g,b=%g,once=%g,twice=%g",
                x, a, b, once, twice)
        )
        checks = checks + 1

        assert_property(
            once >= a and once <= b,
            "clamp_range_preservation",
            string.format("x=%g,a=%g,b=%g,result=%g", x, a, b, once)
        )
        checks = checks + 1

        local y = random_between(random_uniform, -5000.0, 5000.0)
        local monotone_left = foundation.clamp(math.min(x, y), a, b)
        local monotone_right = foundation.clamp(math.max(x, y), a, b)

        assert_property(
            monotone_left <= monotone_right,
            "clamp_monotonicity",
            string.format("x=%g,y=%g,a=%g,b=%g", x, y, a, b)
        )
        checks = checks + 1

        local weights = {
            random_between(random_uniform, 0.001, 10.0),
            random_between(random_uniform, 0.001, 10.0),
            random_between(random_uniform, 0.001, 10.0),
        }

        local normalized = foundation.normalize_weights(weights)
        local normalized_sum = foundation.sum(normalized)

        assert_property(
            approximately_equal(normalized_sum, 1.0),
            "normalized_weights_sum_to_one",
            string.format("sum=%g", normalized_sum)
        )
        checks = checks + 1

        for index = 1, #normalized do
            assert_property(
                normalized[index] >= 0.0,
                "normalized_weights_nonnegative",
                string.format("index=%d,value=%g", index, normalized[index])
            )
            checks = checks + 1
        end

        local values = {
            random_between(random_uniform, -100.0, 100.0),
            random_between(random_uniform, -100.0, 100.0),
            random_between(random_uniform, -100.0, 100.0),
        }

        local weighted_mean = foundation.weighted_mean(values, weights)
        local lower = math.min(values[1], values[2], values[3])
        local upper = math.max(values[1], values[2], values[3])

        assert_property(
            weighted_mean >= lower - 1e-12 and weighted_mean <= upper + 1e-12,
            "weighted_mean_convex_hull",
            string.format("mean=%g,lower=%g,upper=%g",
                weighted_mean, lower, upper)
        )
        checks = checks + 1

        local initial_mass = random_between(random_uniform, 0.001, 1000.0)
        local rate = random_between(random_uniform, 0.0, 1.0)
        local time_1 = random_between(random_uniform, 0.0, 100.0)
        local time_2 = random_between(random_uniform, time_1, 200.0)

        local mass_1 = foundation.decomposition_remaining(initial_mass, rate, time_1)
        local mass_2 = foundation.decomposition_remaining(initial_mass, rate, time_2)

        assert_property(
            mass_2 <= mass_1 + 1e-12,
            "decomposition_monotonic_decay",
            string.format("m1=%g,m2=%g,t1=%g,t2=%g", mass_1, mass_2, time_1, time_2)
        )
        checks = checks + 1

        local fraction = foundation.decomposition_fraction(initial_mass, rate, time_2)

        assert_property(
            fraction >= 0.0 and fraction <= 1.0,
            "decomposition_fraction_bounded",
            string.format("fraction=%g", fraction)
        )
        checks = checks + 1
    end

    return {
        passed = true,
        seed = seed,
        iterations = iterations,
        checks = checks,
    }
end

function property_tests.run_invalid_input_properties()
    local checks = 0

    local ok = pcall(function()
        foundation.clamp(1.0, 2.0, 1.0)
    end)

    assert_property(not ok, "clamp_rejects_reversed_bounds")
    checks = checks + 1

    ok = pcall(function()
        foundation.weighted_mean({ 1.0 }, { 0.0 })
    end)

    assert_property(not ok, "weighted_mean_rejects_zero_total_weight")
    checks = checks + 1

    ok = pcall(function()
        foundation.decomposition_remaining(-1.0, 0.1, 1.0)
    end)

    assert_property(not ok, "decomposition_rejects_negative_mass")
    checks = checks + 1

    return {
        passed = true,
        checks = checks,
    }
end

function property_tests.run_all(options)
    local valid = property_tests.run_foundation_properties(options)
    local invalid = property_tests.run_invalid_input_properties()

    return {
        passed = valid.passed and invalid.passed,
        seed = valid.seed,
        iterations = valid.iterations,
        checks = valid.checks + invalid.checks,
    }
end

return property_tests
