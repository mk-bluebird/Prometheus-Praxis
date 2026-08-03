// File: cpp/tools/rust_bridge_harness.cpp
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

/**
 * @brief C interfaces exported by existing Rust eco-crates.
 *
 * These declarations must match the signatures already exposed by Rust
 * (e.g., via `#[no_mangle] extern "C"`), and are used here without
 * introducing any new crates or Rust build steps.
 *
 * Example Rust side (for reference, not compiled here):
 * ```rust
 * #[repr(C)]
 * pub struct MaterialTestParamsC { ... }
 * #[repr(C)]
 * pub struct MaterialEcoImpactC { ... }
 *
 * #[no_mangle]
 * pub extern "C" fn compute_material_eco_impact(params: MaterialTestParamsC)
 *     -> MaterialEcoImpactC { ... }
 * ```
 */

extern "C" {

    struct MaterialTestParamsC {
        double oxygen_depletion_percent;
        double co2_evolution_percent;
        double bod_removal_percent;
        double doc_removal_percent;
        double days_to_pass_window;
        double toxicity_score;
        double pfas_presence;
    };

    struct MaterialEcoImpactC {
        double k_safe_fraction;
        double e_eco_benefit_band;
        double r_risk_max;
        double ker_score;
        double biodegradability_score;
    };

    // Rust-exported eco engine for material impact.
    MaterialEcoImpactC compute_material_eco_impact(MaterialTestParamsC params);

    // Example Rust-exported PFAS corridor step function.
    struct PFASStateC {
        double mass_kg;
        double sorbed_fraction;
        double cold_survival_factor;
    };

    PFASStateC pfas_corridor_step(PFASStateC state,
                                  double base_degradation_rate,
                                  double current_temp_C,
                                  double cold_temp_C,
                                  double sorption_increment);

    // Example Rust-exported KER summary function over a risk vector.
    double ker_score_bridge(double k, double e, double r_max);
}

/**
 * @brief Run a small material eco-impact scenario via Rust FFI.
 *
 * @param label Scenario name for logging.
 */
void run_material_scenario(const std::string& label) {
    MaterialTestParamsC p{};
    p.oxygen_depletion_percent = 65.0;
    p.co2_evolution_percent    = 62.0;
    p.bod_removal_percent      = 60.0;
    p.doc_removal_percent      = 75.0;
    p.days_to_pass_window      = 9.0;
    p.toxicity_score           = 0.2;
    p.pfas_presence            = 0.1;

    MaterialEcoImpactC impact = compute_material_eco_impact(p);

    std::cout << "[material-scenario] " << label << "\n"
              << "  k_safe_fraction=" << impact.k_safe_fraction << "\n"
              << "  e_eco_benefit_band=" << impact.e_eco_benefit_band << "\n"
              << "  r_risk_max=" << impact.r_risk_max << "\n"
              << "  ker_score=" << impact.ker_score << "\n"
              << "  biodegradability_score=" << impact.biodegradability_score << "\n";
}

/**
 * @brief Run a PFAS corridor scenario via Rust FFI, aligned with qpudatashard semantics.
 */
void run_pfas_scenario() {
    PFASStateC state{};
    state.mass_kg = 0.002;
    state.sorbed_fraction = 0.5;
    state.cold_survival_factor = 1.0;

    double base_rate = 0.01;
    double current_temp_C = 10.0;
    double cold_temp_C = 12.0;
    double sorption_increment = 0.001;

    PFASStateC next = pfas_corridor_step(state, base_rate, current_temp_C, cold_temp_C, sorption_increment);

    std::cout << "[pfas-scenario]\n"
              << "  mass_kg=" << next.mass_kg << "\n"
              << "  sorbed_fraction=" << next.sorbed_fraction << "\n"
              << "  cold_survival_factor=" << next.cold_survival_factor << "\n";
}

/**
 * @brief Run a KER-Lyapunov coupling scenario against Rust KER bridge.
 */
void run_ker_scenario() {
    double r_h = 0.20;
    double r_e = 0.15;
    double r_t = 0.10;
    double r_b = 0.12;
    double r_max = std::max(std::max(r_h, r_e), std::max(r_t, r_b));

    double k = 0.94;          // window K fraction, consistent with governance bands.[59]
    double e = 1.0 - r_max;   // eco-impact margin
    if (e < 0.0) e = 0.0;

    double s = ker_score_bridge(k, e, r_max);

    std::cout << "[ker-scenario]\n"
              << "  r_max=" << r_max << "\n"
              << "  k=" << k << " e=" << e << "\n"
              << "  ker_score=" << s << "\n";
}

int main() {
    try {
        run_material_scenario("ISO/OECD ready-biodegradable test material");
        run_pfas_scenario();
        run_ker_scenario();
    } catch (const std::exception& ex) {
        std::cerr << "Rust bridge harness error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
