// filename: src/cpp/cyboquatic_blastradius_engine.cpp
// license: MIT OR Apache-2.0
// role: Non-actuating blast-radius diagnostic kernel for surcharge breaches and canal nodes.
// note: Numeric-only; computes KER and blast radius diagnostics without any IO or actuator control.

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cyboquatic_blastradius {

struct CanalNodeEnvelope {
    const char* node_id;      // Canal node identifier (not dereferenced here).
    double      lat_deg;      // Latitude (degrees).
    double      lon_deg;      // Longitude (degrees);

    // Hydraulic/structural parameters.
    double surcharge_level_m; // Surcharge height above normal water level (m).
    double hydraulic_head_m;  // Hydraulic head (m).
    double soil_permeability; // Normalized soil permeability [0, 1].
    double structural_factor; // Normalized structural robustness [0, 1].
};

struct SurchargeEvent {
    const char* event_id;     // Logical event identifier.
    double      timestamp_s;  // Seconds since epoch.

    // Event parameters.
    double inflow_m3_s;       // Inflow during event (m^3/s).
    double duration_s;        // Duration of surcharge (s).
    double energy_j;          // Energy associated with the event (J).
};

struct BlastRadiusConfig {
    // Coefficients for radius estimation.
    double base_radius_m;     // Base radius (m) for unit surge.
    double k_surcharge;       // Multiplier for surcharge level.
    double k_inflow;          // Multiplier for inflow.
    double k_duration;        // Multiplier for duration.

    // KER weights.
    double w_knowledge;
    double w_ecoimpact;
    double w_risk;

    // Maximum allowed blast radius (diagnostic threshold).
    double max_radius_m;

    // KER limits for compliance reporting.
    double max_residual_ker;
};

struct BlastRadiusOutput {
    // Estimated blast radius (m) and normalized version [0, 1] for EcoNet spines.
    double radius_m;
    double radius_norm;

    // KER coordinates and residual.
    double k_knowledge;
    double e_ecoimpact;
    double r_risk;
    double ker_score;
    double residual_ker;

    // Diagnostic flags.
    bool radius_within_limit;
    bool ker_within_limit;
};

// Helper clamp to [0, 1].
static double clamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

// Compute a simple blast-radius diagnostic for a canal node and surcharge event.
// Returns 0 on success, non-zero on invalid input.
int compute_blast_radius(
    const CanalNodeEnvelope* node,
    const SurchargeEvent*    event,
    const BlastRadiusConfig* config,
    BlastRadiusOutput*       output
) {
    if (node == nullptr || event == nullptr || config == nullptr || output == nullptr) {
        return 1;
    }

    // Basic positive checks.
    if (event->duration_s <= 0.0 || event->inflow_m3_s < 0.0 || node->surcharge_level_m < 0.0) {
        return 2;
    }

    // Radius estimation using a simple parametric form.
    const double surcharge_term = config->k_surcharge * node->surcharge_level_m;
    const double inflow_term    = config->k_inflow    * event->inflow_m3_s;
    const double duration_term  = config->k_duration  * event->duration_s;

    double radius_m = config->base_radius_m + surcharge_term + inflow_term + duration_term;
    if (radius_m < 0.0) {
        radius_m = 0.0;
    }

    output->radius_m   = radius_m;
    output->radius_norm = clamp01(radius_m / config->max_radius_m);

    // KER interpretation:
    // - knowledge: higher when structural_factor is close to 1 and soil_permeability is well-characterized;
    // - ecoimpact: higher when radius_norm is small (less area impacted);
    // - risk: higher when radius_norm is large and surcharge_level is high.

    const double k = clamp01(node->structural_factor * (1.0 - node->soil_permeability));
    const double e = clamp01(1.0 - output->radius_norm);
    const double r = clamp01(output->radius_norm * (1.0 + node->surcharge_level_m));

    output->k_knowledge = k;
    output->e_ecoimpact = e;
    output->r_risk      = r;

    const double ker_score = k * e - r;
    output->ker_score      = ker_score;

    // Residual KER: weighted magnitude of risk components.
    const double residual_ker =
        config->w_knowledge * (1.0 - k) +
        config->w_ecoimpact * (1.0 - e) +
        config->w_risk      * r;

    output->residual_ker = residual_ker;

    // Compliance flags.
    output->radius_within_limit = (radius_m <= config->max_radius_m);
    output->ker_within_limit    = (residual_ker <= config->max_residual_ker);

    return 0;
}

// This kernel provides numeric diagnostics only.
// Rust, Java, SQL, and ALN layers are responsible for persistence and governance binding.

} // namespace cyboquatic_blastradius
