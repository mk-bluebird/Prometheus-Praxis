// File: cpp/simulation/delayed_cbf_ker_sensitivity.cpp

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

struct DelayedCbfInput {
    double barrier_now{};
    double delta_v{};
    double gamma{};
    double coupling{};
    double barrier_lipschitz{};
    double maximum_state_change_per_step{};
    std::uint32_t delay_steps{};
};

struct DelayedCbfResult {
    double minimum_delayed_barrier{};
    double required_barrier{};
    double maximum_safe_delay{};
    bool satisfied{};
};

DelayedCbfResult check_delayed_cbf(const DelayedCbfInput& input) {
    if (input.barrier_now < 0.0 || input.delta_v < 0.0 ||
        input.gamma < 0.0 || input.gamma > 1.0 || input.coupling < 0.0 ||
        input.barrier_lipschitz <= 0.0 || input.maximum_state_change_per_step <= 0.0) {
        throw std::invalid_argument("invalid delayed CBF parameters");
    }

    const double required =
        (1.0 - input.gamma) * input.barrier_now + input.coupling * input.delta_v;
    const double delayed_lower_bound =
        input.barrier_now - input.barrier_lipschitz *
        input.maximum_state_change_per_step * static_cast<double>(input.delay_steps);

    const double numerator = input.gamma * input.barrier_now - input.coupling * input.delta_v;
    const double maximum_delay = numerator < 0.0
        ? -1.0
        : numerator / (input.barrier_lipschitz * input.maximum_state_change_per_step);

    return {
        delayed_lower_bound,
        required,
        maximum_delay,
        delayed_lower_bound >= required
    };
}

struct CanalState {
    double flow{};
    double head{};
    double sediment{};
};

CanalState step_canal(const CanalState& current) {
    return {
        0.90 * current.flow + 0.06 * current.head,
        0.05 * current.flow + 0.88 * current.head,
        0.03 * current.flow + 0.08 * current.sediment
    };
}

double canal_barrier(const CanalState& state) {
    const double normalized_load =
        0.40 * state.flow * state.flow +
        0.35 * state.head * state.head +
        0.25 * state.sediment * state.sediment;
    return std::max(0.0, 1.0 - normalized_load);
}

double canal_state_change(const CanalState& a, const CanalState& b) {
    return std::sqrt(
        (a.flow - b.flow) * (a.flow - b.flow) +
        (a.head - b.head) * (a.head - b.head) +
        (a.sediment - b.sediment) * (a.sediment - b.sediment));
}

struct WorkloadProfile {
    std::string id;
    double knowledge{};
    double restoration_value{};
    std::array<double, 3> risk_weights{};
    std::array<double, 3> maximum_measurement_error{};
};

double evaluate_eco_impact(const std::vector<double>& variables, const WorkloadProfile& profile) {
    const double energy = clamp01(variables[0] + variables[3] * profile.maximum_measurement_error[0]);
    const double heat = clamp01(variables[1] + variables[4] * profile.maximum_measurement_error[1]);
    const double water = clamp01(variables[2] + variables[5] * profile.maximum_measurement_error[2]);

    const double risk = clamp01(
        profile.risk_weights[0] * energy +
        profile.risk_weights[1] * heat +
        profile.risk_weights[2] * water);

    return clamp01(profile.knowledge * profile.restoration_value * (1.0 - risk));
}

struct SobolIndex {
    std::string variable;
    double first_order{};
    double total_order{};
};

std::vector<SobolIndex> sobol_indices(const WorkloadProfile& profile, std::size_t samples) {
    constexpr std::size_t dimensions = 6;
    if (samples < 1000U) {
        throw std::invalid_argument("use at least 1000 Sobol samples");
    }

    std::mt19937_64 generator(0xEC0512026ULL);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::vector<std::vector<double>> a(samples, std::vector<double>(dimensions));
    std::vector<std::vector<double>> b(samples, std::vector<double>(dimensions));
    std::vector<double> ya(samples), yb(samples);

    double mean = 0.0;
    for (std::size_t row = 0; row < samples; ++row) {
        for (std::size_t col = 0; col < dimensions; ++col) {
            a[row][col] = col < 3U ? unit(generator) : 2.0 * unit(generator) - 1.0;
            b[row][col] = col < 3U ? unit(generator) : 2.0 * unit(generator) - 1.0;
        }
        ya[row] = evaluate_eco_impact(a[row], profile);
        yb[row] = evaluate_eco_impact(b[row], profile);
        mean += ya[row] + yb[row];
    }

    mean /= static_cast<double>(2U * samples);
    double variance = 0.0;
    for (std::size_t row = 0; row < samples; ++row) {
        variance += (ya[row] - mean) * (ya[row] - mean);
        variance += (yb[row] - mean) * (yb[row] - mean);
    }
    variance /= static_cast<double>(2U * samples - 1U);
    if (variance <= 1e-15) {
        throw std::runtime_error("eco-impact variance is too small for sensitivity analysis");
    }

    const std::array<std::string, dimensions> names{
        "energy_risk", "heat_risk", "water_risk",
        "energy_measurement_error", "heat_measurement_error", "water_measurement_error"
    };

    std::vector<SobolIndex> result;
    for (std::size_t dimension = 0; dimension < dimensions; ++dimension) {
        double first = 0.0;
        double total = 0.0;
        for (std::size_t row = 0; row < samples; ++row) {
            std::vector<double> hybrid = a[row];
            hybrid[dimension] = b[row][dimension];
            const double yab = evaluate_eco_impact(hybrid, profile);
            first += yb[row] * (yab - ya[row]);
            total += (ya[row] - yab) * (ya[row] - yab);
        }
        result.push_back({
            names[dimension],
            std::clamp(first / (static_cast<double>(samples) * variance), 0.0, 1.0),
            std::clamp(total / (2.0 * static_cast<double>(samples) * variance), 0.0, 1.0)
        });
    }
    return result;
}

void store_indices(const std::string& database_path,
                   const WorkloadProfile& profile,
                   const std::vector<SobolIndex>& indices) {
    sqlite3* database = nullptr;
    if (sqlite3_open(database_path.c_str(), &database) != SQLITE_OK) {
        throw std::runtime_error("cannot open SQLite database");
    }

    const char* schema =
        "CREATE TABLE IF NOT EXISTS ker_sobol_index ("
        "profile_id TEXT NOT NULL, variable TEXT NOT NULL,"
        "first_order REAL NOT NULL CHECK(first_order BETWEEN 0 AND 1),"
        "total_order REAL NOT NULL CHECK(total_order BETWEEN 0 AND 1),"
        "PRIMARY KEY(profile_id,variable)) STRICT;";

    if (sqlite3_exec(database, schema, nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(database);
        throw std::runtime_error("cannot create SQLite table");
    }

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO ker_sobol_index(profile_id,variable,first_order,total_order) VALUES(?,?,?,?) "
        "ON CONFLICT(profile_id,variable) DO UPDATE SET first_order=excluded.first_order,"
        "total_order=excluded.total_order;",
        -1, &statement, nullptr);

    for (const SobolIndex& index : indices) {
        sqlite3_bind_text(statement, 1, profile.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(statement, 2, index.variable.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(statement, 3, index.first_order);
        sqlite3_bind_double(statement, 4, index.total_order);
        if (sqlite3_step(statement) != SQLITE_DONE) {
            sqlite3_finalize(statement);
            sqlite3_close(database);
            throw std::runtime_error("cannot persist Sobol index");
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 2) {
        std::cerr << "usage: delayed_cbf_ker_sensitivity indices.sqlite\n";
        return 2;
    }

    try {
        CanalState state{0.55, 0.42, 0.31};
        const CanalState next = step_canal(state);
        const DelayedCbfResult cbf = check_delayed_cbf({
            canal_barrier(state), 0.008, 0.12, 0.40, 1.0,
            canal_state_change(state, next), 2U
        });

        const std::vector<WorkloadProfile> profiles{
            {"ai_workload", 0.91, 0.76, {0.45, 0.35, 0.20}, {0.08, 0.12, 0.05}},
            {"restoration_task", 0.86, 0.89, {0.20, 0.30, 0.50}, {0.06, 0.08, 0.14}}
        };

        std::cout << std::fixed << std::setprecision(6)
                  << "{\"cbf_satisfied\":" << (cbf.satisfied ? "true" : "false")
                  << ",\"maximum_safe_delay\":" << cbf.maximum_safe_delay
                  << ",\"profiles\":[";

        for (std::size_t i = 0; i < profiles.size(); ++i) {
            const std::vector<SobolIndex> indices = sobol_indices(profiles[i], 50000U);
            store_indices(argv[1], profiles[i], indices);
            std::cout << "{\"id\":\"" << profiles[i].id << "\",\"indices\":[";
            for (std::size_t j = 0; j < indices.size(); ++j) {
                std::cout << "{\"variable\":\"" << indices[j].variable
                          << "\",\"first_order\":" << indices[j].first_order
                          << ",\"total_order\":" << indices[j].total_order << "}";
                if (j + 1U < indices.size()) std::cout << ',';
            }
            std::cout << "]}";
            if (i + 1U < profiles.size()) std::cout << ',';
        }
        std::cout << "]}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
