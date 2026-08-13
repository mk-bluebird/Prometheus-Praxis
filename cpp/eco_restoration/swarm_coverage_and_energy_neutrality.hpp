// File: cpp/eco_restoration/swarm_coverage_and_energy_neutrality.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct Point2D {
    double x_m{};
    double y_m{};
};

struct CoverageSample {
    Point2D location;
    double ecological_priority{};
};

struct SafeCorridor {
    double min_x_m{};
    double max_x_m{};
    double min_y_m{};
    double max_y_m{};
    std::vector<Point2D> obstacle_centers;
    double obstacle_clearance_m{};
    double agent_clearance_m{};
};

struct CoveragePlan {
    std::vector<Point2D> next_positions;
    double coverage_cost{};
    bool corridor_safe{};
    double knowledge_factor{};
    double eco_impact_value{};
};

/*
For samples q_k with nonnegative ecological density phi_k, assign each sample
to its nearest agent. The weighted centroid of the resulting discrete Voronoi
cell is C_i=sum(q_k*phi_k)/sum(phi_k), and a descent direction is C_i-p_i.

Obstacle-safe planning uses a projected step:
p_i(next)=Project_safe(p_i+step*(C_i-p_i)).
Project_safe enforces corridor boundaries, clearance from obstacles, and
pairwise agent separation. This is a planning component only; it produces
candidate seed-dispersal positions and does not command physical equipment.
*/
inline double squared_distance(const Point2D& a, const Point2D& b) {
    const double dx = a.x_m - b.x_m;
    const double dy = a.y_m - b.y_m;
    return dx * dx + dy * dy;
}

inline Point2D clamp_to_corridor(Point2D point, const SafeCorridor& corridor) {
    point.x_m = std::clamp(point.x_m, corridor.min_x_m, corridor.max_x_m);
    point.y_m = std::clamp(point.y_m, corridor.min_y_m, corridor.max_y_m);

    for (const auto& obstacle : corridor.obstacle_centers) {
        const double dx = point.x_m - obstacle.x_m;
        const double dy = point.y_m - obstacle.y_m;
        const double distance = std::hypot(dx, dy);
        if (distance < corridor.obstacle_clearance_m) {
            const double safe_distance = corridor.obstacle_clearance_m;
            const double ux = distance > 1e-9 ? dx / distance : 1.0;
            const double uy = distance > 1e-9 ? dy / distance : 0.0;
            point.x_m = obstacle.x_m + safe_distance * ux;
            point.y_m = obstacle.y_m + safe_distance * uy;
            point.x_m = std::clamp(point.x_m, corridor.min_x_m, corridor.max_x_m);
            point.y_m = std::clamp(point.y_m, corridor.min_y_m, corridor.max_y_m);
        }
    }
    return point;
}

inline CoveragePlan plan_safe_voronoi_coverage(
    const std::vector<Point2D>& positions,
    const std::vector<CoverageSample>& samples,
    const SafeCorridor& corridor, double step_size_m) {

    if (positions.empty() || samples.empty() || step_size_m <= 0.0 ||
        corridor.min_x_m > corridor.max_x_m || corridor.min_y_m > corridor.max_y_m ||
        corridor.obstacle_clearance_m < 0.0 || corridor.agent_clearance_m < 0.0) {
        throw std::invalid_argument("invalid coverage-planning inputs");
    }

    std::vector<double> weighted_x(positions.size(), 0.0);
    std::vector<double> weighted_y(positions.size(), 0.0);
    std::vector<double> mass(positions.size(), 0.0);

    for (const auto& sample : samples) {
        if (sample.ecological_priority < 0.0) {
            throw std::invalid_argument("ecological priority must be nonnegative");
        }
        std::size_t nearest = 0;
        double nearest_distance = squared_distance(sample.location, positions.front());
        for (std::size_t i = 1; i < positions.size(); ++i) {
            const double distance = squared_distance(sample.location, positions[i]);
            if (distance < nearest_distance) {
                nearest = i;
                nearest_distance = distance;
            }
        }
        weighted_x[nearest] += sample.location.x_m * sample.ecological_priority;
        weighted_y[nearest] += sample.location.y_m * sample.ecological_priority;
        mass[nearest] += sample.ecological_priority;
    }

    std::vector<Point2D> next = positions;
    for (std::size_t i = 0; i < positions.size(); ++i) {
        if (mass[i] <= 0.0) continue;
        const Point2D centroid{weighted_x[i] / mass[i], weighted_y[i] / mass[i]};
        const double dx = centroid.x_m - positions[i].x_m;
        const double dy = centroid.y_m - positions[i].y_m;
        const double distance = std::hypot(dx, dy);
        const double scale = distance > step_size_m ? step_size_m / distance : 1.0;
        next[i] = clamp_to_corridor(
            {positions[i].x_m + scale * dx, positions[i].y_m + scale * dy}, corridor);
    }

    bool safe = true;
    for (std::size_t i = 0; i < next.size(); ++i) {
        for (std::size_t j = i + 1; j < next.size(); ++j) {
            if (std::sqrt(squared_distance(next[i], next[j])) < corridor.agent_clearance_m) {
                safe = false;
            }
        }
    }

    double cost = 0.0;
    for (const auto& sample : samples) {
        double nearest_distance = squared_distance(sample.location, next.front());
        for (std::size_t i = 1; i < next.size(); ++i) {
            nearest_distance = std::min(nearest_distance,
                                        squared_distance(sample.location, next[i]));
        }
        cost += sample.ecological_priority * nearest_distance;
    }

    const double coverage_score = 1.0 / (1.0 + cost / std::max(1.0, static_cast<double>(samples.size())));
    return {next, cost, safe,
            std::clamp(0.55 * coverage_score + (safe ? 0.45 : 0.0), 0.0, 1.0),
            safe ? std::clamp(coverage_score, 0.0, 1.0) : 0.0};
}

enum class EnergyNeutralState {
    EnergyNeutralPlan,
    HoldForRecharge
};

struct EnergyNeutralAssessment {
    EnergyNeutralState state{EnergyNeutralState::HoldForRecharge};
    double final_energy_j{};
    double minimum_energy_j{};
    double harvested_energy_j{};
    double required_energy_j{};
    double knowledge_factor{};
    double eco_impact_value{};
};

/*
For discrete intervals k:
E_(k+1)=E_k+eta*h_k*delta_t-c_a(u_k)*delta_t.

Intermittent energy neutrality requires:
E_k>=E_reserve for every prefix k, and
sum_k eta*h_k*delta_t >= sum_k c_a(u_k)*delta_t.
A planned operation is permitted only when both conditions hold. Otherwise the
safe state is HoldForRecharge, which emits no physical actuation command.
*/
inline EnergyNeutralAssessment assess_energy_neutral_plan(
    double initial_energy_j, double reserve_energy_j, double harvest_efficiency,
    double interval_seconds, const std::vector<double>& harvested_power_w,
    const std::vector<double>& actuation_power_w) {

    if (harvested_power_w.empty() || harvested_power_w.size() != actuation_power_w.size() ||
        initial_energy_j < 0.0 || reserve_energy_j < 0.0 ||
        harvest_efficiency < 0.0 || interval_seconds <= 0.0) {
        throw std::invalid_argument("invalid energy-neutral planning inputs");
    }

    double energy = initial_energy_j;
    double minimum_energy = energy;
    double harvested = 0.0;
    double required = 0.0;

    for (std::size_t i = 0; i < harvested_power_w.size(); ++i) {
        if (harvested_power_w[i] < 0.0 || actuation_power_w[i] < 0.0) {
            throw std::invalid_argument("power inputs must be nonnegative");
        }
        const double gained = harvest_efficiency * harvested_power_w[i] * interval_seconds;
        const double spent = actuation_power_w[i] * interval_seconds;
        harvested += gained;
        required += spent;
        energy += gained - spent;
        minimum_energy = std::min(minimum_energy, energy);
    }

    const bool neutral = minimum_energy >= reserve_energy && harvested >= required;
    const double reserve_margin = std::clamp(
        (minimum_energy - reserve_energy) / std::max(1.0, initial_energy_j), 0.0, 1.0);
    return {neutral ? EnergyNeutralState::EnergyNeutralPlan :
                      EnergyNeutralState::HoldForRecharge,
            energy, minimum_energy, harvested, required,
            neutral ? std::clamp(0.70 + 0.30 * reserve_margin, 0.0, 1.0) : 0.0,
            neutral ? std::clamp(0.60 + 0.40 * reserve_margin, 0.0, 1.0) : 0.0};
}

}  // namespace eco_restoration
