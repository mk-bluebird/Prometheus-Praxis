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

struct Params {
    int limit;
    std::string lane;
    std::string primary_plane;
};

struct Request {
    std::string tool;
    Params params;
};

static std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() &&
           (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start &&
           (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\n' || s[end - 1] == '\r')) {
        --end;
    }
    return s.substr(start, end - start);
}

static Request parse_request(const std::string& line) {
    Request req;
    req.tool = "";
    req.params.limit = 50;
    req.params.lane = "ANY";
    req.params.primary_plane = "ANY";

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

        if (!key.empty() && key.front() == '"') key = key.substr(1);
        if (!key.empty() && key.back() == '"') key.pop_back();

        if (key == "tool") {
            if (!val.empty() && val.front() == '"') val = val.substr(1);
            if (!val.empty() && val.back() == '"') val.pop_back();
            req.tool = val;
        } else if (key == "params") {
            if (!val.empty() && val.front() == '{') {
                std::string p = val;
                if (p.back() == '}') p = p.substr(1, p.size() - 2);
                std::istringstream pss(p);
                std::string ppart;
                while (std::getline(pss, ppart, ',')) {
                    std::string pkv = trim(ppart);
                    std::size_t pcolon = pkv.find(':');
                    if (pcolon == std::string::npos) continue;
                    std::string pkey = trim(pkv.substr(0, pcolon));
                    std::string pval = trim(pkv.substr(pcolon + 1));
                    if (!pkey.empty() && pkey.front() == '"') pkey = pkey.substr(1);
                    if (!pkey.empty() && pkey.back() == '"') pkey.pop_back();

                    if (pkey == "limit") {
                        try {
                            req.params.limit = std::stoi(pval);
                        } catch (...) {
                            req.params.limit = 50;
                        }
                    } else if (pkey == "lane") {
                        if (!pval.empty() && pval.front() == '"') pval = pval.substr(1);
                        if (!pval.empty() && pval.back() == '"') pval.pop_back();
                        req.params.lane = pval;
                    } else if (pkey == "primary_plane") {
                        if (!pval.empty() && pval.front() == '"') pval = pval.substr(1);
                        if (!pval.empty() && pval.back() == '"') pval.pop_back();
                        req.params.primary_plane = pval;
                    }
                }
            }
        }
    }

    if (req.params.limit <= 0) {
        req.params.limit = 50;
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
    std::ostringstream oss;
    int limit = req.params.limit > 0 ? req.params.limit : 50;

    if (req.tool == "hex_stability_carbon") {
        oss << "SELECT "
               "hex_id, region_name, topology_band, primary_plane, "
               "workload_count, total_delta_v_t, avg_delta_v_t, max_delta_v_t, "
               "avg_ker_k, avg_ker_e, avg_ker_r, avg_ker_s, "
               "avg_carbon_intensity_gco2_kwh, "
               "count_green_band, count_neutral_band, count_red_band, "
               "violations_dvt_global, violations_ker_nonpositive, "
               "violations_joint_ker_dvt, "
               "violations_red_band_ker, violations_prod_red_band "
               "FROM v_hex_stability_ker_dvt_carbon ";
        if (req.params.primary_plane != "ANY") {
            oss << "WHERE primary_plane = '" << req.params.primary_plane << "' ";
        }
        oss << "ORDER BY total_delta_v_t DESC, avg_carbon_intensity_gco2_kwh DESC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "ker_overview") {
        oss << "SELECT "
               "kind, repo_or_roleband, name_or_relpath, lane_default, "
               "primary_plane, role, ker_k, ker_e, ker_r, ker_s, "
               "neuro_flag, non_actuating, citizen_ready "
               "FROM v_ker_overview_modules_and_tools ";
        bool where_added = false;
        if (req.params.lane != "ANY") {
            oss << "WHERE lane_default = '" << req.params.lane << "' ";
            where_added = true;
        }
        if (req.params.primary_plane != "ANY") {
            oss << (where_added ? "AND " : "WHERE ")
                << "primary_plane = '" << req.params.primary_plane << "' ";
        }
        oss << "ORDER BY primary_plane, lane_default, ker_s DESC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "ker_high_risk_tools") {
        oss << "SELECT "
               "toolid, toolname, lanedefault, primaryplane, ker_k, ker_e, ker_r, ker_s, "
               "neuroflag, nonactuatingonly, citizen_ready "
               "FROM v_ker_high_risk_tools "
               "ORDER BY ker_r DESC, ker_e DESC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "ker_high_risk_modules") {
        oss << "SELECT "
               "module_id, repo_name, relpath, lane_default, primary_plane, module_role, "
               "ker_k, ker_e, ker_r, ker_s, neuro_flag, citizen_ready "
               "FROM v_ker_high_risk_modules "
               "ORDER BY ker_r DESC, ker_s ASC "
               "LIMIT " << limit << ";";
    } else if (req.tool == "synapse_safe_for_eco") {
        oss << "SELECT "
               "synapse_id, producer_lang, producer_relpath, consumer_lang, consumer_relpath, "
               "synapse_class, transport_kind, lane_default, primary_plane, "
               "non_actuating, allows_readonly, allows_actuation, neuro_flag, "
               "ker_k, ker_e, ker_r, ker_s "
               "FROM v_synapse_safe_for_eco ";
        if (req.params.primary_plane != "ANY") {
            oss << "WHERE primary_plane = '" << req.params.primary_plane << "' ";
        }
        oss << "ORDER BY primary_plane, producer_relpath, consumer_relpath "
               "LIMIT " << limit << ";";
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

static std::string make_hex_stability_carbon_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        oss << "{"
            << "\"hex_id\": \"" << json_escape(r.cols[0]) << "\", "
            << "\"region_name\": \"" << json_escape(r.cols[1]) << "\", "
            << "\"topology_band\": \"" << json_escape(r.cols[2]) << "\", "
            << "\"primary_plane\": \"" << json_escape(r.cols[3]) << "\", "
            << "\"workload_count\": " << r.cols[4] << ", "
            << "\"total_delta_v_t\": " << r.cols[5] << ", "
            << "\"avg_delta_v_t\": " << r.cols[6] << ", "
            << "\"max_delta_v_t\": " << r.cols[7] << ", "
            << "\"avg_ker_k\": " << r.cols[8] << ", "
            << "\"avg_ker_e\": " << r.cols[9] << ", "
            << "\"avg_ker_r\": " << r.cols[10] << ", "
            << "\"avg_ker_s\": " << r.cols[11] << ", "
            << "\"avg_carbon_intensity_gco2_kwh\": " << r.cols[12] << ", "
            << "\"count_green_band\": " << r.cols[13] << ", "
            << "\"count_neutral_band\": " << r.cols[14] << ", "
            << "\"count_red_band\": " << r.cols[15] << ", "
            << "\"violations_dvt_global\": " << r.cols[16] << ", "
            << "\"violations_ker_nonpositive\": " << r.cols[17] << ", "
            << "\"violations_joint_ker_dvt\": " << r.cols[18] << ", "
            << "\"violations_red_band_ker\": " << r.cols[19] << ", "
            << "\"violations_prod_red_band\": " << r.cols[20]
            << "}";
        if (i + 1 < rows.size()) oss << ", ";
    }
    oss << "] }";
    return oss.str();
}

static std::string make_ker_overview_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        oss << "{"
            << "\"kind\": \"" << json_escape(r.cols[0]) << "\", "
            << "\"band\": \"" << json_escape(r.cols[1]) << "\", "
            << "\"name\": \"" << json_escape(r.cols[2]) << "\", "
            << "\"lane_default\": \"" << json_escape(r.cols[3]) << "\", "
            << "\"primary_plane\": \"" << json_escape(r.cols[4]) << "\", "
            << "\"role\": \"" << json_escape(r.cols[5]) << "\", "
            << "\"ker_k\": " << r.cols[6] << ", "
            << "\"ker_e\": " << r.cols[7] << ", "
            << "\"ker_r\": " << r.cols[8] << ", "
            << "\"ker_s\": " << r.cols[9] << ", "
            << "\"neuro_flag\": " << r.cols[10] << ", "
            << "\"non_actuating\": " << r.cols[11] << ", "
            << "\"citizen_ready\": " << r.cols[12]
            << "}";
        if (i + 1 < rows.size()) oss << ", ";
    }
    oss << "] }";
    return oss.str();
}

static std::string make_ker_high_risk_tools_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        oss << "{"
            << "\"toolid\": " << r.cols[0] << ", "
            << "\"toolname\": \"" << json_escape(r.cols[1]) << "\", "
            << "\"lanedefault\": \"" << json_escape(r.cols[2]) << "\", "
            << "\"primary_plane\": \"" << json_escape(r.cols[3]) << "\", "
            << "\"ker_k\": " << r.cols[4] << ", "
            << "\"ker_e\": " << r.cols[5] << ", "
            << "\"ker_r\": " << r.cols[6] << ", "
            << "\"ker_s\": " << r.cols[7] << ", "
            << "\"neuroflag\": " << r.cols[8] << ", "
            << "\"nonactuatingonly\": " << r.cols[9] << ", "
            << "\"citizen_ready\": " << r.cols[10]
            << "}";
        if (i + 1 < rows.size()) oss << ", ";
    }
    oss << "] }";
    return oss.str();
}

static std::string make_ker_high_risk_modules_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        oss << "{"
            << "\"module_id\": " << r.cols[0] << ", "
            << "\"repo_name\": \"" << json_escape(r.cols[1]) << "\", "
            << "\"relpath\": \"" << json_escape(r.cols[2]) << "\", "
            << "\"lane_default\": \"" << json_escape(r.cols[3]) << "\", "
            << "\"primary_plane\": \"" << json_escape(r.cols[4]) << "\", "
            << "\"module_role\": \"" << json_escape(r.cols[5]) << "\", "
            << "\"ker_k\": " << r.cols[6] << ", "
            << "\"ker_e\": " << r.cols[7] << ", "
            << "\"ker_r\": " << r.cols[8] << ", "
            << "\"ker_s\": " << r.cols[9] << ", "
            << "\"neuro_flag\": " << r.cols[10] << ", "
            << "\"citizen_ready\": " << r.cols[11]
            << "}";
        if (i + 1 < rows.size()) oss << ", ";
    }
    oss << "] }";
    return oss.str();
}

static std::string make_synapse_safe_for_eco_response(const std::vector<Row>& rows) {
    std::ostringstream oss;
    oss << "{ \"ok\": true, \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        oss << "{"
            << "\"synapse_id\": " << r.cols[0] << ", "
            << "\"producer_lang\": \"" << json_escape(r.cols[1]) << "\", "
            << "\"producer_relpath\": \"" << json_escape(r.cols[2]) << "\", "
            << "\"consumer_lang\": \"" << json_escape(r.cols[3]) << "\", "
            << "\"consumer_relpath\": \"" << json_escape(r.cols[4]) << "\", "
            << "\"synapse_class\": \"" << json_escape(r.cols[5]) << "\", "
            << "\"transport_kind\": \"" << json_escape(r.cols[6]) << "\", "
            << "\"lane_default\": \"" << json_escape(r.cols[7]) << "\", "
            << "\"primary_plane\": \"" << json_escape(r.cols[8]) << "\", "
            << "\"non_actuating\": " << r.cols[9] << ", "
            << "\"allows_readonly\": " << r.cols[10] << ", "
            << "\"allows_actuation\": " << r.cols[11] << ", "
            << "\"neuro_flag\": " << r.cols[12] << ", "
            << "\"ker_k\": " << r.cols[13] << ", "
            << "\"ker_e\": " << r.cols[14] << ", "
            << "\"ker_r\": " << r.cols[15] << ", "
            << "\"ker_s\": " << r.cols[16]
            << "}";
        if (i + 1 < rows.size()) oss << ", ";
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

                if (req.tool == "hex_stability_carbon") {
                    std::cout << make_hex_stability_carbon_response(rows) << std::endl;
                } else if (req.tool == "ker_overview") {
                    std::cout << make_ker_overview_response(rows) << std::endl;
                } else if (req.tool == "ker_high_risk_tools") {
                    std::cout << make_ker_high_risk_tools_response(rows) << std::endl;
                } else if (req.tool == "ker_high_risk_modules") {
                    std::cout << make_ker_high_risk_modules_response(rows) << std::endl;
                } else if (req.tool == "synapse_safe_for_eco") {
                    std::cout << make_synapse_safe_for_eco_response(rows) << std::endl;
                } else {
                    std::cout << make_error_response("No named response implemented for tool: " + req.tool) << std::endl;
                }
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
