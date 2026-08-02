// File: cpp/eco_restoration/hex_diff_in_diff_spillover.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * Validation of corridor cooling propagation via difference-in-differences
 * on the hex lattice.
 *
 * Setup:
 *  - Treated hex h* receives a cooling intervention at time T0.
 *  - Neighbor hexes N(h*) are potential spillover units.
 *  - Control hexes C are similar but untreated and non-neighbor.
 *  - Landsat-8 overpasses provide UHI_h(t) before and after T0.[65][52][59][165]
 *
 * Difference-in-differences (DiD) estimator for spillover:
 *
 * For a neighbor hex j ∈ N(h*):
 *   ΔUHI_j^N = [UHI_j(post) - UHI_j(pre)]
 *   ΔUHI_C   = average over controls:
 *              ΔUHI_C = mean_{c ∈ C} [UHI_c(post) - UHI_c(pre)]
 *
 * Spillover effect estimate for neighbor j:
 *   τ_spillover,j = ΔUHI_j^N - ΔUHI_C
 *
 * Aggregated spillover around h*:
 *   τ_spillover = mean_{j ∈ N(h*)} τ_spillover,j
 *
 * Background weather is controlled by using controls C that share
 * similar meteorological conditions but lack treatment or adjacency.
 */

struct HexTimeSample {
    std::string hex_id;
    double UHI_pre;
    double UHI_post;
    bool is_neighbor; // j ∈ N(h*)
    bool is_control;  // c ∈ C
};

struct SpilloverResult {
    double deltaUHI_neighbors_mean;
    double deltaUHI_controls_mean;
    double tau_spillover;
};

SpilloverResult estimate_spillover(const std::vector<HexTimeSample>& samples) {
    double sum_delta_neighbors = 0.0;
    int count_neighbors = 0;
    double sum_delta_controls = 0.0;
    int count_controls = 0;

    for (const auto& s : samples) {
        double delta = s.UHI_post - s.UHI_pre;
        if (s.is_neighbor) {
            sum_delta_neighbors += delta;
            ++count_neighbors;
        }
        if (s.is_control) {
            sum_delta_controls += delta;
            ++count_controls;
        }
    }

    double mean_neighbors = (count_neighbors > 0)
                            ? sum_delta_neighbors / static_cast<double>(count_neighbors)
                            : 0.0;
    double mean_controls = (count_controls > 0)
                           ? sum_delta_controls / static_cast<double>(count_controls)
                           : 0.0;
    double tau = mean_neighbors - mean_controls;

    return {mean_neighbors, mean_controls, tau};
}

int main() {
    // Synthetic example: treated hex neighbors vs controls.
    std::vector<HexTimeSample> samples = {
        {"hex_10_20", 7.5, 5.5, true,  false}, // neighbor
        {"hex_10_21", 7.0, 5.8, true,  false}, // neighbor
        {"hex_11_20", 6.0, 5.2, false, true},  // control
        {"hex_12_21", 6.5, 5.7, false, true}   // control
    };

    SpilloverResult res = estimate_spillover(samples);

    std::cout << "Spillover validation (DiD):\n"
              << "  ΔUHI_neighbors_mean = " << res.deltaUHI_neighbors_mean << "\n"
              << "  ΔUHI_controls_mean  = " << res.deltaUHI_controls_mean << "\n"
              << "  τ_spillover         = " << res.tau_spillover << " °C\n";

    return 0;
}
