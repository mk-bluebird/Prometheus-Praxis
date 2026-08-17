#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Step {
    double measurement;
    bool available;
};

struct FilterState {
    double estimate;
    double covariance;
};

struct Output {
    double innovation;
    double innovation_variance;
    double normalized_innovation;
    double gain;
    bool updated;
};

static bool finite(double value) {
    return std::isfinite(value);
}

static Output update(
    FilterState& state,
    double process_variance,
    double measurement_variance,
    double h,
    const Step& step
) {
    if (process_variance < 0.0 || measurement_variance <= 0.0 || h == 0.0) {
        throw std::invalid_argument("require Q >= 0, R > 0, and non-zero H");
    }

    const double prior_estimate = state.estimate;
    const double prior_covariance = state.covariance + process_variance;

    if (!step.available) {
        state.estimate = prior_estimate;
        state.covariance = prior_covariance;
        return {NAN, NAN, NAN, 0.0, false};
    }

    const double innovation = step.measurement - h * prior_estimate;
    const double innovation_variance = h * prior_covariance * h + measurement_variance;
    const double gain = prior_covariance * h / innovation_variance;

    state.estimate = prior_estimate + gain * innovation;
    state.covariance = std::max(0.0, (1.0 - gain * h) * prior_covariance);

    return {
        innovation,
        innovation_variance,
        innovation / std::sqrt(innovation_variance),
        gain,
        true
    };
}

static double lag1_autocorrelation(const std::vector<double>& values) {
    if (values.size() < 3) {
        return NAN;
    }

    double mean = 0.0;
    for (double value : values) {
        mean += value;
    }
    mean /= static_cast<double>(values.size());

    double numerator = 0.0;
    double denominator = 0.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const double centered = values[index] - mean;
        denominator += centered * centered;
        if (index > 0) {
            numerator += centered * (values[index - 1] - mean);
        }
    }

    return denominator <= 0.0 ? NAN : numerator / denominator;
}

int main(int argc, char** argv) {
    if (argc < 8 || ((argc - 4) % 2 != 0)) {
        std::cerr
            << "usage: " << argv[0]
            << " <x0> <P0> <Q> <R> <H> <measurement_or_na> <available_0_or_1> "
            << "[<measurement_or_na> <available_0_or_1> ...]\n";
        return 64;
    }

    try {
        FilterState state{std::stod(argv[1]), std::stod(argv[2])};
        const double process_variance = std::stod(argv[3]);
        const double measurement_variance = std::stod(argv[4]);
        const double h = std::stod(argv[5]);

        if (state.covariance < 0.0) {
            throw std::invalid_argument("P0 must be non-negative");
        }

        std::vector<double> normalized_innovations;
        for (int index = 6, step_index = 0; index < argc; index += 2, ++step_index) {
            const bool available = std::stoi(argv[index + 1]) == 1;
            const Step step{available ? std::stod(argv[index]) : 0.0, available};
            const Output result = update(state, process_variance, measurement_variance, h, step);

            std::cout << std::fixed << std::setprecision(8)
                      << "step=" << step_index
                      << " updated=" << (result.updated ? 1 : 0)
                      << " estimate=" << state.estimate
                      << " covariance=" << state.covariance
                      << " gain=" << result.gain;

            if (result.updated) {
                normalized_innovations.push_back(result.normalized_innovation);
                std::cout << " residual=" << result.innovation
                          << " S=" << result.innovation_variance
                          << " normalized_residual=" << result.normalized_innovation;
            }
            std::cout << '\n';
        }

        const double lag1 = lag1_autocorrelation(normalized_innovations);
        std::cout << "available_innovation_count=" << normalized_innovations.size() << '\n';
        std::cout << "lag1_normalized_residual_autocorrelation=";
        if (finite(lag1)) {
            std::cout << std::fixed << std::setprecision(8) << lag1 << '\n';
        } else {
            std::cout << "UNAVAILABLE\n";
        }
        std::cout << "whiteness_screen="
                  << (finite(lag1) && std::abs(lag1) <= 0.20 ? "PASS_SCREEN_ONLY" : "REVIEW_MODEL_OR_COLLECT_DATA")
                  << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
