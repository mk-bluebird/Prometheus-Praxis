// File: cpp/eco_restoration/ppx_carbon_negative_workload_scheduler.cpp
#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

struct Workload {
    std::string workload_id;
    double eco_impact_value{};
    double power_w{};
    double duration_h{};
    double thermal_load_w{};
    double verified_ecological_benefit_g{};
    double forecast_renewable_fraction{};
    double grid_carbon_g_per_kwh{};
};

struct Capacity {
    double power_w{};
    double renewable_kwh{};
    double thermal_load_w{};
};

struct Schedule {
    std::vector<std::string> workload_ids;
    double total_eco_impact{};
    double power_w{};
    double renewable_kwh{};
    double thermal_load_w{};
    double operational_carbon_g{};
};

class CarbonNegativeScheduler {
public:
    [[nodiscard]] Schedule solve(std::vector<Workload> workloads, const Capacity& capacity) {
        validate(capacity);
        workloads.erase(std::remove_if(workloads.begin(), workloads.end(),
            [](const Workload& w) {
                const double energy_kwh = w.power_w * w.duration_h / 1000.0;
                const double carbon_g = energy_kwh * (1.0 - w.forecast_renewable_fraction) *
                                        w.grid_carbon_g_per_kwh;
                return w.workload_id.empty() || w.eco_impact_value <= 0.0 ||
                       w.power_w < 0.0 || w.duration_h < 0.0 || w.thermal_load_w < 0.0 ||
                       w.forecast_renewable_fraction < 0.0 || w.forecast_renewable_fraction > 1.0 ||
                       w.verified_ecological_benefit_g < carbon_g;
            }), workloads.end());

        std::sort(workloads.begin(), workloads.end(),
            [](const Workload& a, const Workload& b) {
                return a.eco_impact_value > b.eco_impact_value;
            });

        best_ = {};
        suffix_upper_bound_.assign(workloads.size() + 1, 0.0);
        for (std::size_t i = workloads.size(); i > 0; --i) {
            suffix_upper_bound_[i - 1] = suffix_upper_bound_[i] + workloads[i - 1].eco_impact_value;
        }
        search(workloads, capacity, 0, {});
        return best_;
    }

private:
    Schedule best_{};
    std::vector<double> suffix_upper_bound_;

    void search(const std::vector<Workload>& jobs, const Capacity& capacity,
                std::size_t index, Schedule current) {
        if (index == jobs.size()) {
            if (current.total_eco_impact > best_.total_eco_impact) best_ = std::move(current);
            return;
        }
        if (current.total_eco_impact + suffix_upper_bound_[index] <= best_.total_eco_impact) return;

        const Workload& job = jobs[index];
        const double energy_kwh = job.power_w * job.duration_h / 1000.0;
        const double carbon_g = energy_kwh * (1.0 - job.forecast_renewable_fraction) *
                                job.grid_carbon_g_per_kwh;

        if (current.power_w + job.power_w <= capacity.power_w &&
            current.renewable_kwh + energy_kwh <= capacity.renewable_kwh &&
            current.thermal_load_w + job.thermal_load_w <= capacity.thermal_load_w) {
            Schedule selected = current;
            selected.workload_ids.push_back(job.workload_id);
            selected.total_eco_impact += job.eco_impact_value;
            selected.power_w += job.power_w;
            selected.renewable_kwh += energy_kwh;
            selected.thermal_load_w += job.thermal_load_w;
            selected.operational_carbon_g += carbon_g;
            search(jobs, capacity, index + 1, std::move(selected));
        }
        search(jobs, capacity, index + 1, std::move(current));
    }

    static void validate(const Capacity& capacity) {
        if (capacity.power_w < 0.0 || capacity.renewable_kwh < 0.0 ||
            capacity.thermal_load_w < 0.0) {
            throw std::invalid_argument("scheduling capacities must be non-negative");
        }
    }
};

}  // namespace ppx::eco_restoration
