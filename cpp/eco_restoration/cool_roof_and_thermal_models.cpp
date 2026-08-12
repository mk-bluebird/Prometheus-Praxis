// File: cpp/eco_restoration/cool_roof_and_thermal_models.cpp

#include <Eigen/Dense>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace eco_restoration {

struct CoolRoofHex {
    std::uint64_t anchor{};
    double heat_risk{};
    double expected_risk_reduction{};
    double crew_hours{};
    double material_cost{};
    std::vector<std::uint64_t> adjacent_anchors;
};

struct CoolRoofPlan {
    std::vector<std::uint64_t> selected_anchors;
    double total_risk_reduction{};
    double total_crew_hours{};
    double total_material_cost{};
};

bool fits(
    const CoolRoofPlan& plan,
    const CoolRoofHex& candidate,
    double crew_budget,
    double material_budget) {

    return plan.total_crew_hours + candidate.crew_hours <= crew_budget &&
           plan.total_material_cost + candidate.material_cost <= material_budget;
}

CoolRoofPlan expand_cluster(
    const CoolRoofHex& seed,
    const std::unordered_map<std::uint64_t, CoolRoofHex>& hexes,
    double crew_budget,
    double material_budget) {

    CoolRoofPlan plan;
    if (!fits(plan, seed, crew_budget, material_budget)) return plan;

    std::unordered_set<std::uint64_t> selected{seed.anchor};
    plan.selected_anchors.push_back(seed.anchor);
    plan.total_risk_reduction = std::min(seed.heat_risk, seed.expected_risk_reduction);
    plan.total_crew_hours = seed.crew_hours;
    plan.total_material_cost = seed.material_cost;

    using QueueItem = std::pair<double, std::uint64_t>;
    std::priority_queue<QueueItem> frontier;

    const auto enqueue_neighbors = [&](const CoolRoofHex& current) {
        for (std::uint64_t neighbor : current.adjacent_anchors) {
            const auto found = hexes.find(neighbor);
            if (found == hexes.end() || selected.contains(neighbor)) continue;
            const CoolRoofHex& hex = found->second;
            const double burden = hex.crew_hours + hex.material_cost;
            const double score = std::min(hex.heat_risk, hex.expected_risk_reduction) /
                                 std::max(1e-9, burden);
            frontier.push({score, neighbor});
        }
    };

    enqueue_neighbors(seed);
    while (!frontier.empty()) {
        const std::uint64_t anchor = frontier.top().second;
        frontier.pop();
        if (selected.contains(anchor)) continue;

        const CoolRoofHex& candidate = hexes.at(anchor);
        if (!fits(plan, candidate, crew_budget, material_budget)) continue;

        selected.insert(anchor);
        plan.selected_anchors.push_back(anchor);
        plan.total_risk_reduction += std::min(candidate.heat_risk, candidate.expected_risk_reduction);
        plan.total_crew_hours += candidate.crew_hours;
        plan.total_material_cost += candidate.material_cost;
        enqueue_neighbors(candidate);
    }
    return plan;
}

CoolRoofPlan select_cool_roof_clusters(
    const std::vector<CoolRoofHex>& candidates,
    double crew_budget,
    double material_budget,
    std::size_t maximum_clusters) {

    if (crew_budget < 0.0 || material_budget < 0.0 || maximum_clusters == 0U) {
        throw std::invalid_argument("invalid cool-roof resource budget");
    }

    std::unordered_map<std::uint64_t, CoolRoofHex> hexes;
    for (const CoolRoofHex& hex : candidates) {
        if (hex.heat_risk < 0.0 || hex.heat_risk > 1.0 ||
            hex.expected_risk_reduction < 0.0 || hex.crew_hours < 0.0 || hex.material_cost < 0.0) {
            throw std::invalid_argument("invalid cool-roof hex");
        }
        hexes.emplace(hex.anchor, hex);
    }

    CoolRoofPlan result;
    std::unordered_set<std::uint64_t> unavailable;

    for (std::size_t cluster = 0; cluster < maximum_clusters; ++cluster) {
        CoolRoofPlan best;
        for (const auto& [anchor, hex] : hexes) {
            if (unavailable.contains(anchor)) continue;
            CoolRoofPlan candidate = expand_cluster(
                hex, hexes,
                crew_budget - result.total_crew_hours,
                material_budget - result.total_material_cost);
            candidate.selected_anchors.erase(
                std::remove_if(candidate.selected_anchors.begin(), candidate.selected_anchors.end(),
                    [&](std::uint64_t selected) { return unavailable.contains(selected); }),
                candidate.selected_anchors.end());

            if (candidate.total_risk_reduction > best.total_risk_reduction) best = std::move(candidate);
        }

        if (best.selected_anchors.empty()) break;
        for (std::uint64_t anchor : best.selected_anchors) unavailable.insert(anchor);
        result.selected_anchors.insert(
            result.selected_anchors.end(), best.selected_anchors.begin(), best.selected_anchors.end());
        result.total_risk_reduction += best.total_risk_reduction;
        result.total_crew_hours += best.total_crew_hours;
        result.total_material_cost += best.total_material_cost;
    }
    return result;
}

struct ThermalObservation {
    std::uint64_t anchor{};
    std::int64_t observed_unix_s{};
    double power_w{};
    double temperature_delta_c{};
};

struct ThermalModel {
    std::uint64_t anchor{};
    double intercept_c{};
    double prior_temperature_coefficient{};
    double watts_to_temperature_coefficient{};
    double r_squared{};
    double time_constant_s{};
};

ThermalModel fit_thermal_model(
    std::uint64_t anchor,
    std::vector<ThermalObservation> observations,
    double sampling_period_s) {

    if (sampling_period_s <= 0.0 || observations.size() < 4U) {
        throw std::invalid_argument("insufficient thermal observations");
    }

    std::sort(observations.begin(), observations.end(), [](const auto& a, const auto& b) {
        return a.observed_unix_s < b.observed_unix_s;
    });

    const Eigen::Index rows = static_cast<Eigen::Index>(observations.size() - 1U);
    Eigen::MatrixXd design(rows, 3);
    Eigen::VectorXd target(rows);

    for (Eigen::Index i = 0; i < rows; ++i) {
        const ThermalObservation& previous = observations[static_cast<std::size_t>(i)];
        const ThermalObservation& current = observations[static_cast<std::size_t>(i + 1)];
        if (previous.anchor != anchor || current.anchor != anchor) {
            throw std::invalid_argument("mixed anchors in thermal model input");
        }
        design.row(i) << 1.0, previous.temperature_delta_c, previous.power_w;
        target[i] = current.temperature_delta_c;
    }

    const Eigen::Vector3d coefficients = design.colPivHouseholderQr().solve(target);
    const Eigen::VectorXd prediction = design * coefficients;
    const double residual_sum = (target - prediction).squaredNorm();
    const double total_sum = (target.array() - target.mean()).square().sum();
    const double r_squared = total_sum <= 1e-12 ? 0.0 : std::clamp(1.0 - residual_sum / total_sum, 0.0, 1.0);
    const double autoregression = std::clamp(coefficients[1], 1e-9, 0.999999);
    const double time_constant = -sampling_period_s / std::log(autoregression);

    return {anchor, coefficients[0], autoregression, coefficients[2], r_squared, time_constant};
}

void persist_thermal_model(sqlite3* database, const ThermalModel& model) {
    sqlite3_exec(database,
        "CREATE TABLE IF NOT EXISTS hex_thermal_model("
        "hex_anchor INTEGER PRIMARY KEY,intercept_c REAL NOT NULL,"
        "prior_temperature_coefficient REAL NOT NULL,"
        "watts_to_temperature_coefficient REAL NOT NULL,"
        "r_squared REAL NOT NULL CHECK(r_squared BETWEEN 0 AND 1),"
        "time_constant_s REAL NOT NULL CHECK(time_constant_s>=0)) STRICT;",
        nullptr, nullptr, nullptr);

    sqlite3_stmt* statement = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_thermal_model VALUES(?,?,?,?,?,?) "
        "ON CONFLICT(hex_anchor) DO UPDATE SET intercept_c=excluded.intercept_c,"
        "prior_temperature_coefficient=excluded.prior_temperature_coefficient,"
        "watts_to_temperature_coefficient=excluded.watts_to_temperature_coefficient,"
        "r_squared=excluded.r_squared,time_constant_s=excluded.time_constant_s;",
        -1, &statement, nullptr);

    sqlite3_bind_int64(statement, 1, static_cast<sqlite3_int64>(model.anchor));
    sqlite3_bind_double(statement, 2, model.intercept_c);
    sqlite3_bind_double(statement, 3, model.prior_temperature_coefficient);
    sqlite3_bind_double(statement, 4, model.watts_to_temperature_coefficient);
    sqlite3_bind_double(statement, 5, model.r_squared);
    sqlite3_bind_double(statement, 6, model.time_constant_s);

    if (sqlite3_step(statement) != SQLITE_DONE) {
        sqlite3_finalize(statement);
        throw std::runtime_error("cannot persist thermal model");
    }
    sqlite3_finalize(statement);
}

}  // namespace eco_restoration
