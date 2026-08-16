local lidar_canopy_quadtree = {}

local Quadtree = {}
Quadtree.__index = Quadtree

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end
    if value > maximum then
        return maximum
    end
    return value
end

local function copy_array(source)
    local result = {}

    for index = 1, #source do
        result[index] = source[index]
    end

    return result
end

local function validate_grid(grid)
    assert(type(grid) == "table" and #grid > 0,
        "canopy-height grid must be a non-empty table")

    local width = nil

    for row = 1, #grid do
        assert(type(grid[row]) == "table" and #grid[row] > 0,
            "each canopy-height grid row must be non-empty")

        if width == nil then
            width = #grid[row]
        end

        assert(#grid[row] == width, "canopy-height grid must be rectangular")

        for column = 1, width do
            assert(finite_number(grid[row][column]) and grid[row][column] >= 0.0,
                "canopy-height values must be finite and non-negative")
        end
    end

    return #grid, width
end

local function power_of_two_at_least(value)
    local power = 1

    while power < value do
        power = power * 2
    end

    return power
end

local function log_linear_slope(points)
    assert(type(points) == "table" and #points >= 2,
        "at least two positive box-counting points are required")

    local sum_x = 0.0
    local sum_y = 0.0
    local sum_xx = 0.0
    local sum_xy = 0.0

    for index = 1, #points do
        local point = points[index]
        assert(point.box_size_m > 0.0 and point.box_count > 0,
            "box-counting points must be positive")

        local x = math.log(1.0 / point.box_size_m)
        local y = math.log(point.box_count)

        sum_x = sum_x + x
        sum_y = sum_y + y
        sum_xx = sum_xx + x * x
        sum_xy = sum_xy + x * y
    end

    local count = #points
    local denominator = count * sum_xx - sum_x * sum_x

    assert(math.abs(denominator) > 1e-12,
        "box scales must not all be identical")

    return (count * sum_xy - sum_x * sum_y) / denominator
end

function Quadtree.new(height_grid, cell_size_m)
    local height, width = validate_grid(height_grid)

    assert(finite_number(cell_size_m) and cell_size_m > 0.0,
        "cell_size_m must be positive")

    local side = power_of_two_at_least(math.max(height, width))
    local padded = {}

    for row = 1, side do
        padded[row] = {}

        for column = 1, side do
            if row <= height and column <= width then
                padded[row][column] = height_grid[row][column]
            else
                padded[row][column] = nil
            end
        end
    end

    return setmetatable({
        grid = padded,
        original_height = height,
        original_width = width,
        side = side,
        cell_size_m = cell_size_m,
    }, Quadtree)
end

function Quadtree:box_has_canopy(row_start, column_start, box_size_cells, threshold_m)
    for row = row_start, row_start + box_size_cells - 1 do
        for column = column_start, column_start + box_size_cells - 1 do
            local height = self.grid[row][column]

            if height ~= nil and height >= threshold_m then
                return true
            end
        end
    end

    return false
end

function Quadtree:box_count(threshold_m, box_size_cells)
    assert(finite_number(threshold_m) and threshold_m >= 0.0,
        "threshold_m must be non-negative")
    assert(type(box_size_cells) == "number"
        and box_size_cells % 1 == 0
        and box_size_cells >= 1
        and box_size_cells <= self.side
        and self.side % box_size_cells == 0,
        "box_size_cells must divide the quadtree side length")

    local count = 0

    for row = 1, self.side, box_size_cells do
        for column = 1, self.side, box_size_cells do
            if self:box_has_canopy(row, column, box_size_cells, threshold_m) then
                count = count + 1
            end
        end
    end

    return count
end

function Quadtree:box_counting_curve(threshold_m)
    local points = {}
    local box_size_cells = self.side

    while box_size_cells >= 1 do
        local count = self:box_count(threshold_m, box_size_cells)

        if count > 0 then
            points[#points + 1] = {
                box_size_cells = box_size_cells,
                box_size_m = box_size_cells * self.cell_size_m,
                box_count = count,
            }
        end

        box_size_cells = box_size_cells / 2
    end

    return points
end

function Quadtree:fractal_dimension(threshold_m)
    local points = self:box_counting_curve(threshold_m)

    if #points < 2 then
        return {
            threshold_m = threshold_m,
            fractal_dimension = 0.0,
            points = points,
            valid = false,
        }
    end

    return {
        threshold_m = threshold_m,
        fractal_dimension = log_linear_slope(points),
        points = points,
        valid = true,
    }
end

function Quadtree:threshold_sweep(thresholds_m)
    assert(type(thresholds_m) == "table" and #thresholds_m > 0,
        "thresholds_m must be a non-empty array")

    local results = {}

    for index = 1, #thresholds_m do
        results[#results + 1] = self:fractal_dimension(thresholds_m[index])
    end

    return results
end

function Quadtree:select_stable_threshold(thresholds_m, heat_correlations)
    assert(type(thresholds_m) == "table" and #thresholds_m >= 3,
        "at least three thresholds are required")
    assert(type(heat_correlations) == "table"
        and #heat_correlations == #thresholds_m,
        "heat_correlations must align with thresholds_m")

    local results = self:threshold_sweep(thresholds_m)
    local candidates = {}

    for index = 2, #results - 1 do
        local previous = results[index - 1]
        local current = results[index]
        local next_value = results[index + 1]

        assert(finite_number(heat_correlations[index])
            and heat_correlations[index] >= -1.0
            and heat_correlations[index] <= 1.0,
            "heat correlations must be within [-1, 1]")

        if previous.valid and current.valid and next_value.valid then
            local local_dimension_variation = math.abs(
                current.fractal_dimension - previous.fractal_dimension
            ) + math.abs(
                next_value.fractal_dimension - current.fractal_dimension
            )

            local heat_strength = math.abs(heat_correlations[index])
            local stability_score = heat_strength
                / (1.0 + local_dimension_variation)

            candidates[#candidates + 1] = {
                threshold_m = thresholds_m[index],
                fractal_dimension = current.fractal_dimension,
                heat_correlation = heat_correlations[index],
                local_dimension_variation = local_dimension_variation,
                stability_score = stability_score,
            }
        end
    end

    assert(#candidates > 0,
        "no stable threshold candidate has sufficient canopy support")

    table.sort(candidates, function(left, right)
        if left.stability_score == right.stability_score then
            return left.threshold_m < right.threshold_m
        end
        return left.stability_score > right.stability_score
    end)

    return {
        recommended = candidates[1],
        candidates = candidates,
        interpretation = "The threshold is selected from supplied local correlation data; it is not a universal canopy-height standard.",
    }
end

function lidar_canopy_quadtree.new(height_grid, cell_size_m)
    return Quadtree.new(height_grid, cell_size_m)
end

return lidar_canopy_quadtree
