// filename: src/cpp/cyboquatic_drainagedecay_engine.cpp
// license: MIT OR Apache-2.0
// role: Non-actuating drainage-decay kernel mapping BOD/TSS/CEC frames into risk planes and residual slices.
// note: Pure numeric functions; no IO, no device drivers, no actuation.

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cyboquatic_drainagedecay {

struct DrainageFrameInput {
    // Identity
    const char* canal_node_id;   // Canal node identifier (not dereferenced here).
    double      timestamp_s;     // Timestamp (seconds since epoch).

    // Ecological parameters
    double bod_mg_l;             // Biochemical Oxygen Demand (mg/L).
    double tss_mg_l;             // Total Suspended Solids (mg/L).
    double cec_cmol_per_kg;      // Cation Exchange Capacity (cmol/kg).

    // Energy envelope
    double frame_energy_j;       // Energy associated with monitoring/operation for this frame (J).
    double delta_vt_mps;         // Velocity change proxy (m/s) for hydraulic envelope.

    // Prior normalized risk hints (optional; used to compute residual slices).
    double prior_r_bod;
    double prior_r_tss;
    double prior_r_cec;
};

struct DrainageConfig {
    // Bands for normalization based on design constraints.
    double min_bod_mg_l;
    double max_bod_mg_l;

    double min_tss_mg_l;
    double max_tss_mg_l;

    double min_cec_cmol_per_kg;
    double max_cec_cmol_per_kg;

    // Weights for Lyapunov residual over BOD/TSS/CEC risk planes.
    double w_bod;
    double w_tss;
    double w_cec;

    // Maximum allowed per-frame Lyapunov increase.
    double max_delta_v;
};

struct DrainageFrameOutput {
    // Normalized risk coordinates in [0, 1].
    double r_bod;
    double r_tss;
    double r_cec;

    // Residual slices per plane (current - prior), clamped to [0, 1].
    double residual_bod;
    double residual_tss;
    double residual_cec;

    // Lyapunov values and residual.
    double v_t;
    double v_next;
    double delta_v;

    // KER-style aggregates (for ecosafety crates).
    double k_knowledge;
    double e_ecoimpact;
    double r_risk;
    double ker_score;

    // Diagnostic flags.
    bool   lyapunov_ok;
    bool   ecological_ok;   // true if BOD/TSS/CEC remain within bands.
};

static double clamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

static double normalize_band(double value, double v_min, double v_max) {
    if (v_max <= v_min) {
        return 0.0;
    }
    const double scaled = (value - v_min) / (v_max - v_min);
    return clamp01(scaled);
}

static double quadratic_lyapunov(
    double r_bod,
    double r_tss,
    double r_cec,
    double w_bod,
    double w_tss,
    double w_cec
) {
    const double term_bod = w_bod * r_bod * r_bod;
    const double term_tss = w_tss * r_tss * r_tss;
    const double term_cec = w_cec * r_cec * r_cec;
    return term_bod + term_tss + term_cec;
}

// Compute a drainage-decay frame risk and residuals.
// Returns 0 on success, non-zero on invalid input.
int compute_drainagedecay_frame(
    const DrainageFrameInput*  input,
    const DrainageConfig*      config,
    DrainageFrameOutput*       output
) {
    if (input == nullptr || config == nullptr || output == nullptr) {
        return 1;
    }

    // Ecological bands.
    const double r_bod = normalize_band(
        input->bod_mg_l,
        config->min_bod_mg_l,
        config->max_bod_mg_l
    );

    const double r_tss = normalize_band(
        input->tss_mg_l,
        config->min_tss_mg_l,
        config->max_tss_mg_l
    );

    const double r_cec = normalize_band(
        input->cec_cmol_per_kg,
        config->min_cec_cmol_per_kg,
        config->max_cec_cmol_per_kg
    );

    output->r_bod = r_bod;
    output->r_tss = r_tss;
    output->r_cec = r_cec;

    // Per-plane residuals compared to prior hints.
    const double prior_bod = clamp01(input->prior_r_bod);
    const double prior_tss = clamp01(input->prior_r_tss);
    const double prior_cec = clamp01(input->prior_r_cec);

    output->residual_bod = clamp01(r_bod - prior_bod);
    output->residual_tss = clamp01(r_tss - prior_tss);
    output->residual_cec = clamp01(r_cec - prior_cec);

    // Lyapunov evaluation.
    const double v_t = quadratic_lyapunov(
        r_bod,
        r_tss,
        r_cec,
        config->w_bod,
        config->w_tss,
        config->w_cec
    );

    // For v_next, treat delta_vt_mps as a proxy drift factor.
    const double drift_factor = clamp01(std::fabs(input->delta_vt_mps) / 5.0); // 5 m/s band as in SQL trigger.
    const double v_next = v_t + drift_factor * (1.0 - v_t);
    const double delta_v = v_next - v_t;

    output->v_t     = v_t;
    output->v_next  = v_next;
    output->delta_v = delta_v;

    // KER-style aggregates: tighter ecology (lower r_bod, r_tss, r_cec) yields higher k and e, lower risk.
    const double k = clamp01(1.0 - r_bod);
    const double e = clamp01(1.0 - r_tss);
    const double r = clamp01(r_cec);

    output->k_knowledge = k;
    output->e_ecoimpact = e;
    output->r_risk      = r;
    output->ker_score   = k * e - r;

    // Flags.
    const bool bod_in_band = (input->bod_mg_l >= config->min_bod_mg_l) &&
                             (input->bod_mg_l <= config->max_bod_mg_l);
    const bool tss_in_band = (input->tss_mg_l >= config->min_tss_mg_l) &&
                             (input->tss_mg_l <= config->max_tss_mg_l);
    const bool cec_in_band = (input->cec_cmol_per_kg >= config->min_cec_cmol_per_kg) &&
                             (input->cec_cmol_per_kg <= config->max_cec_cmol_per_kg);

    output->ecological_ok = bod_in_band && tss_in_band && cec_in_band;
    output->lyapunov_ok   = (delta_v <= config->max_delta_v);

    return 0;
}

// This engine does not perform any persistence.
// Rust, Java, or SQL layers are responsible for writing DrainageFrameOutput into DB tables and views.

} // namespace cyboquatic_drainagedecay
