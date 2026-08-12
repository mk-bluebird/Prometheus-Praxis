// File: cpp/simulation/nsga_lane_threshold_tuning.cpp

#include <Eigen/Dense>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct Observation {
    double knowledge{};
    double eco_impact{};
    double risk{};
    bool unsafe{};
    bool derate_required{};
    bool budget_excess{};
};

struct Candidate {
    Eigen::Matrix<double, 2, 3> thresholds;
    Eigen::Vector4d objective;
    int rank{};
    double crowding{};
};

std::vector<Observation> load_observations(const std::string& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open calibration CSV");

    std::string line;
    std::getline(input, line);
    std::vector<Observation> result;

    while (std::getline(input, line)) {
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        Observation observation;
        int unsafe = 0;
        int derate = 0;
        int excess = 0;
        row >> observation.knowledge >> observation.eco_impact >> observation.risk
            >> unsafe >> derate >> excess;
        if (!row || observation.knowledge < 0.0 || observation.knowledge > 1.0 ||
            observation.eco_impact < 0.0 || observation.eco_impact > 1.0 ||
            observation.risk < 0.0 || observation.risk > 1.0) {
            throw std::invalid_argument("invalid calibration record");
        }
        observation.unsafe = unsafe != 0;
        observation.derate_required = derate != 0;
        observation.budget_excess = excess != 0;
        result.push_back(observation);
    }
    if (result.size() < 20U) throw std::invalid_argument("at least 20 observations are required");
    return result;
}

int decision(const Observation& observation, const Eigen::Vector3d& threshold) {
    if (observation.knowledge < threshold[0] || observation.eco_impact < threshold[1] ||
        observation.risk > threshold[2]) return 2;
    if (observation.knowledge < threshold[0] + 0.08 ||
        observation.eco_impact < threshold[1] + 0.08 ||
        observation.risk > threshold[2] - 0.08) return 1;
    return 0;
}

void evaluate(Candidate& candidate, const std::vector<Observation>& records) {
    candidate.objective.setZero();
    const std::size_t split = records.size() / 2U;

    for (std::size_t i = 0; i < records.size(); ++i) {
        const int action = decision(records[i], candidate.thresholds.row(i < split ? 0 : 1));
        candidate.objective[0] += records[i].unsafe && action != 2 ? 1.0 : 0.0;
        candidate.objective[1] += records[i].budget_excess && action == 0 ? 1.0 : 0.0;
        candidate.objective[2] += records[i].derate_required && action == 1 ? 1.0 : 0.0;
    }

    candidate.objective[0] /= records.size();
    candidate.objective[1] /= records.size();
    candidate.objective[2] = 1.0 - candidate.objective[2] / records.size();
    candidate.objective[3] = (candidate.thresholds.row(1) - candidate.thresholds.row(0))
        .cwiseAbs().sum() / 3.0;
}

bool dominates(const Candidate& a, const Candidate& b) {
    bool strictly_better = false;
    for (int i = 0; i < 4; ++i) {
        if (a.objective[i] > b.objective[i]) return false;
        strictly_better = strictly_better || a.objective[i] < b.objective[i];
    }
    return strictly_better;
}

std::vector<std::vector<int>> sort_fronts(std::vector<Candidate>& population) {
    const std::size_t count = population.size();
    std::vector<std::vector<int>> dominated(count), fronts(1);
    std::vector<int> domination_count(count);

    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = 0; j < count; ++j) {
            if (dominates(population[i], population[j])) dominated[i].push_back(static_cast<int>(j));
            else if (dominates(population[j], population[i])) ++domination_count[i];
        }
        if (domination_count[i] == 0) {
            population[i].rank = 0;
            fronts[0].push_back(static_cast<int>(i));
        }
    }

    for (std::size_t rank = 0; rank < fronts.size() && !fronts[rank].empty(); ++rank) {
        std::vector<int> next;
        for (int index : fronts[rank]) {
            for (int child : dominated[index]) {
                if (--domination_count[child] == 0) {
                    population[child].rank = static_cast<int>(rank + 1U);
                    next.push_back(child);
                }
            }
        }
        if (!next.empty()) fronts.push_back(std::move(next));
    }
    return fronts;
}

void crowding(std::vector<Candidate>& population, const std::vector<int>& front) {
    for (int index : front) population[index].crowding = 0.0;
    if (front.size() < 3U) {
        for (int index : front) population[index].crowding = 1e12;
        return;
    }

    for (int objective = 0; objective < 4; ++objective) {
        std::vector<int> ordered = front;
        std::sort(ordered.begin(), ordered.end(), [&](int a, int b) {
            return population[a].objective[objective] < population[b].objective[objective];
        });
        population[ordered.front()].crowding = population[ordered.back()].crowding = 1e12;
        const double span = population[ordered.back()].objective[objective] -
                            population[ordered.front()].objective[objective];
        if (span <= 1e-12) continue;
        for (std::size_t i = 1; i + 1U < ordered.size(); ++i) {
            population[ordered[i]].crowding +=
                (population[ordered[i + 1U]].objective[objective] -
                 population[ordered[i - 1U]].objective[objective]) / span;
        }
    }
}

Candidate make_child(const Candidate& a, const Candidate& b, std::mt19937_64& generator) {
    std::normal_distribution<double> mutation(0.0, 0.035);
    std::bernoulli_distribution choose(0.5);
    Candidate child{};

    for (int epoch = 0; epoch < 2; ++epoch) {
        for (int metric = 0; metric < 3; ++metric) {
            child.thresholds(epoch, metric) = std::clamp(
                choose(generator) ? a.thresholds(epoch, metric) : b.thresholds(epoch, metric),
                0.01, 0.99);
            child.thresholds(epoch, metric) = std::clamp(
                child.thresholds(epoch, metric) + mutation(generator), 0.01, 0.99);
        }
    }
    return child;
}

void persist_front(const std::string& path, const std::vector<Candidate>& population) {
    sqlite3* database = nullptr;
    if (sqlite3_open(path.c_str(), &database) != SQLITE_OK) throw std::runtime_error("SQLite open failed");

    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS lane_pareto_front("
        "rank INTEGER,k0 REAL,e0 REAL,r0 REAL,k1 REAL,e1 REAL,r1 REAL,"
        "false_negative REAL,budget_overrun REAL,true_derate_loss REAL,volatility REAL,"
        "knee INTEGER NOT NULL) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_exec(database, "DELETE FROM lane_pareto_front;", nullptr, nullptr, nullptr);
    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO lane_pareto_front VALUES(?,?,?,?,?,?,?,?,?,?,?,?);", -1, &statement, nullptr);

    const Candidate* knee = nullptr;
    double best_distance = std::numeric_limits<double>::infinity();
    for (const Candidate& candidate : population) {
        if (candidate.rank != 0) continue;
        const double distance = candidate.objective.norm();
        if (distance < best_distance) {
            best_distance = distance;
            knee = &candidate;
        }
    }

    for (const Candidate& candidate : population) {
        if (candidate.rank != 0) continue;
        sqlite3_bind_int(statement, 1, candidate.rank);
        for (int i = 0; i < 3; ++i) sqlite3_bind_double(statement, 2 + i, candidate.thresholds(0, i));
        for (int i = 0; i < 3; ++i) sqlite3_bind_double(statement, 5 + i, candidate.thresholds(1, i));
        for (int i = 0; i < 4; ++i) sqlite3_bind_double(statement, 8 + i, candidate.objective[i]);
        sqlite3_bind_int(statement, 12, &candidate == knee ? 1 : 0);
        if (sqlite3_step(statement) != SQLITE_DONE) throw std::runtime_error("SQLite insert failed");
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;
    if (argc != 3) {
        std::cerr << "usage: nsga_lane_threshold_tuning calibration.csv pareto.sqlite\n";
        return 2;
    }

    try {
        const std::vector<Observation> records = load_observations(argv[1]);
        std::mt19937_64 generator(0xECO712026ULL);
        std::uniform_real_distribution<double> initial(0.20, 0.80);
        std::vector<Candidate> population(160);

        for (Candidate& candidate : population) {
            candidate.thresholds << initial(generator), initial(generator), initial(generator),
                                    initial(generator), initial(generator), initial(generator);
            evaluate(candidate, records);
        }

        for (int generation = 0; generation < 180; ++generation) {
            std::vector<Candidate> children;
            const auto fronts = sort_fronts(population);
            for (const auto& front : fronts) crowding(population, front);

            std::uniform_int_distribution<std::size_t> pick(0, population.size() - 1U);
            while (children.size() < population.size()) {
                const Candidate& a = population[pick(generator)];
                const Candidate& b = population[pick(generator)];
                Candidate child = make_child(
                    (a.rank < b.rank || (a.rank == b.rank && a.crowding >= b.crowding)) ? a : b,
                    (a.rank < b.rank || (a.rank == b.rank && a.crowding >= b.crowding)) ? b : a,
                    generator);
                evaluate(child, records);
                children.push_back(std::move(child));
            }

            population.insert(population.end(), children.begin(), children.end());
            const auto combined_fronts = sort_fronts(population);
            std::vector<Candidate> next;
            for (const auto& front : combined_fronts) {
                crowding(population, front);
                std::vector<int> ordered = front;
                std::sort(ordered.begin(), ordered.end(), [&](int a, int b) {
                    return population[a].crowding > population[b].crowding;
                });
                for (int index : ordered) {
                    if (next.size() == 160U) break;
                    next.push_back(population[index]);
                }
                if (next.size() == 160U) break;
            }
            population = std::move(next);
        }

        sort_fronts(population);
        persist_front(argv[2], population);
        std::cout << "{\"status\":\"persisted\",\"population\":" << population.size() << "}\n";
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
