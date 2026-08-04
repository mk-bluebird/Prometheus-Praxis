// File: cpp/simulation/stochastic_reachability_carbon_negative.cpp

#include <iostream>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <string>
#include <algorithm>

namespace prometheus_praxis {
namespace simulation {

// Drift and diffusion functions f(x), sigma(x) for carbon stock dynamics.
// These can be calibrated from telemetry or domain models; here we use
// simple parameterized forms that encourage carbon-negative drift near zero.
struct DriftDiffusionParams {
    double drift_base;        // base drift coefficient
    double drift_slope;       // slope vs. carbon state x
    double diffusion_base;    // base diffusion coefficient
    double diffusion_slope;   // slope vs. carbon state x
};

// Drift function f(x): negative near 0 to push towards carbon-negative states.
double drift_f(double x, const DriftDiffusionParams& p) {
    // Example: f(x) = drift_base - drift_slope * x
    return p.drift_base - p.drift_slope * x;
}

// Diffusion function sigma(x): scaled with |x| to reflect higher uncertainty at high carbon levels.
double diffusion_sigma(double x, const DriftDiffusionParams& p) {
    // Example: sigma(x) = diffusion_base + diffusion_slope * std::fabs(x)
    return p.diffusion_base + p.diffusion_slope * std::fabs(x);
}

// Grid representation for 1D carbon state domain [x_min, x_max] discretized into N points.
struct Grid1D {
    double x_min;
    double x_max;
    std::size_t n_points;
    double dx;
    std::vector<double> nodes;

    Grid1D(double x_min_, double x_max_, std::size_t n_points_)
        : x_min(x_min_), x_max(x_max_), n_points(n_points_), dx(0.0) {
        if (n_points_ < 2) {
            throw std::invalid_argument("Grid must have at least 2 points");
        }
        dx = (x_max - x_min) / static_cast<double>(n_points - 1);
        nodes.resize(n_points);
        for (std::size_t i = 0; i < n_points; ++i) {
            nodes[i] = x_min + dx * static_cast<double>(i);
        }
    }
};

// Transition matrix P for Markov chain approximation of SDE.
// P[i][j] = probability of moving from node i to node j in one time step dt.
// Here we construct nearest-neighbor transitions (i -> i-1, i, i+1) with reflecting boundary.
struct TransitionMatrix {
    std::size_t n;
    std::vector<std::vector<double>> p; // n x n

    explicit TransitionMatrix(std::size_t n_)
        : n(n_), p(n_, std::vector<double>(n_, 0.0)) {}
};

// Build transition probabilities from SDE parameters using local moments.
// SDE: dX_t = f(X_t) dt + sigma(X_t) dW_t.
// We approximate one-step transitions via Euler-Maruyama and project onto grid nodes.
TransitionMatrix buildTransitionMatrix(const Grid1D& grid,
                                       const DriftDiffusionParams& params,
                                       double dt) {
    std::size_t n = grid.n_points;
    TransitionMatrix tm(n);

    for (std::size_t i = 0; i < n; ++i) {
        double x = grid.nodes[i];
        double mu = drift_f(x, params);
        double sig = diffusion_sigma(x, params);

        double mean = x + mu * dt;
        double var = sig * sig * dt;
        double stddev = std::sqrt(var);

        // For numerical robustness, if diffusion is extremely small, treat as deterministic.
        if (stddev < 1e-12) {
            // Map deterministically to nearest grid node.
            std::size_t j = static_cast<std::size_t>(
                std::round((mean - grid.x_min) / grid.dx));
            if (j >= n) j = n - 1;
            tm.p[i][j] = 1.0;
            continue;
        }

        // Use local Gaussian approximation: probability mass to neighbors i-1, i, i+1.
        // Compute probabilities by integrating normal between midpoints.
        auto node_center = [&](std::size_t k) {
            return grid.nodes[k];
        };

        auto normal_cdf = [&](double z) {
            return 0.5 * (1.0 + std::erf(z / std::sqrt(2.0)));
        };

        std::vector<double> probs(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            double left, right;
            if (j == 0) {
                left = grid.x_min - 0.5 * grid.dx;
                right = grid.x_min + 0.5 * grid.dx;
            } else if (j == n - 1) {
                left = grid.x_max - 0.5 * grid.dx;
                right = grid.x_max + 0.5 * grid.dx;
            } else {
                left = node_center(j) - 0.5 * grid.dx;
                right = node_center(j) + 0.5 * grid.dx;
            }
            double z_left = (left - mean) / stddev;
            double z_right = (right - mean) / stddev;
            probs[j] = normal_cdf(z_right) - normal_cdf(z_left);
        }

        double sum_probs = 0.0;
        for (double v : probs) sum_probs += v;
        if (sum_probs <= 0.0) {
            // fallback to deterministic mapping
            std::size_t j = static_cast<std::size_t>(
                std::round((mean - grid.x_min) / grid.dx));
            if (j >= n) j = n - 1;
            tm.p[i][j] = 1.0;
        } else {
            for (std::size_t j = 0; j < n; ++j) {
                tm.p[i][j] = probs[j] / sum_probs;
            }
        }
    }

    return tm;
}

// Safety-staying probability via backward induction on Markov chain.
// Given absorbing unsafe boundary and terminal horizon T = N*dt, HJB-like recursion:
// V_k(i) = max over controls (here we use fixed SDE, so simply propagate P)
//        = sum_j P[i][j] * V_{k+1}(j)
// with boundary conditions defining safe domain (e.g., carbon-negative states).
//
// Here we approximate the safety probability that the process stays within [x_safe_min, x_safe_max]
// over the horizon with no control, using backward induction.
std::vector<double> backwardInductionSafety(
    const Grid1D& grid,
    const TransitionMatrix& tm,
    double x_safe_min,
    double x_safe_max,
    std::size_t n_steps) {

    std::size_t n = grid.n_points;
    std::vector<double> V_curr(n, 0.0);
    std::vector<double> V_next(n, 0.0);

    // Terminal condition: probability 1 if in safe domain at T, else 0.
    for (std::size_t i = 0; i < n; ++i) {
        double x = grid.nodes[i];
        V_next[i] = (x >= x_safe_min && x <= x_safe_max) ? 1.0 : 0.0;
    }

    // Backward induction over n_steps.
    for (std::size_t k = 0; k < n_steps; ++k) {
        for (std::size_t i = 0; i < n; ++i) {
            double value = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                value += tm.p[i][j] * V_next[j];
            }
            // Enforce that states outside safe domain remain unsafe (absorbing).
            double x = grid.nodes[i];
            if (x < x_safe_min || x > x_safe_max) {
                V_curr[i] = 0.0;
            } else {
                V_curr[i] = value;
            }
        }
        V_next = V_curr;
    }

    return V_next;
}

// Pre-compute lookup table: for each grid node x_i, store safety-staying probability over horizon.
struct SafetyLookupTable {
    Grid1D grid;
    std::vector<double> safety_prob;

    SafetyLookupTable(const Grid1D& g, const std::vector<double>& p)
        : grid(g), safety_prob(p) {
        if (g.n_points != p.size()) {
            throw std::invalid_argument("Grid and probability size mismatch");
        }
    }

    // Simple interpolation wrapper: given x*, interpolate safety probability using nearest neighbors.
    double interpolate(double x_query) const {
        if (x_query <= grid.x_min) {
            return safety_prob.front();
        }
        if (x_query >= grid.x_max) {
            return safety_prob.back();
        }
        double pos = (x_query - grid.x_min) / grid.dx;
        std::size_t i = static_cast<std::size_t>(std::floor(pos));
        std::size_t j = i + 1;
        if (j >= grid.n_points) {
            j = grid.n_points - 1;
            i = j - 1;
        }
        double x_i = grid.nodes[i];
        double x_j = grid.nodes[j];
        double w = (x_query - x_i) / (x_j - x_i);
        return (1.0 - w) * safety_prob[i] + w * safety_prob[j];
    }
};

// SQL storage schema for stochastic reachability results.
// These strings can be installed into Postgres or another SQL engine via migrations.
std::string sqlSchemaCarbonNegativeReachability() {
    return R"SQL(
CREATE TABLE IF NOT EXISTS ker_carbon_reachability_grid (
    id SERIAL PRIMARY KEY,
    x_state DOUBLE PRECISION NOT NULL,
    safety_prob DOUBLE PRECISION NOT NULL,
    horizon_steps INTEGER NOT NULL,
    dt_seconds DOUBLE PRECISION NOT NULL,
    drift_base DOUBLE PRECISION NOT NULL,
    drift_slope DOUBLE PRECISION NOT NULL,
    diffusion_base DOUBLE PRECISION NOT NULL,
    diffusion_slope DOUBLE PRECISION NOT NULL,
    x_safe_min DOUBLE PRECISION NOT NULL,
    x_safe_max DOUBLE PRECISION NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_carbon_reachability_x_state
    ON ker_carbon_reachability_grid(x_state);
)SQL";
}

// Simple interpolation query wrapper expressed as SQL function.
std::string sqlInterpolationFunction() {
    return R"SQL(
CREATE OR REPLACE FUNCTION ker_carbon_reachability_interp(x_query DOUBLE PRECISION)
RETURNS DOUBLE PRECISION AS $$
DECLARE
    left_rec  RECORD;
    right_rec RECORD;
    result    DOUBLE PRECISION;
BEGIN
    SELECT *
    INTO left_rec
    FROM ker_carbon_reachability_grid
    WHERE x_state <= x_query
    ORDER BY x_state DESC
    LIMIT 1;

    SELECT *
    INTO right_rec
    FROM ker_carbon_reachability_grid
    WHERE x_state >= x_query
    ORDER BY x_state ASC
    LIMIT 1;

    IF left_rec IS NULL THEN
        RETURN right_rec.safety_prob;
    ELSIF right_rec IS NULL THEN
        RETURN left_rec.safety_prob;
    ELSIF left_rec.x_state = right_rec.x_state THEN
        RETURN left_rec.safety_prob;
    ELSE
        result := left_rec.safety_prob +
                  (x_query - left_rec.x_state) *
                  (right_rec.safety_prob - left_rec.safety_prob) /
                  (right_rec.x_state - left_rec.x_state);
        RETURN result;
    END IF;
END;
$$ LANGUAGE plpgsql;
)SQL";
}

// Example main demonstrating the stochastic reachability computation and emission of SQL schema.
int main() {
    using namespace prometheus_praxis::simulation;

    // Define grid for carbon state (e.g., tons of CO2 equivalent), where lower is better.
    double x_min = -10.0;  // strongly carbon-negative
    double x_max = 50.0;   // high carbon load
    std::size_t n_points = 101;
    Grid1D grid(x_min, x_max, n_points);

    // Drift/diffusion tuned to encourage movement towards negative states.
    DriftDiffusionParams params;
    params.drift_base = -0.5;      // base negative drift
    params.drift_slope = 0.02;     // stronger negative drift for high carbon
    params.diffusion_base = 0.5;   // baseline noise
    params.diffusion_slope = 0.01; // more noise at higher carbon

    double dt = 1.0;               // time step in hours (or normalized units)
    std::size_t n_steps = 48;      // horizon (e.g., 2 days)

    // Build transition matrix from SDE parameters.
    TransitionMatrix tm = buildTransitionMatrix(grid, params, dt);

    // Safe domain: carbon-negative states up to a small buffer.
    double x_safe_min = -10.0;
    double x_safe_max = 0.0;

    // Backward induction to compute safety-staying probability.
    std::vector<double> safety_prob = backwardInductionSafety(
        grid, tm, x_safe_min, x_safe_max, n_steps);

    // Build lookup table and test interpolation.
    SafetyLookupTable table(grid, safety_prob);
    double x_query1 = -5.0;
    double x_query2 = 10.0;
    double p_safe_1 = table.interpolate(x_query1);
    double p_safe_2 = table.interpolate(x_query2);

    std::cout << "Safety-staying probability at x=" << x_query1
              << " is " << p_safe_1 << std::endl;
    std::cout << "Safety-staying probability at x=" << x_query2
              << " is " << p_safe_2 << std::endl;

    // Emit SQL schema and interpolation function for installation.
    std::cout << "\n--- SQL schema for ker_carbon_reachability_grid ---\n";
    std::cout << sqlSchemaCarbonNegativeReachability() << std::endl;

    std::cout << "\n--- SQL interpolation function ker_carbon_reachability_interp ---\n";
    std::cout << sqlInterpolationFunction() << std::endl;

    return 0;
}

} // namespace simulation
} // namespace prometheus_praxis
