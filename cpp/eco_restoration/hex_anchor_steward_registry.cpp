// File: cpp/eco_restoration/hex_anchor_steward_registry.cpp
#include <sqlite3.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

namespace eco_restoration {

struct StewardCommitment {
    std::uint64_t hex_anchor{};
    std::string steward_did;
    std::string commitment_id;
    std::string restoration_commitment;
    std::int64_t recorded_unix_s{};
    double knowledge_factor{};
    double eco_impact_value{};
    double risk{};
};

bool valid_steward_identifier(const std::string& value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '_' || c == '-' || c == ':';
           });
}

void initialize_steward_registry(sqlite3* database) {
    const char* schema =
        "CREATE TABLE IF NOT EXISTS hex_anchor_steward_commitment("
        "hex_anchor INTEGER NOT NULL,steward_did TEXT NOT NULL,commitment_id TEXT NOT NULL,"
        "restoration_commitment TEXT NOT NULL,recorded_unix_s INTEGER NOT NULL,"
        "knowledge_factor REAL NOT NULL CHECK(knowledge_factor BETWEEN 0 AND 1),"
        "eco_impact_value REAL NOT NULL CHECK(eco_impact_value BETWEEN 0 AND 1),"
        "risk REAL NOT NULL CHECK(risk BETWEEN 0 AND 1),"
        "PRIMARY KEY(hex_anchor,steward_did,commitment_id)) STRICT;"
        "CREATE INDEX IF NOT EXISTS hex_anchor_steward_lookup "
        "ON hex_anchor_steward_commitment(hex_anchor,recorded_unix_s);";
    if (sqlite3_exec(database, schema, nullptr, nullptr, nullptr) != SQLITE_OK)
        throw std::runtime_error(sqlite3_errmsg(database));
}

void record_steward_commitment(sqlite3* database, const StewardCommitment& commitment) {
    if (!database || !valid_steward_identifier(commitment.steward_did, 128) ||
        !valid_steward_identifier(commitment.commitment_id, 64) ||
        commitment.restoration_commitment.empty() || commitment.restoration_commitment.size() > 1024 ||
        commitment.recorded_unix_s < 0 || commitment.knowledge_factor < 0.0 ||
        commitment.knowledge_factor > 1.0 || commitment.eco_impact_value < 0.0 ||
        commitment.eco_impact_value > 1.0 || commitment.risk < 0.0 || commitment.risk > 1.0)
        throw std::invalid_argument("invalid steward commitment");

    sqlite3_stmt* raw = nullptr;
    sqlite3_prepare_v2(database,
        "INSERT INTO hex_anchor_steward_commitment VALUES(?,?,?,?,?,?,?,?);",
        -1, &raw, nullptr);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(raw, sqlite3_finalize);
    sqlite3_bind_int64(statement.get(), 1, static_cast<sqlite3_int64>(commitment.hex_anchor));
    sqlite3_bind_text(statement.get(), 2, commitment.steward_did.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 3, commitment.commitment_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(statement.get(), 4, commitment.restoration_commitment.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(statement.get(), 5, commitment.recorded_unix_s);
    sqlite3_bind_double(statement.get(), 6, commitment.knowledge_factor);
    sqlite3_bind_double(statement.get(), 7, commitment.eco_impact_value);
    sqlite3_bind_double(statement.get(), 8, commitment.risk);
    if (sqlite3_step(statement.get()) != SQLITE_DONE)
        throw std::runtime_error("steward commitment persistence failed");
}

}  // namespace eco_restoration
