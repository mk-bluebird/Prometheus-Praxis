// File: cpp/tools/edge_state_sync_protocol.cpp
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct SyncRecord {
    std::uint64_t hex_anchor{};
    std::string action_id;
    std::string replica_id;
    std::uint64_t counter{};
    std::int64_t observed_unix_s{};
};

bool valid_identifier(const std::string& value, std::size_t maximum = 64) {
    return !value.empty() && value.size() <= maximum &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::islower(c) || std::isdigit(c) || c == '_' || c == '-';
           });
}

std::string encode_sync_record(const SyncRecord& record) {
    if (!valid_identifier(record.action_id) || !valid_identifier(record.replica_id) ||
        record.observed_unix_s < 0) throw std::invalid_argument("invalid synchronization record");
    return "{\"protocol\":\"eco-state-sync/1\",\"hex_anchor\":" + std::to_string(record.hex_anchor) +
           ",\"action_id\":\"" + record.action_id + "\",\"replica_id\":\"" + record.replica_id +
           "\",\"counter\":" + std::to_string(record.counter) +
           ",\"observed_unix_s\":" + std::to_string(record.observed_unix_s) + "}";
}

void initialize_sync_schema(sqlite3* database) {
    const char* schema =
        "CREATE TABLE IF NOT EXISTS hex_action_version_vector("
        "hex_anchor INTEGER NOT NULL,action_id TEXT NOT NULL,replica_id TEXT NOT NULL,"
        "counter INTEGER NOT NULL CHECK(counter>=0),observed_unix_s INTEGER NOT NULL,"
        "PRIMARY KEY(hex_anchor,action_id,replica_id)) STRICT;"
        "CREATE TABLE IF NOT EXISTS hex_action_sync_conflict("
        "hex_anchor INTEGER NOT NULL,action_a TEXT NOT NULL,action_b TEXT NOT NULL,"
        "observed_unix_s INTEGER NOT NULL,PRIMARY KEY(hex_anchor,action_a,action_b)) STRICT;";
    if (sqlite3_exec(database, schema, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
}

void apply_sync_record(sqlite3* database, const SyncRecord& record) {
    if (!database || !valid_identifier(record.action_id) || !valid_identifier(record.replica_id) ||
        record.observed_unix_s < 0) throw std::invalid_argument("invalid synchronization record");

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_action_version_vector VALUES(?,?,?,?,?) "
        "ON CONFLICT(hex_anchor,action_id,replica_id) DO UPDATE SET "
        "counter=MAX(counter,excluded.counter),observed_unix_s=MAX(observed_unix_s,excluded.observed_unix_s);",
        -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(record.hex_anchor));
    sqlite3_bind_text(statement.get(), 2, record.action_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, record.replica_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 4, static_cast<sqlite3_int64>(record.counter));
    sqlite3_bind_int64(statement.get(), 5, record.observed_unix_s);
    if (sqlite3_step(statement.get()) != SQLITE_DONE) throw std::runtime_error("sync persistence failed");
}

}  // namespace eco_restoration
