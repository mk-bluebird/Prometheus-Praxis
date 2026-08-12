// File: cpp/tools/hex_state_dashboard.cpp
#include <httplib.h>
#include <sqlite3.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using DatabasePtr = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

std::string json_escape(const std::string& value) {
    std::string result;
    for (char c : value) {
        if (c == '"' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

std::string load_hex_state(sqlite3* database, const std::string& anchor) {
    constexpr const char* sql =
        "SELECT anchor_id,risk_score,lane_decision,carbon_gco2e,water_risk,updated_at "
        "FROM hex_state WHERE anchor_id=?1;";
    sqlite3_stmt* raw = nullptr;
    if (sqlite3_prepare_v2(database, sql, -1, &raw, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, &sqlite3_finalize);
    sqlite3_bind_text(statement.get(), 1, anchor.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(statement.get()) != SQLITE_ROW) return {};
    const auto text = [&](int column) {
        const unsigned char* value = sqlite3_column_text(statement.get(), column);
        return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
    };
    return "{\"anchor_id\":\"" + json_escape(text(0)) + "\",\"risk_score\":" +
           std::to_string(sqlite3_column_double(statement.get(), 1)) +
           ",\"lane_decision\":\"" + json_escape(text(2)) +
           "\",\"carbon_gco2e\":" + std::to_string(sqlite3_column_double(statement.get(), 3)) +
           ",\"water_risk\":" + std::to_string(sqlite3_column_double(statement.get(), 4)) +
           ",\"updated_at\":\"" + json_escape(text(5)) + "\"}";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: hex_state_dashboard <state.sqlite> <port>\n";
        return 2;
    }
    const int port = std::atoi(argv[2]);
    if (port < 1 || port > 65535) return 2;

    sqlite3* raw = nullptr;
    if (sqlite3_open_v2(argv[1], &raw, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        std::cerr << (raw ? sqlite3_errmsg(raw) : "cannot open database") << '\n';
        if (raw) sqlite3_close(raw);
        return 1;
    }
    DatabasePtr database(raw, &sqlite3_close);
    httplib::Server server;

    server.Get(R"(/v1/hex/([^/]+))", [&database](const httplib::Request& request,
                                                 httplib::Response& response) {
        try {
            const std::string body = load_hex_state(database.get(), request.matches[1]);
            if (body.empty()) {
                response.set_content("{\"error\":\"hex state not found\"}", "application/json");
                response.status = 404;
            } else {
                response.set_content(body, "application/json");
            }
        } catch (const std::exception& error) {
            response.set_content("{\"error\":\"" + json_escape(error.what()) + "\"}",
                                 "application/json");
            response.status = 500;
        }
    });

    server.Get("/healthz", [](const httplib::Request&, httplib::Response& response) {
        response.set_content("{\"status\":\"ok\"}", "application/json");
    });
    std::cout << "dashboard listening on port " << port << '\n';
    return server.listen("127.0.0.1", port) ? 0 : 1;
}
