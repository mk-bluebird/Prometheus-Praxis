#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

struct SwaleObservation {
    double removal_fraction;
    double length_m;
    double velocity_m_per_h;
    double slope_fraction;
    double vegetation_density;
    double hydraulic_loading_m_per_h;
};

struct SorptionObservation {
    double concentration_mg_per_l;
    double loading_mg_per_g;
};

struct SwaleFit {
    double beta_0;
    double beta_vegetation;
    double beta_slope;
    double beta_loading;
    double sse;
};

struct LangmuirFit {
    double qmax_mg_per_g;
    double k_l_per_mg;
    double sse;
};

static bool finite(double value) {
    return std::isfinite(value);
}

static std::vector<double> solve_4x4(std::vector<std::vector<double>> matrix, std::vector<double> vector) {
    const int n = 4;

    for (int column = 0; column < n; ++column) {
        int pivot = column;
        for (int row = column + 1; row < n; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }

        if (std::abs(matrix[pivot][column]) < 1.0e-12) {
            throw std::runtime_error("swale regression matrix is singular; provide more varied observations");
        }

        std::swap(matrix[pivot], matrix[column]);
        std::swap(vector[pivot], vector[column]);

        const double divisor = matrix[column][column];
        for (int col = column; col < n; ++col) {
            matrix[column][col] /= divisor;
        }
        vector[column] /= divisor;

        for (int row = 0; row < n; ++row) {
            if (row == column) {
                continue;
            }
            const double factor = matrix[row][column];
            for (int col = column; col < n; ++col) {
                matrix[row][col] -= factor * matrix[column][col];
            }
            vector[row] -= factor * vector[column];
        }
    }

    return vector;
}

static SwaleFit fit_swale(const std::vector<SwaleObservation>& observations) {
    if (observations.size() < 4) {
        throw std::invalid_argument("at least four varied swale observations are required");
    }

    std::vector<std::vector<double>> normal(4, std::vector<double>(4, 0.0));
    std::vector<double> rhs(4, 0.0);

    for (const SwaleObservation& point : observations) {
        if (!finite(point.removal_fraction) || point.removal_fraction <= 0.0 || point.removal_fraction >= 1.0 ||
            point.length_m <= 0.0 || point.velocity_m_per_h <= 0.0 ||
            point.slope_fraction < 0.0 || point.vegetation_density < 0.0 || point.vegetation_density > 1.0 ||
            point.hydraulic_loading_m_per_h < 0.0) {
            throw std::invalid_argument("invalid swale observation");
        }

        const double response = -std::log(1.0 - point.removal_fraction) *
            point.velocity_m_per_h / point.length_m;
        const double features[4] = {
            1.0,
            point.vegetation_density,
            -point.slope_fraction,
            -point.hydraulic_loading_m_per_h
        };

        for (int row = 0; row < 4; ++row) {
            rhs[row] += features[row] * response;
            for (int col = 0; col < 4; ++col) {
                normal[row][col] += features[row] * features[col];
            }
        }
    }

    const std::vector<double> coefficients = solve_4x4(normal, rhs);
    double sse = 0.0;

    for (const SwaleObservation& point : observations) {
        const double observed_k = -std::log(1.0 - point.removal_fraction) *
            point.velocity_m_per_h / point.length_m;
        const double predicted_k =
            coefficients[0] +
            coefficients[1] * point.vegetation_density -
            coefficients[2] * point.slope_fraction -
            coefficients[3] * point.hydraulic_loading_m_per_h;
        const double residual = observed_k - predicted_k;
        sse += residual * residual;
    }

    return {coefficients[0], coefficients[1], coefficients[2], coefficients[3], sse};
}

static LangmuirFit fit_langmuir(
    const std::vector<SorptionObservation>& observations,
    double qmax_min,
    double qmax_max,
    int qmax_steps,
    double k_min,
    double k_max,
    int k_steps
) {
    if (observations.size() < 3 || qmax_min <= 0.0 || qmax_max < qmax_min ||
        k_min <= 0.0 || k_max < k_min || qmax_steps < 2 || k_steps < 2) {
        throw std::invalid_argument("invalid Langmuir fit configuration");
    }

    double best_qmax = qmax_min;
    double best_k = k_min;
    double best_sse = std::numeric_limits<double>::infinity();

    for (int qmax_index = 0; qmax_index < qmax_steps; ++qmax_index) {
        const double qmax = qmax_min + (qmax_max - qmax_min) * qmax_index / (qmax_steps - 1.0);
        for (int k_index = 0; k_index < k_steps; ++k_index) {
            const double k = k_min + (k_max - k_min) * k_index / (k_steps - 1.0);
            double sse = 0.0;

            for (const SorptionObservation& point : observations) {
                if (point.concentration_mg_per_l < 0.0 || point.loading_mg_per_g < 0.0) {
                    throw std::invalid_argument("sorption observations must be non-negative");
                }

                const double predicted = qmax * k * point.concentration_mg_per_l /
                    (1.0 + k * point.concentration_mg_per_l);
                const double residual = point.loading_mg_per_g - predicted;
                sse += residual * residual;
            }

            if (sse < best_sse) {
                best_sse = sse;
                best_qmax = qmax;
                best_k = k;
            }
        }
    }

    return {best_qmax, best_k, best_sse};
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr
            << "usage:\n"
            << "  " << argv[0] << " swale <E> <L_m> <v_m_h> <slope_fraction> <vegetation_density_0_to_1> <hydraulic_loading_m_h> [...]\n"
            << "  " << argv[0] << " langmuir <Qmax_min> <Qmax_max> <Qmax_steps> <K_min> <K_max> <K_steps> <C_mg_L> <q_mg_g> [...]\n";
        return 64;
    }

    try {
        const std::string mode = argv[1];

        if (mode == "swale") {
            if (argc < 26 || ((argc - 2) % 6 != 0)) {
                throw std::invalid_argument("swale mode requires at least four six-value observations");
            }

            std::vector<SwaleObservation> observations;
            for (int index = 2; index < argc; index += 6) {
                observations.push_back({
                    std::stod(argv[index]),
                    std::stod(argv[index + 1]),
                    std::stod(argv[index + 2]),
                    std::stod(argv[index + 3]),
                    std::stod(argv[index + 4]),
                    std::stod(argv[index + 5])
                });
            }

            const SwaleFit result = fit_swale(observations);
            std::cout << std::fixed << std::setprecision(10)
                      << "beta0=" << result.beta_0 << '\n'
                      << "beta_vegetation=" << result.beta_vegetation << '\n'
                      << "beta_slope=" << result.beta_slope << '\n'
                      << "beta_hydraulic_loading=" << result.beta_loading << '\n'
                      << "sum_squared_error=" << result.sse << '\n';
            return 0;
        }

        if (mode == "langmuir") {
            if (argc < 14 || ((argc - 8) % 2 != 0)) {
                throw std::invalid_argument("langmuir mode requires bounds plus at least three concentration/loading observations");
            }

            const double qmax_min = std::stod(argv[2]);
            const double qmax_max = std::stod(argv[3]);
            const int qmax_steps = std::stoi(argv[4]);
            const double k_min = std::stod(argv[5]);
            const double k_max = std::stod(argv[6]);
            const int k_steps = std::stoi(argv[7]);

            std::vector<SorptionObservation> observations;
            for (int index = 8; index < argc; index += 2) {
                observations.push_back({std::stod(argv[index]), std::stod(argv[index + 1])});
            }

            const LangmuirFit result = fit_langmuir(
                observations, qmax_min, qmax_max, qmax_steps, k_min, k_max, k_steps
            );

            std::cout << std::fixed << std::setprecision(10)
                      << "Qmax_mg_per_g=" << result.qmax_mg_per_g << '\n'
                      << "K_L_per_mg=" << result.k_l_per_mg << '\n'
                      << "sum_squared_error=" << result.sse << '\n';
            return 0;
        }

        throw std::invalid_argument("mode must be swale or langmuir");
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
