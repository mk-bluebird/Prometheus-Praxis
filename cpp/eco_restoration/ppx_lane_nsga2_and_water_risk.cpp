// File: cpp/eco_restoration/ppx_lane_nsga2_and_water_risk.cpp
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <random>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace ppx::eco_restoration {

struct Outcome {
    double k{}, e{}, r{}, ecological_cost{};
    bool observed_safe{};
};

struct Thresholds {
    double k_min{}, e_min{}, r_max{};
    double false_negative_halts{}, true_positive_derates{}, budget_excess{};
    double crowding{};
    int rank{};
};

bool admits(const Outcome& o, const Thresholds& t) {
    return o.k >= t.k_min && o.e >= t.e_min && o.r <= t.r_max;
}

void score(Thresholds& t, const std::vector<Outcome>& history, double ecological_budget) {
    t.false_negative_halts = t.true_positive_derates = 0.0;
    double admitted_cost = 0.0;
    for (const Outcome& o : history) {
        const bool accepted = admits(o, t);
        if (o.observed_safe && !accepted) t.false_negative_halts += 1.0;
        if (!o.observed_safe && !accepted) t.true_positive_derates += 1.0;
        if (accepted) admitted_cost += o.ecological_cost;
    }
    t.budget_excess = std::max(0.0, admitted_cost - ecological_budget);
}

bool dominates(const Thresholds& a, const Thresholds& b) {
    const bool no_worse = a.false_negative_halts <= b.false_negative_halts &&
                          a.true_positive_derates >= b.true_positive_derates &&
                          a.budget_excess <= b.budget_excess;
    const bool better = a.false_negative_halts < b.false_negative_halts ||
                        a.true_positive_derates > b.true_positive_derates ||
                        a.budget_excess < b.budget_excess;
    return no_worse && better;
}

std::vector<Thresholds> nsga2_calibrate(std::vector<Outcome> history, double ecological_budget,
                                        std::size_t population_size, std::size_t generations,
                                        std::uint64_t seed = 20260811) {
    if (history.empty() || ecological_budget < 0.0 || population_size < 4) {
        throw std::invalid_argument("invalid calibration data");
    }

    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::normal_distribution<double> mutation(0.0, 0.03);
    std::vector<Thresholds> population(population_size);

    for (Thresholds& t : population) {
        t.k_min = 0.50 + 0.45 * unit(rng);
        t.e_min = 0.50 + 0.45 * unit(rng);
        t.r_max = 0.05 + 0.45 * unit(rng);
        score(t, history, ecological_budget);
    }

    for (std::size_t generation = 0; generation < generations; ++generation) {
        std::vector<Thresholds> offspring;
        offspring.reserve(population_size);
        for (std::size_t i = 0; i < population_size; ++i) {
            const Thresholds& a = population[i];
            const Thresholds& b = population[(i + 1) % population_size];
            Thresholds child{
                std::clamp(0.5 * (a.k_min + b.k_min) + mutation(rng), 0.01, 0.99),
                std::clamp(0.5 * (a.e_min + b.e_min) + mutation(rng), 0.01, 0.99),
                std::clamp(0.5 * (a.r_max + b.r_max) + mutation(rng), 0.01, 0.99)
            };
            score(child, history, ecological_budget);
            offspring.push_back(child);
        }
        population.insert(population.end(), offspring.begin(), offspring.end());

        for (Thresholds& a : population) {
            a.rank = 0;
            a.crowding = 0.0;
            for (const Thresholds& b : population) if (dominates(b, a)) ++a.rank;
        }
        for (int objective = 0; objective < 3; ++objective) {
            std::sort(population.begin(), population.end(), [objective](const Thresholds& a, const Thresholds& b) {
                const double av[] = {a.false_negative_halts, -a.true_positive_derates, a.budget_excess};
                const double bv[] = {b.false_negative_halts, -b.true_positive_derates, b.budget_excess};
                return av[objective] < bv[objective];
            });
            population.front().crowding = population.back().crowding = 1e30;
            for (std::size_t i = 1; i + 1 < population.size(); ++i) {
                const double prev[] = {population[i - 1].false_negative_halts, -population[i - 1].true_positive_derates, population[i - 1].budget_excess};
                const double next[] = {population[i + 1].false_negative_halts, -population[i + 1].true_positive_derates, population[i + 1].budget_excess};
                population[i].crowding += std::abs(next[objective] - prev[objective]);
            }
        }
        std::sort(population.begin(), population.end(), [](const Thresholds& a, const Thresholds& b) {
            return a.rank != b.rank ? a.rank < b.rank : a.crowding > b.crowding;
        });
        population.resize(population_size);
    }

    std::vector<Thresholds> front;
    for (const Thresholds& t : population) if (t.rank == 0) front.push_back(t);
    return front;
}

void persist_pareto_front(const std::vector<Thresholds>& front, const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) throw std::runtime_error("cannot open SQLite");
    const char* schema =
        "CREATE TABLE IF NOT EXISTS ppx_lane_pareto_front("
        "k_min REAL,e_min REAL,r_max REAL,false_negative_halts REAL,"
        "true_positive_derates REAL,budget_excess REAL,"
        "PRIMARY KEY(k_min,e_min,r_max)) STRICT;";
    if (sqlite3_exec(db, schema, nullptr, nullptr, nullptr) != SQLITE_OK) {
        sqlite3_close(db); throw std::runtime_error("cannot create Pareto table");
    }
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO ppx_lane_pareto_front VALUES(?,?,?,?,?,?);",
                       -1, &statement, nullptr);
    for (const Thresholds& t : front) {
        sqlite3_reset(statement);
        sqlite3_bind_double(statement, 1, t.k_min); sqlite3_bind_double(statement, 2, t.e_min);
        sqlite3_bind_double(statement, 3, t.r_max); sqlite3_bind_double(statement, 4, t.false_negative_halts);
        sqlite3_bind_double(statement, 5, t.true_positive_derates); sqlite3_bind_double(statement, 6, t.budget_excess);
        if (sqlite3_step(statement) != SQLITE_DONE) { sqlite3_finalize(statement); sqlite3_close(db); throw std::runtime_error("cannot persist Pareto row"); }
    }
    sqlite3_finalize(statement);
    sqlite3_close(db);
}

struct WaterRiskModel { double intercept{}; Eigen::Vector3d weights{Eigen::Vector3d::Constant(1.0 / 3.0)}; };

WaterRiskModel train_water_risk(const Eigen::MatrixXd& features, const Eigen::VectorXd& labels,
                                double learning_rate, int iterations) {
    if (features.cols() != 3 || features.rows() != labels.size() || features.rows() == 0) {
        throw std::invalid_argument("water-risk training expects N x 3 features and N labels");
    }
    WaterRiskModel model;
    for (int step = 0; step < iterations; ++step) {
        const Eigen::VectorXd logits = Eigen::VectorXd::Constant(features.rows(), model.intercept) + features * model.weights;
        const Eigen::VectorXd probabilities = logits.unaryExpr([](double x) { return 1.0 / (1.0 + std::exp(-x)); });
        const Eigen::VectorXd error = probabilities - labels;
        model.intercept -= learning_rate * error.mean();
        model.weights -= learning_rate * (features.transpose() * error / features.rows());
        model.weights = model.weights.cwiseMax(0.0);
        const double sum = model.weights.sum();
        model.weights = sum > 0.0 ? model.weights / sum : Eigen::Vector3d::Constant(1.0 / 3.0);
    }
    return model;
}

void write_lua_water_risk_config(const WaterRiskModel& model, const std::string& path) {
    std::ofstream output(path);
    if (!output) throw std::runtime_error("cannot write Lua configuration");
    output << "return { intercept=" << model.intercept << ", w1=" << model.weights[0]
           << ", w2=" << model.weights[1] << ", w3=" << model.weights[2] << " }\n";
}

}  // namespace ppx::eco_restoration
