// File: cpp/simulation/canal_lyapunov_calibration.cpp

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

using State = Eigen::Vector3d;
using Matrix = Eigen::Matrix3d;

std::vector<State> load_states(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open canal data CSV");
    }

    std::string line;
    std::getline(input, line);
    std::vector<State> states;

    while (std::getline(input, line)) {
        std::stringstream row(line);
        std::string field;
        std::vector<double> values;
        while (std::getline(row, field, ',')) {
            values.push_back(std::stod(field));
        }
        if (values.size() != 4U) {
            throw std::invalid_argument("CSV rows require timestamp,flow,head,sediment");
        }
        states.emplace_back(values[1], values[2], values[3]);
    }
    if (states.size() < 8U) {
        throw std::invalid_argument("at least eight observations are required");
    }
    return states;
}

Matrix identify_dynamics(const std::vector<State>& states, double regularization) {
    Matrix xx = Matrix::Zero();
    Matrix yx = Matrix::Zero();

    for (std::size_t i = 0; i + 1U < states.size(); ++i) {
        xx.noalias() += states[i] * states[i].transpose();
        yx.noalias() += states[i + 1U] * states[i].transpose();
    }
    return yx * (xx + regularization * Matrix::Identity()).inverse();
}

Matrix solve_discrete_lyapunov(const Matrix& dynamics, const Matrix& q) {
    const double radius = Eigen::EigenSolver<Matrix>(dynamics).eigenvalues().cwiseAbs().maxCoeff();
    if (radius >= 1.0) {
        throw std::runtime_error("identified dynamics are not basin-stable");
    }

    Matrix p = Matrix::Zero();
    Matrix term = q;
    for (int iteration = 0; iteration < 10000; ++iteration) {
        p += term;
        term = dynamics.transpose() * term * dynamics;
        if (term.norm() < 1e-12) {
            return 0.5 * (p + p.transpose());
        }
    }
    throw std::runtime_error("Lyapunov series did not converge");
}

double value(const State& state, const Matrix& p) {
    return state.transpose() * p * state;
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 3) {
        std::cerr << "usage: canal_lyapunov_calibration normalized_canal.csv basin_limit\n";
        return 2;
    }

    try {
        const double basin_limit = std::stod(argv[2]);
        if (basin_limit <= 0.0) {
            throw std::invalid_argument("basin limit must be positive");
        }

        const std::vector<State> states = load_states(argv[1]);
        const Matrix dynamics = identify_dynamics(states, 1e-8);
        const Matrix p = solve_discrete_lyapunov(dynamics, Matrix::Identity());

        double maximum_value = 0.0;
        double maximum_delta = -std::numeric_limits<double>::infinity();
        bool validated = true;

        for (std::size_t i = 0; i < states.size(); ++i) {
            const double current = value(states[i], p);
            maximum_value = std::max(maximum_value, current);
            validated = validated && current <= basin_limit;
            if (i + 1U < states.size()) {
                maximum_delta = std::max(maximum_delta, value(states[i + 1U], p) - current);
            }
        }

        const double current_value = value(states.back(), p);
        const double allowed_delta_v = std::max(0.0, basin_limit - current_value);

        std::cout << std::fixed << std::setprecision(10)
                  << "{\"validated\":" << (validated ? "true" : "false")
                  << ",\"maximum_v\":" << maximum_value
                  << ",\"maximum_observed_delta_v\":" << maximum_delta
                  << ",\"current_v\":" << current_value
                  << ",\"allowed_delta_v\":" << allowed_delta_v
                  << ",\"A\":[[" << dynamics(0, 0) << ',' << dynamics(0, 1) << ',' << dynamics(0, 2)
                  << "],[" << dynamics(1, 0) << ',' << dynamics(1, 1) << ',' << dynamics(1, 2)
                  << "],[" << dynamics(2, 0) << ',' << dynamics(2, 1) << ',' << dynamics(2, 2)
                  << "]]}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
