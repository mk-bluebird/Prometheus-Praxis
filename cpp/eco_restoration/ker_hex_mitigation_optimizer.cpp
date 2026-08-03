// File: cpp/eco_restoration/ker_hex_mitigation_optimizer.cpp
#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <cmath>

namespace eco {

struct Intervention {
    std::string id;
    double delta_k;
    double delta_e;
    double delta_r;
    double cost;
    double water;

    Intervention(const std::string& id_,
                 double dk, double de, double dr,
                 double c, double w)
        : id(id_), delta_k(dk), delta_e(de), delta_r(dr),
          cost(c), water(w) {}
};

struct HexProfile {
    std::string id;
    double k0;
    double e0;
    double r0;
    double weight; // omega_h
    std::vector<Intervention> interventions;
};

struct BudgetConstraints {
    double max_cost;
    double max_water;
};

struct MitigationChoice {
    std::string hex_id;
    std::string intervention_id; // empty if none
    double k;
    double e;
    double r;
    double s;
};

struct OptimizationResult {
    double total_s_weighted;
    double total_cost;
    double total_water;
    std::vector<MitigationChoice> choices;
};

// Compute KER scalar s = k*e - r with clipping to [0,1] for k,e,r.
double ker_scalar(double k, double e, double r) {
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    return k * e - r;
}

// Lane governance predicate for PROD-like corridor.
bool prod_lane_ok(double k, double e, double r) {
    double s = ker_scalar(k, e, r);
    return (s > 0.2 && e >= 0.7 && r <= 0.5);
}

// Evaluate a full allocation of interventions given binary selection vector x.
OptimizationResult evaluate_allocation(
        const std::vector<HexProfile>& hexes,
        const std::vector<int>& selected_indices,
        const BudgetConstraints& budget) {

    OptimizationResult res{};
    res.total_s_weighted = 0.0;
    res.total_cost = 0.0;
    res.total_water = 0.0;
    res.choices.clear();

    for (std::size_t h_idx = 0; h_idx < hexes.size(); ++h_idx) {
        const auto& hex = hexes[h_idx];
        int j_sel = selected_indices[h_idx];

        double k = hex.k0;
        double e = hex.e0;
        double r = hex.r0;

        std::string chosen_id;

        if (j_sel >= 0 && j_sel < static_cast<int>(hex.interventions.size())) {
            const auto& iv = hex.interventions[j_sel];
            k += iv.delta_k;
            e += iv.delta_e;
            r += iv.delta_r;
            double s_iv = ker_scalar(k, e, r);
            if (!prod_lane_ok(k, e, r)) {
                // If lane constraints violated, treat as no intervention.
                k = hex.k0;
                e = hex.e0;
                r = hex.r0;
            } else {
                res.total_cost += iv.cost;
                res.total_water += iv.water;
                chosen_id = iv.id;
            }
        }

        double s = ker_scalar(k, e, r);
        res.total_s_weighted += hex.weight * s;

        MitigationChoice choice{};
        choice.hex_id = hex.id;
        choice.intervention_id = chosen_id;
        choice.k = k;
        choice.e = e;
        choice.r = r;
        choice.s = s;
        res.choices.push_back(choice);
    }

    // Enforce budget constraints: if violated, penalize heavily.
    if (res.total_cost > budget.max_cost || res.total_water > budget.max_water) {
        double penalty = (res.total_cost - budget.max_cost) +
                         (res.total_water - budget.max_water);
        res.total_s_weighted -= 1e3 * penalty;
    }

    return res;
}

// Simple greedy optimizer: per-hex rank interventions by ω_h Δs / cost
OptimizationResult greedy_optimizer(const std::vector<HexProfile>& hexes,
                                    const BudgetConstraints& budget) {
    std::vector<int> selected_indices(hexes.size(), -1);

    for (std::size_t h_idx = 0; h_idx < hexes.size(); ++h_idx) {
        const auto& hex = hexes[h_idx];
        double best_score = -std::numeric_limits<double>::infinity();
        int best_j = -1;

        double base_s = ker_scalar(hex.k0, hex.e0, hex.r0);

        for (int j = 0; j < static_cast<int>(hex.interventions.size()); ++j) {
            const auto& iv = hex.interventions[j];
            double k = hex.k0 + iv.delta_k;
            double e = hex.e0 + iv.delta_e;
            double r = hex.r0 + iv.delta_r;
            double s = ker_scalar(k, e, r);
            if (!prod_lane_ok(k, e, r)) {
                continue;
            }
            double delta_s = s - base_s;
            if (iv.cost <= 0.0) {
                continue;
            }
            double score = hex.weight * delta_s / iv.cost;
            if (score > best_score) {
                best_score = score;
                best_j = j;
            }
        }

        selected_indices[h_idx] = best_j;
    }

    OptimizationResult res = evaluate_allocation(hexes, selected_indices, budget);
    return res;
}

// Print optimization result.
void print_result(const OptimizationResult& res) {
    std::cout << "Total weighted KER score: " << res.total_s_weighted << "\n";
    std::cout << "Total cost: " << res.total_cost << "\n";
    std::cout << "Total water: " << res.total_water << "\n";
    std::cout << "Choices:\n";
    for (const auto& ch : res.choices) {
        std::cout << "Hex " << ch.hex_id << " : ";
        if (!ch.intervention_id.empty()) {
            std::cout << "Intervention " << ch.intervention_id;
        } else {
            std::cout << "No intervention";
        }
        std::cout << " -> k=" << ch.k
                  << " e=" << ch.e
                  << " r=" << ch.r
                  << " s=" << ch.s << "\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example Phoenix hex profiles.
    HexProfile h1{"hex_P1", 0.6, 0.65, 0.4, 1.0, {}};
    h1.interventions.emplace_back("urban_forest",
                                  /*dk=*/0.15, /*de=*/0.20, /*dr=*/-0.10,
                                  /*cost=*/50.0, /*water=*/10.0);
    h1.interventions.emplace_back("reflective_surfaces",
                                  /*dk=*/0.05, /*de=*/0.15, /*dr=*/-0.05,
                                  /*cost=*/30.0, /*water=*/2.0);

    HexProfile h2{"hex_P2", 0.5, 0.6, 0.45, 0.8, {}};
    h2.interventions.emplace_back("water_feature",
                                  /*dk=*/0.10, /*de=*/0.18, /*dr=*/-0.08,
                                  /*cost=*/60.0, /*water=*/20.0);
    h2.interventions.emplace_back("shade_structures",
                                  /*dk=*/0.08, /*de=*/0.12, /*dr=*/-0.06,
                                  /*cost=*/40.0, /*water=*/5.0);

    std::vector<HexProfile> hexes = {h1, h2};
    BudgetConstraints budget{/*max_cost=*/120.0, /*max_water=*/30.0};

    OptimizationResult res = greedy_optimizer(hexes, budget);
    print_result(res);

    return 0;
}
