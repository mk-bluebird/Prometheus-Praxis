// File: cpp/tools/eco_headers_with_doxygen.hpp
#ifndef ECO_HEADERS_WITH_DOXYGEN_HPP
#define ECO_HEADERS_WITH_DOXYGEN_HPP

#include <vector>

/**
 * @file eco_headers_with_doxygen.hpp
 * @brief Doxygen-style documentation for eco-restoration C++ utilities.
 *
 * This header aggregates rich comments describing KER semantics, Lyapunov
 * residuals, PFAS corridor behavior, and input ranges so AI agents and
 * tooling can parse and reason about eco-restoration code paths.[59][78]
 */

namespace eco_tools {

/**
 * @brief Compute the KER composite score.
 *
 * The KER score encodes the coupling between:
 * - @c k : Knowledge/safe-step fraction over a diagnostic window (K in [0,1]).
 * - @c e : Eco-impact margin, typically defined as @c e = 1 - R where R is the
 *          maximum risk coordinate, with e in [0,1].
 * - @c r : Maximum normalized risk-of-harm coordinate @c r_max in [0,1].
 *
 * The scalar KER score is:
 * @f[
 *   s = k \cdot e - r
 * @f]
 * as described in the governance markdown. Positive @c s implies that the
 * Lyapunov residual @f$V_t = \sum_j w_j r_j^2@f$ is expected to decrease
 * under corridor design, enforcing @f$V_{t+1} - V_t \le -\alpha s_t@f$ for
 * some @f$\alpha>0@f$.[59]
 *
 * @param k Knowledge/safe-step fraction in [0,1].
 * @param e Eco-impact margin in [0,1].
 * @param r Maximum risk-of-harm coordinate in [0,1].
 * @return KER composite score @c s = k * e - r (may be negative if risk dominates).
 *
 * @throws std::invalid_argument if any parameter lies outside [0,1].
 */
double ker_score(double k, double e, double r);

/**
 * @brief Compute the Lyapunov residual over risk planes.
 *
 * The Lyapunov residual @c V_t aggregates plane-specific risk coordinates
 * using nonnegative weights:
 * @f[
 *   V_t = \sum_{j=1}^n w_j r_j^2,\quad w_j \ge 0
 * @f]
 * where @c r_j are normalized risk coordinates in [0,1] for hydraulics,
 * energy, topology, biodiversity, and other planes. High-hazard planes
 * (e.g., biodiversity, neurorights) are marked non-offsettable in the
 * governance docs: their weights are chosen large enough that improvements
 * in other planes cannot compensate for worsening in those coordinates.[59]
 *
 * @param w Vector of nonnegative weights @c w_j corresponding to each risk plane.
 * @param r Vector of nonnegative risk coordinates @c r_j in [0,1].
 * @return Lyapunov residual @c V_t.
 *
 * @throws std::invalid_argument if @c w and @c r differ in length or if any
 *         weight or risk coordinate is negative.
 */
double lyapunov_residual(const std::vector<double>& w,
                         const std::vector<double>& r);

} // namespace eco_tools

namespace eco_pfas {

/**
 * @brief PFAS corridor state variables for qpudatashard-style fate modeling.
 *
 * This struct represents the PFAS state in a canal corridor:
 * - @c mass_kg : Total PFAS mass in kilograms.
 * - @c sorbed_fraction : Fraction of PFAS sorbed to sediments [0,1].
 * - @c cold_survival_factor : Dimensionless factor capturing cold-survival
 *   behavior. Higher values indicate slower degradation at low temperatures,
 *   as described in PFAS fate corridor docs.[59]
 */
struct PFASState {
    double mass_kg;
    double sorbed_fraction;
    double cold_survival_factor;
};

/**
 * @brief Step PFAS state forward in discrete time under a corridor design.
 *
 * The update rule follows qpudatashard semantics:
 * - Base degradation rate @c base_degradation_rate is slowed when canal/sediment
 *   temperature @c current_temp_C is below @c cold_temp_C, increasing the
 *   cold-survival factor.
 * - Mass decreases according to an effective rate @c rate / (1 + cold_survival_factor).
 * - Sorbed fraction increases by @c sorption_increment but is clamped to [0,1].
 *
 * This function is typically used in tandem with a Lyapunov residual over PFAS
 * mass, sorbed fraction, and cold-survival factor to enforce non-increasing risk
 * under corridor constraints.[59]
 *
 * @param state Current PFAS corridor state.
 * @param base_degradation_rate Fractional mass loss per step at reference temperature (0–1).
 * @param current_temp_C Current canal/sediment temperature in degrees Celsius.
 * @param cold_temp_C Cold-survival corridor threshold temperature (e.g., 12°C).
 * @param sorption_increment Increment in sorbed fraction per step (can be negative).
 * @return Next PFAS state after one discrete step.
 */
PFASState step_pfas_corridor(const PFASState& state,
                             double base_degradation_rate,
                             double current_temp_C,
                             double cold_temp_C,
                             double sorption_increment);

} // namespace eco_pfas

#endif // ECO_HEADERS_WITH_DOXYGEN_HPP
