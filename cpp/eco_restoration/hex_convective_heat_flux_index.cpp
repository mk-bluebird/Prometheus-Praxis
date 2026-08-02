// File: cpp/eco_restoration/hex_convective_heat_flux_index.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include "hex_models.hpp"

using namespace hex_analytics;

/**
 * 42. Hex-level convective heat flux index and refined offset model.
 *
 * Convective cooling in urban canyons depends on:
 *   - Average building height H_h (sets canyon depth).
 *   - Street orientation relative to prevailing wind (θ_h).
 *   - Vegetation (NDVI_h) providing shading and evapotranspiration.[168][52][65]
 *
 * We define a convective cooling index CCI_h as:
 *
 *   CCI_h = w_H ⋅ f_height(H_h) +
 *           w_S ⋅ f_street(θ_h, θ_wind) +
 *           w_V ⋅ f_veg(NDVI_h)
 *
 * where:
 *   - f_height(H_h) captures how building height modulates ventilation:
 *       f_height(H_h) = exp(-H_h / H_0)  (taller buildings reduce convection).
 *   - f_street(θ_h, θ_wind) measures alignment of streets with wind:
 *       f_street = cos(θ_rel)^p  for θ_rel = θ_street - θ_wind, p ≥ 1.
 *   - f_veg(NDVI_h) increases convection via turbulence and ET:
 *       f_veg = NDVI_h.
 *
 * Radiative vs convective separation in offset model:
 *
 *   ΔT_h = ΔT_rad,h + ΔT_conv,h
 *
 *   ΔT_rad,h ≈ α_rad V_h + β_rad B_h + γ_rad W_h + δ_rad
 *   ΔT_conv,h ≈ κ ⋅ CCI_h
 *
 * Full refined model:
 *
 *   ΔT_h = α_rad V_h + β_rad B_h + γ_rad W_h + δ_rad + κ ⋅ CCI_h
 *
 * This allows α_rad, β_rad, γ_rad to capture radiative balance, while κ and
 * CCI_h encode convective cooling efficiency, improving physical interpretability.
 */

struct HexUrbanMorphology {
    std::string hex_id;
    double building_height; // H_h (m)
    double street_orientation_deg; // θ_h (deg, principal axis)
    double ndvi;
};

struct ConvectiveParams {
    double w_H;
    double w_S;
    double w_V;
    double H0;
    double p;
    double theta_wind_deg;
    double kappa;
};

double deg_to_rad(double deg) {
    return deg * M_PI / 180.0;
}

double compute_cci(const HexUrbanMorphology& h, const ConvectiveParams& cp) {
    double f_height = std::exp(-h.building_height / cp.H0);

    double theta_rel = (h.street_orientation_deg - cp.theta_wind_deg);
    while (theta_rel < 0.0) theta_rel += 360.0;
    while (theta_rel >= 360.0) theta_rel -= 360.0;
    double theta_rel_rad = deg_to_rad(theta_rel);
    double f_street = std::pow(std::max(0.0, std::cos(theta_rel_rad)), cp.p);

    double f_veg = h.ndvi;

    double CCI = cp.w_H * f_height
               + cp.w_S * f_street
               + cp.w_V * f_veg;
    return CCI;
}

struct RefinedOffsetCoeffs {
    double alpha_rad;
    double beta_rad;
    double gamma_rad;
    double delta_rad;
    double kappa;
};

double compute_refined_delta_T(const HexUrbanMorphology& h,
                               double V_h,
                               double B_h,
                               double W_h,
                               const RefinedOffsetCoeffs& coeffs,
                               const ConvectiveParams& cp) {
    double CCI = compute_cci(h, cp);
    double delta_T_rad = coeffs.alpha_rad * V_h
                       + coeffs.beta_rad  * B_h
                       + coeffs.gamma_rad * W_h
                       + coeffs.delta_rad;
    double delta_T_conv = coeffs.kappa * CCI;
    return delta_T_rad + delta_T_conv;
}

int main_convective_index() {
    HexUrbanMorphology h{"hex_10_20", 12.0, 90.0, 0.35};
    ConvectiveParams cp;
    cp.w_H = 0.5;
    cp.w_S = 1.0;
    cp.w_V = 0.8;
    cp.H0 = 10.0;
    cp.p = 2.0;
    cp.theta_wind_deg = 270.0; // westerly wind
    cp.kappa = -2.0; // negative (higher CCI reduces ΔT)

    RefinedOffsetCoeffs coeffs;
    coeffs.alpha_rad = -7.5;
    coeffs.beta_rad  = 3.0;
    coeffs.gamma_rad = -4.8;
    coeffs.delta_rad = 0.5;
    coeffs.kappa     = cp.kappa;

    double V_h = 0.35;
    double B_h = 0.50;
    double W_h = 0.05;

    double delta_T = compute_refined_delta_T(h, V_h, B_h, W_h, coeffs, cp);

    std::cout << "Hex " << h.hex_id
              << " | CCI=" << compute_cci(h, cp)
              << " | refined ΔT=" << delta_T << "\n";

    return 0;
}
