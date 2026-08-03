// File: cpp/tools/ker_synapse_daily_report.cpp
// Destination: mk-bluebird/Prometheus-Praxis/cpp/tools/ker_synapse_daily_report.cpp

#include <iostream>
#include <iomanip>
#include <string>
#include <stdexcept>
#include <vector>
#include <sqlite3.h>

/*
 * KER + Synapse Daily Report CLI
 *
 * Connects to the Prometheus-Praxis SQLite spine and prints:
 *  - High-risk / high-impact modules and tools (KER-based),
 *  - Safe synapse bridges for eco use,
 *  - Neuro-adjacent, non-actuating artifacts.
 *
 * Assumes the presence of:
 *  - module_ker_profile
 *  - synapse_endpoint
 *  - mcp_tool
 *  - Views: v_ker_high_risk_tools, v_ker_high_risk_modules,
 *           v_synapse_safe_for_eco, v_neuro_nonactuating_modules,
 *           v_neuro_nonactuating_tools, v_ker_overview_modules_and_tools.
 *
 * Usage:
 *  ker_synapse_daily_report <sqlite_db_path>
 */

struct ColumnPrinter {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;

    void add_row(const std::vector<std::string>& row) {
        rows.push_back(row);
    }

    void print(std::ostream& os) const {
        if (headers.empty()) {
            return;
        }
        std::vector<std::size_t> widths(headers.size(), 0);
        for (std::size_t i = 0; i < headers.size(); ++i) {
            widths[i] = headers[i].size();
        }
        for (const auto& row : rows) {
            for (std::size_t i = 0; i < row.size() && i < widths.size(); ++i) {
                if (row[i].size() > widths[i]) {
                    widths[i] = row[i].size();
                }
            }
        }

        // Print header
        for (std::size_t i = 0; i < headers.size(); ++i) {
            os << std::setw(static_cast<int>(widths[i])) << headers[i];
            if (i + 1 < headers.size()) {
                os << "  ";
            }
        }
        os << "\n";
        // Print separator
        for (std::size_t i = 0; i < headers.size(); ++i) {
            os << std::string(widths[i], '-');
            if (i + 1 < headers.size()) {
                os << "  ";
            }
        }
        os << "\n";
        // Print rows
        for (const auto& row : rows) {
            for (std::size_t i = 0; i < headers.size(); ++i) {
                std::string val = (i < row.size()) ? row[i] : "";
                os << std::setw(static_cast<int>(widths[i])) << val;
                if (i + 1 < headers.size()) {
                    os << "  ";
                }
            }
            os << "\n";
        }
        os << "\n";
    }
};

class SqliteReporter {
public:
    explicit SqliteReporter(const std::string& db_path)
        : db_(nullptr) {
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    ~SqliteReporter() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    void print_high_risk_tools() {
        std::cout << "=== KER High-Risk / High-Impact Tools ===\n";
        ColumnPrinter printer;
        printer.headers = {"toolid", "toolname", "lane", "plane", "ker_k", "ker_e", "ker_r", "ker_s", "neuro", "citizen_ready"};

        const char* sql =
            "SELECT toolid, toolname, lanedefault, primaryplane, "
            "ker_k, ker_e, ker_r, ker_s, neuroflag, citizen_ready "
            "FROM v_ker_high_risk_tools "
            "ORDER BY ker_r DESC, ker_e DESC "
            "LIMIT 50;";

        execute_and_collect(sql, printer);
        printer.print(std::cout);
    }

    void print_high_risk_modules() {
        std::cout << "=== KER High-Risk Modules ===\n";
        ColumnPrinter printer;
        printer.headers = {"module_id", "repo", "relpath", "lane", "plane", "role", "ker_k", "ker_e", "ker_r", "ker_s", "neuro", "citizen_ready"};

        const char* sql =
            "SELECT module_id, repo_name, relpath, lane_default, primary_plane, module_role, "
            "ker_k, ker_e, ker_r, ker_s, neuro_flag, citizen_ready "
            "FROM v_ker_high_risk_modules "
            "ORDER BY ker_r DESC, ker_s ASC "
            "LIMIT 50;";

        execute_and_collect(sql, printer);
        printer.print(std::cout);
    }

    void print_synapse_safe_for_eco() {
        std::cout << "=== Synapse Bridges Safe for Eco ===\n";
        ColumnPrinter printer;
        printer.headers = {"synapse_id", "producer", "consumer", "class", "transport", "lane", "plane", "ker_k", "ker_e", "ker_r", "ker_s"};

        const char* sql =
            "SELECT synapse_id, producer_relpath, consumer_relpath, synapse_class, transport_kind, "
            "lane_default, primary_plane, ker_k, ker_e, ker_r, ker_s "
            "FROM v_synapse_safe_for_eco "
            "ORDER BY primary_plane, producer_relpath, consumer_relpath "
            "LIMIT 50;";

        execute_and_collect(sql, printer);
        printer.print(std::cout);
    }

    void print_neuro_nonactuating() {
        std::cout << "=== Neuro-Adjacent, Non-Actuating Modules ===\n";
        {
            ColumnPrinter printer;
            printer.headers = {"module_id", "repo", "relpath", "lane", "plane", "role", "ker_s"};

            const char* sql =
                "SELECT module_id, repo_name, relpath, lane_default, primary_plane, module_role, ker_s "
                "FROM v_neuro_nonactuating_modules "
                "ORDER BY lane_default, relpath "
                "LIMIT 50;";

            execute_and_collect(sql, printer);
            printer.print(std::cout);
        }

        std::cout << "=== Neuro-Adjacent, Non-Actuating Tools ===\n";
        {
            ColumnPrinter printer;
            printer.headers = {"toolid", "toolname", "lane", "plane", "synapse_class", "ker_s"};

            const char* sql =
                "SELECT toolid, toolname, lanedefault, primaryplane, synapse_class, ker_s "
                "FROM v_neuro_nonactuating_tools "
                "ORDER BY lanedefault, toolname "
                "LIMIT 50;";

            execute_and_collect(sql, printer);
            printer.print(std::cout);
        }
    }

    void print_ker_overview_summary() {
        std::cout << "=== KER Overview (Modules + Tools) ===\n";
        ColumnPrinter printer;
        printer.headers = {"kind", "band", "name", "lane", "plane", "role", "ker_k", "ker_e", "ker_r", "ker_s", "neuro", "non_actuating", "citizen_ready"};

        const char* sql =
            "SELECT kind, repo_or_roleband, name_or_relpath, lane_default, primary_plane, role, "
            "ker_k, ker_e, ker_r, ker_s, neuro_flag, non_actuating, citizen_ready "
            "FROM v_ker_overview_modules_and_tools "
            "ORDER BY primary_plane, lane_default, ker_s DESC "
            "LIMIT 100;";

        execute_and_collect(sql, printer);
        printer.print(std::cout);
    }

private:
    sqlite3* db_;

    void execute_and_collect(const char* sql, ColumnPrinter& printer) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        }

        int col_count = sqlite3_column_count(stmt);
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            std::vector<std::string> row;
            row.reserve(static_cast<std::size_t>(col_count));
            for (int i = 0; i < col_count; ++i) {
                const unsigned char* text = sqlite3_column_text(stmt, i);
                if (text) {
                    row.emplace_back(reinterpret_cast<const char*>(text));
                } else {
                    row.emplace_back("");
                }
            }
            printer.add_row(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Error executing query: " + std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_finalize(stmt);
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: ker_synapse_daily_report <sqlite_db_path>\n";
        return 1;
    }

    const std::string db_path = argv[1];

    try {
        SqliteReporter reporter(db_path);
        std::cout << std::fixed << std::setprecision(3);

        reporter.print_ker_overview_summary();
        reporter.print_high_risk_tools();
        reporter.print_high_risk_modules();
        reporter.print_synapse_safe_for_eco();
        reporter.print_neuro_nonactuating();

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}
