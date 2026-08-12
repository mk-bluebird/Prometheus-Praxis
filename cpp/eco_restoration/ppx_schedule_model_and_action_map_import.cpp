// File: cpp/eco_restoration/ppx_schedule_model_and_action_map_import.cpp
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

struct Job {
    std::string id;
    double eco_impact{};
    double power_w{};
    double energy_kwh{};
    double heat_load{};
    double water_load{};
};

struct Slot { double energy_budget_kwh{}; };
struct HexCorridor { std::int64_t anchor{}; double power_w{}; double heat_capacity{}; double water_capacity{}; };

struct MilpModel {
    std::vector<double> objective;
    std::vector<std::vector<double>> A;
    std::vector<double> b;
    std::size_t variables{};
};

class EcoAssignmentModel {
public:
    [[nodiscard]] MilpModel build(
        const std::vector<Job>& jobs,
        const std::vector<Slot>& slots,
        const std::vector<HexCorridor>& hexes) const {
        const std::size_t n = jobs.size() * slots.size() * hexes.size();
        MilpModel model{{}, {}, {}, n};
        model.objective.assign(n, 0.0);

        for (std::size_t j = 0; j < jobs.size(); ++j) {
            for (std::size_t t = 0; t < slots.size(); ++t) {
                for (std::size_t h = 0; h < hexes.size(); ++h) {
                    model.objective[index(j, t, h, slots.size(), hexes.size())] = jobs[j].eco_impact;
                }
            }
        }

        for (std::size_t j = 0; j < jobs.size(); ++j) {
            std::vector<double> row(n, 0.0);
            for (std::size_t t = 0; t < slots.size(); ++t)
                for (std::size_t h = 0; h < hexes.size(); ++h)
                    row[index(j, t, h, slots.size(), hexes.size())] = 1.0;
            model.A.push_back(std::move(row));
            model.b.push_back(1.0);
        }

        for (std::size_t t = 0; t < slots.size(); ++t) {
            std::vector<double> energy(n, 0.0);
            for (std::size_t j = 0; j < jobs.size(); ++j)
                for (std::size_t h = 0; h < hexes.size(); ++h)
                    energy[index(j, t, h, slots.size(), hexes.size())] = jobs[j].energy_kwh;
            model.A.push_back(std::move(energy));
            model.b.push_back(slots[t].energy_budget_kwh);
        }

        for (std::size_t t = 0; t < slots.size(); ++t) {
            for (std::size_t h = 0; h < hexes.size(); ++h) {
                std::vector<double> power(n, 0.0), heat(n, 0.0), water(n, 0.0);
                for (std::size_t j = 0; j < jobs.size(); ++j) {
                    const std::size_t column = index(j, t, h, slots.size(), hexes.size());
                    power[column] = jobs[j].power_w;
                    heat[column] = jobs[j].heat_load;
                    water[column] = jobs[j].water_load;
                }
                model.A.push_back(std::move(power)); model.b.push_back(hexes[h].power_w);
                model.A.push_back(std::move(heat)); model.b.push_back(hexes[h].heat_capacity);
                model.A.push_back(std::move(water)); model.b.push_back(hexes[h].water_capacity);
            }
        }
        return model;
    }

private:
    static std::size_t index(std::size_t job, std::size_t slot, std::size_t hex,
                             std::size_t slot_count, std::size_t hex_count) {
        return (job * slot_count + slot) * hex_count + hex;
    }
};

class HexActionMapImporter {
public:
    [[nodiscard]] std::unordered_map<std::int64_t, std::vector<std::string>>
    read_csv(const std::string& path) const {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("cannot open action-map CSV");

        std::string line;
        std::getline(input, line);
        std::unordered_map<std::int64_t, std::vector<std::string>> map;
        while (std::getline(input, line)) {
            const std::size_t separator = line.find(',');
            if (separator == std::string::npos) throw std::runtime_error("CSV requires hex_anchor,action");
            const std::int64_t anchor = std::stoll(line.substr(0, separator));
            const std::string action = line.substr(separator + 1);
            if (anchor < 0 || !safe_action(action)) throw std::runtime_error("invalid action-map row");
            map[anchor].push_back(action);
        }
        return map;
    }

    void write_lua(const std::unordered_map<std::int64_t, std::vector<std::string>>& map,
                   const std::string& output_path) const {
        std::ofstream output(output_path);
        if (!output) throw std::runtime_error("cannot write Lua action map");
        output << "return {\n";
        for (const auto& [anchor, actions] : map) {
            output << "  [" << anchor << "] = {";
            for (std::size_t i = 0; i < actions.size(); ++i) {
                if (i != 0) output << ", ";
                output << std::quoted(actions[i]);
            }
            output << "},\n";
        }
        output << "}\n";
    }

private:
    static bool safe_action(const std::string& action) {
        return !action.empty() && std::all_of(action.begin(), action.end(), [](unsigned char c) {
            return std::islower(c) || c == '_';
        });
    }
};

}  // namespace ppx::eco_restoration
