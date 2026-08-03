// File: cpp/tools/eco_invariant_checker.cpp
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sqlite3.h>

namespace eco_checker {

struct TelemetrySample {
    std::string node_code;
    double deltaVt;
    double ker_score;
    double timestamp_index; // simple numeric index for ordering
};

/**
 * @brief Read telemetry from a CSV file with columns:
 *        node_code,deltaVt,ker_score,timestamp_index
 */
std::vector<TelemetrySample> read_csv(const std::string& path) {
    std::vector<TelemetrySample> out;
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open CSV file: " + path);
    }
    std::string line;
    // Skip header if present.
    if (std::getline(in, line)) {
        if (line.find("node_code") == std::string::npos) {
            // First line is data; process it.
            std::istringstream ss(line);
            TelemetrySample s{};
            std::string field;
            std::getline(ss, s.node_code, ',');
            std::getline(ss, field, ','); s.deltaVt = std::stod(field);
            std::getline(ss, field, ','); s.ker_score = std::stod(field);
            std::getline(ss, field, ','); s.timestamp_index = std::stod(field);
            out.push_back(s);
        }
    }
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        TelemetrySample s{};
        std::string field;
        std::getline(ss, s.node_code, ',');
        std::getline(ss, field, ','); s.deltaVt = std::stod(field);
        std::getline(ss, field, ','); s.ker_score = std::stod(field);
        std::getline(ss, field, ','); s.timestamp_index = std::stod(field);
        out.push_back(s);
    }
    return out;
}

/**
 * @brief Read telemetry from SQLite table cyboquatic_workload_telemetry.
 *
 * Columns used: canal_node (TEXT), deltaVt (REAL), timestamp_utc (TEXT).
 * We map timestamp_utc to a simple increasing index by sort order.
 */
std::vector<TelemetrySample> read_sqlite(const std::string& db_path) {
    std::vector<TelemetrySample> out;
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite DB: " + db_path);
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    const char* sql =
        "SELECT canal_node, deltaVt, timestamp_utc "
        "FROM cyboquatic_workload_telemetry "
        "ORDER BY canal_node, timestamp_utc ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare telemetry query");
    }

    double idx = 0.0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TelemetrySample s{};
        const unsigned char* node = sqlite3_column_text(stmt, 0);
        s.node_code = node ? reinterpret_cast<const char*>(node) : "";
        s.deltaVt   = sqlite3_column_double(stmt, 1);
        s.ker_score = 0.0; // Will be filled from KER invariants table if available.
        s.timestamp_index = idx;
        idx += 1.0;
        out.push_back(s);
    }

    sqlite3_finalize(stmt);

    // Optionally join with KER invariants if present.
    const char* ker_sql =
        "SELECT cn.node_code, ck.ker_score "
        "FROM canal_node cn "
        "JOIN canal_ker_canal_invariant ck ON ck.node_id = cn.node_id;";
    sqlite3_stmt* kstmt = nullptr;
    if (sqlite3_prepare_v2(db, ker_sql, -1, &kstmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(kstmt) == SQLITE_ROW) {
            const unsigned char* node = sqlite3_column_text(kstmt, 0);
            std::string node_code = node ? reinterpret_cast<const char*>(node) : "";
            double ker = sqlite3_column_double(kstmt, 1);
            for (auto& s : out) {
                if (s.node_code == node_code) {
                    s.ker_score = ker;
                }
            }
        }
        sqlite3_finalize(kstmt);
    }

    sqlite3_close(db);
    return out;
}

/**
 * @brief Assert invariants: if ker_score > 0 then deltaVt must be non-increasing.
 *
 * This reflects the KER-Lyapunov coupling lemma:
 * s_t > 0 => V_{t+1} - V_t <= -alpha s_t, encoded here as a discrete
 * non-increase requirement on deltaVt per node.[59]
 */
void check_invariants(const std::vector<TelemetrySample>& samples,
                      const std::string& source_label) {
    // Group by node_code.
    std::vector<std::string> nodes;
    for (const auto& s : samples) {
        if (std::find(nodes.begin(), nodes.end(), s.node_code) == nodes.end()) {
            nodes.push_back(s.node_code);
        }
    }

    bool any_failure = false;

    for (const auto& node : nodes) {
        std::vector<TelemetrySample> node_samples;
        for (const auto& s : samples) {
            if (s.node_code == node) {
                node_samples.push_back(s);
            }
        }
        std::sort(node_samples.begin(), node_samples.end(),
                  [](const TelemetrySample& a, const TelemetrySample& b) {
                      return a.timestamp_index < b.timestamp_index;
                  });

        for (std::size_t i = 1; i < node_samples.size(); ++i) {
            const auto& prev = node_samples[i - 1];
            const auto& cur  = node_samples[i];

            if (prev.ker_score > 0.0) {
                if (cur.deltaVt > prev.deltaVt + 1e-9) {
                    any_failure = true;
                    std::cerr << "[INVARIANT BREACH] DID "
                              << "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7 "
                              << " corridor rule violated for node_code=" << node << "\n"
                              << "  Source=" << source_label << "\n"
                              << "  Condition: ker_score>0 requires deltaVt_t+1 <= deltaVt_t\n"
                              << "  Observed: deltaVt_prev=" << prev.deltaVt
                              << " deltaVt_curr=" << cur.deltaVt << "\n";
                }
            }
        }
    }

    if (!any_failure) {
        std::cout << "[INVARIANT OK] All nodes satisfy KER-Lyapunov corridor rules "
                  << "for source=" << source_label << "\n";
    }
}

} // namespace eco_checker

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: eco_invariant_checker <mode> <path>\n"
                  << "  mode: csv | sqlite\n"
                  << "  path: CSV file or SQLite DB path\n";
        return 1;
    }

    std::string mode = argv[1];
    std::string path = argv[2];

    try {
        if (mode == "csv") {
            auto samples = eco_checker::read_csv(path);
            eco_checker::check_invariants(samples, "CSV:" + path);
        } else if (mode == "sqlite") {
            auto samples = eco_checker::read_sqlite(path);
            eco_checker::check_invariants(samples, "SQLite:" + path);
        } else {
            std::cerr << "Unknown mode: " << mode << "\n";
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "eco_invariant_checker error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
