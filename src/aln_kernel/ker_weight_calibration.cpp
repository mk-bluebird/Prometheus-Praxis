// filename: src/aln_kernel/ker_weight_calibration.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (non-actuating)
// license: MIT OR Apache-2.0

#include <cstdint>
#include <cstddef>

// Input samples: paired surcharge and NDVI-derived risk coordinates
// and an observed Lyapunov residual V_obs over time.[file:11]
struct KerCalibrationSample {
    // Normalized risk coordinates in [0,1].[file:11]
    float r_surcharge; // hydraulic surcharge risk
    float r_ndvi;      // greenness / UHI mitigation risk

    // Observed residual V_t at this time step.[file:11]
    float V_obs;
};

// Calibration configuration, including non-compensatability constraints.[file:11]
struct KerCalibrationConfig {
    // Number of samples in the array.[file:11]
    std::size_t sample_count;

    // Target: V_t ≈ w_surcharge * r_surcharge^2 + w_ndvi * r_ndvi^2.[file:11]
    // Constraints:
    //  - w_surcharge, w_ndvi >= 0
    //  - w_surcharge >= w_ndvi (non-compensatability: surcharge cannot
    //    be underweighted relative to NDVI).[file:11]
    float w_surcharge_min;
    float w_ndvi_min;
};

// Calibrated weights to be written back into weight shards.[file:11]
struct KerWeights {
    float w_surcharge;
    float w_ndvi;
};

// Simple constrained LS calibration for two weights.
// We solve an unconstrained LS for w_surcharge and w_ndvi,
// then project onto the constraint set:
//  - w_surcharge >= w_surcharge_min
//  - w_ndvi >= w_ndvi_min
//  - w_surcharge >= w_ndvi.[file:11]
static KerWeights ker_calibrate_weights(const KerCalibrationSample* samples,
                                        const KerCalibrationConfig& cfg)
{
    KerWeights out{};
    if (!samples || cfg.sample_count == 0U) {
        out.w_surcharge = cfg.w_surcharge_min;
        out.w_ndvi      = cfg.w_ndvi_min;
        return out;
    }

    // Build normal equations for LS:
    // Let x1 = r_surcharge^2, x2 = r_ndvi^2, y = V_obs.
    // We solve for w in:
    // [sum x1^2  sum x1 x2] [w1] = [sum x1 y]
    // [sum x1 x2 sum x2^2] [w2]   [sum x2 y].[file:11]
    double s_x1x1 = 0.0;
    double s_x2x2 = 0.0;
    double s_x1x2 = 0.0;
    double s_x1y  = 0.0;
    double s_x2y  = 0.0;

    for (std::size_t i = 0; i < cfg.sample_count; ++i) {
        const KerCalibrationSample& s = samples[i];

        float x1 = s.r_surcharge * s.r_surcharge;
        float x2 = s.r_ndvi * s.r_ndvi;
        float y  = s.V_obs;

        s_x1x1 += static_cast<double>(x1) * static_cast<double>(x1);
        s_x2x2 += static_cast<double>(x2) * static_cast<double>(x2);
        s_x1x2 += static_cast<double>(x1) * static_cast<double>(x2);
        s_x1y  += static_cast<double>(x1) * static_cast<double>(y);
        s_x2y  += static_cast<double>(x2) * static_cast<double>(y);
    }

    // Solve 2x2 system via explicit inverse (if determinant non-zero).[file:11]
    double det = s_x1x1 * s_x2x2 - s_x1x2 * s_x1x2;

    double w1 = cfg.w_surcharge_min;
    double w2 = cfg.w_ndvi_min;

    if (det != 0.0) {
        double inv11 =  s_x2x2 / det;
        double inv12 = -s_x1x2 / det;
        double inv21 = -s_x1x2 / det;
        double inv22 =  s_x1x1 / det;

        double w1_ls = inv11 * s_x1y + inv12 * s_x2y;
        double w2_ls = inv21 * s_x1y + inv22 * s_x2y;

        w1 = w1_ls;
        w2 = w2_ls;
    }

    // Enforce non-negativity and minimums.[file:11]
    if (w1 < static_cast<double>(cfg.w_surcharge_min)) {
        w1 = static_cast<double>(cfg.w_surcharge_min);
    }
    if (w2 < static_cast<double>(cfg.w_ndvi_min)) {
        w2 = static_cast<double>(cfg.w_ndvi_min);
    }

    // Enforce non-compensatability: surcharge weight >= NDVI weight.[file:11]
    if (w1 < w2) {
        // Project onto w1 = w2 by raising w1, preserving w2;
        // this keeps hydraulic risk at least as important as NDVI.[file:11]
        w1 = w2;
    }

    out.w_surcharge = static_cast<float>(w1);
    out.w_ndvi      = static_cast<float>(w2);
    return out;
}

// -----------------------------------------------------------------------------
// C ABI for Prometheus-Praxis integration
// -----------------------------------------------------------------------------

extern "C" {

// Perform weight calibration and return updated weights.
// Writing back to weight shards (SQLite / ALN) is handled by
// higher-level Rust code; this C++ module is strictly non-actuating
// and non-persistent.[file:11]
void ker_weight_calibration_run(const KerCalibrationSample* samples,
                                KerCalibrationConfig* cfg,
                                KerWeights* out_weights)
{
    if (!samples || !cfg || !out_weights) {
        return;
    }
    *out_weights = ker_calibrate_weights(samples, *cfg);
}

}
