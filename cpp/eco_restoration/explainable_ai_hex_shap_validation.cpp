// File: cpp/eco_restoration/explainable_ai_hex_shap_validation.cpp

#include <vector>
#include <string>
#include <iostream>
#include <cmath>

/**
 * 43. Explainable AI for hex intervention decisions using SHAP.
 *
 * Scenario:
 *  - A random forest or gradient boosting model (GBM) predicts UHI_h from
 *    many hex features: NDVI, NDBI, NDWI, building height, albedo, soil
 *    indices, traffic, etc.
 *  - The linear offset model provides global coefficients α, β, γ for
 *    V_h, B_h, W_h. We want to validate that these remain dominant drivers
 *    and detect when non-linear interactions become important.
 *
 * SHAP-based validation pattern:
 *
 * 1. Train tree model:
 *    - Features x_h = [NDVI_h, NDBI_h, NDWI_h, H_h, albedo_h, traffic_h, ...].
 *    - Target y_h = UHI_h (or ΔT_h).
 *
 * 2. Compute SHAP values φ_i(h) for each feature i at each hex h using a
 *    treeSHAP implementation for RF/GBM. For each feature i:
 *
 *      φ_i(h) measures the contribution of feature i to the prediction
 *      at hex h relative to a baseline.[155][152]
 *
 * 3. Aggregate SHAP importance:
 *
 *      I_i = mean_h |φ_i(h)|
 *
 *    - Rank features by I_i to obtain global importance.
 *    - Compare I_NDVI, I_NDBI, I_NDWI to other features’ importance.
 *    - If NDVI/NDBI/NDWI remain top contributors, this supports the
 *      dominance of α, β, γ as physical drivers.
 *
 * 4. Interaction effects:
 *    - Compute SHAP interaction values φ_{i,j}(h) for feature pairs
 *      (i,j), e.g., (NDVI, building_height) or (NDBI, albedo_h).
 *    - Large |φ_{i,j}| relative to |φ_i|, |φ_j| indicates important
 *      non-linear interactions beyond the linear offset model.
 *
 * 5. Validation of α, β, γ:
 *    - Fit the linear offset model on the same dataset to get α_lin, β_lin, γ_lin.
 *    - Compare SHAP “slope” estimates for NDVI, NDBI, NDWI (e.g., partial
 *      dependence curves) to α_lin, β_lin, γ_lin.
 *    - If SHAP-based marginal effects closely match the linear coefficients,
 *      the linear model remains a good global approximation; deviations
 *      highlight where non-linearities matter (e.g., high NDVI saturation
 *      or threshold effects).
 */

struct ShapFeatureContribution {
    std::string hex_id;
    double shap_ndvi;
    double shap_ndbi;
    double shap_ndwi;
    double shap_height;
    double shap_albedo;
};

struct ShapImportance {
    double I_ndvi;
    double I_ndbi;
    double I_ndwi;
    double I_height;
    double I_albedo;
};

ShapImportance compute_global_importance(const std::vector<ShapFeatureContribution>& shap_values) {
    double sum_ndvi = 0.0, sum_ndbi = 0.0, sum_ndwi = 0.0;
    double sum_height = 0.0, sum_albedo = 0.0;
    int n = static_cast<int>(shap_values.size());

    for (const auto& s : shap_values) {
        sum_ndvi   += std::fabs(s.shap_ndvi);
        sum_ndbi   += std::fabs(s.shap_ndbi);
        sum_ndwi   += std::fabs(s.shap_ndwi);
        sum_height += std::fabs(s.shap_height);
        sum_albedo += std::fabs(s.shap_albedo);
    }

    ShapImportance imp;
    imp.I_ndvi   = (n > 0) ? sum_ndvi   / n : 0.0;
    imp.I_ndbi   = (n > 0) ? sum_ndbi   / n : 0.0;
    imp.I_ndwi   = (n > 0) ? sum_ndwi   / n : 0.0;
    imp.I_height = (n > 0) ? sum_height / n : 0.0;
    imp.I_albedo = (n > 0) ? sum_albedo / n : 0.0;
    return imp;
}

int main_shap() {
    // Synthetic SHAP contributions for a few hexes.
    std::vector<ShapFeatureContribution> shap_values = {
        {"hex_10_20", 1.2, 0.8, 0.5, 0.3, 0.2},
        {"hex_11_20", 1.0, 0.9, 0.4, 0.5, 0.3},
        {"hex_12_20", 1.3, 0.7, 0.6, 0.4, 0.4}
    };

    ShapImportance imp = compute_global_importance(shap_values);

    std::cout << "Global SHAP importance:\n";
    std::cout << "  NDVI  = " << imp.I_ndvi   << "\n";
    std::cout << "  NDBI  = " << imp.I_ndbi   << "\n";
    std::cout << "  NDWI  = " << imp.I_ndwi   << "\n";
    std::cout << "  height= " << imp.I_height << "\n";
    std::cout << "  albedo= " << imp.I_albedo << "\n";

    return 0;
}
