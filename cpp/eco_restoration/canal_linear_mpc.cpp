// File: cpp/eco_restoration/canal_linear_mpc.cpp
#include <Eigen/Dense>

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct CanalMpcModel {
    Eigen::Matrix2d transition;
    Eigen::Matrix<double, 2, 2> control;
    Eigen::Matrix2d state_cost;
    Eigen::Matrix2d control_cost;
    Eigen::Matrix2d lyapunov_p;
    double lyapunov_basin{};
    Eigen::Vector2d control_min;
    Eigen::Vector2d control_max;
};

struct CanalMpcPlan {
    std::vector<Eigen::Vector2d> pump_gate_schedule;
    std::vector<Eigen::Vector2d> predicted_states;
    double objective{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class CanalLinearMpc {
public:
    explicit CanalLinearMpc(CanalMpcModel model) : model_(std::move(model)) {
        if (model_.lyapunov_basin <= 0.0 ||
            (model_.control_max.array() < model_.control_min.array()).any()) {
            throw std::invalid_argument("invalid MPC model bounds");
        }
    }

    std::optional<CanalMpcPlan> solve(const Eigen::Vector2d& initial_state,
                                      const Eigen::Vector2d& target_state,
                                      int horizon, int iterations = 500) const {
        if (horizon < 1 || iterations < 1) throw std::invalid_argument("invalid horizon");
        Eigen::VectorXd controls = Eigen::VectorXd::Zero(2 * horizon);
        for (int step = 0; step < horizon; ++step) {
            controls.segment<2>(2 * step) =
                ((model_.control_min + model_.control_max) * 0.5);
        }

        constexpr double epsilon = 1e-5;
        constexpr double learning_rate = 0.03;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            Eigen::VectorXd gradient(controls.size());
            for (Eigen::Index i = 0; i < controls.size(); ++i) {
                Eigen::VectorXd plus = controls;
                Eigen::VectorXd minus = controls;
                plus(i) += epsilon;
                minus(i) -= epsilon;
                gradient(i) = (objective(initial_state, target_state, plus, horizon) -
                               objective(initial_state, target_state, minus, horizon)) /
                              (2.0 * epsilon);
            }
            controls -= learning_rate * gradient;
            for (int step = 0; step < horizon; ++step) {
                controls.segment<2>(2 * step) =
                    controls.segment<2>(2 * step).cwiseMax(model_.control_min)
                        .cwiseMin(model_.control_max);
            }
        }

        CanalMpcPlan result;
        result.objective = objective(initial_state, target_state, controls, horizon);
        Eigen::Vector2d state = initial_state;
        double worst_basin_fraction = 0.0;
        for (int step = 0; step < horizon; ++step) {
            const Eigen::Vector2d control = controls.segment<2>(2 * step);
            state = model_.transition * state + model_.control * control;
            const double basin_fraction =
                state.dot(model_.lyapunov_p * state) / model_.lyapunov_basin;
            if (!std::isfinite(basin_fraction) || basin_fraction > 1.0) return std::nullopt;
            worst_basin_fraction = std::max(worst_basin_fraction, basin_fraction);
            result.pump_gate_schedule.push_back(control);
            result.predicted_states.push_back(state);
        }

        result.knowledge_factor = std::clamp(
            1.0 - worst_basin_fraction, 0.0, 1.0);
        const double pump_energy = controls.squaredNorm();
        result.eco_impact_value = std::clamp(
            result.knowledge_factor / (1.0 + 0.01 * pump_energy), 0.0, 1.0);
        return result;
    }

private:
    double objective(const Eigen::Vector2d& initial, const Eigen::Vector2d& target,
                     const Eigen::VectorXd& controls, int horizon) const {
        Eigen::Vector2d state = initial;
        double total = 0.0;
        for (int step = 0; step < horizon; ++step) {
            const Eigen::Vector2d control = controls.segment<2>(2 * step);
            state = model_.transition * state + model_.control * control;
            const Eigen::Vector2d error = state - target;
            const double basin = state.dot(model_.lyapunov_p * state) / model_.lyapunov_basin;
            total += error.dot(model_.state_cost * error) +
                     control.dot(model_.control_cost * control) +
                     10000.0 * std::max(0.0, basin - 1.0) * std::max(0.0, basin - 1.0);
        }
        return total;
    }

    CanalMpcModel model_;
};

}  // namespace eco_restoration
