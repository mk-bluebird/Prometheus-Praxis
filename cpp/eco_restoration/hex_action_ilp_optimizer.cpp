// File: cpp/eco_restoration/hex_action_ilp_optimizer.cpp
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace eco_restoration {

struct HexAction {
    std::string id;
    double priority{};
    double crew_hours{};
    double budget{};
    std::vector<std::string> prerequisites;
};

struct OptimisationResult {
    std::vector<std::string> selected_actions;
    double total_priority{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class HexActionIlpOptimizer {
public:
    HexActionIlpOptimizer(std::vector<HexAction> actions, double crew_limit, double budget_limit)
        : actions_(std::move(actions)), crew_limit_(crew_limit), budget_limit_(budget_limit) {
        if (crew_limit < 0.0 || budget_limit < 0.0) throw std::invalid_argument("negative resource limit");
        for (std::size_t i = 0; i < actions_.size(); ++i) index_[actions_[i].id] = i;
        for (const auto& action : actions_) {
            for (const auto& dependency : action.prerequisites) {
                if (!index_.contains(dependency)) throw std::invalid_argument("unknown dependency: " + dependency);
            }
        }
    }

    OptimisationResult solve() {
        selected_.assign(actions_.size(), false);
        best_.assign(actions_.size(), false);
        search(0, 0.0, 0.0, 0.0);
        OptimisationResult result;
        for (std::size_t i = 0; i < actions_.size(); ++i)
            if (best_[i]) result.selected_actions.push_back(actions_[i].id);
        result.total_priority = best_priority_;
        result.knowledge_factor = actions_.empty() ? 0.0 :
            static_cast<double>(result.selected_actions.size()) / static_cast<double>(actions_.size());
        result.eco_impact_value = std::min(1.0, result.total_priority / priority_sum_);
        return result;
    }

private:
    bool dependencies_selected(const HexAction& action) const {
        for (const auto& id : action.prerequisites)
            if (!selected_[index_.at(id)]) return false;
        return true;
    }

    void search(std::size_t position, double crews, double budget, double priority) {
        if (position == actions_.size()) {
            if (priority > best_priority_) {
                best_priority_ = priority;
                best_ = selected_;
            }
            return;
        }
        double optimistic = priority;
        for (std::size_t i = position; i < actions_.size(); ++i) optimistic += actions_[i].priority;
        if (optimistic <= best_priority_) return;

        const auto& action = actions_[position];
        if (dependencies_selected(action) &&
            crews + action.crew_hours <= crew_limit_ &&
            budget + action.budget <= budget_limit_) {
            selected_[position] = true;
            search(position + 1, crews + action.crew_hours, budget + action.budget,
                   priority + action.priority);
            selected_[position] = false;
        }
        search(position + 1, crews, budget, priority);
    }

    std::vector<HexAction> actions_;
    std::unordered_map<std::string, std::size_t> index_;
    std::vector<bool> selected_, best_;
    double crew_limit_{}, budget_limit_{}, best_priority_{};
    double priority_sum_ = [&] {
        double total = 0.0;
        for (const auto& action : actions_) total += action.priority;
        return std::max(1e-12, total);
    }();
};

}  // namespace eco_restoration
