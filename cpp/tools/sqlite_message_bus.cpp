// File: cpp/tools/sqlite_message_bus.cpp

#include <string>
#include <stdexcept>
#include <sqlite3.h>

// Simple SQLite-backed message bus with command and event queues.
class SQLiteMessageBus {
public:
    explicit SQLiteMessageBus(sqlite3* db)
        : db_(db) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
        configureWAL();
        createQueues();
    }

    void enqueueCommand(const std::string& target,
                        const std::string& payloadJson) {
        const char* sql =
            "INSERT INTO cmd_queue (target, payload_json, created_utc) "
            "VALUES (?, ?, strftime('%s','now'));";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare cmd_queue insert");
        }
        sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, payloadJson.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert command");
        }
        sqlite3_finalize(stmt);
    }

    // Poll next command for a component and mark it processed.
    bool dequeueCommand(const std::string& target,
                        std::string& payloadJsonOut) {
        // Begin immediate transaction to lock row while reading.
        execSql("BEGIN IMMEDIATE;");
        const char* sql =
            "SELECT cmd_id, payload_json "
            "FROM cmd_queue "
            "WHERE target = ? AND processed_utc IS NULL "
            "ORDER BY created_utc ASC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            execSql("ROLLBACK;");
            throw std::runtime_error("Failed to prepare cmd_queue select");
        }
        sqlite3_bind_text(stmt, 1, target.c_str(), -1, SQLITE_TRANSIENT);

        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            execSql("ROLLBACK;");
            return false;
        }

        int cmdId = sqlite3_column_int(stmt, 0);
        const unsigned char* txt = sqlite3_column_text(stmt, 1);
        payloadJsonOut = txt ? reinterpret_cast<const char*>(txt) : "";
        sqlite3_finalize(stmt);

        // Mark processed
        sqlite3_stmt* stmtUpdate = nullptr;
        const char* sqlUpdate =
            "UPDATE cmd_queue "
            "SET processed_utc = strftime('%s','now') "
            "WHERE cmd_id = ?;";
        if (sqlite3_prepare_v2(db_, sqlUpdate, -1, &stmtUpdate, nullptr) != SQLITE_OK) {
            execSql("ROLLBACK;");
            throw std::runtime_error("Failed to prepare cmd_queue update");
        }
        sqlite3_bind_int(stmtUpdate, 1, cmdId);

        if (sqlite3_step(stmtUpdate) != SQLITE_DONE) {
            sqlite3_finalize(stmtUpdate);
            execSql("ROLLBACK;");
            throw std::runtime_error("Failed to mark command processed");
        }
        sqlite3_finalize(stmtUpdate);
        execSql("COMMIT;");
        return true;
    }

    void enqueueEvent(const std::string& source,
                      const std::string& payloadJson) {
        const char* sql =
            "INSERT INTO event_queue (source, payload_json, created_utc) "
            "VALUES (?, ?, strftime('%s','now'));";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare event_queue insert");
        }
        sqlite3_bind_text(stmt, 1, source.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, payloadJson.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert event");
        }
        sqlite3_finalize(stmt);
    }

private:
    sqlite3* db_;

    void execSql(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "SQLite error: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }

    void configureWAL() {
        execSql("PRAGMA journal_mode = WAL;");
        execSql("PRAGMA synchronous = NORMAL;");
    }

    void createQueues() {
        const char* cmdSql =
            "CREATE TABLE IF NOT EXISTS cmd_queue ("
            " cmd_id       INTEGER PRIMARY KEY AUTOINCREMENT,"
            " target       TEXT NOT NULL,"
            " payload_json TEXT NOT NULL,"
            " created_utc  INTEGER NOT NULL,"
            " processed_utc INTEGER"
            ");";
        execSql(cmdSql);

        const char* eventSql =
            "CREATE TABLE IF NOT EXISTS event_queue ("
            " event_id     INTEGER PRIMARY KEY AUTOINCREMENT,"
            " source       TEXT NOT NULL,"
            " payload_json TEXT NOT NULL,"
            " created_utc  INTEGER NOT NULL"
            ");";
        execSql(eventSql);
    }
};
