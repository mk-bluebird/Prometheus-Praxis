// File: cpp/eco_restoration/sr_cnn_hex_lst_physical_loss.cpp

#include <vector>
#include <cmath>
#include <iostream>

/**
 * 38. Deep learning super-resolution of hex-level LST with physical loss.
 *
 * Setup:
 *  - Input: 30 m Landsat LST and spectral indices mapped to coarse hex cells.
 *  - Output: 10 m "sub-hex" LST predictions for finer hex sub-cells.
 *  - Model: super-resolution CNN f_θ that upsamples coarse fields to finer grid.
 *  - Physical constraint: α, β, γ relationships should hold locally and
 *    aggregate back to observed coarse ΔT_h.[65][52][171][165]
 *
 * Coarse offset model at hex h:
 *   ΔT_h_coarse = α V_h + β B_h + γ W_h + δ
 *
 * Fine-scale predictions for subcells k in hex h:
 *   LST̂_{h,k}, NDVÎ_{h,k}, NDBÎ_{h,k}, NDWÎ_{h,k}
 *
 * Physical loss components:
 *
 * 1. Aggregation consistency:
 *    - Aggregated fine ΔT̂_h must equal coarse ΔT_h_coarse:
 *
 *      ΔT̂_h_agg = (1 / N_h) Σ_k [LST̂_{h,k} - LST_ref_h]
 *
 *      L_phys_agg = Σ_h (ΔT̂_h_agg - ΔT_h_coarse)^2
 *
 * 2. Local α, β, γ consistency:
 *    - At subcell scale, the offset relationship should still approximate
 *      local cooling:
 *
 *      ΔT̂_{h,k} ≈ α NDVÎ_{h,k} + β NDBÎ_{h,k} + γ NDWÎ_{h,k} + δ
 *
 *      L_phys_local = Σ_{h,k} (ΔT̂_{h,k} -
 *                       (α NDVÎ_{h,k} + β NDBÎ_{h,k} + γ NDWÎ_{h,k} + δ))^2
 *
 * 3. Super-resolution reconstruction loss:
 *    - Downsampled fine predictions should match original 30 m LST / indices.
 *
 *      L_sr = Σ_h ||Downsample({LST̂_{h,k}}) - LST_h||^2
 *
 * Full loss:
 *   L_total = L_sr + λ_agg L_phys_agg + λ_local L_phys_local
 */

struct CoarseHexSample {
    double delta_T_coarse;
    double LST_ref;
    double alpha;
    double beta;
    double gamma;
    double delta;
};

struct FineSubcellSample {
    double lst_hat;
    double ndvi_hat;
    double ndbi_hat;
    double ndwi_hat;
};

struct HexFinePrediction {
    std::string hex_id;
    CoarseHexSample coarse;
    std::vector<FineSubcellSample> fine_cells;
};

struct SRLossTerms {
    double L_sr;
    double L_phys_agg;
    double L_phys_local;
    double L_total;
};

SRLossTerms compute_sr_physical_loss(const std::vector<HexFinePrediction>& preds,
                                     double lambda_agg,
                                     double lambda_local) {
    double L_sr = 0.0;
    double L_phys_agg = 0.0;
    double L_phys_local = 0.0;

    for (const auto& h : preds) {
        const auto& coarse = h.coarse;
        std::size_t N = h.fine_cells.size();
        if (N == 0) continue;

        // Aggregated fine ΔT̂_h.
        double sum_delta_T_hat = 0.0;
        for (const auto& c : h.fine_cells) {
            double delta_T_hat = c.lst_hat - coarse.LST_ref;
            sum_delta_T_hat += delta_T_hat;
        }
        double delta_T_hat_agg = sum_delta_T_hat / static_cast<double>(N);
        double agg_residual = delta_T_hat_agg - coarse.delta_T_coarse;
        L_phys_agg += agg_residual * agg_residual;

        // Local physical consistency.
        for (const auto& c : h.fine_cells) {
            double delta_T_hat = c.lst_hat - coarse.LST_ref;
            double modeled =
                coarse.alpha * c.ndvi_hat +
                coarse.beta  * c.ndbi_hat +
                coarse.gamma * c.ndwi_hat +
                coarse.delta;
            double local_residual = delta_T_hat - modeled;
            L_phys_local += local_residual * local_residual;
        }

        // Super-resolution reconstruction loss (simplified proxy):
        // Use average fine LST as downsampled coarse LST.
        double sum_lst_hat = 0.0;
        for (const auto& c : h.fine_cells) {
            sum_lst_hat += c.lst_hat;
        }
        double lst_hat_down = sum_lst_hat / static_cast<double>(N);
        // Assume coarse.delta_T_coarse + LST_ref approximates coarse LST.
        double lst_coarse = coarse.delta_T_coarse + coarse.LST_ref;
        double lst_residual = lst_hat_down - lst_coarse;
        L_sr += lst_residual * lst_residual;
    }

    double L_total = L_sr + lambda_agg * L_phys_agg + lambda_local * L_phys_local;
    return {L_sr, L_phys_agg, L_phys_local, L_total};
}

int main_sr() {
    // Synthetic example with one hex and four fine subcells.
    CoarseHexSample coarse{7.0, 35.0, -8.0, 3.0, -5.0, 0.5};
    std::vector<FineSubcellSample> fine = {
        {41.0, 0.35, 0.20, 0.10},
        {42.0, 0.32, 0.22, 0.08},
        {40.5, 0.38, 0.18, 0.12},
        {41.5, 0.34, 0.21, 0.09}
    };
    HexFinePrediction pred{"hex_10_20", coarse, fine};

    auto loss = compute_sr_physical_loss({pred}, 2.0, 1.0);

    std::cout << "Super-resolution physical loss terms:\n"
              << "  L_sr        = " << loss.L_sr << "\n"
              << "  L_phys_agg  = " << loss.L_phys_agg << "\n"
              << "  L_phys_local= " << loss.L_phys_local << "\n"
              << "  L_total     = " << loss.L_total << "\n";

    return 0;
}
