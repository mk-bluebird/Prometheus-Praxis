// File: cpp/simulation/adaptive_token_evidence_alpha.cpp

#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct TokenObservation {
    double token_count{};
    double usefulness{};
    double confidence{};
};

std::vector<TokenObservation> load_observations(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open token evidence CSV");

    std::string line;
    std::getline(input, line);
    std::vector<TokenObservation> observations;

    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        TokenObservation observation;
        row >> observation.token_count >> observation.usefulness >> observation.confidence;
        if (!row || observation.token_count <= 0.0 || observation.usefulness < 0.0 ||
            observation.usefulness > 1.0 || observation.confidence < 0.0 ||
            observation.confidence > 1.0) {
            throw std::invalid_argument("invalid token evidence observation");
        }
        observations.push_back(observation);
    }
    if (observations.size() < 10U) throw std::invalid_argument("at least ten observations are required");
    return observations;
}

struct LogNormalModel {
    double log_mean{};
    double log_standard_deviation{};
    double alpha{};
    double mean_squared_error{};
};

double evidence(const TokenObservation& observation, double alpha) {
    return observation.token_count / (observation.token_count + alpha) * observation.confidence;
}

double mse(const std::vector<TokenObservation>& observations, double alpha) {
    double total = 0.0;
    for (const TokenObservation& observation : observations) {
        const double residual = evidence(observation, alpha) - observation.usefulness;
        total += residual * residual;
    }
    return total / static_cast<double>(observations.size());
}

LogNormalModel fit(const std::vector<TokenObservation>& observations) {
    double log_mean = 0.0;
    for (const TokenObservation& observation : observations) log_mean += std::log(observation.token_count);
    log_mean /= static_cast<double>(observations.size());

    double log_variance = 0.0;
    for (const TokenObservation& observation : observations) {
        const double residual = std::log(observation.token_count) - log_mean;
        log_variance += residual * residual;
    }
    log_variance /= static_cast<double>(observations.size());

    const double median = std::exp(log_mean);
    double low = std::max(1e-6, median * 0.001);
    double high = std::max(1.0, median * 1000.0);

    for (int iteration = 0; iteration < 80; ++iteration) {
        const double left = low + (high - low) / 3.0;
        const double right = high - (high - low) / 3.0;
        if (mse(observations, left) < mse(observations, right)) high = right;
        else low = left;
    }

    const double alpha = (low + high) * 0.5;
    return {log_mean, std::sqrt(log_variance), alpha, mse(observations, alpha)};
}

void persist(sqlite3* database, const LogNormalModel& model) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS token_evidence_model("
        "model_id TEXT PRIMARY KEY,log_mean REAL NOT NULL,log_standard_deviation REAL NOT NULL,"
        "alpha REAL NOT NULL CHECK(alpha>0),mean_squared_error REAL NOT NULL CHECK(mean_squared_error>=0)"
        ") STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO token_evidence_model VALUES('default',?,?,?,?) "
        "ON CONFLICT(model_id) DO UPDATE SET log_mean=excluded.log_mean,"
        "log_standard_deviation=excluded.log_standard_deviation,alpha=excluded.alpha,"
        "mean_squared_error=excluded.mean_squared_error;",
        -1, &statement, nullptr);

    sqlite3_bind_double(statement, 1, model.log_mean);
    sqlite3_bind_double(statement, 2, model.log_standard_deviation);
    sqlite3_bind_double(statement, 3, model.alpha);
    sqlite3_bind_double(statement, 4, model.mean_squared_error);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("cannot persist token evidence model");
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 3) {
        std::cerr << "usage: adaptive_token_evidence_alpha observations.csv token_evidence.sqlite\n";
        return 2;
    }

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[2], &database) != SQLITE_OK) return 1;

    try {
        const LogNormalModel model = fit(load_observations(argv[1]));
        persist(database, model);
        sqlite3_close(database);

        std::cout << std::fixed << std::setprecision(8)
                  << "{\"log_mean\":" << model.log_mean
                  << ",\"log_standard_deviation\":" << model.log_standard_deviation
                  << ",\"alpha\":" << model.alpha
                  << ",\"mean_squared_error\":" << model.mean_squared_error << "}\n";
    } catch (const std::exception& error) {
        sqlite3_close(database);
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
