#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

struct Observation {
    double x_m;
    double y_m;
    double value;
};

static double distance(const Observation& a, const Observation& b) {
    const double dx = a.x_m - b.x_m;
    const double dy = a.y_m - b.y_m;
    return std::sqrt(dx * dx + dy * dy);
}

static double distance_to_target(const Observation& observation, double x_m, double y_m) {
    const double dx = observation.x_m - x_m;
    const double dy = observation.y_m - y_m;
    return std::sqrt(dx * dx + dy * dy);
}

static double semivariance(double h, double nugget, double sill, double range_m) {
    if (h <= 0.0) {
        return 0.0;
    }
    return nugget + sill * (1.0 - std::exp(-h / range_m));
}

static std::vector<double> solve(std::vector<std::vector<double>> matrix, std::vector<double> rhs) {
    const int n = static_cast<int>(rhs.size());

    for (int column = 0; column < n; ++column) {
        int pivot = column;
        for (int row = column + 1; row < n; ++row) {
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) {
                pivot = row;
            }
        }

        if (std::abs(matrix[pivot][column]) <= 1.0e-12) {
            throw std::runtime_error("singular kriging system; revise points or variogram parameters");
        }

        std::swap(matrix[pivot], matrix[column]);
        std::swap(rhs[pivot], rhs[column]);

        const double divisor = matrix[column][column];
        for (int col = column; col < n; ++col) {
            matrix[column][col] /= divisor;
        }
        rhs[column] /= divisor;

        for (int row = 0; row < n; ++row) {
            if (row == column) {
                continue;
            }

            const double factor = matrix[row][column];
            for (int col = column; col < n; ++col) {
                matrix[row][col] -= factor * matrix[column][col];
            }
            rhs[row] -= factor * rhs[column];
        }
    }

    return rhs;
}

static void ordinary_kriging(
    const std::vector<Observation>& observations,
    double target_x_m,
    double target_y_m,
    double nugget,
    double sill,
    double range_m,
    double& estimate,
    double& variance,
    double& weight_sum
) {
    if (observations.size() < 2 || nugget < 0.0 || sill < 0.0 || range_m <= 0.0) {
        throw std::invalid_argument("need at least two observations, nugget/sill >= 0, and positive range");
    }

    const int n = static_cast<int>(observations.size());
    std::vector<std::vector<double>> matrix(n + 1, std::vector<double>(n + 1, 0.0));
    std::vector<double> rhs(n + 1, 0.0);

    for (int row = 0; row < n; ++row) {
        for (int col = 0; col < n; ++col) {
            matrix[row][col] = semivariance(distance(observations[row], observations[col]), nugget, sill, range_m);
        }
        matrix[row][n] = 1.0;
        matrix[n][row] = 1.0;
        rhs[row] = semivariance(distance_to_target(observations[row], target_x_m, target_y_m), nugget, sill, range_m);
    }

    matrix[n][n] = 0.0;
    rhs[n] = 1.0;

    const std::vector<double> solution = solve(matrix, rhs);
    estimate = 0.0;
    variance = solution[n];
    weight_sum = 0.0;

    for (int index = 0; index < n; ++index) {
        estimate += solution[index] * observations[index].value;
        variance += solution[index] * rhs[index];
        weight_sum += solution[index];
    }

    if (variance < 0.0 && variance > -1.0e-9) {
        variance = 0.0;
    }
}

static double leave_one_out_rmse(
    const std::vector<Observation>& observations,
    double nugget,
    double sill,
    double range_m
) {
    if (observations.size() < 3) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double squared_error = 0.0;
    for (std::size_t held_out = 0; held_out < observations.size(); ++held_out) {
        std::vector<Observation> retained;
        retained.reserve(observations.size() - 1);

        for (std::size_t index = 0; index < observations.size(); ++index) {
            if (index != held_out) {
                retained.push_back(observations[index]);
            }
        }

        double estimate = 0.0;
        double variance = 0.0;
        double weight_sum = 0.0;
        ordinary_kriging(
            retained,
            observations[held_out].x_m,
            observations[held_out].y_m,
            nugget,
            sill,
            range_m,
            estimate,
            variance,
            weight_sum
        );

        const double residual = observations[held_out].value - estimate;
        squared_error += residual * residual;
    }

    return std::sqrt(squared_error / observations.size());
}

int main(int argc, char** argv) {
    if (argc < 11 || ((argc - 6) % 3 != 0)) {
        std::cerr
            << "usage: " << argv[0]
            << " <target_x_m> <target_y_m> <nugget> <sill> <range_m>"
            << " <x_m> <y_m> <value> [<x_m> <y_m> <value> ...]\n";
        return 64;
    }

    try {
        const double target_x_m = std::stod(argv[1]);
        const double target_y_m = std::stod(argv[2]);
        const double nugget = std::stod(argv[3]);
        const double sill = std::stod(argv[4]);
        const double range_m = std::stod(argv[5]);

        std::vector<Observation> observations;
        for (int index = 6; index < argc; index += 3) {
            observations.push_back({
                std::stod(argv[index]),
                std::stod(argv[index + 1]),
                std::stod(argv[index + 2])
            });
        }

        double estimate = 0.0;
        double variance = 0.0;
        double weight_sum = 0.0;
        ordinary_kriging(
            observations, target_x_m, target_y_m, nugget, sill, range_m,
            estimate, variance, weight_sum
        );

        const double rmse = leave_one_out_rmse(observations, nugget, sill, range_m);
        const double knowledge_factor = observations.size() >= 6 ? 0.75 : 0.45;
        const double harm_risk = observations.size() >= 6 && std::isfinite(rmse) ? 0.30 : 0.65;
        const double eco_impact_value = knowledge_factor * (1.0 - harm_risk);

        std::cout << std::fixed << std::setprecision(10);
        std::cout << "estimate=" << estimate << '\n';
        std::cout << "kriging_variance=" << variance << '\n';
        std::cout << "weight_sum=" << weight_sum << '\n';
        std::cout << "leave_one_out_rmse=";
        if (std::isfinite(rmse)) {
            std::cout << rmse << '\n';
        } else {
            std::cout << "UNAVAILABLE\n";
        }
        std::cout << "knowledge_factor=" << knowledge_factor << '\n';
        std::cout << "eco_impact_value=" << eco_impact_value << '\n';
        std::cout << "harm_risk=" << harm_risk << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
