// File: cpp/tools/lorawan_telemetry_bridge.cpp
#include <httplib.h>
#include <sqlite3.h>

#include <cctype>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

bool is_hex_eui(const std::string& value) {
    return value.size() == 16 &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c); });
}

double numeric(const httplib::Request& request, const char* name) {
    if (!request.has_param(name)) throw std::invalid_argument("missing telemetry field");
    std::size_t consumed = 0;
    const std::string text = request.get_param_value(name);
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value)) throw std::invalid_argument("invalid telemetry value");
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const char* token = std::getenv("CHIRPSTACK_BRIDGE_TOKEN");
    if (token == nullptr || std::string(token).empty()) return 2;

    sqlite3* raw = nullptr;
    if (sqlite3_open(argv[1], &raw) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
    sqlite3_exec(database.get(),
        "CREATE TABLE IF NOT EXISTS lorawan_telemetry("
        "device_eui TEXT NOT NULL,hex_anchor INTEGER NOT NULL,observed_unix_s INTEGER NOT NULL,"
        "temperature_c REAL NOT NULL,turbidity_ntu REAL NOT NULL,oxygen_mg_l REAL NOT NULL,"
        "water_quality_index REAL NOT NULL CHECK(water_quality_index BETWEEN 0 AND 1),"
        "PRIMARY KEY(device_eui,observed_unix_s)) STRICT;", nullptr, nullptr, nullptr);

    httplib::Server server;
    server.Post("/v1/chirpstack/uplink", [&database, token](const httplib::Request& request,
                                                            httplib::Response& response) {
        try {
            if (!request.has_header("Authorization") ||
                request.get_header_value("Authorization") != "Bearer " + std::string(token))
                throw std::invalid_argument("unauthorized uplink");
            const std::string eui = request.get_param_value("device_eui");
            if (!is_hex_eui(eui)) throw std::invalid_argument("invalid device EUI");

            sqlite3_stmt* raw_statement = nullptr;
            sqlite3_prepare_v2(database.get(),
                "INSERT OR IGNORE INTO lorawan_telemetry VALUES(?,?,?,?,?,?,?);",
                -1, &raw_statement, nullptr);
            std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw_statement, sqlite3_finalize);
            sqlite3_bind_text(statement.get(), 1, eui.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(statement.get(), 2, static_cast<sqlite3_int64>(numeric(request, "hex_anchor")));
            sqlite3_bind_int64(statement.get(), 3, static_cast<sqlite3_int64>(numeric(request, "observed_unix_s")));
            sqlite3_bind_double(statement.get(), 4, numeric(request, "temperature_c"));
            sqlite3_bind_double(statement.get(), 5, numeric(request, "turbidity_ntu"));
            sqlite3_bind_double(statement.get(), 6, numeric(request, "oxygen_mg_l"));
            sqlite3_bind_double(statement.get(), 7, numeric(request, "water_quality_index"));
            if (sqlite3_step(statement.get()) != SQLITE_DONE) throw std::runtime_error("SQLite insert failed");
            response.status = 201;
            response.set_content("{\"accepted\":true}", "application/json");
        } catch (const std::exception& error) {
            response.status = 400;
            response.set_content("{\"error\":\"" + std::string(error.what()) + "\"}", "application/json");
        }
    });
    return server.listen("127.0.0.1", std::stoi(argv[2])) ? 0 : 1;
}
