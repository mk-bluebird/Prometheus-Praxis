// File: ecorestorationshard/tools/egd_happiness/src/EcoToGrossDomesticHappiness.cpp

#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

/**
 * Domain: Eco‑to‑Gross‑Domestic‑Happiness (EGDH) translation layer.
 *
 * This C++ module computes a greenness index in [0,1] for a region
 * from the mean E‑score of cyboquatic workloads, using a logistic
 * mapping calibrated to local restoration targets.
 *
 * The design assumes:
 *  - E‑scores are already corridor‑backed and in [0,1].
 *  - Local bands (degraded, neutral, target) are supplied from
 *    ecowealth portfolio views or corridor shards.
 *
 * No external dependencies beyond the C++ standard library.
 */

struct RegionEBandCalibration {
    // Mean E in degraded regions (net eco‑wealth decline).
    double e_degraded;
    // Mean E in neutral regions (no net restoration progress).
    double e_mid;
    // Mean E in target/gold restoration band (policy goals hit).
    double e_target;
    // Desired greenness for degraded band (e.g. 0.2).
    double g_degraded;
    // Desired greenness for target band (e.g. 0.85).
    double g_target;

    RegionEBandCalibration(
        double eDeg,
        double eMid,
        double eTarget,
        double gDeg = 0.2,
        double gTgt = 0.85
    )
        : e_degraded(eDeg),
          e_mid(eMid),
          e_target(eTarget),
          g_degraded(gDeg),
          g_target(gTgt)
    {
        if (e_target <= e_mid) {
            throw std::invalid_argument(
                "e_target must be greater than e_mid for calibration."
            );
        }
        if (g_degraded <= 0.0 || g_degraded >= 0.5) {
            throw std::invalid_argument(
                "g_degraded should be in (0,0.5) for meaningful bands."
            );
        }
        if (g_target <= 0.5 || g_target >= 1.0) {
            throw std::invalid_argument(
                "g_target should be in (0.5,1) for meaningful bands."
            );
        }
    }
};

struct LogisticGreennessParams {
    double alpha;       // Steepness parameter (>0).
    double e_mid;       // Neutral mean E.
    double e_scale;     // Scale for logistic input.

    LogisticGreennessParams(double a, double mid, double scale)
        : alpha(a), e_mid(mid), e_scale(scale)
    {
        if (alpha <= 0.0) {
            throw std::invalid_argument("alpha must be > 0.");
        }
        if (e_scale <= 0.0) {
            throw std::invalid_argument("e_scale must be > 0.");
        }
    }
};

/**
 * Compute logistic parameters for a region given band calibration.
 * We use a simple two‑point calibration:
 *
 *   G(e_mid)   ≈ 0.5
 *   G(e_target) ≈ g_target
 *
 * and check that G(e_degraded) ≈ g_degraded. If the degraded point
 * is too far off, the caller can re‑tune bands.
 */
inline LogisticGreennessParams
calibrate_logistic_params(const RegionEBandCalibration &calib)
{
    // Fix e_mid and scale between mid and target.
    const double e_mid = calib.e_mid;
    const double e_scale = calib.e_target - calib.e_mid;

    // We want G(e_mid) ≈ 0.5, so x_mid = 0 and G(0) = 1/(1+exp(0)) = 0.5.
    // For e_target:
    //   x_target = (e_target - e_mid) / e_scale = 1
    //   G_target = 1 / (1 + exp(-alpha))
    //
    // Solve for alpha:
    const double g_t = calib.g_target;
    if (g_t <= 0.5 || g_t >= 1.0) {
        throw std::invalid_argument("g_target must be in (0.5,1).");
    }

    const double ratio = (1.0 / g_t) - 1.0; // = exp(-alpha)
    const double alpha = -std::log(ratio);

    return LogisticGreennessParams(alpha, e_mid, e_scale);
}

/**
 * Logistic greenness index in [0,1] from mean E.
 *
 *   x = (E_mean - e_mid) / e_scale
 *   G = 1 / (1 + exp(-alpha * x))
 *
 * The caller must supply parameters calibrated via
 * calibrate_logistic_params.
 */
inline double greenness_index(double e_mean, const LogisticGreennessParams &p)
{
    // Guard against extreme inputs but do not clamp E; we allow mean
    // E to wander slightly outside [0,1] if corridor math permits.
    const double x = (e_mean - p.e_mid) / p.e_scale;
    const double exponent = -p.alpha * x;

    // Numerical stability: cap exponent to avoid overflow.
    const double capped = std::max(std::min(exponent, 50.0), -50.0);
    const double g = 1.0 / (1.0 + std::exp(capped));

    // Ensure return is strictly in (0,1).
    if (!std::isfinite(g)) {
        throw std::runtime_error("Non‑finite greenness index.");
    }
    return g;
}

/**
 * Convenience aggregation: compute mean E and greenness for a region.
 */
struct RegionGreennessResult {
    double mean_E;
    double greenness;
};

inline RegionGreennessResult
compute_region_greenness(
    const std::vector<double> &e_scores,
    const RegionEBandCalibration &calib
)
{
    if (e_scores.empty()) {
        throw std::invalid_argument("e_scores cannot be empty.");
    }

    double sum = 0.0;
    for (double e : e_scores) {
        if (!std::isfinite(e)) {
            throw std::invalid_argument("E‑score must be finite.");
        }
        sum += e;
    }
    const double mean_E = sum / static_cast<double>(e_scores.size());

    const LogisticGreennessParams params = calibrate_logistic_params(calib);
    const double g = greenness_index(mean_E, params);

    RegionGreennessResult result;
    result.mean_E = mean_E;
    result.greenness = g;
    return result;
}

/**
 * Simple Eco‑to‑Gross‑Domestic‑Happiness proxy:
 *
 *   EGDH = greenness * H_local
 *
 * where H_local is a normalized local happiness or
 * wellbeing scalar in [0,1] (e.g. from non‑financial
 * ecowealth portfolios).
 */
inline double egdh_score(double greenness, double local_happiness_norm)
{
    if (!std::isfinite(greenness) || !std::isfinite(local_happiness_norm)) {
        throw std::invalid_argument("Inputs must be finite.");
    }
    if (greenness < 0.0 || greenness > 1.0) {
        throw std::invalid_argument("Greenness must be in [0,1].");
    }
    if (local_happiness_norm < 0.0 || local_happiness_norm > 1.0) {
        throw std::invalid_argument("Local happiness must be in [0,1].");
    }

    return greenness * local_happiness_norm;
}

/**
 * Example policy‑grade wrapper:
 * Convert a region's cyboquatic E‑scores plus local
 * happiness scalar into a public‑facing index.
 */
struct PolicyGreennessIndex {
    std::string region_id;
    double mean_E;
    double greenness;
    double egdh;
};

inline PolicyGreennessIndex
compute_policy_index(
    const std::string &region_id,
    const std::vector<double> &e_scores,
    const RegionEBandCalibration &calib,
    double local_happiness_norm
)
{
    RegionGreennessResult g_res = compute_region_greenness(e_scores, calib);
    const double egdh = egdh_score(g_res.greenness, local_happiness_norm);

    PolicyGreennessIndex idx;
    idx.region_id = region_id;
    idx.mean_E = g_res.mean_E;
    idx.greenness = g_res.greenness;
    idx.egdh = egdh;
    return idx;
}
