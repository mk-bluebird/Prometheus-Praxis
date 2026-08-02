// File: cpp/eco_restoration/hex_corridor_mutual_information.cpp
#include <vector>
#include <cmath>
#include <iostream>

// Corridor calibration via mutual information between vegetation index time series
// of two adjacent hex cells.
//
// Model:
//   - Each hex h has a time series of vegetation index (e.g., NDVI) over T timestamps.
//   - For adjacent hexes h1 and h2, we compute the mutual information I(V1; V2)
//     between their discretized NDVI sequences.
//   - A high mutual information suggests strong coupling in vegetation dynamics,
//     indicative of a functional wildlife corridor (shared habitat, movement, hydrologic coupling).
//
// Implementation:
//   - Discretize NDVI values into K bins.
//   - Estimate empirical joint distribution p_ij and marginals p_i, q_j.
//   - Compute I(V1; V2) = Σ_{i,j} p_ij log( p_ij / (p_i q_j) ).
//   - Compare I to a threshold based on normalized maximum entropy to decide corridor status.

struct NdviTimeSeries {
    int32_t hex_id;
    std::vector<double> ndvi_values;
};

struct MutualInformationResult {
    double mutual_information;
    bool corridor_functional;
};

// Discretize NDVI into K bins spanning [min_val, max_val].
std::vector<int> discretize_ndvi(const std::vector<double>& values,
                                 double min_val,
                                 double max_val,
                                 int K) {
    std::vector<int> bins;
    bins.reserve(values.size());
    double width = (max_val - min_val) / static_cast<double>(K);
    if (width <= 0.0) width = 1.0;
    for (double v : values) {
        double clamped = std::max(min_val, std::min(max_val, v));
        int b = static_cast<int>((clamped - min_val) / width);
        if (b >= K) b = K - 1;
        if (b < 0) b = 0;
        bins.push_back(b);
    }
    return bins;
}

// Compute mutual information between two NDVI time series.
MutualInformationResult compute_corridor_mutual_information(
    const NdviTimeSeries& ts1,
    const NdviTimeSeries& ts2,
    int K,
    double corridor_threshold_fraction // fraction of max possible MI used as threshold
) {
    std::size_t T = std::min(ts1.ndvi_values.size(), ts2.ndvi_values.size());
    if (T == 0) {
        return MutualInformationResult{0.0, false};
    }

    double min_val = -1.0;
    double max_val = 1.0;

    auto bins1 = discretize_ndvi(ts1.ndvi_values, min_val, max_val, K);
    auto bins2 = discretize_ndvi(ts2.ndvi_values, min_val, max_val, K);

    std::vector<double> joint(K * K, 0.0);
    std::vector<double> p(K, 0.0);
    std::vector<double> q(K, 0.0);

    for (std::size_t t = 0; t < T; ++t) {
        int i = bins1[t];
        int j = bins2[t];
        joint[i * K + j] += 1.0;
        p[i] += 1.0;
        q[j] += 1.0;
    }

    double invT = 1.0 / static_cast<double>(T);
    for (int i = 0; i < K; ++i) {
        p[i] *= invT;
        q[i] *= invT;
    }
    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < K; ++j) {
            joint[i * K + j] *= invT;
        }
    }

    double I = 0.0;
    for (int i = 0; i < K; ++i) {
        for (int j = 0; j < K; ++j) {
            double pij = joint[i * K + j];
            if (pij <= 0.0) continue;
            double pi = p[i];
            double qj = q[j];
            if (pi <= 0.0 || qj <= 0.0) continue;
            double ratio = pij / (pi * qj);
            I += pij * std::log(ratio);
        }
    }

    // Maximum mutual information given K bins is log(K) (natural log) if variables are perfectly coupled.
    double I_max = std::log(static_cast<double>(K));
    double normalized_I = (I_max > 0.0) ? (I / I_max) : 0.0;

    // Corridor threshold: e.g., normalized mutual information ≥ 0.3 indicates functional corridor.
    double threshold = corridor_threshold_fraction;
    bool functional = normalized_I >= threshold;

    return MutualInformationResult{I, functional};
}

// Example synthetic usage demonstrating corridor-calibration metric on coupled vs decoupled NDVI.
int main() {
    const int K = 10;
    const double threshold_fraction = 0.3;

    NdviTimeSeries ts1;
    NdviTimeSeries ts2_coupled;
    NdviTimeSeries ts2_decoupled;

    ts1.hex_id = 1;
    ts2_coupled.hex_id = 2;
    ts2_decoupled.hex_id = 3;

    const std::size_t T = 365;
    ts1.ndvi_values.reserve(T);
    ts2_coupled.ndvi_values.reserve(T);
    ts2_decoupled.ndvi_values.reserve(T);

    for (std::size_t t = 0; t < T; ++t) {
        double season = std::sin(2.0 * M_PI * static_cast<double>(t) / 365.0);
        double noise1 = 0.05 * std::sin(static_cast<double>(t) * 0.3);
        double v1 = 0.4 + 0.3 * season + noise1;

        ts1.ndvi_values.push_back(v1);

        double noise2c = 0.03 * std::sin(static_cast<double>(t) * 0.5);
        double v2c = 0.45 + 0.28 * season + noise2c;

        double noise2d = 0.20 * std::cos(static_cast<double>(t) * 0.7);
        double v2d = 0.2 + 0.1 * std::sin(static_cast<double>(t) * 0.1) + noise2d;

        ts2_coupled.ndvi_values.push_back(v2c);
        ts2_decoupled.ndvi_values.push_back(v2d);
    }

    auto res_coupled = compute_corridor_mutual_information(ts1, ts2_coupled, K, threshold_fraction);
    auto res_decoupled = compute_corridor_mutual_information(ts1, ts2_decoupled, K, threshold_fraction);

    std::cout << "Coupled MI: " << res_coupled.mutual_information
              << " functional=" << (res_coupled.corridor_functional ? "yes" : "no") << "\n";
    std::cout << "Decoupled MI: " << res_decoupled.mutual_information
              << " functional=" << (res_decoupled.corridor_functional ? "yes" : "no") << "\n";

    return 0;
}
