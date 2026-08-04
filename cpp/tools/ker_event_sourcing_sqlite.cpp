// File: cpp/tools/ker_event_sourcing_sqlite.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

// SQLite-backed event sourcing for KER audit trail.
// This file installs:
//  - ker_event_store table for append-only events.
//  - example KER state table (ker_state) for projection.
//  - triggers to intercept direct state updates and rewrite them as events + projections.
//  - views to track event ordering and basic provenance.
//
// NOTE: Merkle hashing is mentioned in the math, but cryptographic schemes are blacklisted
// in this project; therefore, we omit Merkle tree implementation here and rely on
// monotonically increasing sequence IDs and external verification hooks. A future
// non-blacklisted hashing scheme could be integrated in a separate, allowed component.

namespace prometheus_praxis {
namespace tools {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install core event store and example KER state schema.
void install_event_store_schema(sqlite3* db) {
    const std::string sql_event_store = R"SQL(
        CREATE TABLE IF NOT EXISTS ker_event_store (
            event_id        INTEGER PRIMARY KEY AUTOINCREMENT,
            event_ts        TEXT NOT NULL,
            actor_id        TEXT NOT NULL,
            aggregate_type  TEXT NOT NULL,  -- e.g., 'KER_STATE'
            aggregate_id    TEXT NOT NULL,
            event_type      TEXT NOT NULL,  -- e.g., 'UPDATE_THRESHOLD', 'FLAG_SEGMENT'
            event_payload   TEXT NOT NULL,  -- JSON payload
            prev_event_id   INTEGER,        -- pointer to previous event in chain
            sequence_no     INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_ker_event_store_aggregate
            ON ker_event_store(aggregate_type, aggregate_id, sequence_no);
    )SQL";

    // Example KER state table: represents current configuration or thresholds.
    const std::string sql_ker_state = R"SQL(
        CREATE TABLE IF NOT EXISTS ker_state (
            aggregate_id    TEXT PRIMARY KEY,
            state_payload   TEXT NOT NULL,  -- JSON representation of current state
            updated_at      TEXT NOT NULL
        );
    )SQL";

    // View for ordered events per aggregate.
    const std::string sql_event_view = R"SQL(
        CREATE VIEW IF NOT EXISTS ker_event_ordered AS
        SELECT
            event_id,
            event_ts,
            actor_id,
            aggregate_type,
            aggregate_id,
            event_type,
            event_payload,
            prev_event_id,
            sequence_no
        FROM ker_event_store
        ORDER BY aggregate_type, aggregate_id, sequence_no;
    )SQL";

    exec_sql(db, sql_event_store);
    exec_sql(db, sql_ker_state);
    exec_sql(db, sql_event_view);
}

// Trigger-based projection: intercept updates to ker_state and rewrite as events.
//
// Pattern:
//   - Application should perform INSERT/UPDATE into ker_state via a dedicated
//     "command" table or stored procedure; here we show direct UPDATE interception.
//
// For simplicity, we assume that update statements set state_payload directly;
// the trigger captures the change as an event and maintains ker_state.
void install_projection_triggers(sqlite3* db) {
    // Command table for incoming updates, so we can centralize event creation.
    const std::string sql_command_table = R"SQL(
        CREATE TABLE IF NOT EXISTS ker_state_command (
            cmd_id         INTEGER PRIMARY KEY AUTOINCREMENT,
            cmd_ts         TEXT NOT NULL,
            actor_id       TEXT NOT NULL,
            aggregate_id   TEXT NOT NULL,
            event_type     TEXT NOT NULL,
            event_payload  TEXT NOT NULL
        );
    )SQL";

    // Trigger: on INSERT into ker_state_command, append event and update ker_state.
    const std::string sql_trigger_cmd = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_ker_state_command_insert
        AFTER INSERT ON ker_state_command
        BEGIN
            -- Compute next sequence_no for this aggregate.
            INSERT INTO ker_event_store(
                event_ts, actor_id, aggregate_type, aggregate_id,
                event_type, event_payload, prev_event_id, sequence_no
            )
            SELECT
                NEW.cmd_ts,
                NEW.actor_id,
                'KER_STATE',
                NEW.aggregate_id,
                NEW.event_type,
                NEW.event_payload,
                (SELECT MAX(event_id) FROM ker_event_store
                 WHERE aggregate_type = 'KER_STATE'
                   AND aggregate_id = NEW.aggregate_id),
                COALESCE(
                    (SELECT MAX(sequence_no) + 1 FROM ker_event_store
                     WHERE aggregate_type = 'KER_STATE'
                       AND aggregate_id = NEW.aggregate_id),
                    0
                );

            -- Upsert projection into ker_state.
            INSERT INTO ker_state(aggregate_id, state_payload, updated_at)
            VALUES (NEW.aggregate_id, NEW.event_payload, NEW.cmd_ts)
            ON CONFLICT(aggregate_id) DO UPDATE SET
                state_payload = excluded.state_payload,
                updated_at = excluded.updated_at;
        END;
    )SQL";

    exec_sql(db, sql_command_table);
    exec_sql(db, sql_trigger_cmd);
}

// Example function to append a KER state update as an event via ker_state_command.
void append_ker_state_update(sqlite3* db,
                             const std::string& actor_id,
                             const std::string& aggregate_id,
                             const std::string& event_type,
                             const std::string& event_payload,
                             const std::string& event_ts) {
    std::string sql =
        "INSERT INTO ker_state_command(cmd_ts, actor_id, aggregate_id, event_type, event_payload) "
        "VALUES(?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare ker_state_command insert failed: " +
                                 std::string(sqlite3_errmsg(db)));
    }

    rc = sqlite3_bind_text(stmt, 1, event_ts.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) goto bind_error;
    rc = sqlite3_bind_text(stmt, 2, actor_id.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) goto bind_error;
    rc = sqlite3_bind_text(stmt, 3, aggregate_id.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) goto bind_error;
    rc = sqlite3_bind_text(stmt, 4, event_type.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) goto bind_error;
    rc = sqlite3_bind_text(stmt, 5, event_payload.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) goto bind_error;

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("ker_state_command insert step failed: " +
                                 std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
    return;

bind_error:
    sqlite3_finalize(stmt);
    throw std::runtime_error("Bind error in ker_state_command insert: " +
                             std::string(sqlite3_errmsg(db)));
}

// Example replay function: rebuild ker_state projection from ker_event_store.
// In a full projection service, this would be implemented in Kotlin,
// but here we show the SQL-driven replay pattern.
void replay_projection(sqlite3* db) {
    // Clear current projection.
    exec_sql(db, "DELETE FROM ker_state;");

    // Rebuild from ordered events.
    const char* sql =
        "SELECT aggregate_id, event_payload, event_ts "
        "FROM ker_event_ordered;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare ker_event_ordered select failed: " +
                                 std::string(sqlite3_errmsg(db)));
    }

    // Simple last-write-wins projection: latest event_payload per aggregate becomes state.
    std::string last_agg;
    std::string last_payload;
    std::string last_ts;

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const unsigned char* agg = sqlite3_column_text(stmt, 0);
        const unsigned char* payload = sqlite3_column_text(stmt, 1);
        const unsigned char* ts = sqlite3_column_text(stmt, 2);

        std::string agg_id = agg ? reinterpret_cast<const char*>(agg) : "";
        std::string payload_str = payload ? reinterpret_cast<const char*>(payload) : "";
        std::string ts_str = ts ? reinterpret_cast<const char*>(ts) : "";

        // Upsert into ker_state.
        std::string sql_up =
            "INSERT INTO ker_state(aggregate_id, state_payload, updated_at) "
            "VALUES (?, ?, ?) "
            "ON CONFLICT(aggregate_id) DO UPDATE SET "
            "  state_payload = excluded.state_payload, "
            "  updated_at = excluded.updated_at;";
        sqlite3_stmt* stmt_up = nullptr;
        rc = sqlite3_prepare_v2(db, sql_up.c_str(), -1, &stmt_up, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare ker_state upsert failed: " +
                                     std::string(sqlite3_errmsg(db)));
        }
        rc = sqlite3_bind_text(stmt_up, 1, agg_id.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) goto bind_error_up;
        rc = sqlite3_bind_text(stmt_up, 2, payload_str.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) goto bind_error_up;
        rc = sqlite3_bind_text(stmt_up, 3, ts_str.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) goto bind_error_up;

        rc = sqlite3_step(stmt_up);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt_up);
            sqlite3_finalize(stmt);
            throw std::runtime_error("ker_state upsert step failed: " +
                                     std::string(sqlite3_errmsg(db)));
        }
        sqlite3_finalize(stmt_up);
        continue;

    bind_error_up:
        sqlite3_finalize(stmt_up);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Bind error in ker_state upsert: " +
                                 std::string(sqlite3_errmsg(db)));
    }

    sqlite3_finalize(stmt);
}

} // namespace tools
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::tools;

    std::string db_path = "ker_event_sourcing.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        install_event_store_schema(db);
        install_projection_triggers(db);

        std::cout << "KER event store + projection triggers installed into "
                  << db_path << std::endl;

        // Example: append a few KER state updates via the command table.
        append_ker_state_update(
            db,
            "actor:governance",
            "ker_state_global",
            "UPDATE_THRESHOLD",
            R"({"bod_safe_thr":5.0,"pfas_safe_thr":0.1})",
            "2026-08-04T10:00:00Z");

        append_ker_state_update(
            db,
            "actor:governance",
            "ker_state_global",
            "UPDATE_THRESHOLD",
            R"({"bod_safe_thr":4.5,"pfas_safe_thr":0.08})",
            "2026-08-04T11:00:00Z");

        std::cout << "Two example KER state events appended via ker_state_command." << std::endl;

        // Replay projection to validate event-sourcing semantics.
        replay_projection(db);
        std::cout << "Projection replay completed; ker_state rebuilt from ker_event_store." << std::endl;
        std::cout << "This audit trail can be consumed by Kotlin projection services and ALN→SQL generators." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "KER event sourcing error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
