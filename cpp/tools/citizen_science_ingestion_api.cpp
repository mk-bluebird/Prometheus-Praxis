// File: cpp/tools/citizen_science_ingestion_api.cpp
#include <httplib.h>
#include <sqlite3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using DatabasePtr = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

double required_double(const httplib::Request& request, const char* key) {
    if (!request.has_param(key)) throw std::invalid_argument(std::string("missing ") + key);
    const std::string value = request.get_param_value(key);
    std::size_t consumed = 0;
    const double result = std::stod(value, &consumed);
    if (consumed != value.size() || !std::isfinite(result)) throw std::invalid_argument("invalid numeric value");
    return result;
}

std::string quality_flag(double confidence, double value, const std::string& type) {
    if (confidence < 0.25 || value < 0.0 || value > 100000.0) return "BAD";
    if (confidence < 0.60 || type != "invasive_species" && type != "water_clarity") return "SUSPECT";
    return "GOOD";
}

void create_schema(sqlite3* database) {
    const char* schema =
        "CREATE TABLE IF NOT EXISTS citizen_science_telemetry("
        "observation_id INTEGER PRIMARY KEY,hex_anchor INTEGER NOT NULL,"
        "observed_unix_s INTEGER NOT NULL,observation_type TEXT NOT NULL,"
        "observed_value REAL NOT NULL,unit TEXT NOT NULL,confidence REAL NOT NULL "
        "CHECK(confidence BETWEEN 0 AND 1),quality_flag TEXT NOT NULL "
        "CHECK(quality_flag IN('GOOD','SUSPECT','BAD')),note TEXT NOT NULL,"
        "UNIQUE(hex_anchor,observed_unix_s,observation_type,note)) STRICT;"
        "CREATE INDEX IF NOT EXISTS citizen_science_hex_time "
        "ON citizen_science_telemetry(hex_anchor,observed_unix_s);";
    if (sqlite3_exec(database, schema, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    sqlite3* raw = nullptr;
    if (sqlite3_open(argv[1], &raw) != SQLITE_OK) return 1;
    DatabasePtr database(raw, sqlite3_close);

    try {
        create_schema(database.get());
        const int port = std::stoi(argv[2]);
        httplib::Server server;
        server.Post("/v1/community-observations", [&database](const httplib::Request& request,
                                                               httplib::Response& response) {
            try {
                const auto anchor = static_cast<sqlite3_int64>(required_double(request, "hex_anchor"));
                const auto timestamp = static_cast<sqlite3_int64>(required_double(request, "observed_unix_s"));
                const double value = required_double(request, "observed_value");
                const double confidence = required_double(request, "confidence");
                const std::string type = request.get_param_value("observation_type");
                const std::string unit = request.get_param_value("unit");
                const std::string note = request.has_param("note") ? request.get_param_value("note") : "";
                if (anchor < 0 || timestamp < 0 || confidence < 0.0 || confidence > 1.0 ||
                    type.empty() || type.size() > 64 || unit.empty() || unit.size() > 24 || note.size() > 512)
                    throw std::invalid_argument("observation violates input bounds");

                const std::string flag = quality_flag(confidence, value, type);
                sqlite3_stmt* raw_statement = nullptr;
                const char* sql = "INSERT OR IGNORE INTO citizen_science_telemetry"
                    "(hex_anchor,observed_unix_s,observation_type,observed_value,unit,confidence,quality_flag,note)"
                    "VALUES(?,?,?,?,?,?,?,?);";
                if (sqlite3_prepare_v2(database.get(), sql, -1, &raw_statement, nullptr) != SQLITE_OK)
                    throw std::runtime_error(sqlite3_errmsg(database.get()));
                std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>
                    statement(raw_statement, sqlite3_finalize);
                sqlite3_bind_int64(statement.get(), 1, anchor);
                sqlite3_bind_int64(statement.get(), 2, timestamp);
                sqlite3_bind_text(statement.get(), 3, type.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(statement.get(), 4, value);
                sqlite3_bind_text(statement.get(), 5, unit.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_double(statement.get(), 6, confidence);
                sqlite3_bind_text(statement.get(), 7, flag.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(statement.get(), 8, note.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(statement.get()) != SQLITE_DONE) throw std::runtime_error("telemetry insert failed");
                response.status = 201;
                response.set_content("{\"quality_flag\":\"" + flag + "\"}", "application/json");
            } catch (const std::exception& error) {
                response.status = 400;
                response.set_content("{\"error\":\"" + std::string(error.what()) + "\"}", "application/json");
            }
        });
        return server.listen("127.0.0.1", port) ? 0 : 1;
    } catch (...) {
        return 1;
    }
}
