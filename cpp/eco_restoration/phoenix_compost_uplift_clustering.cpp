// File: cpp/eco_restoration/phoenix_compost_uplift_clustering.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>

// Phoenix neighbourhood clustering and uplift modelling for subsidised compost bins.
// Data sources:
//  - csv_material_loader: waste composition per neighbourhood (fractions of recyclable, compostable, landfill).
//  - Socio-economic census data: income, education, renter fraction, etc.
//  - eco_behavior_analytics: eco-focus score per neighbourhood (baseline and, where available, post-intervention).
//
// Pipeline:
//  1. Build feature vectors per neighbourhood combining waste composition and socio-economic variables.
//  2. Run k-means-style clustering to group neighbourhoods with similar profiles.
//  3. Within each cluster, fit a simple uplift model that estimates how much a subsidised compost bin programme
//     would increase eco-focus score.
//  4. Rank neighbourhoods by predicted uplift to target the programme where it yields maximal eco-restoration benefit.

struct NeighbourhoodFeatures {
    std::string id;
    // Waste composition fractions [0,1], summing to ~1.
    double frac_recyclable;
    double frac_compostable;
    double frac_landfill;
    // Socio-economic variables (normalized).
    double income_norm;
    double education_norm;
    double renter_frac;
    // Baseline eco-focus score from eco_behavior_analytics.
    double eco_focus_baseline;
    // Optional: observed eco-focus after existing compost interventions (if any).
    bool   has_post_intervention;
    double eco_focus_post;
};

struct ClusterAssignment {
    std::string id;
    int cluster_id;
};

struct ClusterCenter {
    std::vector<double> coords; // feature means
};

struct UpliftParams {
    // Linear uplift model coefficients for compost bin intervention:
    // uplift ≈ alpha + beta1 * frac_compostable + beta2 * frac_landfill + beta3 * renter_frac.
    double alpha;
    double beta1;
    double beta2;
    double beta3;
};

double euclidean_distance(const std::vector<double>& a,
                          const std::vector<double>& b) {
    double sum = 0.0;
    std::size_t n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        double d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

// Build feature vector from NeighbourhoodFeatures.
std::vector<double> features_to_vector(const NeighbourhoodFeatures& nf) {
    std::vector<double> v;
    v.reserve(7);
    v.push_back(nf.frac_recyclable);
    v.push_back(nf.frac_compostable);
    v.push_back(nf.frac_landfill);
    v.push_back(nf.income_norm);
    v.push_back(nf.education_norm);
    v.push_back(nf.renter_frac);
    v.push_back(nf.eco_focus_baseline);
    return v;
}

// Simple k-means clustering over neighbourhood feature vectors.
std::vector<ClusterAssignment> kmeans_cluster(
    const std::vector<NeighbourhoodFeatures>& data,
    int k,
    int max_iter
) {
    std::size_t n = data.size();
    std::vector<std::vector<double>> X;
    X.reserve(n);
    for (const auto& nf : data) {
        X.push_back(features_to_vector(nf));
    }

    std::size_t dim = X.empty() ? 0 : X[0].size();
    std::vector<ClusterCenter> centers(k);
    for (int c = 0; c < k; ++c) {
        centers[c].coords.assign(dim, 0.0);
    }

    // Initialize centers using first k points.
    for (int c = 0; c < k && c < static_cast<int>(n); ++c) {
        centers[c].coords = X[c];
    }
    for (int c = static_cast<int>(n); c < k; ++c) {
        centers[c].coords = X[n - 1];
    }

    std::vector<int> assignments(n, 0);

    for (int iter = 0; iter < max_iter; ++iter) {
        // Assignment step.
        bool changed = false;
        for (std::size_t i = 0; i < n; ++i) {
            double best_dist = std::numeric_limits<double>::infinity();
            int best_c = 0;
            for (int c = 0; c < k; ++c) {
                double d = euclidean_distance(X[i], centers[c].coords);
                if (d < best_dist) {
                    best_dist = d;
                    best_c = c;
                }
            }
            if (assignments[i] != best_c) {
                assignments[i] = best_c;
                changed = true;
            }
        }
        if (!changed) break;

        // Update step.
        std::vector<std::vector<double>> sums(k, std::vector<double>(dim, 0.0));
        std::vector<int> counts(k, 0);
        for (std::size_t i = 0; i < n; ++i) {
            int c = assignments[i];
            for (std::size_t d = 0; d < dim; ++d) {
                sums[c][d] += X[i][d];
            }
            counts[c] += 1;
        }
        for (int c = 0; c < k; ++c) {
            if (counts[c] == 0) continue;
            for (std::size_t d = 0; d < dim; ++d) {
                centers[c].coords[d] = sums[c][d] / static_cast<double>(counts[c]);
            }
        }
    }

    std::vector<ClusterAssignment> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(ClusterAssignment{data[i].id, assignments[i]});
    }
    return out;
}

// Fit uplift model coefficients for a cluster using simple linear regression on observed uplift.
// This assumes we have some post-intervention data points; otherwise, we fall back to neutral parameters.
UpliftParams fit_cluster_uplift(
    const std::vector<NeighbourhoodFeatures>& data,
    const std::vector<int>& cluster_assign,
    int cluster_id
) {
    double sum_x1 = 0.0, sum_x2 = 0.0, sum_x3 = 0.0;
    double sum_y = 0.0;
    double sum_x1x1 = 0.0, sum_x2x2 = 0.0, sum_x3x3 = 0.0;
    double sum_x1x2 = 0.0, sum_x1x3 = 0.0, sum_x2x3 = 0.0;
    double sum_x1y = 0.0, sum_x2y = 0.0, sum_x3y = 0.0;

    int count = 0;
    for (std::size_t i = 0; i < data.size(); ++i) {
        if (cluster_assign[i] != cluster_id) continue;
        const auto& nf = data[i];
        if (!nf.has_post_intervention) continue;

        double x1 = nf.frac_compostable;
        double x2 = nf.frac_landfill;
        double x3 = nf.renter_frac;
        double y = nf.eco_focus_post - nf.eco_focus_baseline;

        sum_x1 += x1;
        sum_x2 += x2;
        sum_x3 += x3;
        sum_y += y;

        sum_x1x1 += x1 * x1;
        sum_x2x2 += x2 * x2;
        sum_x3x3 += x3 * x3;
        sum_x1x2 += x1 * x2;
        sum_x1x3 += x1 * x3;
        sum_x2x3 += x2 * x3;

        sum_x1y += x1 * y;
        sum_x2y += x2 * y;
        sum_x3y += x3 * y;

        count += 1;
    }

    UpliftParams params;
    if (count < 3) {
        // Not enough data; use conservative defaults.
        params.alpha = 0.0;
        params.beta1 = 0.1;
        params.beta2 = -0.05;
        params.beta3 = 0.05;
        return params;
    }

    // Solve a simple diagonal-approximate regression:
    // y ≈ alpha + beta1*x1 + beta2*x2 + beta3*x3
    double mean_x1 = sum_x1 / count;
    double mean_x2 = sum_x2 / count;
    double mean_x3 = sum_x3 / count;
    double mean_y = sum_y / count;

    double var_x1 = sum_x1x1 / count - mean_x1 * mean_x1;
    double var_x2 = sum_x2x2 / count - mean_x2 * mean_x2;
    double var_x3 = sum_x3x3 / count - mean_x3 * mean_x3;

    double cov_x1y = sum_x1y / count - mean_x1 * mean_y;
    double cov_x2y = sum_x2y / count - mean_x2 * mean_y;
    double cov_x3y = sum_x3y / count - mean_x3 * mean_y;

    params.beta1 = (var_x1 > 0.0) ? (cov_x1y / var_x1) : 0.0;
    params.beta2 = (var_x2 > 0.0) ? (cov_x2y / var_x2) : 0.0;
    params.beta3 = (var_x3 > 0.0) ? (cov_x3y / var_x3) : 0.0;

    // alpha from mean equation.
    params.alpha = mean_y - params.beta1 * mean_x1 - params.beta2 * mean_x2 - params.beta3 * mean_x3;

    return params;
}

// Predict uplift in eco-focus for a neighbourhood given uplift params.
double predict_uplift(const NeighbourhoodFeatures& nf,
                      const UpliftParams& params) {
    double x1 = nf.frac_compostable;
    double x2 = nf.frac_landfill;
    double x3 = nf.renter_frac;
    double y = params.alpha + params.beta1 * x1 + params.beta2 * x2 + params.beta3 * x3;
    return y;
}

int main() {
    // Example synthetic data for Phoenix neighbourhoods.
    std::vector<NeighbourhoodFeatures> data;

    NeighbourhoodFeatures n1;
    n1.id = "nbhd_A";
    n1.frac_recyclable = 0.30;
    n1.frac_compostable = 0.25;
    n1.frac_landfill = 0.45;
    n1.income_norm = 0.4;
    n1.education_norm = 0.5;
    n1.renter_frac = 0.6;
    n1.eco_focus_baseline = 0.35;
    n1.has_post_intervention = true;
    n1.eco_focus_post = 0.48;
    data.push_back(n1);

    NeighbourhoodFeatures n2;
    n2.id = "nbhd_B";
    n2.frac_recyclable = 0.20;
    n2.frac_compostable = 0.15;
    n2.frac_landfill = 0.65;
    n2.income_norm = 0.3;
    n2.education_norm = 0.4;
    n2.renter_frac = 0.7;
    n2.eco_focus_baseline = 0.25;
    n2.has_post_intervention = true;
    n2.eco_focus_post = 0.40;
    data.push_back(n2);

    NeighbourhoodFeatures n3;
    n3.id = "nbhd_C";
    n3.frac_recyclable = 0.35;
    n3.frac_compostable = 0.30;
    n3.frac_landfill = 0.35;
    n3.income_norm = 0.6;
    n3.education_norm = 0.7;
    n3.renter_frac = 0.4;
    n3.eco_focus_baseline = 0.45;
    n3.has_post_intervention = false;
    n3.eco_focus_post = 0.0;
    data.push_back(n3);

    int k = 2;
    auto assignments = kmeans_cluster(data, k, 20);

    // Build cluster assignment index.
    std::vector<int> cluster_assign;
    cluster_assign.reserve(data.size());
    for (const auto& a : assignments) {
        cluster_assign.push_back(a.cluster_id);
    }

    // Fit uplift models per cluster.
    std::vector<UpliftParams> cluster_params;
    cluster_params.reserve(k);
    for (int c = 0; c < k; ++c) {
        cluster_params.push_back(fit_cluster_uplift(data, cluster_assign, c));
    }

    // Predict uplift for each neighbourhood and print ranking.
    for (std::size_t i = 0; i < data.size(); ++i) {
        int cid = cluster_assign[i];
        double uplift = predict_uplift(data[i], cluster_params[cid]);
        double eco_focus_pred = data[i].eco_focus_baseline + uplift;
        std::cout << "Neighbourhood " << data[i].id
                  << " cluster=" << cid
                  << " baseline_eco_focus=" << data[i].eco_focus_baseline
                  << " predicted_uplift=" << uplift
                  << " predicted_eco_focus_with_program=" << eco_focus_pred
                  << "\n";
    }

    return 0;
}
