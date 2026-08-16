local nonnegative_waste_factorization = {}

local NMF = {}
NMF.__index = NMF

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function assert_matrix(matrix, rows, columns, name)
    assert(type(matrix) == "table", name .. " must be a table")
    assert(#matrix == rows, name .. " row count is invalid")

    for row = 1, rows do
        assert(type(matrix[row]) == "table",
            name .. " row " .. row .. " must be a table")
        assert(#matrix[row] == columns,
            name .. " column count is invalid at row " .. row)

        for column = 1, columns do
            assert(finite_number(matrix[row][column])
                and matrix[row][column] >= 0.0,
                name .. " must contain finite non-negative values")
        end
    end
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

local function copy_matrix(source)
    local result = matrix(#source, #source[1], 0.0)

    for row = 1, #source do
        for column = 1, #source[1] do
            result[row][column] = source[row][column]
        end
    end

    return result
end

local function transpose(source)
    local result = matrix(#source[1], #source, 0.0)

    for row = 1, #source do
        for column = 1, #source[1] do
            result[column][row] = source[row][column]
        end
    end

    return result
end

local function multiply(left, right)
    assert(#left[1] == #right, "matrix dimensions are incompatible")

    local result = matrix(#left, #right[1], 0.0)

    for row = 1, #left do
        for column = 1, #right[1] do
            local sum = 0.0

            for index = 1, #right do
                sum = sum + left[row][index] * right[index][column]
            end

            result[row][column] = sum
        end
    end

    return result
end

local function frobenius_squared(source)
    local sum = 0.0

    for row = 1, #source do
        for column = 1, #source[row] do
            sum = sum + source[row][column] * source[row][column]
        end
    end

    return sum
end

local function reconstruction_error_squared(source, left, right)
    local approximation = multiply(left, right)
    local error = 0.0

    for row = 1, #source do
        for column = 1, #source[row] do
            local difference = source[row][column] - approximation[row][column]
            error = error + difference * difference
        end
    end

    return error
end

local function dot(left, right)
    assert(#left == #right, "vectors must have matching dimensions")

    local sum = 0.0

    for index = 1, #left do
        sum = sum + left[index] * right[index]
    end

    return sum
end

local function nonnegative_least_squares(design, target, options)
    local features = #design[1]
    local samples = #design
    local maximum_iterations = options.maximum_inner_iterations or 200
    local tolerance = options.inner_tolerance or 1e-9
    local ridge = options.ridge or 1e-8

    local coefficients = {}
    local residual = {}

    for feature = 1, features do
        coefficients[feature] = 0.0
    end

    for sample = 1, samples do
        residual[sample] = target[sample]
    end

    for iteration = 1, maximum_iterations do
        local maximum_change = 0.0

        for feature = 1, features do
            local numerator = 0.0
            local denominator = ridge

            for sample = 1, samples do
                local feature_value = design[sample][feature]
                numerator = numerator + feature_value * residual[sample]
                denominator = denominator + feature_value * feature_value
            end

            local old_value = coefficients[feature]
            local updated_value = math.max(
                0.0,
                old_value + numerator / denominator
            )
            local delta = updated_value - old_value

            if delta ~= 0.0 then
                coefficients[feature] = updated_value

                for sample = 1, samples do
                    residual[sample] = residual[sample]
                        - design[sample][feature] * delta
                end
            end

            maximum_change = math.max(maximum_change, math.abs(delta))
        end

        if maximum_change <= tolerance then
            break
        end
    end

    return coefficients
end

local function deterministic_seeded_value(row, column, seed)
    local value = (row * 73856093 + column * 19349663 + seed * 83492791) % 1000003
    return 0.05 + (value / 1000003.0)
end

local function initialize_matrix(rows, columns, seed)
    local result = matrix(rows, columns, 0.0)

    for row = 1, rows do
        for column = 1, columns do
            result[row][column] = deterministic_seeded_value(row, column, seed)
        end
    end

    return result
end

function NMF.new(source, rank, options)
    assert(type(source) == "table" and #source > 0,
        "source must be a non-empty matrix")
    assert(type(source[1]) == "table" and #source[1] > 0,
        "source must have at least one column")
    assert(type(rank) == "number" and rank % 1 == 0 and rank > 0,
        "rank must be a positive integer")

    local rows = #source
    local columns = #source[1]
    assert(rank <= math.min(rows, columns),
        "rank must not exceed the smallest source dimension")

    assert_matrix(source, rows, columns, "source")

    options = options or {}

    return setmetatable({
        X = copy_matrix(source),
        rows = rows,
        columns = columns,
        rank = rank,
        options = {
            maximum_iterations = options.maximum_iterations or 300,
            tolerance = options.tolerance or 1e-7,
            maximum_inner_iterations = options.maximum_inner_iterations or 200,
            inner_tolerance = options.inner_tolerance or 1e-9,
            ridge = options.ridge or 1e-8,
        },
        W = initialize_matrix(rows, rank, 17),
        H = initialize_matrix(rank, columns, 31),
        history = {},
    }, NMF)
end

function NMF:reconstruction()
    return multiply(self.W, self.H)
end

function NMF:error_squared()
    return reconstruction_error_squared(self.X, self.W, self.H)
end

function NMF:relative_error()
    local source_energy = frobenius_squared(self.X)

    if source_energy == 0.0 then
        return 0.0
    end

    return math.sqrt(self:error_squared() / source_energy)
end

function NMF:update_h()
    local design = self.W

    for column = 1, self.columns do
        local target = {}

        for row = 1, self.rows do
            target[row] = self.X[row][column]
        end

        local solution = nonnegative_least_squares(design, target, self.options)

        for component = 1, self.rank do
            self.H[component][column] = solution[component]
        end
    end
end

function NMF:update_w()
    local design = transpose(self.H)

    for row = 1, self.rows do
        local target = {}

        for column = 1, self.columns do
            target[column] = self.X[row][column]
        end

        local solution = nonnegative_least_squares(design, target, self.options)

        for component = 1, self.rank do
            self.W[row][component] = solution[component]
        end
    end
end

function NMF:normalize_components()
    for component = 1, self.rank do
        local norm_squared = 0.0

        for row = 1, self.rows do
            norm_squared = norm_squared + self.W[row][component] ^ 2
        end

        local norm = math.sqrt(norm_squared)

        if norm > 1e-12 then
            for row = 1, self.rows do
                self.W[row][component] = self.W[row][component] / norm
            end

            for column = 1, self.columns do
                self.H[component][column] = self.H[component][column] * norm
            end
        end
    end
end

function NMF:fit()
    local previous_error = math.huge

    for iteration = 1, self.options.maximum_iterations do
        self:update_h()
        self:update_w()
        self:normalize_components()

        local error = self:error_squared()
        local relative_improvement = 0.0

        if previous_error < math.huge then
            relative_improvement = (previous_error - error)
                / math.max(previous_error, 1e-12)
        end

        self.history[#self.history + 1] = {
            iteration = iteration,
            error_squared = error,
            relative_error = self:relative_error(),
            relative_improvement = relative_improvement,
        }

        if previous_error < math.huge
            and math.abs(relative_improvement) <= self.options.tolerance
        then
            break
        end

        previous_error = error
    end

    return {
        W = copy_matrix(self.W),
        H = copy_matrix(self.H),
        rank = self.rank,
        iterations = #self.history,
        error_squared = self:error_squared(),
        relative_error = self:relative_error(),
        history = self.history,
    }
end

function nonnegative_waste_factorization.new(source, rank, options)
    return NMF.new(source, rank, options)
end

function nonnegative_waste_factorization.rank_sweep(source, ranks, options)
    assert(type(ranks) == "table" and #ranks > 0,
        "ranks must be a non-empty array")

    local results = {}

    for index = 1, #ranks do
        local model = NMF.new(source, ranks[index], options)
        local fit = model:fit()

        results[#results + 1] = {
            rank = fit.rank,
            relative_error = fit.relative_error,
            error_squared = fit.error_squared,
            iterations = fit.iterations,
        }
    end

    table.sort(results, function(left, right)
        return left.rank < right.rank
    end)

    return results
end

function nonnegative_waste_factorization.select_rank(rank_results, minimum_relative_improvement)
    assert(type(rank_results) == "table" and #rank_results > 0,
        "rank_results must be a non-empty array")
    assert(finite_number(minimum_relative_improvement)
        and minimum_relative_improvement >= 0.0
        and minimum_relative_improvement <= 1.0,
        "minimum_relative_improvement must be within [0, 1]")

    local selected = rank_results[1]

    for index = 2, #rank_results do
        local previous = rank_results[index - 1]
        local current = rank_results[index]

        local improvement = (
            previous.relative_error - current.relative_error
        ) / math.max(previous.relative_error, 1e-12)

        if improvement < minimum_relative_improvement then
            return selected
        end

        selected = current
    end

    return selected
end

return nonnegative_waste_factorization
