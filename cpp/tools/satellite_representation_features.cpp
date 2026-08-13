// File: cpp/tools/satellite_representation_features.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct SpectralPixel {
    double blue{};
    double green{};
    double red{};
    double nir{};
    double swir{};
    double lst_c{};
    double vegetation_health{};
};

struct DerivedFeatures {
    std::vector<std::string> names;
    std::vector<double> values;
};

double safe_ratio(double numerator, double denominator) {
    return numerator / (std::abs(denominator) > 1e-9 ? denominator : 1e-9);
}

/*
Feature library:
F_t={X_ti, X_ti^2, X_ti X_tj, NDVI, NDWI, SAVI},
NDVI=(NIR-Red)/(NIR+Red),
NDWI=(NIR-SWIR)/(NIR+SWIR),
SAVI=(1+L)(NIR-Red)/(NIR+Red+L), L=0.5.

For time-slice coefficients b_t and feature groups G:
min_b sum_t ||y_t-X_t b_t||^2
      +lambda_G sum_g ||(b_tj)_(j in g,t)||_2
      +lambda_T sum_j sum_(t>1)(b_tj-b_(t-1)j)^2.
The group term selects whole spectral-transform families; the temporal term
rejects features whose predictive contribution is unstable across dates.
*/
DerivedFeatures derive_features(const SpectralPixel& pixel) {
    const std::vector<double> bands{
        pixel.blue, pixel.green, pixel.red, pixel.nir, pixel.swir};
    const std::vector<std::string> band_names{"blue", "green", "red", "nir", "swir"};
    DerivedFeatures output;

    for (std::size_t i = 0; i < bands.size(); ++i) {
        output.names.push_back(band_names[i]);
        output.values.push_back(bands[i]);
        output.names.push_back(band_names[i] + "_squared");
        output.values.push_back(bands[i] * bands[i]);
    }
    for (std::size_t i = 0; i < bands.size(); ++i) {
        for (std::size_t j = i + 1; j < bands.size(); ++j) {
            output.names.push_back(band_names[i] + "_times_" + band_names[j]);
            output.values.push_back(bands[i] * bands[j]);
        }
    }

    const double ndvi = safe_ratio(pixel.nir - pixel.red, pixel.nir + pixel.red);
    const double ndwi = safe_ratio(pixel.nir - pixel.swir, pixel.nir + pixel.swir);
    constexpr double soil_adjustment = 0.5;
    const double savi = safe_ratio((1.0 + soil_adjustment) * (pixel.nir - pixel.red),
                                   pixel.nir + pixel.red + soil_adjustment);

    output.names.push_back("ndvi");
    output.values.push_back(ndvi);
    output.names.push_back("ndwi");
    output.values.push_back(ndwi);
    output.names.push_back("savi");
    output.values.push_back(savi);
    return output;
}

double masked_autoencoder_loss(const std::vector<double>& input,
                               const std::vector<bool>& visible,
                               const std::vector<std::vector<double>>& decoder) {
    if (input.empty() || visible.size() != input.size() || decoder.size() != input.size()) {
        throw std::invalid_argument("masked-autoencoder dimensions differ");
    }

    std::vector<double> masked(input.size(), 0.0);
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (visible[i]) masked[i] = input[i];
        if (decoder[i].size() != input.size()) {
            throw std::invalid_argument("decoder must be square");
        }
    }

    double squared_error = 0.0;
    for (std::size_t row = 0; row < input.size(); ++row) {
        double reconstruction = 0.0;
        for (std::size_t column = 0; column < input.size(); ++column) {
            reconstruction += decoder[row][column] * masked[column];
        }
        const double error = input[row] - reconstruction;
        squared_error += error * error;
    }
    return squared_error / static_cast<double>(input.size());
}

/*
Pretraining objective:
L_MAE=E_(X,M)[||X-f_theta(X odot M)||^2].

For Phoenix 10 m downstream fitting, initialize an LST regression head from
the pretrained representation and minimize weighted Huber LST loss on
cloud-screened 10 m labels. Initialize a vegetation-health head separately
and minimize squared error or calibrated ordinal loss against field/NDVI
health labels. Split validation spatially and temporally to prevent leakage
from nearby pixels or repeated dates.
*/
double temporal_stability_score(const std::vector<std::vector<double>>& coefficients,
                                std::size_t feature_index) {
    if (coefficients.size() < 2) return 1.0;
    double difference_sum = 0.0;
    for (std::size_t t = 1; t < coefficients.size(); ++t) {
        const double difference = coefficients[t][feature_index] -
                                  coefficients[t - 1][feature_index];
        difference_sum += difference * difference;
    }
    return 1.0 / (1.0 + difference_sum / static_cast<double>(coefficients.size() - 1));
}

double group_sparse_stable_score(const std::vector<std::vector<double>>& coefficients,
                                 const std::vector<std::size_t>& group,
                                 double group_penalty, double stability_penalty) {
    if (coefficients.empty() || group.empty()) return 0.0;
    double energy = 0.0;
    double stability = 0.0;
    for (std::size_t feature : group) {
        for (const auto& slice : coefficients) {
            if (feature >= slice.size()) throw std::invalid_argument("feature index outside coefficient matrix");
            energy += slice[feature] * slice[feature];
        }
        stability += temporal_stability_score(coefficients, feature);
    }
    const double group_norm = std::sqrt(energy);
    return std::max(0.0, group_norm - group_penalty) *
           std::max(0.0, stability / static_cast<double>(group.size()) - stability_penalty);
}

}  // namespace

int main() {
    try {
        const SpectralPixel pixel{0.08, 0.12, 0.10, 0.42, 0.19, 47.5, 0.71};
        const DerivedFeatures features = derive_features(pixel);

        std::vector<bool> visible(features.values.size(), true);
        for (std::size_t i = 0; i < visible.size(); ++i) visible[i] = (i % 4) != 0;

        std::vector<std::vector<double>> decoder(
            features.values.size(), std::vector<double>(features.values.size(), 0.0));
        for (std::size_t i = 0; i < decoder.size(); ++i) decoder[i][i] = 1.0;
        const double mae_loss = masked_autoencoder_loss(features.values, visible, decoder);

        std::vector<std::vector<double>> coefficients{
            std::vector<double>(features.values.size(), 0.0),
            std::vector<double>(features.values.size(), 0.0),
            std::vector<double>(features.values.size(), 0.0)};
        for (std::size_t j = 0; j < features.values.size(); ++j) {
            coefficients[0][j] = 0.03 * static_cast<double>(j + 1);
            coefficients[1][j] = 0.032 * static_cast<double>(j + 1);
            coefficients[2][j] = 0.031 * static_cast<double>(j + 1);
        }

        const std::vector<std::size_t> vegetation_group{
            features.values.size() - 3, features.values.size() - 2, features.values.size() - 1};
        const double stable_score = group_sparse_stable_score(
            coefficients, vegetation_group, 0.02, 0.60);
        const double knowledge_factor = std::clamp(
            0.55 * (1.0 / (1.0 + mae_loss)) + 0.45 * stable_score, 0.0, 1.0);
        const double eco_impact_value = std::clamp(
            0.50 * knowledge_factor + 0.50 * pixel.vegetation_health, 0.0, 1.0);

        std::cout << std::fixed << std::setprecision(6)
                  << "feature_count=" << features.values.size() << '\n'
                  << "ndvi=" << features.values[features.values.size() - 3] << '\n'
                  << "ndwi=" << features.values[features.values.size() - 2] << '\n'
                  << "savi=" << features.values[features.values.size() - 1] << '\n'
                  << "masked_autoencoder_loss=" << mae_loss << '\n'
                  << "vegetation_group_stability_score=" << stable_score << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "satellite representation assessment failed: " << error.what() << '\n';
        return 1;
    }
}
