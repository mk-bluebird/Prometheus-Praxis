// File: cpp/eco_restoration/route_thermal_comfort_optimization.cpp

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

/**
 * Cumulative UHI reduction along a transit route and optimization.
 *
 * Thermal comfort metric along a hex-path:
 *
 *   TC_route = (1 / L) Σ_i d_i ⋅ UHI_i ⋅ exp(-τ ⋅ t_i)
 *
 * where:
 *   d_i  : distance traveled in hex i (e.g., meters).
 *   UHI_i: UHI intensity in hex i.
 *   t_i  : time spent in hex i.
 *   τ    : time-decay constant (how quickly thermal discomfort fades).
 *   L    : total route length Σ_i d_i.
 *
 * Planned interventions change UHI_i to UHI'_i:
 *   UHI'_i = UHI_i + ΔT_i
 *   ΔT_i = α_i ΔV_i + β_i ΔB_i + γ_i ΔW_i
 *
 * Under a fixed budget B_total, we choose interventions (ΔV_i, ΔB_i, ΔW_i)
 * to minimize TC_route.
 *
 * Here we implement a greedy budget allocation that:
 *   - Computes marginal reduction in TC_route per unit cost for each hex.
 *   - Sorts hexes by benefit per dollar.
 *   - Applies interventions up to the budget constraint.
 */

struct HexRouteNode {
    std::string hex_id;
    double distance;    // d_i
    double time_in_hex; // t_i
    double UHI;         // baseline UHI_i
    double alpha;
    double beta;
    double gamma;
    double dV_max;
    double dB_opt;      // optimal ΔB (e.g., negative for cool roofs) at full intervention
    double dW_max;
    double cost_tree;
    double cost_roof;
    double cost_water;
};

struct HexInterventionChoice {
    std::string hex_id;
    double dV;
    double dB;
    double dW;
    double cost;
    double UHI_new;
};

double compute_TC_route(const std::vector<HexRouteNode>& route,
                         const std::vector<HexInterventionChoice>& choices,
                         double tau) {
    double L = 0.0;
    for (const auto& h : route) {
        L += h.distance;
    }

    double tc_sum = 0.0;
    for (const auto& h : route) {
        double UHI_eff = h.UHI;
        for (const auto& c : choices) {
            if (c.hex_id == h.hex_id) {
                UHI_eff = c.UHI_new;
                break;
            }
        }
        tc_sum += h.distance * UHI_eff * std::exp(-tau * h.time_in_hex);
    }

    return (L > 0.0) ? tc_sum / L : 0.0;
}

struct HexBenefit {
    std::string hex_id;
    double benefit_per_dollar;
    double max_cost;
};

std::vector<HexInterventionChoice> optimize_route_cooling(
        const std::vector<HexRouteNode>& route,
        double tau,
        double total_budget) {
    // Compute baseline TC.
    std::vector<HexInterventionChoice> no_choices;
    double TC_baseline = compute_TC_route(route, no_choices, tau);

    // For each hex, compute full-intervention UHI_new and TC reduction per dollar.
    std::vector<HexBenefit> benefits;
    benefits.reserve(route.size());

    for (const auto& h : route) {
        HexInterventionChoice c;
        c.hex_id = h.hex_id;
        c.dV = h.dV_max;
        c.dB = h.dB_opt;
        c.dW = h.dW_max;
        c.cost = h.dV_max * h.cost_tree
               + std::fabs(h.dB_opt) * h.cost_roof
               + h.dW_max * h.cost_water;

        double delta_T = h.alpha * c.dV + h.beta * c.dB + h.gamma * c.dW;
        c.UHI_new = h.UHI + delta_T;

        std::vector<HexInterventionChoice> tmp_choices = {c};
        double TC_new = compute_TC_route(route, tmp_choices, tau);
        double benefit = TC_baseline - TC_new;
        double bpd = (c.cost > 0.0) ? benefit / c.cost : 0.0;

        benefits.push_back({h.hex_id, bpd, c.cost});
    }

    // Sort hexes by benefit per dollar.
    std::sort(benefits.begin(), benefits.end(),
              [](const HexBenefit& a, const HexBenefit& b) {
                  return a.benefit_per_dollar > b.benefit_per_dollar;
              });

    // Allocate budget greedily.
    std::vector<HexInterventionChoice> chosen;
    double budget_used = 0.0;

    for (const auto& b : benefits) {
        if (budget_used >= total_budget) break;

        // Retrieve hex node.
        const HexRouteNode* h = nullptr;
        for (const auto& hn : route) {
            if (hn.hex_id == b.hex_id) {
                h = &hn;
                break;
            }
        }
        if (!h) continue;

        HexInterventionChoice c;
        c.hex_id = h->hex_id;
        c.dV = h->dV_max;
        c.dB = h->dB_opt;
        c.dW = h->dW_max;
        c.cost = h->dV_max * h->cost_tree
               + std::fabs(h->dB_opt) * h->cost_roof
               + h->dW_max * h->cost_water;

        if (budget_used + c.cost > total_budget) {
            // Scale interventions to fit remaining budget.
            double scale = (total_budget - budget_used) / c.cost;
            c.dV *= scale;
            c.dB *= scale;
            c.dW *= scale;
            c.cost *= scale;
        }

        double delta_T = h->alpha * c.dV + h->beta * c.dB + h->gamma * c.dW;
        c.UHI_new = h->UHI + delta_T;

        budget_used += c.cost;
        chosen.push_back(c);
    }

    return chosen;
}

int main() {
    std::vector<HexRouteNode> route = {
        {"hex_10_20", 300.0, 180.0, 8.0, -8.0, 3.0, -5.0, 0.15, -0.10, 0.08, 100000.0, 80000.0, 60000.0},
        {"hex_11_20", 250.0, 150.0, 7.0, -7.5, 2.8, -4.8, 0.12, -0.12, 0.06, 90000.0, 70000.0, 55000.0},
        {"hex_12_20", 400.0, 240.0, 9.0, -8.2, 3.1, -5.2, 0.10, -0.15, 0.10, 110000.0, 85000.0, 65000.0}
    };

    double tau = 0.002;        // time-decay constant
    double budget = 250000.0;  // total budget

    auto choices = optimize_route_cooling(route, tau, budget);
    double TC_baseline = compute_TC_route(route, {}, tau);
    double TC_new = compute_TC_route(route, choices, tau);

    std::cout << "Baseline TC_route = " << TC_baseline << "\n";
    std::cout << "Optimized TC_route = " << TC_new << "\n";
    std::cout << "Interventions:\n";
    for (const auto& c : choices) {
        std::cout << "Hex " << c.hex_id
                  << " | dV=" << c.dV
                  << " | dB=" << c.dB
                  << " | dW=" << c.dW
                  << " | cost=" << c.cost
                  << " | UHI_new=" << c.UHI_new << "\n";
    }

    return 0;
}
