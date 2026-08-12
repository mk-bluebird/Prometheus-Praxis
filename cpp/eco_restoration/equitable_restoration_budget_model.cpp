// File: cpp/eco_restoration/equitable_restoration_budget_model.cpp

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct RestorationProject {
    std::uint64_t hex_anchor{};
    double annual_cost{};
    double ecological_impact{};
    double community_benefit{};
    double equity_priority{};
};

struct AllocationWeights {
    double ecological{};
    double community{};
    double equity{};
    double minimum_community_benefit{};
    double minimum_equity_priority{};
};

struct MilpModel {
    std::vector<double> objective;
    std::vector<std::vector<double>> inequalities;
    std::vector<double> bounds;
    std::vector<std::size_t> binary_columns;
};

class EquitableRestorationBudgetModel {
public:
    [[nodiscard]] MilpModel build(
        const std::vector<RestorationProject>& projects,
        const AllocationWeights& weights,
        double annual_budget) const {

        if (projects.empty() || annual_budget < 0.0 ||
            weights.ecological < 0.0 || weights.community < 0.0 || weights.equity < 0.0 ||
            weights.minimum_community_benefit < 0.0 || weights.minimum_equity_priority < 0.0) {
            throw std::invalid_argument("invalid allocation model inputs");
        }

        MilpModel model;
        model.objective.resize(projects.size());
        model.binary_columns.reserve(projects.size());

        std::vector<double> budget(projects.size(), 0.0);
        std::vector<double> community_goal(projects.size(), 0.0);
        std::vector<double> equity_goal(projects.size(), 0.0);

        for (std::size_t i = 0; i < projects.size(); ++i) {
            const RestorationProject& project = projects[i];
            if (project.annual_cost < 0.0 || project.ecological_impact < 0.0 ||
                project.community_benefit < 0.0 || project.equity_priority < 0.0) {
                throw std::invalid_argument("project values must be nonnegative");
            }

            model.objective[i] =
                weights.ecological * project.ecological_impact +
                weights.community * project.community_benefit +
                weights.equity * project.equity_priority;

            budget[i] = project.annual_cost;
            community_goal[i] = -project.community_benefit;
            equity_goal[i] = -project.equity_priority;
            model.binary_columns.push_back(i);
        }

        model.inequalities.push_back(std::move(budget));
        model.bounds.push_back(annual_budget);

        model.inequalities.push_back(std::move(community_goal));
        model.bounds.push_back(-weights.minimum_community_benefit);

        model.inequalities.push_back(std::move(equity_goal));
        model.bounds.push_back(-weights.minimum_equity_priority);
        return model;
    }
};

}  // namespace eco_restoration
