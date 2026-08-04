-- File: lua/hex_laplace_multigrid.lua
-- Hex-anchor consistency as a discrete Laplace/Poisson equation with multigrid V-cycle solver.

-- Mathematical setup:
-- Let each H3 hex-cell i have green_fraction g_i and LST anomaly ΔT_i.
-- At equilibrium, zero thermal gradient between neighbours implies:
--   For all i: sum_{j in N(i)} (ΔT_i - ΔT_j) = 0,
-- which, when expressed in terms of green_fraction via an empirical relation
-- ΔT_i = F(g_i) + residual_i, leads to a discrete Poisson equation:
--   L g = -f(ΔT_anomaly),
-- where L is the graph Laplacian over hex adjacency and f(ΔT_anomaly) encodes
-- desired cooling response (e.g., more greening where anomalies are high).

-- Discrete Laplace equation:
-- For each cell i:
--   (deg(i) * g_i - sum_{j in N(i)} g_j) = -f_i,
-- i.e.,
--   L g = -f,
-- where:
--   L_ii = deg(i), L_ij = -1 for j in N(i), 0 otherwise,
--   f_i = f(ΔT_i) is a source term from LST anomaly.

-- Data structures:
-- - hex_cells: table keyed by h3_index with fields:
--      { g = green_fraction, deltaT = LST_anomaly, neighbors = {h3_j1, h3_j2, ...} }
-- - levels: multigrid hierarchy represented as nested tables of hex_cells.

local HexGrid = {}
HexGrid.__index = HexGrid

function HexGrid.new()
    return setmetatable({ cells = {} }, HexGrid)
end

function HexGrid:add_cell(h3_index, deltaT)
    if not self.cells[h3_index] then
        self.cells[h3_index] = {
            g = 0.0,            -- initial green_fraction
            deltaT = deltaT,    -- LST anomaly
            neighbors = {}
        }
    else
        self.cells[h3_index].deltaT = deltaT
    end
end

function HexGrid:add_edge(h3_i, h3_j)
    if self.cells[h3_i] and self.cells[h3_j] then
        table.insert(self.cells[h3_i].neighbors, h3_j)
        table.insert(self.cells[h3_j].neighbors, h3_i)
    end
end

-- Source term f_i = f(ΔT_i); simple linear mapping: f_i = alpha * ΔT_i.
local function source_term(deltaT, alpha)
    return alpha * deltaT
end

-- Relaxation step (Gauss-Seidel) for discrete Poisson equation:
--   deg(i) * g_i - sum_{j in N(i)} g_j = -f_i
-- Solve for g_i:
--   g_i = (sum_{j in N(i)} g_j - f_i) / deg(i)
local function relax(grid, alpha, iterations)
    iterations = iterations or 1
    for _ = 1, iterations do
        for h3, cell in pairs(grid.cells) do
            local deg = #cell.neighbors
            if deg > 0 then
                local sum_neighbors = 0.0
                for _, nb in ipairs(cell.neighbors) do
                    sum_neighbors = sum_neighbors + grid.cells[nb].g
                end
                local f_i = source_term(cell.deltaT, alpha)
                cell.g = (sum_neighbors - f_i) / deg
            end
        end
    end
end

-- Residual r_i = -f_i - (L g)_i; measures inconsistency.
local function compute_residual(grid, alpha)
    local residual = {}
    for h3, cell in pairs(grid.cells) do
        local deg = #cell.neighbors
        if deg > 0 then
            local sum_neighbors = 0.0
            for _, nb in ipairs(cell.neighbors) do
                sum_neighbors = sum_neighbors + grid.cells[nb].g
            end
            local f_i = source_term(cell.deltaT, alpha)
            local Lg = deg * cell.g - sum_neighbors
            residual[h3] = -f_i - Lg
        else
            residual[h3] = 0.0
        end
    end
    return residual
end

-- Multigrid V-cycle skeleton: for simplicity, we implement a single-level V-cycle
-- (pre-relax, compute residual, correct), but in practice, coarse grids would
-- aggregate hex-cells (e.g., lower H3 resolution) and recurse.
local function v_cycle(grid, alpha, preRelax, postRelax)
    preRelax = preRelax or 3
    postRelax = postRelax or 3

    -- Pre-smoothing.
    relax(grid, alpha, preRelax)

    -- Residual computation.
    local residual = compute_residual(grid, alpha)

    -- Simple correction: treat residual as additional source term and adjust g.
    -- In a full multigrid, residual would be restricted to coarse grid, solved,
    -- then prolongated. Here we apply a direct correction:
    for h3, r in pairs(residual) do
        local cell = grid.cells[h3]
        local deg = #cell.neighbors
        if deg > 0 then
            cell.g = cell.g + r / deg
        end
    end

    -- Post-smoothing.
    relax(grid, alpha, postRelax)
end

-- Example usage with a small hex graph.
local function example()
    local grid = HexGrid.new()
    grid:add_cell("hex-A", 4.5)
    grid:add_cell("hex-B", 3.8)
    grid:add_cell("hex-C", 5.2)
    grid:add_edge("hex-A", "hex-B")
    grid:add_edge("hex-B", "hex-C")

    local alpha = 0.5 -- source scaling
    for i = 1, 10 do
        v_cycle(grid, alpha, 2, 2)
    end

    for h3, cell in pairs(grid.cells) do
        print(h3, "green_fraction =", cell.g, "LST anomaly =", cell.deltaT)
    end
end

-- Uncomment to run example:
-- example()

return {
    HexGrid = HexGrid,
    relax = relax,
    v_cycle = v_cycle
}
