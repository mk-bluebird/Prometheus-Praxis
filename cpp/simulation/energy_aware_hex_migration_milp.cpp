// File: cpp/simulation/energy_aware_hex_migration_milp.cpp
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct Job {
    std::string id;
    double energy_kwh{};
    double heat_load{};
    double eco_impact{};
};

struct HexSlot {
    std::uint64_t anchor{};
    double energy_capacity_kwh{};
    double thermal_capacity{};
};

struct LinearMilp {
    std::vector<double> objective;
    std::vector<std::vector<double>> inequalities;
    std::vector<double> upper_bounds;
    std::vector<std::size_t> binary_columns;
};

class EnergyAwareMigrationMilp {
public:
    LinearMilp build(const std::vector<Job>& jobs, const std::vector<HexSlot>& hexes,
                     std::size_t time_slots, double migration_energy_kwh) const {
        if (jobs.empty() || hexes.empty() || time_slots == 0 || migration_energy_kwh < 0.0)
            throw std::invalid_argument("invalid migration model input");

        const std::size_t assignments = jobs.size() * time_slots * hexes.size();
        const std::size_t transitions = jobs.size() * (time_slots - 1) * hexes.size() * hexes.size();
        LinearMilp model;
        model.objective.assign(assignments + transitions, 0.0);
        model.binary_columns.resize(model.objective.size());
        for (std::size_t i = 0; i < model.binary_columns.size(); ++i) model.binary_columns[i] = i;

        for (std::size_t job = 0; job < jobs.size(); ++job)
            for (std::size_t time = 0; time < time_slots; ++time)
                for (std::size_t hex = 0; hex < hexes.size(); ++hex)
                    model.objective[assignment_index(job, time, hex, time_slots, hexes.size())] =
                        jobs[job].eco_impact;

        for (std::size_t job = 0; job < jobs.size(); ++job)
            for (std::size_t time = 1; time < time_slots; ++time)
                for (std::size_t from = 0; from < hexes.size(); ++from)
                    for (std::size_t to = 0; to < hexes.size(); ++to)
                        if (from != to)
                            model.objective[transition_index(
                                assignments, job, time, from, to, time_slots, hexes.size())] =
                                -migration_energy_kwh;

        for (std::size_t job = 0; job < jobs.size(); ++job) {
            for (std::size_t time = 0; time < time_slots; ++time) {
                std::vector<double> assignment(model.objective.size());
                for (std::size_t hex = 0; hex < hexes.size(); ++hex)
                    assignment[assignment_index(job, time, hex, time_slots, hexes.size())] = 1.0;
                model.inequalities.push_back(std::move(assignment));
                model.upper_bounds.push_back(1.0);
            }
        }

        for (std::size_t time = 0; time < time_slots; ++time) {
            for (std::size_t hex = 0; hex < hexes.size(); ++hex) {
                std::vector<double> energy(model.objective.size()), heat(model.objective.size());
                for (std::size_t job = 0; job < jobs.size(); ++job) {
                    const auto column = assignment_index(job, time, hex, time_slots, hexes.size());
                    energy[column] = jobs[job].energy_kwh;
                    heat[column] = jobs[job].heat_load;
                }
                model.inequalities.push_back(std::move(energy));
                model.upper_bounds.push_back(hexes[hex].energy_capacity_kwh);
                model.inequalities.push_back(std::move(heat));
                model.upper_bounds.push_back(hexes[hex].thermal_capacity);
            }
        }

        for (std::size_t job = 0; job < jobs.size(); ++job)
            for (std::size_t time = 1; time < time_slots; ++time)
                for (std::size_t from = 0; from < hexes.size(); ++from)
                    for (std::size_t to = 0; to < hexes.size(); ++to) {
                        if (from == to) continue;
                        std::vector<double> link(model.objective.size());
                        const auto migration = transition_index(assignments, job, time, from, to,
                                                               time_slots, hexes.size());
                        link[migration] = 1.0;
                        link[assignment_index(job, time - 1, from, time_slots, hexes.size())] = -1.0;
                        link[assignment_index(job, time, to, time_slots, hexes.size())] = -1.0;
                        model.inequalities.push_back(std::move(link));
                        model.upper_bounds.push_back(0.0);
                    }
        return model;
    }

private:
    static std::size_t assignment_index(std::size_t job, std::size_t time, std::size_t hex,
                                        std::size_t slots, std::size_t hexes) {
        return (job * slots + time) * hexes + hex;
    }

    static std::size_t transition_index(std::size_t assignment_count, std::size_t job,
                                        std::size_t time, std::size_t from, std::size_t to,
                                        std::size_t slots, std::size_t hexes) {
        return assignment_count + (((job * (slots - 1) + (time - 1)) * hexes + from) * hexes + to);
    }
};

}  // namespace eco_restoration
