// File: cpp/eco_restoration/hex_spatial_fairness_gini.cpp

#include <iostream>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <string>

struct HexCell {
    std::string id;
    double mrt_reduction; // MRT reduction [°C] in this hex cell (positive is good)
    bool is_marginalised; // true if cell lies in historically marginalised neighborhood
};

struct GiniResult {
    double gini_overall;
    double gini_marginalised;
};

class SpatialFairnessGini {
public:
    // Compute Gini coefficient over MRT reductions for all hex cells.
    GiniResult compute(const std::vector<HexCell>& cells) const {
        if (cells.empty()) {
            throw std::runtime_error("No hex cells provided.");
        }

        std::vector<double> values;
        values.reserve(cells.size());
        for (const auto& c : cells) {
            values.push_back(c.mrt_reduction);
        }

        double g_all = gini(values);

        // Gini for marginalised neighborhoods only
        std::vector<double> values_marg;
        for (const auto& c : cells) {
            if (c.is_marginalised) {
                values_marg.push_back(c.mrt_reduction);
            }
        }
        double g_marg = 0.0;
        if (!values_marg.empty()) {
            g_marg = gini(values_marg);
        }

        return {g_all, g_marg};
    }

    // Check sovereignty_compliant based on maximum acceptable Gini thresholds.
    bool check_sovereignty_compliant(const GiniResult& gr,
                                     double max_gini_overall,
                                     double max_gini_marginalised,
                                     std::string& reason) const
    {
        if (gr.gini_overall > max_gini_overall) {
            reason = "Overall MRT reduction inequality exceeds allowed threshold.";
            return false;
        }
        if (gr.gini_marginalised > max_gini_marginalised) {
            reason = "MRT reduction inequality in marginalised neighborhoods exceeds allowed threshold.";
            return false;
        }
        reason.clear();
        return true;
    }

private:
    static double gini(std::vector<double> values) {
        // Standard Gini formula for non-negative values:
        // G = (2 * sum_i i * x_i) / (n * sum_i x_i) - (n + 1) / n
        std::sort(values.begin(), values.end());
        const std::size_t n = values.size();
        double sum = 0.0;
        double weighted_sum = 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            sum += values[i];
            weighted_sum += static_cast<double>(i + 1) * values[i];
        }
        if (sum <= 0.0) return 0.0;
        double g = (2.0 * weighted_sum) / (static_cast<double>(n) * sum)
                   - (static_cast<double>(n) + 1.0) / static_cast<double>(n);
        return g;
    }
};

int main() {
    // Example MRT reductions across Phoenix hex cells, including marginalised neighborhoods.
    std::vector<HexCell> cells = {
        {"hex-001", 3.0, false},
        {"hex-002", 2.5, false},
        {"hex-003", 1.0, true},  // marginalised
        {"hex-004", 0.8, true},  // marginalised
        {"hex-005", 4.5, false},
        {"hex-006", 2.0, true},  // marginalised
        {"hex-007", 3.8, false}
    };

    SpatialFairnessGini fairness;
    GiniResult gr = fairness.compute(cells);

    std::cout << "Overall Gini of MRT reductions: " << gr.gini_overall << "\n";
    std::cout << "Gini in marginalised neighborhoods: " << gr.gini_marginalised << "\n";

    // Set maximum acceptable Gini thresholds. For example:
    // - overall Gini <= 0.3 for acceptable spatial fairness,
    // - marginalised Gini <= 0.2 to ensure particularly fair treatment of historically burdened areas.
    double max_g_overall = 0.3;
    double max_g_marginalised = 0.2;

    std::string reason;
    bool sovereignty_ok = fairness.check_sovereignty_compliant(gr,
                                                               max_g_overall,
                                                               max_g_marginalised,
                                                               reason);

    std::cout << "sovereignty_compliant: " << (sovereignty_ok ? "true" : "false") << "\n";
    if (!sovereignty_ok) {
        std::cout << "Reason: " << reason << "\n";
    }

    return 0;
}
