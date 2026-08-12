// File: cpp/tools/pareto_front_selection.cpp
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Candidate {
    std::string id;
    double false_negative_halts{};
    double budget_excess{};
    double true_positive_derates{};
    double ecological_benefit{};
    std::string thresholds_json;
    double knee_distance{};
};

std::vector<Candidate> load_front(sqlite3* database) {
    constexpr const char* query =
        "SELECT candidate_id,false_negative_halts,budget_excess,"
        "true_positive_derates,ecological_benefit,thresholds_json "
        "FROM lane_threshold_pareto WHERE pareto_rank=0;";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(database, query, -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);

    std::vector<Candidate> candidates;
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        const auto text = [&](int index) {
            const auto* value = sqlite3_column_text(statement.get(), index);
            return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
        };
        candidates.push_back({text(0), sqlite3_column_double(statement.get(), 1),
                              sqlite3_column_double(statement.get(), 2),
                              sqlite3_column_double(statement.get(), 3),
                              sqlite3_column_double(statement.get(), 4), text(5)});
    }
    if (candidates.empty()) throw std::runtime_error("no rank-zero Pareto candidates found");
    return candidates;
}

void score_knees(std::vector<Candidate>& candidates) {
    double min_fn = std::numeric_limits<double>::infinity(), max_fn = -min_fn;
    double min_budget = min_fn, max_budget = -min_fn;
    double min_derate = min_fn, max_derate = -min_fn;
    double min_benefit = min_fn, max_benefit = -min_fn;
    for (const auto& c : candidates) {
        min_fn = std::min(min_fn, c.false_negative_halts); max_fn = std::max(max_fn, c.false_negative_halts);
        min_budget = std::min(min_budget, c.budget_excess); max_budget = std::max(max_budget, c.budget_excess);
        min_derate = std::min(min_derate, c.true_positive_derates); max_derate = std::max(max_derate, c.true_positive_derates);
        min_benefit = std::min(min_benefit, c.ecological_benefit); max_benefit = std::max(max_benefit, c.ecological_benefit);
    }
    const auto normal = [](double value, double low, double high) {
        return high == low ? 0.0 : (value - low) / (high - low);
    };
    for (auto& c : candidates) {
        const double a = normal(c.false_negative_halts, min_fn, max_fn);
        const double b = normal(c.budget_excess, min_budget, max_budget);
        const double d = 1.0 - normal(c.true_positive_derates, min_derate, max_derate);
        const double e = 1.0 - normal(c.ecological_benefit, min_benefit, max_benefit);
        c.knee_distance = std::sqrt(a * a + b * b + d * d + e * e);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: pareto_front_selection <pareto.sqlite>\n";
        return 2;
    }
    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(argv[1], &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);

    try {
        auto candidates = load_front(database.get());
        score_knees(candidates);
        std::sort(candidates.begin(), candidates.end(),
                  [](const Candidate& a, const Candidate& b) { return a.knee_distance < b.knee_distance; });

        std::cout << "Rank-zero candidates; lowest knee distance balances all four objectives.\n";
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& c = candidates[i];
            const int bar = static_cast<int>(std::clamp(40.0 * (1.0 - c.knee_distance / 2.0), 0.0, 40.0));
            std::cout << '[' << i << "] " << c.id << " knee=" << std::fixed << std::setprecision(3)
                      << c.knee_distance << " |" << std::string(bar, '#') << "\n"
                      << "  FN-halts=" << c.false_negative_halts << " budget=" << c.budget_excess
                      << " TP-derates=" << c.true_positive_derates
                      << " eco-benefit=" << c.ecological_benefit << '\n';
        }
        std::cout << "Select candidate index: ";
        std::size_t selected{};
        if (!(std::cin >> selected) || selected >= candidates.size()) return 2;
        const auto& c = candidates[selected];
        std::cout << "{\"candidate_id\":\"" << c.id << "\",\"thresholds\":"
                  << c.thresholds_json << ",\"knee_distance\":" << c.knee_distance << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
