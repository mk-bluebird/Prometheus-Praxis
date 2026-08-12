// File: cpp/simulation/battery_chance_constrained_scheduler.cpp

#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

constexpr std::size_t kHours = 24;
constexpr std::size_t kScenarios = 50;

struct BatteryParameters {
    double capacity_kwh{};
    double initial_soc_kwh{};
    double max_charge_kw{};
    double max_discharge_kw{};
    double charge_efficiency{};
    double discharge_efficiency{};
    double degradation_cost_per_kwh{};
};

struct Job {
    std::string id;
    double energy_kwh{};
    double eco_impact{};
};

struct MilpModel {
    std::vector<double> objective;
    std::vector<std::vector<double>> inequalities;
    std::vector<double> bounds;
    std::vector<std::size_t> binary_columns;
};

class BatteryChanceModel {
public:
    BatteryChanceModel(std::vector<Job> jobs, BatteryParameters battery)
        : jobs_(std::move(jobs)), battery_(battery) {
        validate();
    }

    [[nodiscard]] MilpModel build(const std::vector<std::vector<double>>& renewable) const {
        if (renewable.size() != kScenarios) {
            throw std::invalid_argument("exactly 50 renewable scenarios are required");
        }
        for (const auto& scenario : renewable) {
            if (scenario.size() != kHours) {
                throw std::invalid_argument("each scenario requires 24 hourly values");
            }
        }

        MilpModel model;
        model.objective.assign(variable_count(), 0.0);

        for (std::size_t j = 0; j < jobs_.size(); ++j) {
            for (std::size_t hour = 0; hour < kHours; ++hour) {
                const std::size_t column = job_column(j, hour);
                model.objective[column] = jobs_[j].eco_impact;
                model.binary_columns.push_back(column);
            }
        }

        for (std::size_t hour = 0; hour < kHours; ++hour) {
            model.objective[charge_column(hour)] = -battery_.degradation_cost_per_kwh;
            model.objective[discharge_column(hour)] = -battery_.degradation_cost_per_kwh;
        }

        for (std::size_t j = 0; j < jobs_.size(); ++j) {
            std::vector<double> row(variable_count(), 0.0);
            for (std::size_t hour = 0; hour < kHours; ++hour) {
                row[job_column(j, hour)] = 1.0;
            }
            add(model, std::move(row), 1.0);
        }

        for (std::size_t hour = 0; hour < kHours; ++hour) {
            std::vector<double> charge_limit(variable_count(), 0.0);
            charge_limit[charge_column(hour)] = 1.0;
            add(model, std::move(charge_limit), battery_.max_charge_kw);

            std::vector<double> discharge_limit(variable_count(), 0.0);
            discharge_limit[discharge_column(hour)] = 1.0;
            add(model, std::move(discharge_limit), battery_.max_discharge_kw);

            std::vector<double> capacity_limit(variable_count(), 0.0);
            capacity_limit[soc_column(hour + 1U)] = 1.0;
            add(model, std::move(capacity_limit), battery_.capacity_kwh);

            std::vector<double> dynamic(variable_count(), 0.0);
            dynamic[soc_column(hour + 1U)] = 1.0;
            dynamic[soc_column(hour)] = -1.0;
            dynamic[charge_column(hour)] = -battery_.charge_efficiency;
            dynamic[discharge_column(hour)] = 1.0 / battery_.discharge_efficiency;
            add_equality(model, std::move(dynamic), 0.0);
        }

        std::vector<double> initial(variable_count(), 0.0);
        initial[soc_column(0)] = 1.0;
        add_equality(model, std::move(initial), battery_.initial_soc_kwh);

        const std::size_t allowed_violations = static_cast<std::size_t>(
            std::floor(0.05 * static_cast<double>(kScenarios)));

        for (std::size_t hour = 0; hour < kHours; ++hour) {
            std::vector<double> probability_limit(variable_count(), 0.0);
            for (std::size_t scenario = 0; scenario < kScenarios; ++scenario) {
                const std::size_t column = violation_column(hour, scenario);
                probability_limit[column] = 1.0;
                model.binary_columns.push_back(column);
            }
            add(model, std::move(probability_limit), static_cast<double>(allowed_violations));

            for (std::size_t scenario = 0; scenario < kScenarios; ++scenario) {
                std::vector<double> chance(variable_count(), 0.0);
                for (std::size_t j = 0; j < jobs_.size(); ++j) {
                    chance[job_column(j, hour)] = jobs_[j].energy_kwh;
                }
                chance[charge_column(hour)] = 1.0;
                chance[discharge_column(hour)] = -1.0;
                chance[violation_column(hour, scenario)] = -big_m();
                add(model, std::move(chance), renewable[scenario][hour]);
            }
        }
        return model;
    }

private:
    [[nodiscard]] std::size_t job_column(std::size_t job, std::size_t hour) const {
        return job * kHours + hour;
    }

    [[nodiscard]] std::size_t charge_column(std::size_t hour) const {
        return jobs_.size() * kHours + hour;
    }

    [[nodiscard]] std::size_t discharge_column(std::size_t hour) const {
        return jobs_.size() * kHours + kHours + hour;
    }

    [[nodiscard]] std::size_t soc_column(std::size_t hour) const {
        return jobs_.size() * kHours + 2U * kHours + hour;
    }

    [[nodiscard]] std::size_t violation_column(std::size_t hour, std::size_t scenario) const {
        return jobs_.size() * kHours + 2U * kHours + (kHours + 1U) +
               hour * kScenarios + scenario;
    }

    [[nodiscard]] std::size_t variable_count() const {
        return jobs_.size() * kHours + 2U * kHours + (kHours + 1U) +
               kHours * kScenarios;
    }

    [[nodiscard]] double big_m() const {
        double job_energy = 0.0;
        for (const Job& job : jobs_) job_energy += job.energy_kwh;
        return job_energy + battery_.max_charge_kw + battery_.max_discharge_kw;
    }

    static void add(MilpModel& model, std::vector<double> row, double bound) {
        model.inequalities.push_back(std::move(row));
        model.bounds.push_back(bound);
    }

    static void add_equality(MilpModel& model, std::vector<double> row, double value) {
        add(model, row, value);
        for (double& coefficient : row) coefficient = -coefficient;
        add(model, std::move(row), -value);
    }

    void validate() const {
        if (jobs_.empty() || battery_.capacity_kwh <= 0.0 ||
            battery_.initial_soc_kwh < 0.0 || battery_.initial_soc_kwh > battery_.capacity_kwh ||
            battery_.max_charge_kw < 0.0 || battery_.max_discharge_kw < 0.0 ||
            battery_.charge_efficiency <= 0.0 || battery_.charge_efficiency > 1.0 ||
            battery_.discharge_efficiency <= 0.0 || battery_.discharge_efficiency > 1.0 ||
            battery_.degradation_cost_per_kwh < 0.0) {
            throw std::invalid_argument("invalid battery configuration");
        }
        for (const Job& job : jobs_) {
            if (job.id.empty() || job.energy_kwh <= 0.0 || job.eco_impact < 0.0) {
                throw std::invalid_argument("invalid job");
            }
        }
    }

    std::vector<Job> jobs_;
    BatteryParameters battery_;
};

BatteryParameters load_battery(sqlite3* database) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS battery_parameters("
        "profile_id TEXT PRIMARY KEY,capacity_kwh REAL NOT NULL CHECK(capacity_kwh>0),"
        "initial_soc_kwh REAL NOT NULL CHECK(initial_soc_kwh>=0),"
        "max_charge_kw REAL NOT NULL CHECK(max_charge_kw>=0),"
        "max_discharge_kw REAL NOT NULL CHECK(max_discharge_kw>=0),"
        "charge_efficiency REAL NOT NULL CHECK(charge_efficiency>0 AND charge_efficiency<=1),"
        "discharge_efficiency REAL NOT NULL CHECK(discharge_efficiency>0 AND discharge_efficiency<=1),"
        "degradation_cost_per_kwh REAL NOT NULL CHECK(degradation_cost_per_kwh>=0)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "SELECT capacity_kwh,initial_soc_kwh,max_charge_kw,max_discharge_kw,"
        "charge_efficiency,discharge_efficiency,degradation_cost_per_kwh "
        "FROM battery_parameters WHERE profile_id='default';",
        -1, &statement, nullptr);

    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        throw std::runtime_error("battery profile 'default' is required");
    }

    BatteryParameters result{
        sqlite3_column_double(statement, 0),
        sqlite3_column_double(statement, 1),
        sqlite3_column_double(statement, 2),
        sqlite3_column_double(statement, 3),
        sqlite3_column_double(statement, 4),
        sqlite3_column_double(statement, 5),
        sqlite3_column_double(statement, 6)
    };
    sqlite3_finalize(statement);
    return result;
}

std::vector<std::vector<double>> solar_scenarios() {
    std::mt19937_64 generator(0xECO92026ULL);
    std::normal_distribution<double> noise(0.0, 0.12);
    std::vector<std::vector<double>> scenarios(kScenarios, std::vector<double>(kHours));

    for (std::size_t scenario = 0; scenario < kScenarios; ++scenario) {
        for (std::size_t hour = 0; hour < kHours; ++hour) {
            const double daylight = std::max(0.0, std::sin(
                (static_cast<double>(hour) - 6.0) * 3.14159265358979323846 / 12.0));
            scenarios[scenario][hour] = std::max(0.0, 10.0 * daylight * (1.0 + noise(generator)));
        }
    }
    return scenarios;
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    if (argc != 2) {
        std::cerr << "usage: battery_chance_constrained_scheduler battery.sqlite\n";
        return 2;
    }

    sqlite3* database = nullptr;
    if (sqlite3_open(argv[1], &database) != SQLITE_OK) {
        std::cerr << "{\"error\":\"cannot open SQLite database\"}\n";
        return 1;
    }

    try {
        const BatteryParameters battery = load_battery(database);
        sqlite3_close(database);

        const std::vector<Job> jobs{
            {"canal_quality_model", 2.2, 0.86},
            {"soil_carbon_analysis", 1.6, 0.79},
            {"habitat_connectivity_run", 3.1, 0.91}
        };

        const MilpModel model = BatteryChanceModel(jobs, battery).build(solar_scenarios());
        std::cout << std::fixed << std::setprecision(2)
                  << "{\"objective\":\"max eco_impact - battery_throughput_cost\","
                  << "\"variables\":" << model.objective.size()
                  << ",\"constraints\":" << model.inequalities.size()
                  << ",\"binary_variables\":" << model.binary_columns.size()
                  << ",\"scenario_count\":" << kScenarios
                  << ",\"maximum_hourly_violations\":" << static_cast<int>(0.05 * kScenarios)
                  << "}\n";
    } catch (const std::exception& error) {
        sqlite3_close(database);
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
