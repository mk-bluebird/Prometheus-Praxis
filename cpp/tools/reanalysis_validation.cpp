// File: cpp/tools/reanalysis_validation.cpp
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

struct Metrics { double nse{}, kge{}, rmse{}; };

Metrics validate(const std::vector<double>& observed, const std::vector<double>& predicted) {
    if (observed.size() < 2 || observed.size() != predicted.size()) throw std::invalid_argument("invalid series");
    double mean_o = 0.0, mean_p = 0.0;
    for (std::size_t i = 0; i < observed.size(); ++i) { mean_o += observed[i]; mean_p += predicted[i]; }
    mean_o /= observed.size(); mean_p /= predicted.size();

    double ss_res = 0.0, ss_tot = 0.0, covariance = 0.0, var_o = 0.0, var_p = 0.0;
    for (std::size_t i = 0; i < observed.size(); ++i) {
        ss_res += std::pow(predicted[i] - observed[i], 2);
        ss_tot += std::pow(observed[i] - mean_o, 2);
        covariance += (observed[i] - mean_o) * (predicted[i] - mean_p);
        var_o += std::pow(observed[i] - mean_o, 2);
        var_p += std::pow(predicted[i] - mean_p, 2);
    }
    const double correlation = covariance / std::sqrt(std::max(1e-18, var_o * var_p));
    const double alpha = std::sqrt(var_p / std::max(1e-18, var_o));
    const double beta = mean_p / std::max(1e-18, mean_o);
    return {1.0 - ss_res / std::max(1e-18, ss_tot),
            1.0 - std::sqrt(std::pow(correlation - 1.0, 2) + std::pow(alpha - 1.0, 2) +
                            std::pow(beta - 1.0, 2)),
            std::sqrt(ss_res / observed.size())};
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1]);
    if (!input) return 1;
    std::vector<double> observed, predicted;
    std::string line;
    std::getline(input, line);
    while (std::getline(input, line)) {
        std::stringstream row(line);
        std::string timestamp, observed_text, predicted_text;
        if (std::getline(row, timestamp, ',') && std::getline(row, observed_text, ',') &&
            std::getline(row, predicted_text, ',')) {
            observed.push_back(std::stod(observed_text));
            predicted.push_back(std::stod(predicted_text));
        }
    }
    const Metrics result = validate(observed, predicted);
    std::cout << "{\"nse\":" << result.nse << ",\"kge\":" << result.kge
              << ",\"rmse\":" << result.rmse << ",\"samples\":" << observed.size() << "}\n";
}
