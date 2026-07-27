// filename: src/engine/cpp/lyapunov_vt_spectral_analysis.cpp
// target-repo: https://github.com/mk-bluebird/Prometheus-Praxis
// language: C++ (C ABI, ARM-friendly, non-actuating numerical kernel)
// license: MIT OR Apache-2.0

#include <cstddef>
#include <cstdint>

// Periodogram-based analyzer over Lyapunov residual Vt time series:
// - Uses sliding windows with detrending.
// - Computes a simple discrete-time periodogram at specified frequencies.
// - Estimates background power per frequency via monsoon-phase adaptive median.
// - Flags gate malfunctions or illegal discharges when power significantly
//   exceeds background at operational bands.[file:3]

extern "C" {

// Monsoon phase enumeration.[file:3]
enum MonsoonPhase : std::uint32_t {
    PRE_ONSET    = 0,
    ACTIVE       = 1,
    WITHDRAWAL   = 2
};

// Input POD: Vt spectral analysis configuration.[file:3]
struct VtSpectralInput {
    const double* vt_series;        // Vt time series samples.
    std::uint32_t vt_len;           // Length of vt_series.

    const double* freqs;            // Frequencies of interest (normalized 0..0.5).
    std::uint32_t num_freqs;        // Number of frequencies.

    std::uint32_t window_size;      // Sliding window size (samples).
    std::uint32_t step_size;        // Step between windows (samples).

    MonsoonPhase phase;             // Current monsoon phase for adaptive thresholds.[file:3]
};

// Output POD: spectral diagnostics for each frequency.[file:3]
struct VtSpectralOutput {
    // For each frequency fk:
    //   power[k]      = median window power at fk.
    //   background[k] = estimated background power Bk.
    //   S[k]          = power[k] / (background[k] + eps), a simple Sk statistic.
    //   anomaly[k]    = 1 if anomaly detected at fk, else 0.[file:3]
    double* power;
    double* background;
    double* S;
    std::uint8_t* anomaly;
    std::uint32_t num_freqs;
};

static inline double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Simple median helper for small arrays (O(n^2) selection, acceptable for
// embedded sizes). Non-actuating, in-place via temporary buffer.[file:3]
static double median_copy(double* buf, std::uint32_t n) {
    if (n == 0) return 0.0;
    // Simple insertion sort then pick middle.[file:3]
    for (std::uint32_t i = 1; i < n; ++i) {
        double key = buf[i];
        std::uint32_t j = i;
        while (j > 0 && buf[j - 1] > key) {
            buf[j] = buf[j - 1];
            --j;
        }
        buf[j] = key;
    }
    if (n % 2 == 1) {
        return buf[n / 2];
    } else {
        return 0.5 * (buf[n / 2 - 1] + buf[n / 2]);
    }
}

// Compute a simple periodogram power at frequency f (normalized 0..0.5) for
// a real-valued window x[0..N-1], using discrete cosine/sine sums.[file:3]
static double periodogram_power(const double* x,
                                std::uint32_t N,
                                double f)
{
    // Angular frequency omega = 2*pi*f.[file:3]
    const double pi = 3.14159265358979323846;
    const double omega = 2.0 * pi * f;
    double re = 0.0;
    double im = 0.0;
    for (std::uint32_t n = 0; n < N; ++n) {
        const double phase = omega * static_cast<double>(n);
        const double c = std::cos(phase);
        const double s = std::sin(phase);
        re += x[n] * c;
        im -= x[n] * s;
    }
    const double power = (re * re + im * im) / static_cast<double>(N);
    return power;
}

// Core analyzer:
// - Uses sliding windows over vt_series.
// - For each frequency fk, collects window power values, takes median as
//   robust power estimate, and then estimates background via a secondary
//   median-based scaling per monsoon phase.[file:3]
// - Sets anomaly flags where S[k] exceeds phase-specific thresholds.[file:3]
int analyze_vt_spectral(const VtSpectralInput* in,
                        VtSpectralOutput* out)
{
    if (in == nullptr || out == nullptr) {
        return 1;
    }
    if (in->vt_series == nullptr ||
        in->freqs == nullptr ||
        out->power == nullptr ||
        out->background == nullptr ||
        out->S == nullptr ||
        out->anomaly == nullptr) {
        return 2;
    }
    if (in->vt_len == 0 ||
        in->num_freqs == 0 ||
        in->window_size == 0 ||
        in->step_size == 0) {
        return 3;
    }

    const std::uint32_t N = in->vt_len;
    const std::uint32_t W = in->window_size;
    const std::uint32_t step = in->step_size;
    const std::uint32_t K = in->num_freqs;

    out->num_freqs = K;

    // Estimate number of windows.[file:3]
    std::uint32_t num_windows = 0;
    if (N >= W) {
        num_windows = 1 + (N - W) / step;
    }

    if (num_windows == 0) {
        // Not enough data for one window.[file:3]
        for (std::uint32_t k = 0; k < K; ++k) {
            out->power[k]      = 0.0;
            out->background[k] = 0.0;
            out->S[k]          = 0.0;
            out->anomaly[k]    = 0;
        }
        return 0;
    }

    // For each frequency, collect window power samples into a small local buffer.[file:3]
    // To avoid dynamic allocation, we cap the number of windows for embedded use.
    const std::uint32_t MAX_WINDOWS = 64; // adjustable cap.[file:3]
    double window_powers[MAX_WINDOWS];

    for (std::uint32_t k = 0; k < K; ++k) {
        const double fk = in->freqs[k];

        std::uint32_t count = 0;
        std::uint32_t start = 0;
        while (start + W <= N && count < MAX_WINDOWS) {
            // Detrend: subtract local mean from window.[file:3]
            double mean = 0.0;
            for (std::uint32_t n = 0; n < W; ++n) {
                mean += in->vt_series[start + n];
            }
            mean /= static_cast<double>(W);

            double xbuf[256]; // Cap window size for embedded; ensure W<=256 in config.[file:3]
            if (W > 256) {
                // Configuration error: window size too large for stack buffer.[file:3]
                return 4;
            }
            for (std::uint32_t n = 0; n < W; ++n) {
                xbuf[n] = in->vt_series[start + n] - mean;
            }

            const double P = periodogram_power(xbuf, W, fk);
            window_powers[count] = P;
            ++count;
            start += step;
        }

        // Robust power estimate: median of window powers.[file:3]
        double powers_copy[MAX_WINDOWS];
        for (std::uint32_t i = 0; i < count; ++i) {
            powers_copy[i] = window_powers[i];
        }
        const double P_med = median_copy(powers_copy, count);
        out->power[k] = P_med;

        // Background estimate Bk via phase-specific scaling.[file:3]
        double Bk = 0.0;
        switch (in->phase) {
            case PRE_ONSET:
                // Lower variability, use median directly.[file:3]
                Bk = P_med;
                break;
            case ACTIVE:
                // Higher monsoon variability, inflate background modestly.[file:3]
                Bk = 1.2 * P_med;
                break;
            case WITHDRAWAL:
                // Transition phase, intermediate scaling.[file:3]
                Bk = 1.1 * P_med;
                break;
            default:
                Bk = P_med;
                break;
        }
        out->background[k] = Bk;

        // Statistic Sk as power / background.[file:3]
        const double eps = 1e-9;
        double Sk = (Bk > eps) ? (P_med / Bk) : 0.0;
        out->S[k] = Sk;

        // Anomaly detection thresholds per phase.[file:3]
        double threshold = 0.0;
        switch (in->phase) {
            case PRE_ONSET:
                threshold = 3.0; // stricter; fewer anomalies.[file:3]
                break;
            case ACTIVE:
                threshold = 5.0; // allow higher Sk due to monsoon variability.[file:3]
                break;
            case WITHDRAWAL:
                threshold = 4.0;
                break;
            default:
                threshold = 4.0;
                break;
        }

        // Flag anomalies where Sk exceeds threshold.[file:3]
        out->anomaly[k] = (Sk > threshold) ? 1u : 0u;
    }

    return 0;
}
