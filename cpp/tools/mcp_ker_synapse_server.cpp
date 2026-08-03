// File: cpp/tools/mcp_ker_synapse_server.cpp
// Destination: mk-bluebird/Prometheus-Praxis/cpp/tools/mcp_ker_synapse_server.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <sqlite3.h>

struct Row {
    std::vector<std::string> cols;
};

class SqliteClient {
public:
    explicit SqliteClient(const std::string& db_path)
        : db_(nullptr) {
        int rc = sqlite3_open(db_path.c_str(), &db_);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    ~SqliteClient() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    std::vector<Row> query(const std::string& sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        }

        std::vector<Row> results;
        int col_count = sqlite3_column_count(stmt);

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            Row row;
            row.cols.reserve(static_cast<std::size_t>(col_count));
            for (int i = 0; i < col_count; ++i) {
                const unsigned char* text = sqlite3_column_text(stmt, i);
                if (text) {
                    row.cols.emplace_back(reinterpret_cast<const char*>(text));
                } else {
                    row.cols.emplace_back("");
                }
            }
            results.push_back(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Error executing query: " + std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_finalize(stmt);
        return results;
    }

private:
    sqlite3* db_;
};

struct Request {
    std::string tool;
    int limit;
};

static std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(start, end - start);
}

static Request parse_request(const std::string& line) {
    Request req;
    req.tool = "";
    req.limit = 50;

    std::string s = trim(line);
    if (s.empty() || s.front() != '{' || s.back() != '}') {
        return req;
    }
    s = s.substr(1, s.size() - 2);

    std::istringstream iss(s);
    std::string part;
    while (std::getline(iss, part, ',')) {
        std::string kv = trim(part);
        std::size_t colon_pos = kv.find(':');
        if (colon_pos == std::string::npos) {
            continue;
        }
        std::string key = trim(kv.substr(0, colon_pos));
        std::string val = trim(kv.substr(colon_pos + 1));

        if (!key.empty() && key.front() == '"') {
            key = key.substr(1);
        }
        if (!key.empty() && key.back() == '"') {
            key.pop_back();
        }

        if (key == "tool") {
            if (!val.empty() && val.front() == '"') {
                val = val.substr(1);
            }
            if (!val.empty() && val.back() == '"') {
                val.pop_back();
            }
            req.tool = val;
        } else if (key == "limit") {
            try {
                req.limit = std::stoi(val);
            } catch (...) {
                req.limit = 50;
            }
        }
    }
    return req;
}

static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

static std::string make_sql_for_tool(const Request& req) {
    int limit = req.limit > 0 ? req.limit : 50;
    std::ostringstream oss;

    if (req.tool == "ker_overview") {
        oss << "SELECT kind, repo_or_roleband, name_or_relpath, lane_default, primary_plane, role, "
               "ker_k, ker_e, ker_r, ker_s, neuro_flag, non_actuating, citizen_ready "
               "FROM v_ker_overview_modules_and_tools "
               "ORDER BY primary_plane, lane_default, ker_s DESC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "ker_high_risk_tools") {
        oss << "SELECT toolid, toolname, lanedefault, primaryplane, ker_k, ker_e, ker_r, ker_s, neuroflag, citizen_ready "
               "FROM v_ker_high_risk_tools "
               "ORDER BY ker_r DESC, ker_e DESC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "ker_high_risk_modules") {
        oss << "SELECT module_id, repo_name, relpath, lane_default, primary_plane, module_role, "
               "ker_k, ker_e, ker_r, ker_s, neuro_flag, citizen_ready "
               "FROM v_ker_high_risk_modules "
               "ORDER BY ker_r DESC, ker_s ASC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "synapse_safe_for_eco") {
        oss << "SELECT synapse_id, producer_relpath, consumer_relpath, synapse_class, transport_kind, "
               "lane_default, primary_plane, ker_k, ker_e, ker_r, ker_s "
               "FROM v_synapse_safe_for_eco "
               "ORDER BY primary_plane, producer_relpath, consumer_relpath "
               "LIMIT " << limit << ";";
    } else if (req.tool == "neuro_nonactuating_modules") {
        oss << "SELECT module_id, repo_name, relpath, lane_default, primary_plane, module_role, ker_s "
               "FROM v_neuro_nonactuating_modules "
               "ORDER BY lane_default, relpath "
               "LIMIT " << limit << ";";
    } else if (req.tool == "neuro_nonactuating_tools") {
        oss << "SELECT toolid, toolname, lanedefault, primaryplane, synapse_class, ker_s "
               "FROM v_neuro_nonactuating_tools "
               "ORDER BY lanedefault, toolname "
               "LIMIT " << limit << ";";
    } else if (req.tool == "hex_stability") {
        int hex_limit = req.limit > 0 ? req.limit : 100;
        oss << "SELECT "
               "hex_id, region_name, topology_band, primary_plane, "
               "workload_count, total_delta_v_t, avg_delta_v_t, max_delta_v_t, "
               "avg_ker_k, avg_ker_e, avg_ker_r, avg_ker_s, "
               "violations_dvt_global, violations_ker_nonpositive, violations_joint_ker_dvt "
               "FROM v_hex_stability_ker_dvt "
               "ORDER BY total_delta_v_t DESC "
               "LIMIT " << hex_limit << ";";
    } else if (req.tool == "hex_stability_carbon") {
        int hex_limit = req.limit > 0 ? req.limit : 100;
        oss << "SELECT "
               "hex_id, region_name, topology_band, primary_plane, "
               "workload_count, total_delta_v_t, avg_delta_v_t, max_delta_v_t, "
               "avg_ker_k, avg_ker_e, avg_ker_r, avg_ker_s, "
               "avg_carbon_intensity_gco2_kwh, "
               "count_green_band, count_neutral_band, count_red_band, "
               "violations_dvt_global, violations_ker_nonpositive, "
               "violations_joint_ker_dvt, "
               "violations_red_band_ker, violations_prod_red_band "
               "FROM v_hex_stability_ker_dvt_carbon "
               "ORDER BY total_delta_v_t DESC, avg_carbon_intensity_gco2_kwh DESC "
               "LIMIT " << hex_limit << ";";
    } else {
        return "";
    }

    return oss.str();
}

static std::string make_error_response(const std::string& message) {
    std::ostringstream oss;
    oss << "{ \"ok\": false, \"error\": \"" << json_escape(message) << "\" }";
    return oss.str();
}

static std::string make_rows_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        oss << "{";
        for (std::size_t c = 0; c < row.cols.size(); ++c) {
            oss << "\"col" << c << "\": \"" << json_escape(row.cols[c]) << "\"";
            if (c + 1 < row.cols.size()) {
                oss << ", ";
            }
        }
        oss << "}";
        if (i + 1 < rows.size()) {
            oss << ", ";
        }
    }
    oss << "] }";
    return oss.str();
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: mcp_ker_synapse_server <sqlite_db_path>\n";
        return 1;
    }

    const std::string db_path = argv[1];

    try {
        SqliteClient client(db_path);

        std::string line;
        while (std::getline(std::cin, line)) {
            line = trim(line);
            if (line.empty()) {
                continue;
            }

            Request req = parse_request(line);
            if (req.tool.empty()) {
                std::cout << make_error_response("Missing or invalid 'tool' field") << std::endl;
                continue;
            }

            std::string sql = make_sql_for_tool(req);
            if (sql.empty()) {
                std::cout << make_error_response("Unknown tool: " + req.tool) << std::endl;
                continue;
            }

            try {
                std::vector<Row> rows = client.query(sql);
                std::cout << make_rows_response(rows) << std::endl;
            } catch (const std::exception& ex) {
                std::cout << make_error_response(ex.what()) << std::endl;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
