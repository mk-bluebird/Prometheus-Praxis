// File: cpp/tools/mcp_ker_synapse_server_named.cpp
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <cctype>
#include <sqlite3.h>

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

struct Request {
    std::string tool;
    std::optional<std::string> primary_plane;
    std::optional<std::string> lane;
    std::optional<std::string> hex_id;
    std::optional<std::string> region_name;
    std::optional<std::string> carbon_band;
    std::optional<int>        limit;
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

static std::optional<int> extract_int_field(const std::string &json_line,
                                            const std::string &field) {
    std::string key = "\"" + field + "\"";
    std::size_t pos = json_line.find(key);
    if (pos == std::string::npos) return std::nullopt;
    pos = json_line.find(':', pos + key.size());
    if (pos == std::string::npos) return std::nullopt;
    ++pos;
    while (pos < json_line.size() && std::isspace(static_cast<unsigned char>(json_line[pos]))) ++pos;
    std::size_t start = pos;
    while (pos < json_line.size() && (std::isdigit(static_cast<unsigned char>(json_line[pos])) ||
                                      json_line[pos] == '-')) {
        ++pos;
    }
    if (start == pos) return std::nullopt;
    int value = std::stoi(json_line.substr(start, pos - start));
    return value;
}

static bool parse_request_line(const std::string &line, Request &req) {
    std::string tline = trim(line);
    auto tool_opt = extract_string_field(tline, "tool");
    if (!tool_opt.has_value()) return false;
    req.tool = tool_opt.value();
    req.primary_plane = extract_string_field(tline, "primary_plane");
    req.lane = extract_string_field(tline, "lane");
    req.hex_id = extract_string_field(tline, "hex_id");
    req.region_name = extract_string_field(tline, "region_name");
    req.carbon_band = extract_string_field(tline, "carbon_band");
    req.limit = extract_int_field(tline, "limit");
    return true;
}

// SQLite access and typed rows.
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

    struct HexStabilityCarbonRow {
        std::string hex_id;
        std::string region_name;
        std::string primary_plane;
        double avg_delta_v_t;
        double max_delta_v_t;
        double total_delta_v_t;
        double avg_ker_s;
        double avg_carbon_intensity;
        int red_band_count;
        int green_band_count;
        int neutral_band_count;
        int violations_prod_red_band;
        int corridor_violations;
    };

    std::vector<HexStabilityCarbonRow> query_hex_stability_carbon(const Request &req) {
        std::vector<HexStabilityCarbonRow> rows;
        std::ostringstream sql;
        sql << "SELECT hex_id, region_name, primary_plane, avg_delta_v_t, max_delta_v_t, "
            << "total_delta_v_t, avg_ker_s, avg_carbon_intensity_gco2_kwh, "
            << "red_band_count, green_band_count, neutral_band_count, "
            << "violations_prod_red_band, corridor_violations "
            << "FROM v_hex_stability_ker_dvt_carbon";

        bool has_where = false;
        if (req.hex_id.has_value()) {
            sql << (has_where ? " AND" : " WHERE") << " hex_id = ?1";
            has_where = true;
        }
        int bind_index = 2;
        if (req.region_name.has_value()) {
            sql << (has_where ? " AND" : " WHERE") << " region_name = ?" << bind_index++;
            has_where = true;
        }
        if (req.primary_plane.has_value()) {
            sql << (has_where ? " AND" : " WHERE") << " primary_plane = ?" << bind_index++;
            has_where = true;
        }
        if (req.carbon_band.has_value()) {
            // Map band string to column condition.
            if (*req.carbon_band == "RED_BAND") {
                sql << (has_where ? " AND" : " WHERE") << " red_band_count > 0";
                has_where = true;
            } else if (*req.carbon_band == "GREEN_BAND") {
                sql << (has_where ? " AND" : " WHERE") << " green_band_count > 0";
                has_where = true;
            } else if (*req.carbon_band == "NEUTRAL") {
                sql << (has_where ? " AND" : " WHERE") << " neutral_band_count > 0";
                has_where = true;
            }
        }
        sql << " ORDER BY total_delta_v_t DESC";
        if (req.limit.has_value() && req.limit.value() > 0) {
            sql << " LIMIT " << req.limit.value();
        }

        sqlite3_stmt *stmt = nullptr;
        std::string sql_str = sql.str();
        if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare hex_stability_carbon statement: " + std::string(sqlite3_errmsg(db_)));
        }

        int idx = 1;
        if (req.hex_id.has_value()) {
            sqlite3_bind_text(stmt, idx++, req.hex_id->c_str(), -1, SQLITE_TRANSIENT);
        }
        if (req.region_name.has_value()) {
            sqlite3_bind_text(stmt, idx++, req.region_name->c_str(), -1, SQLITE_TRANSIENT);
        }
        if (req.primary_plane.has_value()) {
            sqlite3_bind_text(stmt, idx++, req.primary_plane->c_str(), -1, SQLITE_TRANSIENT);
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            HexStabilityCarbonRow row{};
            const unsigned char *hex = sqlite3_column_text(stmt, 0);
            const unsigned char *region = sqlite3_column_text(stmt, 1);
            const unsigned char *plane = sqlite3_column_text(stmt, 2);
            row.hex_id = hex ? reinterpret_cast<const char *>(hex) : "";
            row.region_name = region ? reinterpret_cast<const char *>(region) : "";
            row.primary_plane = plane ? reinterpret_cast<const char *>(plane) : "";
            row.avg_delta_v_t = sqlite3_column_double(stmt, 3);
            row.max_delta_v_t = sqlite3_column_double(stmt, 4);
            row.total_delta_v_t = sqlite3_column_double(stmt, 5);
            row.avg_ker_s = sqlite3_column_double(stmt, 6);
            row.avg_carbon_intensity = sqlite3_column_double(stmt, 7);
            row.red_band_count = sqlite3_column_int(stmt, 8);
            row.green_band_count = sqlite3_column_int(stmt, 9);
            row.neutral_band_count = sqlite3_column_int(stmt, 10);
            row.violations_prod_red_band = sqlite3_column_int(stmt, 11);
            row.corridor_violations = sqlite3_column_int(stmt, 12);
            rows.push_back(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to step hex_stability_carbon statement: " + std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_finalize(stmt);
        return rows;
    }

    struct KerRow {
        std::string relpath;
        std::string kind;
        double ker_k;
        double ker_e;
        double ker_r;
        double ker_s;
        std::string lane_default;
    };

    std::vector<KerRow> query_ker_overview(const Request &req) {
        std::vector<KerRow> rows;
        std::ostringstream sql;
        sql << "SELECT relpath, kind, ker_k, ker_e, ker_r, ker_s, lane_default "
            << "FROM v_ker_overview_modules_and_tools";

        bool has_where = false;
        if (req.lane.has_value()) {
            sql << (has_where ? " AND" : " WHERE") << " lane_default = ?1";
            has_where = true;
        }
        sql << " ORDER BY ker_s DESC";
        if (req.limit.has_value() && req.limit.value() > 0) {
            sql << " LIMIT " << req.limit.value();
        }

        sqlite3_stmt *stmt = nullptr;
        std::string sql_str = sql.str();
        if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare ker_overview statement: " + std::string(sqlite3_errmsg(db_)));
        }

        int idx = 1;
        if (req.lane.has_value()) {
            sqlite3_bind_text(stmt, idx++, req.lane->c_str(), -1, SQLITE_TRANSIENT);
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            KerRow row{};
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
            throw std::runtime_error("Failed to step ker_overview statement: " + std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_finalize(stmt);
        return rows;
    }

    struct SynapseSafeRow {
        std::string producer_relpath;
        std::string consumer_relpath;
        std::string transport_kind;
        std::string synapse_class;
        double ker_k;
        double ker_e;
        double ker_r;
        double ker_s;
        std::string lane_default;
    };

    std::vector<SynapseSafeRow> query_synapse_safe_for_eco(const Request &req) {
        std::vector<SynapseSafeRow> rows;
        std::ostringstream sql;
        sql << "SELECT producer_relpath, consumer_relpath, transport_kind, synapse_class, "
            << "ker_k, ker_e, ker_r, ker_s, lane_default "
            << "FROM v_synapse_safe_for_eco";

        bool has_where = false;
        if (req.lane.has_value()) {
            sql << (has_where ? " AND" : " WHERE") << " lane_default = ?1";
            has_where = true;
        }
        sql << " ORDER BY ker_s DESC";
        if (req.limit.has_value() && req.limit.value() > 0) {
            sql << " LIMIT " << req.limit.value();
        }

        sqlite3_stmt *stmt = nullptr;
        std::string sql_str = sql.str();
        if (sqlite3_prepare_v2(db_, sql_str.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare synapse_safe_for_eco statement: " + std::string(sqlite3_errmsg(db_)));
        }

        int idx = 1;
        if (req.lane.has_value()) {
            sqlite3_bind_text(stmt, idx++, req.lane->c_str(), -1, SQLITE_TRANSIENT);
        }

        int rc;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            SynapseSafeRow row{};
            const unsigned char *prod = sqlite3_column_text(stmt, 0);
            const unsigned char *cons = sqlite3_column_text(stmt, 1);
            const unsigned char *transport = sqlite3_column_text(stmt, 2);
            const unsigned char *synapse = sqlite3_column_text(stmt, 3);
            const unsigned char *lane = sqlite3_column_text(stmt, 8);
            row.producer_relpath = prod ? reinterpret_cast<const char *>(prod) : "";
            row.consumer_relpath = cons ? reinterpret_cast<const char *>(cons) : "";
            row.transport_kind = transport ? reinterpret_cast<const char *>(transport) : "";
            row.synapse_class = synapse ? reinterpret_cast<const char *>(synapse) : "";
            row.ker_k = sqlite3_column_double(stmt, 4);
            row.ker_e = sqlite3_column_double(stmt, 5);
            row.ker_r = sqlite3_column_double(stmt, 6);
            row.ker_s = sqlite3_column_double(stmt, 7);
            row.lane_default = lane ? reinterpret_cast<const char *>(lane) : "";
            rows.push_back(row);
        }

        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to step synapse_safe_for_eco statement: " + std::string(sqlite3_errmsg(db_)));
        }
        sqlite3_finalize(stmt);
        return rows;
    }

private:
    sqlite3 *db_;
};

static void respond_error(const std::string &message) {
    std::cout << "{ \"error\": \"" << json::escape(message) << "\" }\n";
}

static void respond_hex_stability_carbon(const std::vector<GovernanceDatabase::HexStabilityCarbonRow> &rows) {
    std::cout << "{ \"tool\": \"hex_stability_carbon\", \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &r = rows[i];
        std::cout << "{"
                  << json::kv("hex_id", r.hex_id) << ","
                  << json::kv("region_name", r.region_name) << ","
                  << json::kv("primary_plane", r.primary_plane) << ","
                  << json::kv("avg_delta_v_t", r.avg_delta_v_t) << ","
                  << json::kv("max_delta_v_t", r.max_delta_v_t) << ","
                  << json::kv("total_delta_v_t", r.total_delta_v_t) << ","
                  << json::kv("avg_ker_s", r.avg_ker_s) << ","
                  << json::kv("avg_carbon_intensity_gco2_kwh", r.avg_carbon_intensity) << ","
                  << json::kv("red_band_count", r.red_band_count) << ","
                  << json::kv("green_band_count", r.green_band_count) << ","
                  << json::kv("neutral_band_count", r.neutral_band_count) << ","
                  << json::kv("violations_prod_red_band", r.violations_prod_red_band) << ","
                  << json::kv("corridor_violations", r.corridor_violations)
                  << "}";
        if (i + 1 < rows.size()) std::cout << ",";
    }
    std::cout << "] }\n";
}

static void respond_ker_overview(const std::vector<GovernanceDatabase::KerRow> &rows) {
    std::cout << "{ \"tool\": \"ker_overview\", \"rows\": [";
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
        if (i + 1 < rows.size()) std::cout << ",";
    }
    std::cout << "] }\n";
}

static void respond_synapse_safe_for_eco(const std::vector<GovernanceDatabase::SynapseSafeRow> &rows) {
    std::cout << "{ \"tool\": \"synapse_safe_for_eco\", \"rows\": [";
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto &r = rows[i];
        std::cout << "{"
                  << json::kv("producer_relpath", r.producer_relpath) << ","
                  << json::kv("consumer_relpath", r.consumer_relpath) << ","
                  << json::kv("transport_kind", r.transport_kind) << ","
                  << json::kv("synapse_class", r.synapse_class) << ","
                  << json::kv("ker_k", r.ker_k) << ","
                  << json::kv("ker_e", r.ker_e) << ","
                  << json::kv("ker_r", r.ker_r) << ","
                  << json::kv("ker_s", r.ker_s) << ","
                  << json::kv("lane_default", r.lane_default)
                  << "}";
        if (i + 1 < rows.size()) std::cout << ",";
    }
    std::cout << "] }\n";
}

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
                respond_error("Failed to parse request JSON; expected fields {tool, [primary_plane], [lane], [hex_id], [region_name], [carbon_band], [limit]}");
                continue;
            }

            try {
                if (req.tool == "hex_stability_carbon") {
                    auto rows = db.query_hex_stability_carbon(req);
                    respond_hex_stability_carbon(rows);
                } else if (req.tool == "ker_overview") {
                    auto rows = db.query_ker_overview(req);
                    respond_ker_overview(rows);
                } else if (req.tool == "synapse_safe_for_eco") {
                    auto rows = db.query_synapse_safe_for_eco(req);
                    respond_synapse_safe_for_eco(rows);
                } else {
                    respond_error("Unknown tool: " + req.tool);
                }
            } catch (const std::exception &e) {
                respond_error(std::string("Tool execution failed: ") + e.what());
            }
        }
    } catch (const std::exception &e) {
        respond_error(std::string("Initialization failed: ") + e.what());
        return 1;
    }

    return 0;
}
