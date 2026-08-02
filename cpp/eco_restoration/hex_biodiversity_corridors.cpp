// File: cpp/eco_restoration/hex_biodiversity_corridors.cpp

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <iostream>

/**
 * 40. Hex-anchoring for biodiversity corridors.
 *
 * We reuse the Phoenix hex grid as a scaffold for desert biodiversity corridors.
 * Each hex h has:
 *   - Vegetation / canopy (V_h), built / impervious (B_h), water (W_h).
 *   - Additional habitat-related indices:
 *       * NDSI_h: Normalized Difference Soil Index or snow/soil index.
 *       * SM_h: soil moisture proxy (e.g., from SAR or optical indices).
 *       * NDVI_h, NDWI_h as before.[83][87][171]
 *
 * We define habitat suitability H_h for a target species group as:
 *
 *   H_h = α' V_h + β' (1 - B_h) + γ' W_h + η' NDSI_h + ζ' SM_h + δ'
 *
 * where:
 *   - α' > 0: more vegetation increases suitability.
 *   - β' > 0: less impervious (1 - B_h) increases suitability.
 *   - γ' > 0: water / riparian features increase suitability.
 *   - η', ζ': contributions from soil type and moisture.
 *   - δ' : baseline suitability.
 *
 * This mirrors the offset model but with habitat parameters α', β', γ', η', ζ'
 * instead of thermal α, β, γ, capturing species-specific needs.
 *
 * Biodiversity corridor design:
 *   - Use the hex adjacency graph to find paths P with high H_h.
 *   - Define corridor score:
 *
 *       Corr(P) = (1 / |P|) Σ_{h ∈ P} H_h
 *
 *   - Optimize Corr(P) under constraints such as connectivity, minimum
 *     vegetation width, and avoidance of high-UHI hexes when species
 *     are heat-sensitive.
 */

struct HexHabitatState {
    std::string hex_id;
    double V;
    double B;
    double W;
    double NDSI;
    double SM;
};

struct HabitatParams {
    double alpha_prime;
    double beta_prime;
    double gamma_prime;
    double eta_prime;
    double zeta_prime;
    double delta_prime;
};

double compute_habitat_suitability(const HexHabitatState& h,
                                   const HabitatParams& p) {
    double suitability =
        p.alpha_prime * h.V +
        p.beta_prime  * (1.0 - h.B) +
        p.gamma_prime * h.W +
        p.eta_prime   * h.NDSI +
        p.zeta_prime  * h.SM +
        p.delta_prime;
    return suitability;
}

struct Corridor {
    std::vector<std::string> hex_ids;
    double mean_suitability;
};

Corridor evaluate_corridor(const std::vector<HexHabitatState>& hexes,
                           const HabitatParams& p,
                           const std::vector<std::string>& path_hex_ids) {
    double sum_H = 0.0;
    int count = 0;
    for (const auto& id : path_hex_ids) {
        auto it = std::find_if(hexes.begin(), hexes.end(),
                               [&](const HexHabitatState& h) { return h.hex_id == id; });
        if (it != hexes.end()) {
            sum_H += compute_habitat_suitability(*it, p);
            ++count;
        }
    }
    double mean_H = (count > 0) ? sum_H / static_cast<double>(count) : 0.0;
    return {path_hex_ids, mean_H};
}

int main_biodiversity() {
    std::vector<HexHabitatState> hexes = {
        {"hex_10_20", 0.35, 0.40, 0.05, 0.6, 0.3},
        {"hex_11_20", 0.40, 0.35, 0.08, 0.7, 0.4},
        {"hex_12_20", 0.25, 0.60, 0.02, 0.5, 0.2}
    };

    HabitatParams p;
    p.alpha_prime = 1.0;   // vegetation
    p.beta_prime  = 0.8;   // low impervious
    p.gamma_prime = 0.9;   // water
    p.eta_prime   = 0.5;   // soil index
    p.zeta_prime  = 0.7;   // moisture
    p.delta_prime = 0.0;

    std::vector<std::string> path = {"hex_10_20", "hex_11_20"};
    Corridor corridor = evaluate_corridor(hexes, p, path);

    std::cout << "Biodiversity corridor mean suitability along path:\n";
    std::cout << "  Hexes:";
    for (const auto& id : corridor.hex_ids) {
        std::cout << " " << id;
    }
    std::cout << " | mean H=" << corridor.mean_suitability << "\n";

    return 0;
}
