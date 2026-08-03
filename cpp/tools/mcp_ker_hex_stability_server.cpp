// File: cpp/tools/mcp_ker_hex_stability_server.cpp
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cctype>
#include <sqlite3.h>

// Minimal JSON encoding helpers for MCP-style responses.
namespace json {

std::string escape(const std::string &s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '\\': oss << "\\\\"; break;
            case '"':  oss << "\\\""; break;
            case '\n': oss << "\\n";  break;
            case '\r': oss << "\\r";  break;
            case '\t': oss << "\\t";  break;
            default:   oss << c;      break;
        }
    }
    return oss.str();
}

std::string kv(const std::string &k, const std::string &v) {
    return "\"" + escape(k) + "\":\"" + escape(v) + "\"";
}

std::string kv(const std::string &k, double v) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(6);
    oss << v;
    return "\"" + escape(k) + "\":" + oss.str();
}

std::string kv(const std::string &k, int v) {
    return "\"" + escape(k) + "\":" + std::to_string(v);
}

} // namespace json

// Simple line-oriented JSON request parser for MCP calls.
// Expected request format (one JSON object per line):
// { "tool": "hex_stability_carbon", "hex_id": "PHX-001" }
// or
// { "tool": "ker_overview" }
struct Request {
    std::string tool;
    std::optional<std::string> hex_id;
};

static std::string trim(const std::string &s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static std::optional<std::string> extract_string_field(const std::string &json_line,
                                                       const std::string &field) {
    std::string key = "\"" + field + "\"";
    std::size_t pos = json_line.find(key);
    if (pos == std::string::npos) return std::nullopt;
    pos = json_line.find(':', pos + key.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json_line.size() && std::isspace(static_cast<unsigned char>(json_line[pos]))) ++pos;
    if (pos >= json_line.size() || json_line[pos] != '"') return std::nullopt;
    ++pos;
    std::ostringstream value;
    bool escaped = false;
    for (; pos < json_line.size(); ++pos) {
        char c = json_line[pos];
        if (escaped) {
            value << c;
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            break;
        } else {
            value << c;
        }
    }
    return value.str();
}

static bool parse_request_line(const std::string &line, Request &req) {
    std::string tline = trim(line);
    auto tool_opt = extract_string_field(tline, "tool");
    if (!tool_opt.has_value()) {
        return false;
    }
    req.tool = tool_opt.value();
    auto hex_opt = extract_string_field(tline, "hex_id");
    if (hex_opt.has_value()) {
        req.hex_id = hex_opt.value();
    }
    return true;
}

// SQLite helper for hex stability + carbon-aware view.
struct HexStabilityCarbonRow {
    std::string hex_id;
    std::string region_name;
    double avg_delta_v_t;
    double max_delta_v_t;
    double avg_ker_s;
    double avg_carbon_intensity;
    int red_band_count;
    int green_band_count;
    int neutral_band_count;
    int corridor_violations;
};

class GovernanceDatabase {
public:
    explicit GovernanceDatabase(const std::string &path)
        : db_(nullptr) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite database: " + std::string(sqlite3_errmsg(db_)));
        }
    }

    ~GovernanceDatabase() {
        if (db_) sqlite3_close(db_);
    }

    std::vector<HexStabilityCarbonRow> query_hex_stability_carbon(const std::optional<std::string> &hex_id) {
        std::vector<HexStabilityCarbonRow> rows;
        const char *base_sql =
            "SELECT hex_id, region_name, avg_delta_v_t, max_delta_v_t, "
            "avg_ker_s, avg_carbon_intensity_gco2_kwh, red_band_count, "
            "green_band_count, neutral_band_count, corridor_violations "
            "FROM v_hex_stability_ker_dvt_carbon";

        std::string sql;
        if (hex_id.has_value()) {
            sql = std::string(base_sql) + " WHERE hex_id = ?1";
        } else {
            sql = std::string(base_sql);
        }

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db_)));
        }

        if (hex_id.has_value()) {
            if (sqlite3_bind_text(stmt, 1, hex_id->c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
                sqlite3_finalize(stmt);
                throw std::runtime_error("Failed to bind hex_id: " + std::string(sqlite3_errmsg(db_)));
            }
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            HexStabilityCarbonRow row{};
            const unsigned char *hex = sqlite3_column_text(stmt, 0);
            const unsigned char *region = sqlite3_column_text(stmt, 1);

            row.hex_id = hex ? reinterpret_cast<const char *>(hex) : "";
            row.region_name = region ? reinterpret_cast<const char *>(region) : "";
            row.avg_delta_v_t = sqlite3_column_double(stmt, 2);
            row.max_delta_v_t = sqlite3_column_double(stmt, 3);
            row.avg_ker_s = sqlite3_column_double(stmt, 4);
            row.avg_carbon_intensity = sqlite3_column_double(stmt, 5);
            row.red_band_count = sqlite3_column_int(stmt, 6);
            row.green_band_count = sqlite3_column_int(stmt, 7);
            row.neutral_band_count = sqlite3_column_int(stmt, 8);
            row.corridor_violations = sqlite3_column_int(stmt, 9);

            rows.push_back(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to step statement: " + std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_finalize(stmt);
        return rows;
    }

    // Simple KER overview tool: aggregate s,k,e,r for modules and tools.
    struct KerOverviewRow {
        std::string relpath;
        std::string kind;  // "module" or "tool"
        double ker_k;
        double ker_e;
        double ker_r;
        double ker_s;
        std::string lane_default;
    };

    std::vector<KerOverviewRow> query_ker_overview() {
        std::vector<KerOverviewRow> rows;
        const char *sql =
            "SELECT relpath, 'module' AS kind, ker_k, ker_e, ker_r, ker_s, lane_default "
            "FROM v_ker_overview_modules_and_tools "
            "WHERE kind = 'module' "
            "UNION ALL "
            "SELECT relpath, 'tool' AS kind, ker_k, ker_e, ker_r, ker_s, lane_default "
            "FROM v_ker_overview_modules_and_tools "
            "WHERE kind = 'tool'";

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare KER overview statement: " + std::string(sqlite3_errmsg(db_)));
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            KerOverviewRow row{};
            const unsigned char *rel = sqlite3_column_text(stmt, 0);
            const unsigned char *kind = sqlite3_column_text(stmt, 1);
            const unsigned char *lane = sqlite3_column_text(stmt, 6);
            row.relpath = rel ? reinterpret_cast<const char *>(rel) : "";
            row.kind = kind ? reinterpret_cast<const char *>(kind) : "";
            row.ker_k = sqlite3_column_double(stmt, 2);
            row.ker_e = sqlite3_column_double(stmt, 3);
            row.ker_r = sqlite3_column_double(stmt, 4);
            row.ker_s = sqlite3_column_double(stmt, 5);
            row.lane_default = lane ? reinterpret_cast<const char *>(lane) : "";
            rows.push_back(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to step KER overview statement: " + std::string(sqlite3_errmsg(db_)));
        }

        sqlite3_finalize(stmt);
        return rows;
    }

private:
    sqlite3 *db_;
};

// JSON response encoders for MCP tools.
static void respond_hex_stability_carbon(const std::vector<HexStabilityCarbonRow> &rows) {
    std::cout << "{ \"result\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &r = rows[i];
        std::cout << "{"
                  << json::kv("hex_id", r.hex_id) << ","
                  << json::kv("region_name", r.region_name) << ","
                  << json::kv("avg_delta_v_t", r.avg_delta_v_t) << ","
                  << json::kv("max_delta_v_t", r.max_delta_v_t) << ","
                  << json::kv("avg_ker_s", r.avg_ker_s) << ","
                  << json::kv("avg_carbon_intensity_gco2_kwh", r.avg_carbon_intensity) << ","
                  << json::kv("red_band_count", r.red_band_count) << ","
                  << json::kv("green_band_count", r.green_band_count) << ","
                  << json::kv("neutral_band_count", r.neutral_band_count) << ","
                  << json::kv("corridor_violations", r.corridor_violations)
                  << "}";
        if (i + 1 < rows.size()) {
            std::cout << ",";
        }
    }
    std::cout << "] }\n";
}

static void respond_ker_overview(const std::vector<GovernanceDatabase::KerOverviewRow> &rows) {
    std::cout << "{ \"result\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &r = rows[i];
        std::cout << "{"
                  << json::kv("relpath", r.relpath) << ","
                  << json::kv("kind", r.kind) << ","
                  << json::kv("ker_k", r.ker_k) << ","
                  << json::kv("ker_e", r.ker_e) << ","
                  << json::kv("ker_r", r.ker_r) << ","
                  << json::kv("ker_s", r.ker_s) << ","
                  << json::kv("lane_default", r.lane_default)
                  << "}";
        if (i + 1 < rows.size()) {
            std::cout << ",";
        }
    }
    std::cout << "] }\n";
}

static void respond_error(const std::string &message) {
    std::cout << "{ \"error\": \"" << json::escape(message) << "\" }\n";
}

// Main MCP server loop: read JSON requests from stdin, respond with JSON.
int main(int argc, char **argv) {
    std::string db_path = "prometheus_praxis_governance.db";
    if (argc >= 2) {
        db_path = argv[1];
    }

    try {
        GovernanceDatabase db(db_path);

        std::string line;
        while (std::getline(std::cin, line)) {
            line = trim(line);
            if (line.empty()) continue;

            Request req;
            if (!parse_request_line(line, req)) {
                respond_error("Failed to parse request JSON; expected fields {tool, [hex_id]}");
                continue;
            }

            if (req.tool == "hex_stability_carbon") {
                try {
                    auto rows = db.query_hex_stability_carbon(req.hex_id);
                    respond_hex_stability_carbon(rows);
                } catch (const std::exception &e) {
                    respond_error(std::string("hex_stability_carbon query failed: ") + e.what());
                }
            } else if (req.tool == "ker_overview") {
                try {
                    auto rows = db.query_ker_overview();
                    respond_ker_overview(rows);
                } catch (const std::exception &e) {
                    respond_error(std::string("ker_overview query failed: ") + e.what());
                }
            } else {
                respond_error("Unknown tool: " + req.tool);
            }
        }

    } catch (const std::exception &e) {
        respond_error(std::string("Initialization failed: ") + e.what());
        return 1;
    }

    return 0;
}
