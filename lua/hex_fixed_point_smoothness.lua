-- File: lua/hex_fixed_point_smoothness.lua
-- Hex-anchor consistency as a fixed-point problem with distributed Gauss-Seidel updates.

-- We represent Phoenix H3 cells as nodes in a graph, with adjacency encoded
-- via a neighbor list. Each cell carries an intervention value u_i (e.g.,
-- added tree canopy fraction or equivalent thermal-recovery intervention).
-- Smoothness constraint: for any two adjacent cells i ~ j, the interventions
-- should not differ by more than a specified threshold and should satisfy a
-- discrete Laplacian consistency relation.

-- Mathematical formulation:
-- Let G = (V, E) be the graph of H3 cells (vertices V, edges E).
-- For each cell i in V, define u_i as the intervention magnitude.
-- Smoothness constraint:
--   For all (i, j) in E: |u_i - u_j| <= epsilon_smooth
-- Fixed-point condition:
--   For each i:
--     u_i = (1 - alpha) * u_i + alpha * (1 / deg(i)) * sum_{j in N(i)} u_j + alpha * b_i
-- where:
--   alpha in (0,1] is a relaxation parameter,
--   deg(i) is the degree of node i,
--   N(i) is the neighbor set of i,
--   b_i is a local bias term derived from the hex's thermal-recovery target
--       (e.g., required canopy-temperature drop from the hex thermal-recovery formula).
--
-- Rearranged:
--   u_i = (1 / (1 - alpha)) * (alpha / deg(i)) * sum_{j in N(i)} u_j
--         + (alpha / (1 - alpha)) * b_i
-- This defines a global fixed point u* such that:
--   u* = T(u*),  where T is the update operator.
--
-- Distributed Gauss-Seidel:
--   On each iteration k+1, cells update u_i using the most recent neighbor values:
--     u_i^{(k+1)} = (1 - alpha) * u_i^{(k)} + alpha * (1 / deg(i)) * sum_{j in N(i)} u_j^{(k+1 or k)} + alpha * b_i
--   Using Gauss-Seidel ordering, neighbors that have already been updated in
--   this iteration use their new values; others use old values.
--   Convergence to the fixed point is achieved under standard conditions:
--   the update operator is a contraction (e.g., alpha small enough, graph connected).

local HexGraph = {}
HexGraph.__index = HexGraph

function HexGraph.new(epsilon_smooth, alpha)
    return setmetatable({
        nodes = {},           -- key: h3_index, value: {u, b, neighbors}
        epsilon_smooth = epsilon_smooth or 0.05,
        alpha = alpha or 0.5
    }, HexGraph)
end

function HexGraph:add_node(h3_index, initial_u, bias_b)
    if not self.nodes[h3_index] then
        self.nodes[h3_index] = {
            u = initial_u or 0.0,
            b = bias_b or 0.0,
            neighbors = {}
        }
    end
end

function HexGraph:add_edge(h3_index_i, h3_index_j)
    if self.nodes[h3_index_i] and self.nodes[h3_index_j] then
        table.insert(self.nodes[h3_index_i].neighbors, h3_index_j)
        table.insert(self.nodes[h3_index_j].neighbors, h3_index_i)
    end
end

-- Single Gauss-Seidel sweep over all nodes.
function HexGraph:gauss_seidel_sweep()
    local alpha = self.alpha
    for h3_index, node in pairs(self.nodes) do
        local deg = #node.neighbors
        if deg > 0 then
            local sum_neighbors = 0.0
            for _, nb in ipairs(node.neighbors) do
                sum_neighbors = sum_neighbors + self.nodes[nb].u
            end
            local u_old = node.u
            local avg_neighbors = sum_neighbors / deg
            node.u = (1.0 - alpha) * u_old + alpha * (avg_neighbors + node.b)
        end
    end
end

-- Check smoothness constraint across all edges: |u_i - u_j| <= epsilon_smooth.
function HexGraph:check_smoothness()
    local violations = {}
    local eps = self.epsilon_smooth
    for h3_index, node in pairs(self.nodes) do
        for _, nb in ipairs(node.neighbors) do
            local diff = math.abs(node.u - self.nodes[nb].u)
            if diff > eps then
                table.insert(violations, {h3_index, nb, diff})
            end
        end
    end
    return violations
end

-- Iterative solver: run Gauss-Seidel until smoothness is satisfied or max iterations reached.
function HexGraph:solve(max_iters)
    max_iters = max_iters or 100
    for iter = 1, max_iters do
        self:gauss_seidel_sweep()
        local violations = self:check_smoothness()
        if #violations == 0 then
            return true, iter
        end
    end
    return false, max_iters
end

-- Example usage:
-- local graph = HexGraph.new(0.05, 0.4)
-- graph:add_node("hex-A", 0.3, 0.01)
-- graph:add_node("hex-B", 0.1, 0.02)
-- graph:add_node("hex-C", 0.5, -0.01)
-- graph:add_edge("hex-A", "hex-B")
-- graph:add_edge("hex-B", "hex-C")
-- local ok, iters = graph:solve(200)
-- print("Converged:", ok, "iterations:", iters)

return HexGraph
