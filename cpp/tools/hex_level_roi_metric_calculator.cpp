// File: cpp/tools/hex_level_roi_metric_calculator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <numeric>

// Periodic job: summarise hex_eco_roi_history and rank hexes by ROI metric,
// producing SQL or JSON suitable for AI-chat queries.

namespace eco {

struct HexEcoROIHistoryRow {
    std::string hex_id;
    double did_effect;
    double synthetic_effect;
    double roi_metric;
};

struct HexRoiSummary {
    std::string hex_id;
    double mean_roi;
    double mean_did;
    double mean_synth;
};

std::vector<HexRoiSummary> summarise_roi(const std::vector<HexEcoROIHistoryRow>& rows) {
    std::unordered_map<std::string, std::vector<HexEcoROIHistoryRow>> by_hex;
    for (const auto& r : rows) {
        by_hex[r.hex_id].push_back(r);
    }

    std::vector<HexRoiSummary> summaries;
    summaries.reserve(by_hex.size());
    for (const auto& kv : by_hex) {
        const auto& hex_id = kv.first;
        const auto& vec = kv.second;

        double sum_roi = 0.0, sum_did = 0.0, sum_synth = 0.0;
        for (const auto& r : vec) {
            sum_roi += r.roi_metric;
            sum_did += r.did_effect;
            sum_synth += r.synthetic_effect;
        }
        double n = static_cast<double>(vec.size());
        HexRoiSummary s{};
        s.hex_id = hex_id;
        s.mean_roi = sum_roi / n;
        s.mean_did = sum_did / n;
        s.mean_synth = sum_synth / n;
        summaries.push_back(s);
    }
    return summaries;
}

void sort_by_roi(std::vector<HexRoiSummary>& summaries) {
    std::sort(summaries.begin(), summaries.end(),
              [](const HexRoiSummary& a, const HexRoiSummary& b) {
                  return a.mean_roi > b.mean_roi;
              });
}

// Emit SQL to update a summary table for AI-chat consumption.
void emit_summary_sql(const std::vector<HexRoiSummary>& summaries) {
    std::cout << "DELETE FROM hex_eco_roi_summary;\n";
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const auto& s = summaries[i];
        std::cout << "INSERT INTO hex_eco_roi_summary "
                  << "(hex_id, rank, mean_roi, mean_did_effect, mean_synthetic_effect) VALUES ('"
                  << s.hex_id << "', "
                  << i << ", "
                  << s.mean_roi << ", "
                  << s.mean_did << ", "
                  << s.mean_synth << ");\n";
    }
}

// Emit JSON ranking for AI-chat queries.
void emit_summary_json(const std::vector<HexRoiSummary>& summaries) {
    std::cout << "{ \"hex_roi_ranking\": [\n";
    for (std::size_t i = 0; i < summaries.size(); ++i) {
        const auto& s = summaries[i];
        std::cout << "  { \"hex_id\": \"" << s.hex_id << "\", "
                  << "\"rank\": " << i << ", "
                  << "\"mean_roi\": " << s.mean_roi << ", "
                  << "\"mean_did_effect\": " << s.mean_did << ", "
                  << "\"mean_synthetic_effect\": " << s.mean_synth << " }";
        if (i + 1 < summaries.size()) std::cout << ",";
        std::cout << "\n";
    }
    std::cout << "] }\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example history rows (would be read from hex_eco_roi_history).
    std::vector<HexEcoROIHistoryRow> rows = {
        {"hex_PHX_001", 0.05, 0.04, 0.05},
        {"hex_PHX_001", 0.06, 0.05, 0.06},
        {"hex_PHX_002", 0.03, 0.02, 0.03},
        {"hex_PHX_003", 0.08, 0.07, 0.08},
        {"hex_PHX_003", 0.07, 0.06, 0.07}
    };

    auto summaries = summarise_roi(rows);
    sort_by_roi(summaries);

    emit_summary_sql(summaries);
    std::cout << "\n";
    emit_summary_json(summaries);

    return 0;
}
