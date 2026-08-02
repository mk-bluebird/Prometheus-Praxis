// File: cpp/eco_restoration/hex_corridor_heat_budget.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 46. Hex-corridor heat budget using α, β, γ.
 *
 * For a corridor P of hexes, the surface energy balance per hex h is:
 *
 *   Q*_h + QF_h = QH_h + QE_h + ΔQS_h
 *
 * where:[178][184][187]
 *   Q*_h  : net all-wave radiation (shortwave + longwave).
 *   QF_h  : anthropogenic heat flux (buildings, traffic).
 *   QH_h  : sensible heat flux (convective, to air).
 *   QE_h  : latent heat flux (evapotranspiration).
 *   ΔQS_h : storage heat flux (into materials/soil).
 *
 * We parameterize QH_h and QE_h via α, β, γ:
 *   - α (vegetation) largely controls latent cooling and shading.
 *   - β (built/roof) controls sensible heating from impervious surfaces.
 *   - γ (water) contributes to latent cooling.
 *
 * Define hex-level proxies:
 *   QE_h ≈ L_0 ⋅ (|α| V_h + |γ| W_h)
 *   QH_h ≈ H_0 ⋅ (β B_h - |α| V_h)
 *
 * where:
 *   L_0, H_0 are scaling constants that convert index contributions
 *   to flux units (W/m^2).
 *
 * Corridor-integrated heat budget:
 *
 *   Σ_{h ∈ P} (Q*_h + QF_h - ΔQS_h) = Σ_{h ∈ P} (QH_h + QE_h)
 *
 * with:
 *   QH_h(α,β,V,B) and QE_h(α,γ,V,W) as above.
 */

struct HexHeatBudgetState {
    std::string hex_id;
    double Q_star; // net radiation Q*_h (W/m^2)
    double QF;     // anthropogenic heat QF_h (W/m^2)
    double V;      // vegetation index
    double B;      // built/roof index
    double W;      // water index
};

struct OffsetFluxParams {
    double alpha;
    double beta;
    double gamma;
    double L0;
    double H0;
};

struct HexFluxes {
    double QH;   // sensible
    double QE;   // latent
    double Q_storage; // storage (residual)
};

HexFluxes compute_hex_fluxes(const HexHeatBudgetState& h,
                             const OffsetFluxParams& p) {
    double QE = p.L0 * (std::fabs(p.alpha) * h.V + std::fabs(p.gamma) * h.W);
    double QH = p.H0 * (p.beta * h.B - std::fabs(p.alpha) * h.V);
    double Q_storage = h.Q_star + h.QF - (QH + QE);
    return {QH, QE, Q_storage};
}

struct CorridorHeatBudget {
    double sum_Q_star;
    double sum_QF;
    double sum_QH;
    double sum_QE;
    double sum_QS;
};

CorridorHeatBudget compute_corridor_budget(const std::vector<HexHeatBudgetState>& corridor,
                                           const OffsetFluxParams& p) {
    CorridorHeatBudget bud{0,0,0,0,0};
    for (const auto& h : corridor) {
        HexFluxes f = compute_hex_fluxes(h, p);
        bud.sum_Q_star += h.Q_star;
        bud.sum_QF     += h.QF;
        bud.sum_QH     += f.QH;
        bud.sum_QE     += f.QE;
        bud.sum_QS     += f.Q_storage;
    }
    return bud;
}

int main_heat_budget() {
    std::vector<HexHeatBudgetState> corridor = {
        {"hex_10_20", 600.0, 50.0, 0.35, 0.50, 0.05},
        {"hex_11_20", 580.0, 45.0, 0.40, 0.45, 0.08},
        {"hex_12_20", 620.0, 60.0, 0.25, 0.60, 0.02}
    };

    OffsetFluxParams p;
    p.alpha = -8.0;
    p.beta  = 3.0;
    p.gamma = -5.0;
    p.L0    = 10.0;  // latent flux scaling (W/m^2 per unit index)
    p.H0    = 8.0;   // sensible flux scaling

    CorridorHeatBudget bud = compute_corridor_budget(corridor, p);

    std::cout << "Corridor heat budget:\n"
              << "  ΣQ*  = " << bud.sum_Q_star << " W/m^2\n"
              << "  ΣQF  = " << bud.sum_QF     << " W/m^2\n"
              << "  ΣQH  = " << bud.sum_QH     << " W/m^2\n"
              << "  ΣQE  = " << bud.sum_QE     << " W/m^2\n"
              << "  ΣΔQS = " << bud.sum_QS     << " W/m^2\n";

    return 0;
}
