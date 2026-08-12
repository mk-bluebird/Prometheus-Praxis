// File: cpp/tools/hex_action_web_editor.cpp
#include <httplib.h>
#include <sqlite3.h>

#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

bool valid_action(const std::string& action) {
    return !action.empty() && action.size() <= 64 &&
           std::all_of(action.begin(), action.end(), [](unsigned char c) {
               return std::islower(c) || std::isdigit(c) || c == '_';
           });
}

void schema(sqlite3* database) {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS hex_action_catalog("
        "action_id TEXT PRIMARY KEY CHECK(length(action_id) BETWEEN 1 AND 64),"
        "description TEXT NOT NULL,enabled INTEGER NOT NULL CHECK(enabled IN(0,1))) STRICT;"
        "CREATE TABLE IF NOT EXISTS hex_action_assignment("
        "hex_anchor INTEGER PRIMARY KEY,action_id TEXT NOT NULL,"
        "operator_id TEXT NOT NULL,assigned_unix_s INTEGER NOT NULL,"
        "FOREIGN KEY(action_id) REFERENCES hex_action_catalog(action_id)) STRICT;"
        "CREATE TABLE IF NOT EXISTS hex_action_assignment_event("
        "event_id INTEGER PRIMARY KEY,hex_anchor INTEGER NOT NULL,action_id TEXT NOT NULL,"
        "operator_id TEXT NOT NULL,assigned_unix_s INTEGER NOT NULL) STRICT;";
    if (sqlite3_exec(database, sql, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
}

std::string page(sqlite3* database) {
    std::string html =
        "<!doctype html><title>Hex Action Editor</title><h1>Hex Action Editor</h1>"
        "<form method='post' action='/assign'>"
        "Hex anchor <input name='hex_anchor' required pattern='[0-9]+'><br>"
        "Action <input name='action_id' required pattern='[a-z0-9_]+'><br>"
        "Operator <input name='operator_id' required maxlength='64'><br>"
        "UTC seconds <input name='assigned_unix_s' required pattern='[0-9]+'><br>"
        "<button type='submit'>Assign action</button></form><h2>Current assignments</h2><table>"
        "<tr><th>Hex anchor</th><th>Action</th><th>Operator</th><th>UTC</th></tr>";

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "SELECT hex_anchor,action_id,operator_id,assigned_unix_s FROM hex_action_assignment "
        "ORDER BY assigned_unix_s DESC LIMIT 200;", -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    while (sqlite3_step(statement.get()) == SQLITE_ROW) {
        html += "<tr><td>" + std::to_string(sqlite3_column_int64(statement.get(), 0)) +
                "</td><td>" + reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)) +
                "</td><td>" + reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2)) +
                "</td><td>" + std::to_string(sqlite3_column_int64(statement.get(), 3)) + "</td></tr>";
    }
    return html + "</table>";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    sqlite3* raw = nullptr;
    if (sqlite3_open(argv[1], &raw) != SQLITE_OK) return 1;
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
    schema(database.get());

    httplib::Server server;
    server.Get("/", [&database](const httplib::Request&, httplib::Response& response) {
        response.set_content(page(database.get()), "text/html; charset=utf-8");
    });
    server.Post("/assign", [&database](const httplib::Request& request, httplib::Response& response) {
        try {
            const std::string action = request.get_param_value("action_id");
            const std::string operator_id = request.get_param_value("operator_id");
            if (!valid_action(action) || operator_id.empty() || operator_id.size() > 64)
                throw std::invalid_argument("invalid action or operator identifier");
            const auto anchor = std::stoll(request.get_param_value("hex_anchor"));
            const auto timestamp = std::stoll(request.get_param_value("assigned_unix_s"));
            if (anchor < 0 || timestamp < 0) throw std::invalid_argument("invalid numeric field");

            sqlite3_exec(database.get(), "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
            sqlite3_stmt* assignment = nullptr;
            sqlite3_prepare_v2(database.get(),
                "INSERT INTO hex_action_assignment VALUES(?,?,?,?) "
                "ON CONFLICT(hex_anchor) DO UPDATE SET action_id=excluded.action_id,"
                "operator_id=excluded.operator_id,assigned_unix_s=excluded.assigned_unix_s;",
                -1, &assignment, nullptr);
            sqlite3_bind_int64(assignment, 1, anchor);
            sqlite3_bind_text(assignment, 2, action.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(assignment, 3, operator_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(assignment, 4, timestamp);
            if (sqlite3_step(assignment) != SQLITE_DONE) throw std::runtime_error("assignment rejected");
            sqlite3_finalize(assignment);

            sqlite3_stmt* event = nullptr;
            sqlite3_prepare_v2(database.get(),
                "INSERT INTO hex_action_assignment_event(hex_anchor,action_id,operator_id,assigned_unix_s)"
                "VALUES(?,?,?,?);", -1, &event, nullptr);
            sqlite3_bind_int64(event, 1, anchor);
            sqlite3_bind_text(event, 2, action.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(event, 3, operator_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(event, 4, timestamp);
            if (sqlite3_step(event) != SQLITE_DONE) throw std::runtime_error("event persistence failed");
            sqlite3_finalize(event);
            sqlite3_exec(database.get(), "COMMIT;", nullptr, nullptr, nullptr);
            response.set_redirect("/", 303);
        } catch (...) {
            sqlite3_exec(database.get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            response.status = 400;
            response.set_content("invalid assignment", "text/plain");
        }
    });
    return server.listen("127.0.0.1", std::stoi(argv[2])) ? 0 : 1;
}
