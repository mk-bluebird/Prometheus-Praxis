// File: cpp/tools/eco_workload_planner.cpp

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eco_restoration {

struct Job {
    std::string id;
    double eco_impact{};
    double energy_kwh{};
    double power_w{};
    double heat_load{};
    double water_load{};
    double verified_benefit_g{};
    double carbon_g{};
};

struct Slot {
    std::string id;
    double energy_budget_kwh{};
};

struct HexCorridor {
    std::uint64_t anchor{};
    double power_budget_w{};
    double heat_budget{};
    double water_budget{};
};

struct MilpModel {
    std::vector<double> objective;
    std::vector<std::vector<double>> inequalities;
    std::vector<double> bounds;
    std::size_t binary_variable_count{};
};

struct HexAnchor {
    std::uint8_t level{};
    std::uint32_t row{};
    std::uint32_t column{};

    [[nodiscard]] std::optional<std::uint64_t> pack64() const {
        constexpr std::uint32_t coordinate_max = (1U << 30U) - 1U;
        if (level > 15U || row > coordinate_max || column > coordinate_max) {
            return std::nullopt;
        }
        return (static_cast<std::uint64_t>(level) << 60U) |
               (static_cast<std::uint64_t>(row) << 30U) |
               static_cast<std::uint64_t>(column);
    }

    [[nodiscard]] static HexAnchor unpack64(std::uint64_t anchor) {
        return {
            static_cast<std::uint8_t>((anchor >> 60U) & 0x0FU),
            static_cast<std::uint32_t>((anchor >> 30U) & 0x3FFFFFFFU),
            static_cast<std::uint32_t>(anchor & 0x3FFFFFFFU)
        };
    }
};

[[nodiscard]] double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] double weighted_clamped_risk_sum(
    const std::vector<double>& coordinates,
    const std::vector<double>& weights) {

    if (coordinates.size() != weights.size()) {
        throw std::invalid_argument("risk coordinate and weight counts differ");
    }

    double total = 0.0;
    for (std::size_t i = 0; i < coordinates.size(); ++i) {
        if (!std::isfinite(coordinates[i]) || !std::isfinite(weights[i]) || weights[i] < 0.0) {
            throw std::invalid_argument("risk inputs must be finite with nonnegative weights");
        }
        total += clamp01(coordinates[i]) * weights[i];
    }
    return clamp01(total);
}

[[nodiscard]] double knowledge_factor(
    double confidence,
    const std::vector<double>& reliabilities,
    const std::vector<double>& weights) {

    if (confidence <= 0.0 || confidence > 1.0 || reliabilities.size() != weights.size()) {
        throw std::invalid_argument("invalid knowledge-factor inputs");
    }

    double log_sum = std::log(confidence);
    double total_weight = 1.0;
    for (std::size_t i = 0; i < reliabilities.size(); ++i) {
        if (reliabilities[i] <= 0.0 || reliabilities[i] > 1.0 || weights[i] < 0.0) {
            throw std::invalid_argument("invalid sensor reliability");
        }
        log_sum += weights[i] * std::log(reliabilities[i]);
        total_weight += weights[i];
    }
    return std::exp(log_sum / total_weight);
}

class EcoAssignmentModel {
public:
    [[nodiscard]] MilpModel build(
        const std::vector<Job>& jobs,
        const std::vector<Slot>& slots,
        const std::vector<HexCorridor>& hexes) const {

        if (jobs.empty() || slots.empty() || hexes.empty()) {
            throw std::invalid_argument("jobs, slots, and hex corridors must be nonempty");
        }

        const std::size_t variable_count = jobs.size() * slots.size() * hexes.size();
        MilpModel model{{}, {}, {}, variable_count};
        model.objective.assign(variable_count, 0.0);

        for (std::size_t j = 0; j < jobs.size(); ++j) {
            validate_job(jobs[j]);
            for (std::size_t t = 0; t < slots.size(); ++t) {
                for (std::size_t h = 0; h < hexes.size(); ++h) {
                    model.objective[index(j, t, h, slots.size(), hexes.size())] =
                        jobs[j].eco_impact;
                }
            }
        }

        for (std::size_t j = 0; j < jobs.size(); ++j) {
            std::vector<double> row(variable_count, 0.0);
            for (std::size_t t = 0; t < slots.size(); ++t) {
                for (std::size_t h = 0; h < hexes.size(); ++h) {
                    row[index(j, t, h, slots.size(), hexes.size())] = 1.0;
                }
            }
            add_constraint(model, std::move(row), 1.0);
        }

        for (std::size_t t = 0; t < slots.size(); ++t) {
            if (slots[t].energy_budget_kwh < 0.0) {
                throw std::invalid_argument("slot energy budget is negative");
            }
            std::vector<double> row(variable_count, 0.0);
            for (std::size_t j = 0; j < jobs.size(); ++j) {
                for (std::size_t h = 0; h < hexes.size(); ++h) {
                    row[index(j, t, h, slots.size(), hexes.size())] = jobs[j].energy_kwh;
                }
            }
            add_constraint(model, std::move(row), slots[t].energy_budget_kwh);
        }

        for (std::size_t t = 0; t < slots.size(); ++t) {
            for (std::size_t h = 0; h < hexes.size(); ++h) {
                validate_hex(hexes[h]);
                add_hex_constraint(model, jobs, t, h, hexes, &Job::power_w, hexes[h].power_budget_w);
                add_hex_constraint(model, jobs, t, h, hexes, &Job::heat_load, hexes[h].heat_budget);
                add_hex_constraint(model, jobs, t, h, hexes, &Job::water_load, hexes[h].water_budget);
            }
        }
        return model;
    }

private:
    static std::size_t index(
        std::size_t job, std::size_t slot, std::size_t hex,
        std::size_t slot_count, std::size_t hex_count) {
        return ((job * slot_count) + slot) * hex_count + hex;
    }

    static void add_constraint(MilpModel& model, std::vector<double> row, double bound) {
        model.inequalities.push_back(std::move(row));
        model.bounds.push_back(bound);
    }

    static void validate_job(const Job& job) {
        if (job.id.empty() || job.eco_impact < 0.0 || job.energy_kwh < 0.0 ||
            job.power_w < 0.0 || job.heat_load < 0.0 || job.water_load < 0.0 ||
            job.verified_benefit_g <= job.carbon_g) {
            throw std::invalid_argument("job fails ecological admission requirements");
        }
    }

    static void validate_hex(const HexCorridor& hex) {
        if (hex.power_budget_w < 0.0 || hex.heat_budget < 0.0 || hex.water_budget < 0.0) {
            throw std::invalid_argument("hex corridor budget is negative");
        }
    }

    static void add_hex_constraint(
        MilpModel& model,
        const std::vector<Job>& jobs,
        std::size_t slot,
        std::size_t hex,
        const std::vector<HexCorridor>& hexes,
        double Job::*metric,
        double bound) {

        const std::size_t variable_count = model.binary_variable_count;
        const std::size_t slot_count = variable_count / (jobs.size() * hexes.size());
        std::vector<double> row(variable_count, 0.0);
        for (std::size_t j = 0; j < jobs.size(); ++j) {
            row[index(j, slot, hex, slot_count, hexes.size())] = jobs[j].*metric;
        }
        add_constraint(model, std::move(row), bound);
    }
};

[[nodiscard]] bool valid_action(std::string_view action) {
    if (action.empty() || action.size() > 64U) {
        return false;
    }
    for (const char character : action) {
        const bool allowed = (character >= 'a' && character <= 'z') ||
                             (character >= '0' && character <= '9') ||
                             character == '_';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<std::pair<std::uint64_t, std::string>> parse_record(
    const std::string& line) {

    const std::size_t delimiter = line.find(',');
    if (delimiter == std::string::npos || line.find(',', delimiter + 1U) != std::string::npos) {
        return std::nullopt;
    }

    try {
        const std::uint64_t anchor = std::stoull(line.substr(0, delimiter));
        std::string action = line.substr(delimiter + 1U);
        if (!valid_action(action)) {
            return std::nullopt;
        }
        return std::make_pair(anchor, std::move(action));
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void import_hex_actions(const std::string& csv_path, const std::string& lua_path) {
    std::ifstream input(csv_path);
    if (!input) {
        throw std::runtime_error("unable to open input CSV");
    }

    std::ofstream output(lua_path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("unable to create Lua module");
    }

    std::string header;
    if (!std::getline(input, header) || header != "hex_anchor,action") {
        throw std::invalid_argument("CSV header must be hex_anchor,action");
    }

    std::unordered_set<std::uint64_t> anchors;
    std::vector<std::pair<std::uint64_t, std::string>> actions;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto record = parse_record(line);
        if (!record || !anchors.insert(record->first).second) {
            throw std::invalid_argument("invalid or duplicate hex action record");
        }
        actions.push_back(*record);
    }

    std::sort(actions.begin(), actions.end());
    output << "local actions = {\n";
    for (const auto& [anchor, action] : actions) {
        output << "  [" << anchor << "] = \"" << action << "\",\n";
    }
    output << "}\nreturn actions\n";
}

void print_model_summary(const MilpModel& model) {
    std::cout << "{\"objective\":\"max c^T x\",\"inequality\":\"A x <= b\","
              << "\"binary_variables\":" << model.binary_variable_count
              << ",\"constraints\":" << model.inequalities.size() << "}\n";
}

}  // namespace eco_restoration

int main(int argc, char** argv) {
    using namespace eco_restoration;

    try {
        if (argc == 4 && std::string_view(argv[1]) == "import-actions") {
            import_hex_actions(argv[2], argv[3]);
            std::cout << "{\"status\":\"imported\"}\n";
            return 0;
        }

        if (argc == 1) {
            const std::vector<Job> jobs{
                {"canal_monitoring", 0.82, 0.42, 250.0, 0.10, 0.02, 310.0, 95.0},
                {"soil_assessment", 0.74, 0.31, 180.0, 0.06, 0.01, 220.0, 70.0}
            };
            const std::vector<Slot> slots{{"hour_00", 1.0}, {"hour_01", 1.0}};
            const std::vector<HexCorridor> hexes{{101, 500.0, 0.30, 0.08}};
            print_model_summary(EcoAssignmentModel{}.build(jobs, slots, hexes));
            return 0;
        }

        std::cerr << "usage: eco_workload_planner [import-actions input.csv output.lua]\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "{\"error\":\"" << error.what() << "\"}\n";
        return 1;
    }
}
