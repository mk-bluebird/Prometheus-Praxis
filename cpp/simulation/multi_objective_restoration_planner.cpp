// File: cpp/simulation/multi_objective_restoration_planner.cpp
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace eco_restoration {

struct RestorationAction {
    std::string id;
    std::string community_group;
    double cost_usd{};
    double priority{};
    double cooling_benefit{};
    double ecological_benefit{};
    std::vector<std::string> prerequisites;
};

struct PlannerPolicy {
    double total_budget_usd{};
    std::unordered_map<std::string, double> minimum_group_budget_usd;
    double cooling_weight{};
    double ecological_weight{};
    double priority_weight{};
};

struct BinaryMilp {
    std::vector<double> objective;
    std::vector<std::vector<double>> inequalities;
    std::vector<double> bounds;
    std::vector<std::size_t> binary_columns;
};

class MultiObjectiveRestorationPlanner {
public:
    BinaryMilp build(const std::vector<RestorationAction>& actions,
                     const PlannerPolicy& policy) const {
        if (actions.empty() || policy.total_budget_usd < 0.0) throw std::invalid_argument("invalid planner input");

        BinaryMilp model;
        model.objective.resize(actions.size());
        model.binary_columns.resize(actions.size());
        std::unordered_map<std::string, std::size_t> index;
        for (std::size_t i = 0; i < actions.size(); ++i) {
            const auto& action = actions[i];
            if (action.id.empty() || action.community_group.empty() || action.cost_usd < 0.0 ||
                action.priority < 0.0 || action.cooling_benefit < 0.0 || action.ecological_benefit < 0.0)
                throw std::invalid_argument("invalid restoration action");
            index[action.id] = i;
            model.binary_columns[i] = i;
            model.objective[i] = policy.priority_weight * action.priority +
                                 policy.cooling_weight * action.cooling_benefit +
                                 policy.ecological_weight * action.ecological_benefit;
        }

        std::vector<double> total_cost(actions.size());
        for (std::size_t i = 0; i < actions.size(); ++i) total_cost[i] = actions[i].cost_usd;
        model.inequalities.push_back(std::move(total_cost));
        model.bounds.push_back(policy.total_budget_usd);

        for (std::size_t i = 0; i < actions.size(); ++i) {
            for (const auto& requirement : actions[i].prerequisites) {
                const auto found = index.find(requirement);
                if (found == index.end()) throw std::invalid_argument("unknown restoration dependency");
                std::vector<double> dependency(actions.size());
                dependency[i] = 1.0;
                dependency[found->second] = -1.0;
                model.inequalities.push_back(std::move(dependency));
                model.bounds.push_back(0.0);
            }
        }

        for (const auto& [group, minimum_budget] : policy.minimum_group_budget_usd) {
            if (minimum_budget < 0.0) throw std::invalid_argument("negative equity allocation");
            std::vector<double> allocation(actions.size());
            for (std::size_t i = 0; i < actions.size(); ++i)
                if (actions[i].community_group == group) allocation[i] = -actions[i].cost_usd;
            model.inequalities.push_back(std::move(allocation));
            model.bounds.push_back(-minimum_budget);
        }
        return model;
    }
};

}  // namespace eco_restoration
