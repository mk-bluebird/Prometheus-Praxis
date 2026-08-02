// File: cpp/eco_restoration/hex_models.hpp
// Shared data structures for hex analytics modules

#ifndef HEX_MODELS_HPP
#define HEX_MODELS_HPP

#include <string>
#include <vector>

namespace hex_analytics {

/**
 * Core metrics for a hex cell used across cooling decision modules.
 */
struct HexMetrics {
    std::string hex_id;
    double UHI;         // current UHI_h
    double alpha;       // cooling coefficient for vegetation
    double beta;        // roof/built coefficient
    double gamma;       // water coefficient
    double delta;       // intercept
    double dV_max;      // max feasible vegetation increment
    double dB_opt;      // optimal cool-roof ΔB (negative)
    double dW_max;      // max feasible water increment
};

/**
 * Cooling command issued by edge decision logic.
 */
enum class CoolingActionType {
    None,
    ActivateMisting,
    DeployShade,
    TriggerCoolRoofRetrofit
};

struct CoolingCommand {
    CoolingActionType action;
    std::string hex_id;
    double expected_delta_T; // predicted cooling from action
};

/**
 * Urban morphology parameters for convective heat flux calculations.
 */
struct HexUrbanMorphology {
    std::string hex_id;
    double building_height;      // H_h (m)
    double street_orientation_deg; // θ_h (deg, principal axis)
    double ndvi;
};

/**
 * Parameters for convective cooling index (CCI) computation.
 */
struct ConvectiveParams {
    double w_H;
    double w_S;
    double w_V;
    double H0;
    double p;
    double theta_wind_deg;
    double kappa;
};

/**
 * Refined offset model coefficients including radiative and convective terms.
 */
struct RefinedOffsetCoeffs {
    double alpha_rad;
    double beta_rad;
    double gamma_rad;
    double delta_rad;
    double kappa;
};

/**
 * Sample data point for climate stress-test scenarios.
 */
struct HexScenarioSample {
    double V;       // vegetation index
    double B;       // built/roof index
    double W;       // water/wetness index
    double delta_T; // simulated ΔT under scenario
};

/**
 * Standard offset model coefficients (α, β, γ, δ).
 */
struct OffsetCoeffs {
    double alpha;
    double beta;
    double gamma;
    double delta;
};

/**
 * Per-hex heat budget state for corridor energy balance.
 */
struct HexHeatBudgetState {
    std::string hex_id;
    double Q_star; // net radiation Q*_h (W/m^2)
    double QF;     // anthropogenic heat QF_h (W/m^2)
    double V;      // vegetation index
    double B;      // built/roof index
    double W;      // water index
};

/**
 * Parameters for computing fluxes from offset coefficients.
 */
struct OffsetFluxParams {
    double alpha;
    double beta;
    double gamma;
    double L0;
    double H0;
};

/**
 * Computed flux components for a single hex.
 */
struct HexFluxes {
    double QH;          // sensible
    double QE;          // latent
    double Q_storage;   // storage (residual)
};

/**
 * Aggregated heat budget for a corridor of hexes.
 */
struct CorridorHeatBudget {
    double sum_Q_star;
    double sum_QF;
    double sum_QH;
    double sum_QE;
    double sum_QS;
};

/**
 * Sample for regression-based AIC/IC criterion evaluation.
 */
struct HexRegressionSample {
    double V;
    double B;
    double W;
    double delta_T;
};

} // namespace hex_analytics

#endif // HEX_MODELS_HPP
