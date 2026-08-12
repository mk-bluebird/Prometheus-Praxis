// File: cpp/simulation/vcg_eco_workload_admission.cpp

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace eco_restoration {

struct TenantJob {
    std::string tenant_id;
    std::string job_id;
    double energy_kwh{};
    double ecological_value{};
};

struct Allocation {
    std::vector<std::size_t> selected;
    double welfare{};
};

class VcgEcoAdmission {
public:
    explicit VcgEcoAdmission(double energy_capacity_kwh)
        : energy_capacity_kwh_(energy_capacity_kwh) {
        if (energy_capacity_kwh_ < 0.0) throw std::invalid_argument("energy capacity is negative");
    }

    Allocation optimize(
        const std::vector<TenantJob>& jobs,
        const std::unordered_set<std::string>& excluded_tenants = {}) const {

        Allocation best;
        std::vector<std::size_t> selected;
        branch(jobs, excluded_tenants, 0, 0.0, 0.0, selected, best);
        return best;
    }

    struct TenantOutcome {
        std::string tenant_id;
        double allocated_value{};
        double externality_payment{};
    };

    std::vector<TenantOutcome> allocate(const std::vector<TenantJob>& jobs) const {
        const Allocation social_optimum = optimize(jobs);
        std::unordered_set<std::string> tenants;
        for (const TenantJob& job : jobs) tenants.insert(job.tenant_id);

        std::vector<TenantOutcome> outcomes;
        for (const std::string& tenant : tenants) {
            double tenant_value = 0.0;
            double others_with_tenant = 0.0;

            for (std::size_t index : social_optimum.selected) {
                if (jobs[index].tenant_id == tenant) tenant_value += jobs[index].ecological_value;
                else others_with_tenant += jobs[index].ecological_value;
            }

            const Allocation without_tenant = optimize(jobs, {tenant});
            outcomes.push_back({
                tenant,
                tenant_value,
                std::max(0.0, without_tenant.welfare - others_with_tenant)
            });
        }
        return outcomes;
    }

private:
    void branch(
        const std::vector<TenantJob>& jobs,
        const std::unordered_set<std::string>& excluded,
        std::size_t index,
        double used_energy,
        double welfare,
        std::vector<std::size_t>& selected,
        Allocation& best) const {

        if (index == jobs.size()) {
            if (welfare > best.welfare) best = {selected, welfare};
            return;
        }

        branch(jobs, excluded, index + 1U, used_energy, welfare, selected, best);
        const TenantJob& job = jobs[index];

        if (!excluded.contains(job.tenant_id) && job.energy_kwh >= 0.0 &&
            job.ecological_value >= 0.0 &&
            used_energy + job.energy_kwh <= energy_capacity_kwh_) {
            selected.push_back(index);
            branch(
                jobs, excluded, index + 1U, used_energy + job.energy_kwh,
                welfare + job.ecological_value, selected, best);
            selected.pop_back();
        }
    }

    double energy_capacity_kwh_;
};

}  // namespace eco_restoration
