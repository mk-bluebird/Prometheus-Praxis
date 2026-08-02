// File: cpp/eco_restoration/tree_priority_with_carbon_sequestration.cpp

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

/**
 * 32. Eco-restoration coupled with carbon sequestration modeling
 *
 * Tree-planting priority score (temperature-driven) with equity:
 *
 *   P_h^{equity} =
 *     UHI_h ⋅ |α| ⋅ (V_max - V_h) ⋅ f_feasible(h) ⋅ exp(-λ ⋅ SVI_h)
 *
 * We augment this with a carbon sequestration benefit term using
 * an allometric model for Phoenix urban tree species.[87][90]
 *
 * Allometric carbon model (simplified):
 *   For a species s with parameters (a_s, b_s), biomass B_s(d) ≈ a_s d^{b_s},
 *   where d is trunk diameter or canopy radius.
 *   Carbon C_s(d) ≈ f_C * B_s(d), with f_C ~ 0.5.
 *
 * At hex scale, we model expected annual carbon sequestration from
 * new canopy ΔV_h as:
 *   C_h ≈ k_C ⋅ ΔV_h
 *
 * where k_C aggregates species mix, growth rates, and productivity.
 *
 * Carbon-augmented priority:
 *
 *   P_h^{eco} =
 *     [UHI_h ⋅ |α| ⋅ (V_max - V_h) + w_C ⋅ C_h] ⋅ f_feasible(h) ⋅ exp(-λ ⋅ SVI_h)
 *
 *   with C_h = k_C ⋅ (V_max - V_h), w_C a weight converting carbon benefit
 *   into comparable units (e.g., °C-equivalent or social cost of carbon).
 *
 * This shifts optimal placement toward hexes that not only cool effectively
 * but also have high long-term carbon gain potential, often favoring
 * locations with space for large, long-lived trees.
 */

struct HexTreeCarbonData {
    std::string hex_id;
    double UHI;
    double alpha;
    double V_max;
    double V_current;
    double feasible;
    double SVI;
    double k_C;         // carbon per unit ΔV (e.g., tons CO2/year per ΔNDVI)
    double cost_eff;    // effective cost per unit ΔV (including non-monetary)
};

struct EcoPriorityResult {
    std::string hex_id;
    double P_equity;
    double P_eco;
};

double compute_P_equity(const HexTreeCarbonData& h, double lambda) {
    double gap = std::max(0.0, h.V_max - h.V_current);
    double base = h.UHI * std::fabs(h.alpha) * gap * h.feasible;
    double equity_term = std::exp(-lambda * h.SVI);
    return base * equity_term;
}

double compute_P_eco(const HexTreeCarbonData& h, double lambda, double w_C) {
    double gap = std::max(0.0, h.V_max - h.V_current);
    double cooling_term = h.UHI * std::fabs(h.alpha) * gap;
    double carbon_term  = w_C * h.k_C * gap;
    double base = (cooling_term + carbon_term) * h.feasible;
    double equity_term = std::exp(-lambda * h.SVI);
    return base * equity_term;
}

std::vector<EcoPriorityResult> rank_hexes_with_carbon(
        const std::vector<HexTreeCarbonData>& hexes,
        double lambda,
        double w_C) {
    std::vector<EcoPriorityResult> out;
    out.reserve(hexes.size());
    for (const auto& h : hexes) {
        double P_eq  = compute_P_equity(h, lambda);
        double P_eco = compute_P_eco(h, lambda, w_C);
        out.push_back({h.hex_id, P_eq, P_eco});
    }
    std::sort(out.begin(), out.end(),
              [](const EcoPriorityResult& a, const EcoPriorityResult& b) {
                  return a.P_eco > b.P_eco;
              });
    return out;
}

int main_carbon() {
    std::vector<HexTreeCarbonData> hexes = {
        // hex with high UHI, high SVI, good tree space, strong carbon potential
        {"hex_10_20", 7.5, -8.0, 0.6, 0.2, 1.0, 0.8, 2.0, 100000.0},
        // hex with moderate UHI, moderate SVI, limited space but still some carbon
        {"hex_11_20", 5.5, -7.5, 0.5, 0.3, 1.0, 0.5, 1.2, 90000.0},
        // hex with high UHI but low carbon potential (species constraints, shallow soils)
        {"hex_12_20", 6.5, -8.2, 0.4, 0.25, 0.9, 0.3, 0.5, 110000.0}
    };

    double lambda = 1.0; // equity weighting from community calibration
    double w_C = 0.5;    // relative importance of carbon vs cooling

    auto results = rank_hexes_with_carbon(hexes, lambda, w_C);

    std::cout << "Hex rankings with carbon-augmented priority:\n";
    for (const auto& r : results) {
        std::cout << "Hex " << r.hex_id
                  << " | P_equity=" << r.P_equity
                  << " | P_eco=" << r.P_eco << "\n";
    }

    return 0;
}
