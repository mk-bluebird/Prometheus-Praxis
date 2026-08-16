-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: lua/tools/spatial_matrix_benchmark.lua
-- Destination: lua/tools

local spatial_matrix_benchmark = {}

local Benchmark = {}
Benchmark.__index = Benchmark

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function matrix(rows, columns, value)
    local result = {}

    for row = 1, rows do
        result[row] = {}

        for column = 1, columns do
            result[row][column] = value or 0.0
        end
    end

    return result
end

local function deterministic_value(row, column, seed)
    local value = (row * 73856093 + column * 19349663 + seed * 83492791)
        % 1000003

    return value / 1000003.0
end

local function build_matrix(size, seed)
    local result = matrix(size, size, 0.0)

    for row = 1, size do
        for column = 1, size do
            result[row][column] = deterministic_value(row, column, seed)
        end
    end

    return result
end

local function build_vector(size, seed)
    local result = {}

    for index = 1, size do
        result[index] = deterministic_value(index, 1, seed)
    end

    return result
end

local function pure_lua_matrix_vector(matrix_value, vector)
    local size = #matrix_value
    local result = {}

    for row = 1, size do
        local sum = 0.0

        for column = 1, size do
            sum = sum + matrix_value[row][column] * vector[column]
        end

        result[row] = sum
    end

    return result
end

local function pure_lua_laplacian_vector(adjacency, vector)
    local size = #adjacency
    local result = {}

    for row = 1, size do
        local degree = 0.0
        local weighted_neighbors = 0.0

        for column = 1, size do
            local weight = adjacency[row][column]
            degree = degree + weight
            weighted_neighbors = weighted_neighbors + weight * vector[column]
        end

        result[row] = degree * vector[row] - weighted_neighbors
    end

    return result
end

local function monotonic_clock()
    return os.clock()
end

local function checksum(vector)
    local sum = 0.0

    for index = 1, #vector do
        sum = sum + vector[index]
    end

    return sum
end

function Benchmark.new(options)
    options = options or {}

    local repeats = options.repeats or 8
    local ffi_backend = options.ffi_backend

    assert(type(repeats) == "number" and repeats % 1 == 0 and repeats >= 1,
        "repeats must be a positive integer")

    if ffi_backend ~= nil then
        assert(type(ffi_backend) == "table",
            "ffi_backend must be a table when supplied")
        assert(type(ffi_backend.matrix_vector) == "function",
            "ffi_backend.matrix_vector must be a function")
        assert(type(ffi_backend.laplacian_vector) == "function",
            "ffi_backend.laplacian_vector must be a function")
    end

    return setmetatable({
        repeats = repeats,
        ffi_backend = ffi_backend,
    }, Benchmark)
end

function Benchmark:time_operation(callback)
    assert(type(callback) == "function", "callback must be a function")

    local started = monotonic_clock()
    local result = callback()
    local elapsed_seconds = monotonic_clock() - started

    return {
        elapsed_seconds = elapsed_seconds,
        result = result,
    }
end

function Benchmark:benchmark_size(size)
    assert(type(size) == "number" and size % 1 == 0 and size >= 2,
        "size must be an integer of at least two")

    local matrix_value = build_matrix(size, 17)
    local vector = build_vector(size, 31)
    local pure_matrix_seconds = 0.0
    local pure_laplacian_seconds = 0.0
    local pure_matrix_checksum = 0.0
    local pure_laplacian_checksum = 0.0

    for _ = 1, self.repeats do
        local matrix_result = self:time_operation(function()
            return pure_lua_matrix_vector(matrix_value, vector)
        end)

        pure_matrix_seconds = pure_matrix_seconds + matrix_result.elapsed_seconds
        pure_matrix_checksum = pure_matrix_checksum + checksum(matrix_result.result)

        local laplacian_result = self:time_operation(function()
            return pure_lua_laplacian_vector(matrix_value, vector)
        end)

        pure_laplacian_seconds = pure_laplacian_seconds
            + laplacian_result.elapsed_seconds
        pure_laplacian_checksum = pure_laplacian_checksum
            + checksum(laplacian_result.result)
    end

    local result = {
        matrix_size = size,
        repeats = self.repeats,
        pure_lua = {
            matrix_vector_seconds = pure_matrix_seconds / self.repeats,
            laplacian_vector_seconds = pure_laplacian_seconds / self.repeats,
            matrix_vector_checksum = pure_matrix_checksum / self.repeats,
            laplacian_vector_checksum = pure_laplacian_checksum / self.repeats,
        },
    }

    if self.ffi_backend then
        local ffi_matrix_seconds = 0.0
        local ffi_laplacian_seconds = 0.0
        local ffi_matrix_checksum = 0.0
        local ffi_laplacian_checksum = 0.0

        for _ = 1, self.repeats do
            local matrix_result = self:time_operation(function()
                return self.ffi_backend.matrix_vector(matrix_value, vector)
            end)

            ffi_matrix_seconds = ffi_matrix_seconds + matrix_result.elapsed_seconds
            ffi_matrix_checksum = ffi_matrix_checksum + checksum(matrix_result.result)

            local laplacian_result = self:time_operation(function()
                return self.ffi_backend.laplacian_vector(matrix_value, vector)
            end)

            ffi_laplacian_seconds = ffi_laplacian_seconds
                + laplacian_result.elapsed_seconds
            ffi_laplacian_checksum = ffi_laplacian_checksum
                + checksum(laplacian_result.result)
        end

        result.ffi = {
            matrix_vector_seconds = ffi_matrix_seconds / self.repeats,
            laplacian_vector_seconds = ffi_laplacian_seconds / self.repeats,
            matrix_vector_checksum = ffi_matrix_checksum / self.repeats,
            laplacian_vector_checksum = ffi_laplacian_checksum / self.repeats,
        }

        result.speedup = {
            matrix_vector = result.pure_lua.matrix_vector_seconds
                / math.max(result.ffi.matrix_vector_seconds, 1e-12),
            laplacian_vector = result.pure_lua.laplacian_vector_seconds
                / math.max(result.ffi.laplacian_vector_seconds, 1e-12),
        }
    end

    return result
end

function Benchmark:run(sizes)
    assert(type(sizes) == "table" and #sizes > 0,
        "sizes must be a non-empty array")

    local results = {}

    for index = 1, #sizes do
        results[index] = self:benchmark_size(sizes[index])
    end

    return results
end

function Benchmark:recommend_threshold(results, frame_budget_ms, minimum_speedup)
    assert(type(results) == "table" and #results > 0,
        "results must be a non-empty array")

    frame_budget_ms = frame_budget_ms or 16.0
    minimum_speedup = minimum_speedup or 2.0

    assert(finite_number(frame_budget_ms) and frame_budget_ms > 0.0,
        "frame_budget_ms must be positive")
    assert(finite_number(minimum_speedup) and minimum_speedup >= 1.0,
        "minimum_speedup must be at least one")

    local threshold = nil

    for index = 1, #results do
        local result = results[index]
        local pure_worst_ms = math.max(
            result.pure_lua.matrix_vector_seconds,
            result.pure_lua.laplacian_vector_seconds
        ) * 1000.0

        local ffi_helpful = result.speedup
            and math.max(
                result.speedup.matrix_vector,
                result.speedup.laplacian_vector
            ) >= minimum_speedup

        if pure_worst_ms > frame_budget_ms and ffi_helpful then
            threshold = result
            break
        end
    end

    return {
        recommended_threshold = threshold,
        frame_budget_ms = frame_budget_ms,
        minimum_speedup = minimum_speedup,
        interpretation = "Use measured local timings. FFI is warranted only when a known native backend exceeds the selected latency budget and retains numerical agreement with the Lua reference.",
    }
end

function Benchmark:metrics(
    benchmark_coverage,
    numerical_agreement,
    expected_energy_efficiency,
    unsafe_native_boundary_risk
)
    assert(finite_number(benchmark_coverage)
        and benchmark_coverage >= 0.0
        and benchmark_coverage <= 1.0,
        "benchmark_coverage must be within [0, 1]")
    assert(finite_number(numerical_agreement)
        and numerical_agreement >= 0.0
        and numerical_agreement <= 1.0,
        "numerical_agreement must be within [0, 1]")
    assert(finite_number(expected_energy_efficiency)
        and expected_energy_efficiency >= 0.0
        and expected_energy_efficiency <= 1.0,
        "expected_energy_efficiency must be within [0, 1]")
    assert(finite_number(unsafe_native_boundary_risk)
        and unsafe_native_boundary_risk >= 0.0
        and unsafe_native_boundary_risk <= 1.0,
        "unsafe_native_boundary_risk must be within [0, 1]")

    local knowledge_factor = math.sqrt(
        benchmark_coverage * numerical_agreement
    )

    return {
        knowledge_factor = knowledge_factor,
        eco_impact_value = knowledge_factor
            * expected_energy_efficiency
            * (1.0 - unsafe_native_boundary_risk),
        harm_risk = unsafe_native_boundary_risk,
    }
end

function spatial_matrix_benchmark.new(options)
    return Benchmark.new(options)
end

return spatial_matrix_benchmark
